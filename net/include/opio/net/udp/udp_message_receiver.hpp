#pragma once

#include <array>
#include <stdexcept>
#include <memory>

#include <opio/net/asio_include.hpp>

#include <opio/binary_view_fmt.hpp>
#include <opio/exception.hpp>

#include <opio/log.hpp>

#include <opio/net/udp/udp_message_receiver_fwd.hpp>

namespace opio::net::udp
{

//
// udp_message_receiver_t
//

/**
 * @brief A generic UDP message receiver that wraps transport details
 *        and delegates messages it receives (known to him as raw bytes)
 *        to a handler.
 *
 * @tparam Message_Handler  A function object capable of handling a message (span).
 *
 * Example usage:
 * @code
 * using msg_handler_t = std::function< void (std::span< const std::byte > ) >;
 * using msg_receiver_t = feed::udp_message_receiver_t< msg_handler_t >;
 * @endcode
 */
template < typename Message_Handler, typename Logger >
class udp_message_receiver_t
    : public std::enable_shared_from_this<
          udp_message_receiver_t< Message_Handler, Logger > >
{
public:
    using message_handler_t = Message_Handler;
    using logger_t          = Logger;
    using self_t = udp_message_receiver_t< message_handler_t, logger_t >;
    using sptr_t = std::shared_ptr< self_t >;

    explicit udp_message_receiver_t( asio_ns::io_context & ioctx,
                                     const std::string & device_iface,
                                     asio_ns::ip::address multicast_address,
                                     std::uint16_t multicast_port,
                                     Logger logger,
                                     message_handler_t message_handler )
        : m_ioctx{ ioctx }
        , m_socket{ m_ioctx.get_executor() }
        , m_logger{ std::move( logger ) }
        , m_message_handler{ std::move( message_handler ) }
    {
        asio_ns::ip::udp::endpoint listen_endpoint(
            asio_ns::ip::make_address( "0.0.0.0" ), multicast_port );

        // Join the multicast group.
        m_socket.open( listen_endpoint.protocol() );
        m_socket.set_option( asio_ns::ip::udp::socket::reuse_address( true ) );

        if( !device_iface.empty() )
        {
#if defined( _WIN32 )
            throw_exception( "Setting SO_BINDTODEVICE not supported on windows" );
#else
            if( 0
                != setsockopt( m_socket.native_handle(),
                               SOL_SOCKET,
                               SO_BINDTODEVICE,
                               device_iface.c_str(),
                               device_iface.size() ) )
            {
                throw_exception( "Setting SO_BINDTODEVICE to \"{}\" failed",
                                 device_iface );
            }
#endif  // defined(_WIN32)
        }
        m_socket.bind( listen_endpoint );

        asio_ns::error_code ec;
        m_socket.set_option(
            asio_ns::ip::multicast::join_group( multicast_address ), ec );
        m_socket.non_blocking( true );

        asio_ns::socket_base::receive_buffer_size receive_buffer_option{ 8 * 1024
                                                                         * 1024 };

        // Maybe needs adjustments for `net.core.rmem_max`:
        // sysctl net.core.rmem_max
        // sudo sysctl -w net.core.rmem_max=N
        // sudo sysctl -p
        m_socket.set_option( receive_buffer_option );
        m_socket.get_option( receive_buffer_option );

        if( ec )
        {
            throw_exception(
                "Unable to join socket to group (multicast_address={}), "
                "listen_on={}): {}",
                multicast_address.to_string(),
                listen_endpoint.address().to_string(),
                net::fmt_integrator( ec ) );
        }

        m_logger.info( OPIO_SRC_LOCATION, [ & ]( auto out ) {
            format_to( out,
                       "start receiving (multicast={}:{}, listen_on={}, "
                       "rcvbuf.size={})",
                       multicast_address.to_string(),
                       multicast_port,
                       listen_endpoint.address().to_string(),
                       receive_buffer_option.value() );
        } );
    }

    void start_listening()
    {
        if( !m_receiving )
        {
            m_logger.info( OPIO_SRC_LOCATION, "start listening for udp" );
            asio_ns::post( m_ioctx, [ self = this->shared_from_this() ] {
                self->async_receive();
            } );
            m_receiving = true;
        }
        else
        {
            m_logger.warn( OPIO_SRC_LOCATION,
                           "duplicate start listening for udp: ignored" );
        }
    }

    void stop_listening()
    {
        if( m_receiving )
        {
            m_receiving = false;
            asio_ns::post( m_ioctx, [ self = this->shared_from_this() ] {
                self->m_logger.info( OPIO_SRC_LOCATION,
                                     "cancel receive operations" );
                self->m_socket.cancel();
            } );
        }
        else
        {
            m_logger.warn( OPIO_SRC_LOCATION,
                           "unepected stop listening for udp: ignored" );
        }
    }

    message_handler_t & handler() noexcept { return m_message_handler; }

    const message_handler_t & handler() const noexcept
    {
        return m_message_handler;
    }

private:
    void async_receive()
    {
        m_logger.trace( OPIO_SRC_LOCATION, "schedule async receive" );

        m_socket.async_receive_from(
            asio_ns::buffer( m_buffer.data(), m_buffer.size() ),
            m_sender_ep,
            [ self = this->shared_from_this() ]( auto ec, auto length ) {
                self->handle_received( ec, length );
            } );
    }

    void handle_received( asio_ns::error_code ec, std::size_t length )
    {
        while( true )
        {
            if( !ec )
            {
                udp_raw_message_t raw_message{ raw_data_span_t{ m_buffer.cbegin(),
                                                                length } };

                m_logger.trace( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                    format_to( out,
                               "received message from {}:{}\n{}",
                               m_sender_ep.address().to_string(),
                               m_sender_ep.port(),
                               make_binary_view_fmt( raw_message.raw_data() ) );
                } );
                try
                {
                    m_message_handler( raw_message );
                }
                catch( const std::exception & ex )
                {
                    m_logger.error( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                        format_to( out, "Failed rceive operation: {}", ex.what() );
                    } );
                    m_logger.flush();
                    std::abort();
                }
            }
            else if( net::error_is_operation_aborted( ec ) )
            {
                m_logger.debug( OPIO_SRC_LOCATION, "abort receiving messages" );
                return;
            }
            else
            {
                m_logger.error( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                    format_to( out,
                               "Failed rceive operation: {}",
                               net::fmt_integrator( ec ) );
                } );
                break;
            }

            length = m_socket.receive_from(
                asio_ns::buffer( m_buffer.data(), m_buffer.size() ),
                m_sender_ep,
                asio_ns::socket_base::message_flags( 0 ),
                ec );

            if( error_is_would_block( ec ) )
            {
                break;
            }
        }

        async_receive();
    }

    asio_ns::io_context & m_ioctx;
    asio_ns::ip::udp::socket m_socket;

    [[no_unique_address]] logger_t m_logger;
    message_handler_t m_message_handler{};

    // A placeholder variable for async receive operations.
    asio_ns::ip::udp::endpoint m_sender_ep;

    inline static constexpr std::size_t max_udp_message_buffer = 1ull << 16;
    alignas( 128 ) std::array< byte_t, ( 1 << 16 ) > m_buffer;

    // That places passed the above as it would be used
    // in the thread other than the io.
    std::atomic_bool m_receiving{ false };
};

}  // namespace opio::net::udp
