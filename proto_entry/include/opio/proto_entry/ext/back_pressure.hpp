/**
 * @file
 *
 * This header file contains a back pressure add-ons for protocol entry.
 *
 * This routine first lived in client_server_utils project,
 * but was moved here to contribute to overal functionoality of the projects and
 * also to complement a tests suite of `proto_entry` project.
 *
 * @since v1.0.0
 */

#pragma once

#include <optional>

#if defined( OPIO_USE_BOOST_ASIO )
#    include <boost/container/flat_map.hpp>
#else  // defined( OPIO_USE_BOOST_ASIO )
#    include <unordered_map>
#endif  // defined( OPIO_USE_BOOST_ASIO )

#include <opio/proto_entry/entry_base.hpp>

namespace opio::proto_entry::ext
{

//
// bp_entry_t
//

/**
 * @brief An extension class to add back pressure to entry.
 *
 * Acts as a extend-through-inheritance class for eventual protocol entry type.
 *
 * @tparam Entry       A type of entry to inherit from.
 * @tparam Stream_Tag  A type of a tag to distinguish streams.
 *
 * @since v1.0.0
 */
template < typename Entry, typename Stream_Tag >
class bp_entry_t : public Entry
{
public:
    using sptr_t       = std::shared_ptr< bp_entry_t >;
    using wptr_t       = std::weak_ptr< bp_entry_t >;
    using base_type_t  = Entry;
    using stream_tag_t = Stream_Tag;
    using buffer_t     = typename base_type_t::buffer_driver_t::output_buffer_t;

protected:
    friend base_type_t;

    template < typename... Args >
    bp_entry_t( Args &&... args )
        : base_type_t{ std::forward< Args >( args )... }
    {
        m_streams.reserve( 32 );
    }

    /**
     * @brief Implementation of pkg-msg handling with this type.
     *
     * Introduces override that act on the level of this type so
     * the eventual type is known.
     */
    base_type_t::package_handling_result handle_incoming_message(
        const ::opio::proto_entry::pkg_header_t & header,
        ::opio::proto_entry::pkg_input_base_t & stream ) override
    {
        return base_type_t::handle_incoming_message_custom(
            header, stream, *this );
    }

public:
    [[nodiscard]] sptr_t shared_from_this()
    {
        return std::dynamic_pointer_cast< bp_entry_t >(
            this->base_type_t::shared_from_this() );
    }

    [[nodiscard]] wptr_t weak_from_this()
    {
        return wptr_t( this->shared_from_this() );
    }

    /**
     * @brief Factory to create an entry object.
     *
     * Delegates creation to base class
     */
    template < typename... Args >
    [[nodiscard]] static sptr_t make( Args &&... args )
    {
        return base_type_t::template make_custom< bp_entry_t >(
            std::forward< Args >( args )... );
    }

    /**
     * @brief Send back pressure controlled piece of data.
     *
     * Checks if for a given stream a piece of data is already in flight
     * and if so it stores the buffer passed as a parameter as the one
     * to be send next once the previous send would be reported as written to
     * socket. While there is data in flight (meaning a buffer scheduled for send
     * previously was not reported as written to socket) we can experience
     * `bp_send_raw_buf()` calls multiple times but only latest call would be
     * meaningfull leaving the buffer that would be considered for future send.
     *
     * You can think of it as if we have a single slot to store a buffer
     * and while we can't write it to socket (because we implement back pressure)
     * we simply drop any outdated pieces of data.
     *
     * @param tag  Tag value to identify a stream.
     * @param buf  A buffer to append to stream.
     */
    void bp_send_raw_buf( stream_tag_t tag, buffer_t buf )
    {
        opio::net::asio_ns::dispatch(
            this->strand(),
            [ tag,
              buf  = std::move( buf ),
              self = this->shared_from_this() ]() mutable {
                self->logger().trace( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                    format_to( out,
                               "BP, handling back pressure "
                               "controlled send, tag: {}",
                               tag );
                } );

                auto & ctx = self->m_streams[ tag ];

                if( ctx.in_flight < 1 ) [[likely]]
                {
                    self->logger().trace( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                        format_to( out,
                                   "BP, buffer will be sent right away, tag: {}",
                                   tag );
                    } );

                    ++ctx.in_flight;
                    self->send_buffer( tag, std::move( buf ) );

                    return;
                }

                if( ctx.memorized_buf )
                {
                    ++ctx.dropped_bufs;
                }

                auto memorization_digest = [ & ]( auto out ) {
                    format_to( out,
                               "BP, substitute memorized buffer, tag: "
                               "{}, dropped_before: {}",
                               tag,
                               ctx.dropped_bufs );

                    if( logr::log_message_level::trace
                        == self->logger().log_level() )
                    {
                        format_to( out, "; {}", buf_fmt_integrator( buf ) );
                    }
                };

                static constexpr std::size_t period_of_way_too_much_drops = 128;
                if( 1 == ctx.dropped_bufs || 10 == ctx.dropped_bufs
                    || ( 0 != ctx.dropped_bufs
                         && 0
                                == ( ctx.dropped_bufs
                                     % period_of_way_too_much_drops ) ) )
                {
                    // 1 dropped - warning;
                    // 10 dropped - warning;
                    // each 128 dropped - warning.
                    self->logger().warn( OPIO_SRC_LOCATION, memorization_digest );
                }
                else
                {
                    self->logger().trace( OPIO_SRC_LOCATION, memorization_digest );
                }

                ctx.memorized_buf = std::move( buf );
            } );
    }

private:
    void send_buffer( stream_tag_t tag, buffer_t && buf )
    {
        this->schedule_send_raw_bufs_with_cb(
            [ tag, wp = this->weak_from_this() ]( auto res ) {
                if( opio::net::tcp::send_buffers_result::success != res )
                {
                    return;
                }

                if( auto s = wp.lock(); s )
                {
                    s->logger().trace( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                        format_to( out, "BP, buffer was sent, tag: {}", tag );
                    } );
                    opio::net::asio_ns::post(
                        s->strand(), [ s, tag ] { s->send_finished( tag ); } );
                }
            },
            std::move( buf ) );
    }

    void send_finished( stream_tag_t tag )
    {
        this->logger().trace( OPIO_SRC_LOCATION, [ & ]( auto out ) {
            format_to( out, "BP, previous send finished, tag: {}", tag );
        } );

        auto & ctx = m_streams[ tag ];
        if( ctx.memorized_buf )
        {
            this->logger().trace( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                format_to(
                    out, "BP, sending latest memorized buffer, tag: {}", tag );
            } );

            buffer_t buf = std::move( *ctx.memorized_buf );
            ctx.memorized_buf.reset();
            send_buffer( tag, std::move( buf ) );
            ctx.dropped_bufs = 0;
        }
        else
        {
            this->logger().trace( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                format_to( out, "BP, nothing to followup for tag: {}", tag );
            } );
            --ctx.in_flight;
        }
    }

    /**
     * @brief A context for a given stream subjected to back pressure.
     */
    struct stream_context_t
    {
        int in_flight{};
        int dropped_bufs{};
        std::optional< buffer_t > memorized_buf;
    };

    using stream_table_t =
#if defined( OPIO_USE_BOOST_ASIO )
        boost::container::flat_map< stream_tag_t, stream_context_t >;
#else   // defined( OPIO_USE_BOOST_ASIO )
        std::unordered_map< stream_tag_t, stream_context_t >;
#endif  // defined( OPIO_USE_BOOST_ASIO )

    stream_table_t m_streams;
};

}  // namespace opio::proto_entry::ext
