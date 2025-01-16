#pragma once

#include <variant>

#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

#include <opio/net/tcp/connection.hpp>
#include <opio/net/heterogeneous_buffer.hpp>

#include <opio/log.hpp>
#include <opio/proto_entry/cfg.hpp>
#include <opio/proto_entry/pkg_input.hpp>
#include <opio/proto_entry/utils.hpp>
#include <opio/proto_entry/message_carrier.hpp>

#include <opio/proto_entry/impl/protobuf_parsing_engines.hpp>

namespace opio::proto_entry
{

namespace details
{

//
// make_global_unique_connection_id()
//

/**
 * @brief Created a unique connection id.
 *
 * @return Uniqe id for connections.
 */
opio::net::tcp::connection_id_t make_global_unique_connection_id() noexcept;

//
// param_giveaway()
//

/**
 * @brief A helper funcion for entry_ctor_params_t.
 *
 * Provides a entry constructor parameter from `optional<T>` value holder.
 * If the type allows default value then it is used as a fallback option.
 */
template < typename Param_Type >
static Param_Type param_giveaway( std::optional< Param_Type > param,
                                  std::string_view param_name )
{
    if( !param )
    {
        if constexpr( std::is_default_constructible_v< Param_Type > )
        {
            return Param_Type{};
        }
        else
        {
            throw std::runtime_error{ fmt::format(
                "entry parameter {} "
                "must be explicitly set "
                "(the type is not default constructible)",
                param_name ) };
        }
    }

    Param_Type result{ std::move( *param ) };
    return result;
}

//
//  safe_make_message_consumer()
//

/**
 * @name Customization for a safely getting the eventual  message consumer.
 */
///@{

/**
 * @brief The version of safely getting the eventual message consumer
 *        for an instance .
 */
template < typename Message_Consumer >
auto safe_make_message_consumer( std::optional< Message_Consumer > param )
{
    return param_giveaway( std::move( param ), "message_consumer" );
}

/**
 * @brief The version of safely getting the eventual message consumer
 *        for pointer.
 */
template < typename Message_Consumer >
auto safe_make_message_consumer( std::optional< Message_Consumer * > param )
{
    if( !param )
    {
        throw std::runtime_error{
            "entry parameter message_consumer "
            "must be explicitly set "
            "(the type is a pointer, default value is not allowed)"
        };
    }

    return *param;
}

/**
 * @brief The version of safely getting the eventual message consumer
 *        unique pointer.
 */
template < typename Message_Consumer >
auto safe_make_message_consumer(
    std::optional< std::unique_ptr< Message_Consumer > > param )
{
    if( !param )
    {
        throw std::runtime_error{
            "entry parameter message_consumer is unique_ptr<T> "
            "and must be explicitly set "
            "(default value is not allowed)"
        };
    }

    return std::move( *param );
}

/**
 * @brief The version of safely getting the eventual message consumer
 *        for shared pointer.
 */
template < typename Message_Consumer >
auto safe_make_message_consumer(
    std::optional< std::shared_ptr< Message_Consumer > > param )
{
    if( !param )
    {
        throw std::runtime_error{
            "entry parameter message_consumer is shared_ptr<T> "
            "and must be explicitly set "
            "(default value is not allowed)"
        };
    }

    return *param;
}
///@}

/**
 * @name Customization for running lambda with various types of message consumers
 *       in a unified way.
 *
 * Those customization allow to write code for doing smth.
 * with different types of message consumers seamlessly.
 * E.g. for pointer types (`Consumer*`) it will use arrow method calls
 * for object types it will use dot method calls.
 * So the functions in this group allow to nail all down to a code
 * that works with reference.
 *
 * @code
 *   auto consumer = std::make_unique<some_consumer_t >( params );
 *   details::execute_for_reference( consumer, [&]( auto & consumer_ref ){
 *     assert( consumer.get(), &ref );
 *     // Here you can work with `consumer_ref`
 *     // which is reference.
 *     do_smth_with_ref( consumer_ref );
 *   } );
 * @endcode
 */
///@{

template < typename Message_Consumer, typename F >
void execute_for_reference_impl( Message_Consumer & message_consumer, F f )
{
    f( message_consumer );
}

template < typename Message_Consumer, typename F >
void execute_for_reference( Message_Consumer & message_consumer, F f )
{
    execute_for_reference_impl( message_consumer, std::move( f ) );
}

template < typename Message_Consumer, typename F >
void execute_for_reference( Message_Consumer * message_consumer, F f )
{
    assert( message_consumer );
    execute_for_reference_impl( *message_consumer, std::move( f ) );
}

template < typename Message_Consumer, typename F >
void execute_for_reference( std::unique_ptr< Message_Consumer > & message_consumer,
                            F f )
{
    assert( message_consumer );
    execute_for_reference_impl( *message_consumer, std::move( f ) );
}

template < typename Message_Consumer, typename F >
void execute_for_reference( std::shared_ptr< Message_Consumer > & message_consumer,
                            F f )
{
    assert( message_consumer );
    execute_for_reference( *message_consumer, std::move( f ) );
}

template < typename Message_Consumer, typename F >
void execute_for_reference( std::weak_ptr< Message_Consumer > & message_consumer,
                            F f )
{
    if( auto real_consumer = message_consumer.lock(); real_consumer )
    {
        execute_for_reference( *real_consumer, std::move( f ) );
    }
}

template < typename Consumer, typename Message, typename Entry, typename = void >
struct on_message_callback_support_ext_info : std::false_type
{
};

//
// consume_message()
//

/**
 * @brief Call a consumer hook as necessary.
 */
template < typename Message_Consumer, typename Message, typename Entry >
void consume_message( Message_Consumer & message_consumer,
                      Message message,
                      Entry & entry )
{
    execute_for_reference( message_consumer, [ & ]( auto & mc ) {
        mc.on_message( std::move( message ), entry );
    } );
}
///@}

}  // namespace details

//
// connection_shutdown_reason
//

/**
 * @brief Flags for connection shutdown reason.
 *
 * Acts as an argument for shutdown handler assigned to connection.
 */
enum class entry_shutdown_reason
{
    underlying_connection,
    user_initiated,
    exception_handling_input,
    invalid_input_package,
    unexpected_input_package_size,
    invalid_input_package_size,
    invalid_heartbeat_package,
    unknown_pkg_content_type,
    hearbeat_reply_timeout,
};

//
// connection_shutdown_context_t
//

/**
 * @brief The context regarding entry's shutdown event.
 *
 * @since v0.8.0.
 */
struct connection_shutdown_context_t
{
    explicit connection_shutdown_context_t( entry_shutdown_reason r,
                                            std::string err_msg )
        : reason{ r }
        , err_message{ std::move( err_msg ) }
    {
    }

    explicit connection_shutdown_context_t( entry_shutdown_reason r )
        : connection_shutdown_context_t{ r, "" }
    {
    }

    explicit connection_shutdown_context_t(
        opio::net::tcp::connection_shutdown_reason r )
        : reason{ entry_shutdown_reason::underlying_connection }
        , underlying_reason{ r }
    {
    }

    entry_shutdown_reason reason;
    std::optional< opio::net::tcp::connection_shutdown_reason > underlying_reason;
    std::string err_message;
};

//
// shutdown_handler_t
//

using shutdown_handler_t =
    std::function< void( opio::net::tcp::connection_id_t conn_id ) >;

//
// shutdown_handler2_t
//

/**
 * @brief A complete context aware shutdown handler.
 *
 * @since v0.8.0.
 */
using shutdown_handler2_t =
    std::function< void( opio::net::tcp::connection_id_t conn_id,
                         connection_shutdown_context_t ) >;

namespace details
{

struct shutdown_not_supplied_t
{
};
struct shutdown_was_called_t
{
};

// Helper to call variant visitor providing a bunch of lambdas.
// https://en.cppreference.com/w/cpp/utility/variant/visit
template < class... Ts >
struct overloaded_sh : Ts...
{
    using Ts::operator()...;
};
template < class... Ts >
overloaded_sh( Ts... ) -> overloaded_sh< Ts... >;

}  // namespace details

using shutdown_handler_variant_t = std::variant< details::shutdown_not_supplied_t,
                                                 details::shutdown_was_called_t,
                                                 shutdown_handler_t,
                                                 shutdown_handler2_t >;

//
// connection_ctor_params_base
//

/**
 * @brief Fluent interface param setter for creating an instance of
 *        protocol entries.
 *
 * Acts as a factory for parameters that entry requires for its
 * construction. The intention is to reduce the boilerplate necessary
 * to provide all the parameters.
 *
 * It allows to skip setting of parameters that are otherwise unavoidable
 * when using direct constructor or a fully parameterized `make()` function.
 *
 * If the user skips a setting of a given parameter
 * (`params.logger( make_my_logger() );` then a couple of options are possible:
 *
 *  - Connection id is generated (using internal unique counter).
 *
 * - If param type is default constructible then a default
 *   constructed instance would be used.
 *
 * - If the type is not default constructible an exception would be thrown.
 *
 * - For strand types (underlying connection strand and entries own strand)
 *   an instance of `optional<S>` is returned upon request, and the checks
 *   for default constructible property is not performed.
 *   The reason is that even in case when strand is not supplied by user
 *   and cannot be defaul constructed (which mean an exception considering only
 *   two first bullets) a constructor of the entry might consider
 *   another option - a constructor with executor as a parameter. The executor
 *   in that case is provided by `socket` instance. That works well
 *   with standard ASIO strand types: `any_executor` (aka noop strand)
 *   and `strand< any_io_executor >` (aka real strand).
 *
 * @note This class is not intended to be reused to create multiple instances
 *       of enties because of move-semantics approach. This means
 *       that once the object wos used for constructing an entry
 *       it moved out what it was holding and so is no longer the same.
 *
 * A sample of using this param factory:
 * @code
 * entry = sample_entry_t::make(
 *     std::move( socket ),
 *     [ & ]( auto & params ) {
 *         params
 *             .connection_id( 42 )
 *             .logger( make_logger() )
 *             .message_consumer( &message_consumer );
 *             .shutdown_handler( []( auto id ){
 *                 // ...
 *             } );
 *     } );

 * @endcode
 */
template < typename Traits, typename Message_Consumer >
class entry_ctor_params_t
{
public:
    using socket_io_operation_watchdog_t =
        typename Traits::socket_io_operation_watchdog_t;
    using buffer_driver_t           = typename Traits::buffer_driver_t;
    using underlying_stats_driver_t = typename Traits::underlying_stats_driver_t;

    using logger_t           = typename Traits::logger_t;
    using strand_t           = typename Traits::strand_t;
    using message_consumer_t = Message_Consumer;
    using stats_driver_t     = typename Traits::stats_driver_t;

    entry_ctor_params_t() = default;

    entry_ctor_params_t( const entry_ctor_params_t & ) = delete;
    entry_ctor_params_t & operator=( const entry_ctor_params_t & ) = delete;

    entry_ctor_params_t( entry_ctor_params_t && ) = default;
    entry_ctor_params_t & operator=( entry_ctor_params_t && ) = default;
    //============================================================

    entry_ctor_params_t & connection_id(
        opio::net::tcp::connection_id_t conn_id ) & noexcept
    {
        m_conn_id = conn_id;
        return *this;
    }

    entry_ctor_params_t && connection_id(
        opio::net::tcp::connection_id_t conn_id ) && noexcept
    {
        return std::move( this->connection_id( conn_id ) );
    }

    opio::net::tcp::connection_id_t connection_id() noexcept
    {
        if( !m_conn_id )
        {
            m_conn_id = details::make_global_unique_connection_id();
        }
        return *m_conn_id;
    }
    //============================================================

    entry_ctor_params_t & underlying_connection_cfg(
        opio::net::tcp::connection_cfg_t underlying_cfg ) & noexcept
    {
        m_underlying_cfg = underlying_cfg;
        return *this;
    }

    entry_ctor_params_t && underlying_connection_cfg(
        opio::net::tcp::connection_cfg_t underlying_cfg ) && noexcept
    {
        return std::move( this->connection_cfg( underlying_cfg ) );
    }

    const opio::net::tcp::connection_cfg_t & underlying_connection_cfg()
        const noexcept
    {
        return m_underlying_cfg;
    }
    //============================================================

    entry_ctor_params_t & logger( logger_t log ) &
    {
        m_logger.emplace( std::move( log ) );
        return *this;
    }

    entry_ctor_params_t && logger( logger_t log ) &&
    {
        return std::move( this->logger( std::move( log ) ) );
    }

    logger_t logger_giveaway()
    {
        return details::param_giveaway( std::move( m_logger ), "logger" );
    }
    //============================================================

    entry_ctor_params_t & buffer_driver( buffer_driver_t buffer_driver ) &
    {
        m_buffer_driver.emplace( std::move( buffer_driver ) );
        return *this;
    }

    entry_ctor_params_t && buffer_driver( buffer_driver_t buffer_driver ) &&
    {
        return std::move( this->buffer_driver( std::move( buffer_driver ) ) );
    }

    buffer_driver_t buffer_driver_giveaway()
    {
        return details::param_giveaway( std::move( m_buffer_driver ),
                                        "buffer_driver" );
    }
    //============================================================

    entry_ctor_params_t & operation_watchdog(
        socket_io_operation_watchdog_t operation_watchdog ) &
    {
        m_operation_watchdog.emplace( std::move( operation_watchdog ) );
        return *this;
    }

    entry_ctor_params_t && operation_watchdog(
        socket_io_operation_watchdog_t operation_watchdog ) &&
    {
        return std::move(
            this->operation_watchdog( std::move( operation_watchdog ) ) );
    }

    std::optional< socket_io_operation_watchdog_t > operation_watchdog_giveaway()
    {
        // We don't use `giveaway(p)` here because we can try
        // one more approach:
        //     When we are creating an instance of connection we
        //     have a socket and we can get its executor
        //     (`sock.get_executor()`) so we can also check
        //     if we can construct operation_watchdog of a given type
        //     with executor as parameter.
        //
        // So here we leave a higher level to decide whether it can handle it
        // or not.
        return std::move( m_operation_watchdog );
    }
    //============================================================

    entry_ctor_params_t & underlying_stats_driver(
        underlying_stats_driver_t underlying_stats_driver ) &
    {
        m_underlying_stats_driver.emplace( std::move( underlying_stats_driver ) );
        return *this;
    }

    entry_ctor_params_t && underlying_stats_driver(
        underlying_stats_driver_t underlying_stats_driver ) &&
    {
        return std::move( this->underlying_stats_driver(
            std::move( underlying_stats_driver ) ) );
    }

    underlying_stats_driver_t underlying_stats_driver_giveaway()
    {
        return details::param_giveaway( std::move( m_underlying_stats_driver ),
                                        "underlying_stats_driver" );
    }
    //============================================================

    entry_ctor_params_t & strand( strand_t strand ) &
    {
        m_strand.emplace( std::move( strand ) );
        return *this;
    }

    entry_ctor_params_t && strand( strand_t strand ) &&
    {
        return std::move( this->strand( std::move( strand ) ) );
    }

    std::optional< strand_t > strand_giveaway()
    {
        // We don't use `giveaway(p)` here because we can try
        // one more approach:
        //     When we are creating an instance of connection we
        //     have a socket and we can get its executor
        //     (`sock.get_executor()`) so we can also check
        //     if we can construct strand of a given type
        //     with executor as parameter.
        //
        // So here we leave a higher level to decide whether it can handle it
        // or not.
        return std::move( m_strand );
    }
    //============================================================

    entry_ctor_params_t & entry_config( entry_cfg_t cfg ) & noexcept
    {
        m_cfg = cfg;
        return *this;
    }

    entry_ctor_params_t && entry_config( entry_cfg_t cfg ) && noexcept
    {
        return std::move( this->entry_config( cfg ) );
    }

    const entry_cfg_t & entry_config() const noexcept
    {
        using namespace std::chrono;
        return m_cfg;
    }
    //============================================================

    entry_ctor_params_t & shutdown_handler( shutdown_handler_t shutdown_handler ) &
    {
        m_shutdown_handler = std::move( shutdown_handler );
        return *this;
    }

    entry_ctor_params_t && shutdown_handler(
        shutdown_handler_t shutdown_handler ) &&
    {
        return std::move(
            this->shutdown_handler( std::move( shutdown_handler ) ) );
    }

    entry_ctor_params_t & shutdown_handler(
        shutdown_handler2_t shutdown_handler ) &
    {
        m_shutdown_handler = std::move( shutdown_handler );
        return *this;
    }

    entry_ctor_params_t && shutdown_handler(
        shutdown_handler2_t shutdown_handler ) &&
    {
        return std::move(
            this->shutdown_handler( std::move( shutdown_handler ) ) );
    }

    shutdown_handler_variant_t shutdown_handler_giveaway()
    {
        return std::move( m_shutdown_handler );
    }
    //============================================================

    entry_ctor_params_t & message_consumer( message_consumer_t message_consumer ) &
    {
        m_message_consumer.emplace( std::move( message_consumer ) );
        return *this;
    }

    entry_ctor_params_t && message_consumer(
        message_consumer_t message_consumer ) &&
    {
        return std::move(
            this->message_consumer( std::move( message_consumer ) ) );
    }

    message_consumer_t message_consumer_giveaway()
    {
        return details::safe_make_message_consumer(
            std::move( m_message_consumer ) );
    }
    //============================================================

    entry_ctor_params_t & stats_driver( stats_driver_t stats_driver ) &
    {
        m_stats_driver.emplace( std::move( stats_driver ) );
        return *this;
    }

    entry_ctor_params_t && stats_driver( stats_driver_t stats_driver ) &&
    {
        return std::move( this->stats_driver( std::move( stats_driver ) ) );
    }

    stats_driver_t stats_driver_giveaway()
    {
        return details::param_giveaway( std::move( m_stats_driver ),
                                        "stats_driver" );
    }
    //============================================================

private:
    std::optional< opio::net::tcp::connection_id_t > m_conn_id;
    opio::net::tcp::connection_cfg_t m_underlying_cfg{};
    std::optional< logger_t > m_logger;

    std::optional< buffer_driver_t > m_buffer_driver;
    std::optional< socket_io_operation_watchdog_t > m_operation_watchdog;
    std::optional< underlying_stats_driver_t > m_underlying_stats_driver;

    std::optional< strand_t > m_strand;
    entry_cfg_t m_cfg;
    shutdown_handler_variant_t m_shutdown_handler;
    std::optional< message_consumer_t > m_message_consumer;
    std::optional< stats_driver_t > m_stats_driver;
};

//
// entry_base_t
//

/**
 * @brief A base class to  service a protocol based on a set of protobuf messages.
 *
 * @tparam Underlying_Connection  Underlying bytes IO-stream connection.
 * @tparam Package_Message        "Package" message which serves as a single
 *                                point to aggregate protocol message,
 *                                which is a matter of contract between peers
 *                                any paylod of a business package is
 *                                a serialization of `Package_Message`,
 *                                so both parties knows what to parse
 *                                from the package payload.
 *
 * This base class provides the following:
 *
 *   - It manages tcp connection object running on asio context,
 *     Consumes input bytes from tcp-connection and provides
 *     it with output bytes.
 *
 *
 *   - It serves as serialization/deserialization framework
 *     atop of which derrived classes build its lightweight
 *     selection logic. It means derrived class provides a set of
 *     `send()` overloads which receive various protocol messages.
 *     When a `send()` is called with a protocol message
 *     it is converted into a package (binary image of a Package_Message
 *     which wrapps a given message) and lets this base class handle the rest.
 *
 *   - It handles service packages (heartbeats), responding to
 *     heartbeat request and initiating heartbeat if no traffic
 *     happened in recent period (configurable timeout).
 *
 *   - Enables user to provide callback on connection events (disconnect).
 *
 * @note From here and further in comments related to this class,
 *       a term Client is used. The client within a context of this
 *       class and its descendants means a party that wants to receive protocol
 *       messages from peer and wants to send protocol messages to peer.
 *
 * @note Descendants for specific protocols are intended to be generated
 *       from spec files (json format meta that connects protocol messages
 *       with enum values and field names used in in "package" message).
 *       See ping-pong example. You should not write such boilerplate by hand
 *       as it is boring, repetitive and is very valnurable to copy paste
 *       errors.
 *
 * @todo Think of buffer reuse practice: comming input buffers can be
 *       stored to buffer-pool and later be used for carrying
 *       outpud data. The same mechanics can be achieved on
 *       the part of connection object, so that we may end up with
 *       some bunch of buffer reused in a ring fashion.
 */
template < typename Traits >
class entry_base_t : public std::enable_shared_from_this< entry_base_t< Traits > >
{
public:
    using traits_t = Traits;
    using socket_t = typename Traits::socket_t;
    using underlying_connection_strand_t =
        typename Traits::underlying_connection_strand_t;
    using socket_io_operation_watchdog_t =
        typename Traits::socket_io_operation_watchdog_t;

    using buffer_driver_t = typename Traits::buffer_driver_t;
    static_assert( ::opio::net::Buffer_Driver_Concept< buffer_driver_t > );

    using underlying_stats_driver_t = typename Traits::underlying_stats_driver_t;

    using strand_t = typename Traits::strand_t;
    using logger_t = typename Traits::logger_t;

    using weak_ptr_t = std::weak_ptr< entry_base_t >;

    template < typename Message >
    using protobuf_engine_t =
        typename Traits::template protobuf_parsing_engine_t< Message >;

    template < typename Message >
    using message_carrier_t = protobuf_engine_t< Message >::message_carrier_t;

    /**
     * @brief The handler of input supplied by
     *        underlying connection.
     *
     * Acts as a proxy which posts the input to the strand of the entry.
     */
    class raw_bytes_handler_t
    {
    public:
        raw_bytes_handler_t( strand_t strand, weak_ptr_t entry )
            : m_strand{ std::move( strand ) }
            , m_entry{ std::move( entry ) }
        {
        }

        /**
         * @brief Handles incoming data from remote host.
         */
        template < typename Input_Context >
        void operator()( Input_Context & ctx )
        {
            const auto input_buffer_size =
                buffer_driver_t::make_asio_const_buffer( ctx.buf() ).size();

            opio::net::asio_ns::dispatch(
                m_strand,
                [ buf = std::move( ctx.buf() ), entry_wp = m_entry ]() mutable {
                    // NOTE: this callback must be mutable to avoid copies.
                    if( auto entry = entry_wp.lock(); entry )
                    {
                        entry->handle_input( std::move( buf ) );
                    }
                } );

            const auto confugured_buf_size =
                ctx.connection().cfg().input_buffer_size();
            const auto read_buf_was_fully_utilized =
                latest_explicitly_allocated_read_buf_size ?
                    // If the latest read buffer was allocated explicitly
                    // we check with the allocated size.
                    latest_explicitly_allocated_read_buf_size == input_buffer_size
                    // if no, then we should compare to connection
                    // configured read buffer size
                    :
                    confugured_buf_size == input_buffer_size;

            // That is a hard max of what we read in one go.
            constexpr std::size_t size_buf_32mb = 32 * 1024 * 1024;

            if( read_buf_was_fully_utilized )
            {
                // We have a case the whole buffer
                // used for read operation was used,
                // so it makes sense to use a larger buffer
                // for next read operation.

                latest_explicitly_allocated_read_buf_size =
                    std::min( input_buffer_size * 2, size_buf_32mb );
            }
            else if( confugured_buf_size <= input_buffer_size )
            {
                // So we didn't utilze all buf capacity (using adjusted size)
                // but it is still smth. that is more than
                // a configured (default) size of a read buffer
                // connection operated with.
                // So we need to find a proper value for the next buffer size.

                // TODO: that can be optimized, for now
                //       it is just a simple approach that works.
                latest_explicitly_allocated_read_buf_size =
                    confugured_buf_size * 2;

                while( latest_explicitly_allocated_read_buf_size
                       < input_buffer_size )
                {
                    latest_explicitly_allocated_read_buf_size *= 2;
                    assert( latest_explicitly_allocated_read_buf_size
                            <= size_buf_32mb );
                }
            }
            else
            {
                // If we are here it means that for current chunk size
                // a defult connection buffer size is enaugh.
                latest_explicitly_allocated_read_buf_size = 0;
            }

            if( latest_explicitly_allocated_read_buf_size )
            {
                ctx.next_read_buffer(
                    ctx.connection().buffer_driver().allocate_input(
                        latest_explicitly_allocated_read_buf_size ) );
            }
        }

    private:
        strand_t m_strand;
        weak_ptr_t m_entry;

        /**
         * @brief What was the size of the latest explicitly alocated buffer
         *        for read operation.
         *
         * This is used to adjust the size of read buf in case
         * we observe that the read buffer is fully utilized.
         * In that case we start growing the input buffer
         * so that we can read more in one go.
         */
        std::size_t latest_explicitly_allocated_read_buf_size{};
    };

    /**
     * @brief Defines underlying connection traits
     *        which are aligned to be used with this entry.
     */
    struct underlying_connection_traits_t
    {
        using socket_t             = typename Traits::socket_t;
        using strand_t             = underlying_connection_strand_t;
        using logger_t             = typename Traits::logger_t;
        using operation_watchdog_t = socket_io_operation_watchdog_t;
        using buffer_driver_t      = typename Traits::buffer_driver_t;
        using stats_driver_t       = underlying_stats_driver_t;
        using input_handler_t      = raw_bytes_handler_t;
        using locking_t            = typename Traits::locking_t;
    };

    using underlying_connection_t =
        typename Traits::template underlying_connection_t<
            underlying_connection_traits_t >;
    using underlying_connection_sptr_t = typename underlying_connection_t::sptr_t;

    using input_buffer_t = typename underlying_connection_t::input_buffer_t;

protected:
    /**
     * @brief Constructs a base layer of a connection servicing a proto.
     *
     * @param  strand                      Strand for this connection.
     * @param  logger                      Logger for connection.
     * @param  cfg                         Configuration of the entry.
     * @param  buffer_driver               Buffer driver for entry and
     *                                     underlying connection.
     * @param  shutdown_handler            Entry shutdown handler.
     */
    entry_base_t( strand_t strand,
                  logger_t logger,
                  const entry_cfg_t & cfg,
                  buffer_driver_t buffer_driver,
                  shutdown_handler_variant_t shutdown_handler )
        : m_strand{ std::move( strand ) }
        , m_logger{ std::move( logger ) }
        , m_buffer_driver{ std::move( buffer_driver ) }
        , m_cfg{ cfg }
        , m_shutdown_handler{ std::move( shutdown_handler ) }
        , m_heartbeat_timer{ m_strand }
    {
        m_logger.trace( OPIO_SRC_LOCATION, [ this ]( auto out ) {
            format_to( out,
                       "start proto entry (@{})",
                       static_cast< const void * >( this ) );
        } );
    }

    /**
     * @brief Init underlying connection.
     *
     * @param socket              Connected socket to run entry on.
     * @param conn_id             Id to assign to connection.
     * @param underlying_cfg      Underlying connection object configuration.
     * @param operation_watchdog  Watchdog for write
     * @param stats               Stats counter for underlying connection.
     */
    void init_underlying_connection(
        socket_t socket,
        opio::net::tcp::connection_id_t conn_id,
        const opio::net::tcp::connection_cfg_t & underlying_cfg,
        socket_io_operation_watchdog_t operation_watchdog,
        underlying_stats_driver_t stats )
    {
        m_connection_id = conn_id;
        m_connection    = underlying_connection_t::make(
            std::move( socket ),
            conn_id,
            underlying_cfg,
            m_logger,
            m_buffer_driver,
            raw_bytes_handler_t{ m_strand, this->weak_from_this() },
            [ wp = this->weak_from_this(), s = m_strand ]( auto reason ) {
                if( auto entry = wp.lock(); entry )
                {
                    opio::net::asio_ns::post( s, [ entry, reason ] {
                        entry->handle_shutdown( reason );
                    } );
                }
            },
            std::move( operation_watchdog ),
            std::move( stats ) );

        this->logger().debug( OPIO_SRC_LOCATION, [ & ]( auto out ) {
            format_to( out,
                       "[{};cid:{}] init proto entry with connection ",
                       this->remote_endpoint_str(),
                       this->underlying_connection_id() );
        } );

        // As connection agent starts running
        // we can start reading from our connection.
        this->underlying_connection()->start_reading();

        // Start heartbeat mechanics.
        update_last_input_at();
        schedule_next_heartbeat_check();
    }

    virtual ~entry_base_t()
    {
        logger().trace( OPIO_SRC_LOCATION, [ & ]( auto out ) {
            if( underlying_connection() )
            {
                format_to( out,
                           "[{};cid:{}] finish proto entry (@{})",
                           this->remote_endpoint_str(),
                           this->underlying_connection_id(),
                           static_cast< const void * >( this ) );
            }
            else
            {
                format_to( out,
                           "[<uninitialized>;cid:<uninitialized>] "
                           "finish proto entry (@{})",
                           static_cast< const void * >( this ) );
            }
        } );
        logger().flush();
    }

public:
    /**
     * @brief Access logger.
     */
    [[nodiscard]] logger_t & logger() noexcept { return m_logger; }

    /**
     * @brief Get a strand.
     *
     * @since v0.11.0
     */
    [[nodiscard]] strand_t & strand() noexcept { return m_strand; }

    [[nodiscard]] buffer_driver_t & buffer_driver() noexcept
    {
        return m_buffer_driver;
    }

    /**
     * @brief Get the entry's config.
     *
     * @since v0.11.0
     */
    [[nodiscard]] const entry_cfg_t & cfg() const noexcept { return m_cfg; };

    /**
     * @brief Initiate closing of this entry.
     */
    void close()
    {
        // We use post here so that we don't end up
        // with a shutdown callback be called.
        // As it is the user who defines implementation
        // of the shutdown callback we choose the safest
        // approach here: `asio::post` which means
        // shutdown handler (if any) would be called separately
        // and later.
        opio::net::asio_ns::post( m_strand, [ self = this->shared_from_this() ] {
            self->shutdown_and_terminate( connection_shutdown_context_t{
                entry_shutdown_reason::user_initiated } );
        } );
    }

    /**
     * @brief Access underlying connection object.
     *
     * @return Reference to connection.
     */
    underlying_connection_t * underlying_connection() noexcept
    {
        return m_connection.get();
    }

    /**
     * @brief Access underlying connection object.
     *
     * @return Reference to connection.
     */
    const underlying_connection_t * underlying_connection() const noexcept
    {
        return m_connection.get();
    }

    /**
     * @brief Schedules sending of a raw buffers through
     *        underlying connection.
     */
    template < typename... Buffers >
    void schedule_send_raw_bufs( Buffers &&... bufs )
    {
        if( m_connection_is_active )
        {
            underlying_connection()->schedule_send(
                std::forward< Buffers >( bufs )... );
        }
    }

    /**
     * @brief Schedules sending of a raw buffers through
     *        underlying connection.
     */
    template < typename Buffer_Vec >
    void schedule_send_vec_raw_bufs( Buffer_Vec bufs )
    {
        if( m_connection_is_active )
        {
            underlying_connection()->schedule_send( std::move( bufs ) );
        }
    }

    /**
     * @brief Schedules sending of a raw buffers through
     *        underlying connection.
     */
    template < typename... Buffers >
    void schedule_send_raw_bufs_with_cb( ::opio::net::tcp::send_complete_cb_t cb,
                                         Buffers &&... bufs )
    {
        if( m_connection_is_active )
        {
            underlying_connection()->schedule_send_with_cb(
                std::move( cb ), std::forward< Buffers >( bufs )... );
        }
    }

    /**
     * @brief Schedules sending of a raw buffers through
     *        underlying connection.
     */
    template < typename Buffer_Vec >
    void schedule_send_vec_raw_bufs_with_cb(
        ::opio::net::tcp::send_complete_cb_t cb,
        Buffer_Vec bufs )
    {
        if( m_connection_is_active )
        {
            underlying_connection()->schedule_send_vec_with_cb(
                std::move( cb ), std::move( bufs ) );
        }
    }

    /**
     * @brief Get underlying connection id.
     *
     * @return Connection id.
     */
    opio::net::tcp::connection_id_t underlying_connection_id() const noexcept
    {
        return m_connection_id;
    }

    /**
     * @brief Get a remote endpoint string (like `ip:port`).
     */
    const std::string & remote_endpoint_str() const noexcept
    {
        return underlying_connection()->remote_endpoint_str();
    }

protected:
    /**
     * @brief Handle a portion of raw input bytes from connection.
     *
     * When a new buffer comes from connection-object
     * (the one that runs on asio contet), it should be appended to
     * `m_pkg_input` and an iteration of parse-handle-parse-handle...
     * should be started.
     */
    void handle_input( input_buffer_t buf )
    {
        try
        {
            m_pkg_input.append( std::move( buf ) );
            run_input_stream_loop();
            update_last_input_at();
        }
        catch( const std::exception & ex )
        {
            // Parsing fails.
            logger().error( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                format_to( out,
                           "[{};cid:{}] failed to handle incoming data: {}",
                           this->remote_endpoint_str(),
                           this->underlying_connection_id(),
                           ex.what() );
            } );

            shutdown_and_terminate( connection_shutdown_context_t{
                entry_shutdown_reason::exception_handling_input, ex.what() } );
        }
    }

    /**
     * @brief The result od handling a package.
     */
    enum class package_handling_result
    {
        //! The package was fully consumed (header+message)
        fully_consumed,
        //! Not enough data to read the whole package.
        needs_more_input_data,
        /**
         *  @brief Invalid package.
         *
         *  Meet a bad package, so the input stream becomes invalid
         *  for further consuming.
         */
        invalid_package
    };

    /**
     * @brief A hook function to handle content of the message package.
     *
     * That is the obligation of a derrived class to handle
     * a packages message (getting protocol message out of it
     * and send it to client).
     *
     * @param header       Header for this package.
     * @param stream       protobuf stream to handle the data.
     *
     */
    virtual package_handling_result handle_incoming_message(
        const pkg_header_t & header,
        ::opio::proto_entry::pkg_input_base_t & stream ) = 0;

    /**
     * @brief Start input consume loop.
     *
     * Continuesly runs package handling from loop.
     * After new incoming data comes input might contain several complete
     * packages. This function is intended to run handling logic
     * for each of them.
     */
    void run_input_stream_loop()
    {
        auto handle_single_package = [ this ] {
            if( m_pkg_input.size() >= sizeof( pkg_header_t ) )
            {
                const auto header = m_pkg_input.view_pkg_header();

                logger().trace( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                    format_to( out,
                               "[{};cid:{}] Consider next header, "
                               "pkg_content_type: {}, content_size: {}, "
                               "attached_binary_size: {}, "
                               "current stream size: {} (includes this header)",
                               this->remote_endpoint_str(),
                               this->underlying_connection_id(),
                               header.pkg_content_type,
                               header.content_size,
                               header.attached_binary_size,
                               m_pkg_input.size() );
                } );

                switch( header.pkg_content_type )
                {
                    case pkg_content_message:
                        return handle_message_pkg( header );
                    case pkg_content_heartbeat_request:
                        return handle_heartbeat_request_pkg( header );
                    case pkg_content_heartbeat_reply:
                        return handle_heartbeat_reply_pkg( header );
                    default:
                        return handle_unknown_pkg_content_type( header );
                }
            }

            return package_handling_result::needs_more_input_data;
        };

        auto pkg_handling_res = handle_single_package();
        while( package_handling_result::fully_consumed == pkg_handling_res )
        {
            pkg_handling_res = handle_single_package();
        }
    }

    /**
     * @brief Shutdown socket and terminate session.
     */
    void shutdown_and_terminate(
        connection_shutdown_context_t connection_shutdown_context )
    {
        if( !m_connection_is_active ) [[unlikely]]
        {
            return;
        }

        logger().info( OPIO_SRC_LOCATION, [ & ]( auto out ) {
            format_to( out,
                       "[{};cid:{}] terminating entry",
                       this->remote_endpoint_str(),
                       this->underlying_connection_id() );
        } );

        this->underlying_connection()->shutdown();

        terminate( std::move( connection_shutdown_context ) );
        m_connection_is_active = false;
    }

    /**
     * @brief Move to termination state.
     *
     * This will guarantee that no more messages will be received
     * by the agent.
     */
    void terminate( connection_shutdown_context_t connection_shutdown_context )
    {
        assert( m_connection_is_active );

        auto cancel_hearbeat_timer = [ & ] {
            logger().trace( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                format_to( out,
                           "[{};cid:{}] cancel heartbeat timer",
                           this->remote_endpoint_str(),
                           this->underlying_connection_id() );
            } );

            m_heartbeat_timer.cancel();
        };

        std::visit( details::overloaded_sh{
                        [ & ]( details::shutdown_not_supplied_t && ) {
                            logger().trace( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                                format_to( out,
                                           "[{};cid:{}] skip shutdown handler "
                                           "for entry (not supplied)",
                                           this->remote_endpoint_str(),
                                           this->underlying_connection_id() );
                            } );
                            cancel_hearbeat_timer();
                        },
                        []( details::shutdown_was_called_t && ) {
                            // was called already
                        },
                        [ & ]( shutdown_handler_t && sh ) {
                            logger().trace( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                                format_to( out,
                                           "[{};cid:{}] executing "
                                           "shutdown handler for entry",
                                           this->remote_endpoint_str(),
                                           this->underlying_connection_id() );
                            } );

                            sh( this->underlying_connection_id() );
                            cancel_hearbeat_timer();
                        },
                        [ & ]( shutdown_handler2_t && sh ) {
                            logger().trace( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                                format_to( out,
                                           "[{};cid:{}] executing context aware "
                                           "shutdown handler for entry",
                                           this->remote_endpoint_str(),
                                           this->underlying_connection_id() );
                            } );

                            sh( this->underlying_connection_id(),
                                std::move( connection_shutdown_context ) );

                            cancel_hearbeat_timer();
                        } },
                    std::move( m_shutdown_handler ) );

        m_shutdown_handler = details::shutdown_was_called_t{};
    }

    /**
     * @brief Check package size is valid.
     *
     * Acts as a reusable check routine for handlers of various types of packages.
     */
    template < typename Src_Location >
    bool pkg_has_valid_size( Src_Location src_location,
                             std::string_view pkg_type_name,
                             pkg_header_t header )
    {
        // TODO: also check attached_binary_size

        if( m_cfg.max_valid_package_size < header.content_size ) [[unlikely]]
        {
            // Need more data before reading the whole package.
            logger().error( src_location, [ & ]( auto out ) {
                format_to( out,
                           "[{};cid:{}] invalid '{}' package size {}, "
                           "max_valid_package_size is {}",
                           this->remote_endpoint_str(),
                           this->underlying_connection_id(),
                           pkg_type_name,
                           header.content_size,
                           m_cfg.max_valid_package_size );
            } );
            return false;
        }

        return true;
    }

    template < typename Src_Location >
    bool pkg_has_all_the_data( Src_Location src_location,
                               std::string_view pkg_type_name,
                               pkg_header_t header )
    {
        const auto header_plus_content_size = header.advertized_header_size()
                                              + header.content_size
                                              + header.attached_binary_size;

        if( header_plus_content_size > m_pkg_input.size() ) [[unlikely]]
        {
            // Need more data before reading the whole package.
            logger().trace( src_location, [ & ]( auto out ) {
                format_to( out,
                           "[{};cid:{}] '{}' package data is not complete, "
                           "header+content+attached_bin size is {} bytes, "
                           "while only {} "
                           "is available; waiting for more data to come...",
                           this->remote_endpoint_str(),
                           this->underlying_connection_id(),
                           pkg_type_name,
                           header_plus_content_size,
                           m_pkg_input.size() );
            } );
            return false;
        }

        return true;
    }

    /**
     * @brief Handle message package.
     *
     * @param header  The header of the package at the head of the input stream.
     */
    [[nodiscard]] package_handling_result handle_message_pkg( pkg_header_t header )
    {
        constexpr std::string_view pkg_type_string{ "message" };

        if( !pkg_has_valid_size( OPIO_SRC_LOCATION, pkg_type_string, header ) )
            [[unlikely]]
        {
            shutdown_and_terminate( connection_shutdown_context_t{
                entry_shutdown_reason::invalid_input_package_size } );
            return package_handling_result::invalid_package;
        }

        if( !pkg_has_all_the_data( OPIO_SRC_LOCATION, pkg_type_string, header ) )
            [[unlikely]]
        {
            return package_handling_result::needs_more_input_data;
        }

        // We have enough data to read the whole package.
        // So we remove header from the head of the stream
        // (confirm we consume header bytes).
        // Note that we need to use the advertized size
        // to skip header bytes correctly.
        assert( header.advertized_header_size()
                >= 4 * pkg_header_t::image_size_dwords );
        m_pkg_input.skip_bytes( header.advertized_header_size() );

        if( auto res = handle_incoming_message( header, m_pkg_input );
            res != package_handling_result::fully_consumed ) [[unlikely]]
        {
            // There was an error in handling content of the package.
            return res;
        }

        return package_handling_result::fully_consumed;
    }

    /**
     * @brief Handle heartbeat request from peer.
     *
     * @param header  The header of the package at the head of the input stream.
     */
    [[nodiscard]] package_handling_result handle_heartbeat_request_pkg(
        pkg_header_t header )
    {
        assert( pkg_content_heartbeat_request == header.pkg_content_type );

        if( ( 0 != header.content_size ) | ( 0 != header.attached_binary_size ) )
        {
            logger().error( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                format_to(
                    out,
                    "[{};cid:{}] heartbeat request package with "
                    "nonzero content: content_size={}, attached_binary_size={}",
                    this->remote_endpoint_str(),
                    this->underlying_connection_id(),
                    header.content_size,
                    header.attached_binary_size );
            } );
            shutdown_and_terminate( connection_shutdown_context_t{
                entry_shutdown_reason::invalid_heartbeat_package } );
            return package_handling_result::invalid_package;
        }

        m_pkg_input.skip_bytes( header.advertized_header_size() );

        if( !m_connection_is_active ) [[unlikely]]
        {
            // We already disconnected.
            logger().warn( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                format_to( out,
                           "[{};cid:{}] skip handling heartbeatrequest: "
                           "already disconnected",
                           this->remote_endpoint_str(),
                           this->underlying_connection_id() );
            } );
            return package_handling_result::fully_consumed;
        }

        logger().trace( OPIO_SRC_LOCATION, [ & ]( auto out ) {
            format_to( out,
                       "[{};cid:{}] heartbeat request package came, sending reply",
                       this->remote_endpoint_str(),
                       this->underlying_connection_id() );
        } );

        const auto resp = pkg_header_t::make( pkg_content_heartbeat_reply );

        this->underlying_connection()->schedule_send(
            opio::net::simple_buffer_t{ &resp, sizeof( resp ) } );

        return package_handling_result::fully_consumed;
    }

    /**
     * @brief Handle heartbeat reply from peer.
     *
     * @param header  The header of the package at the head of the input stream.
     */
    [[nodiscard]] package_handling_result handle_heartbeat_reply_pkg(
        pkg_header_t header )
    {
        assert( pkg_content_heartbeat_reply == header.pkg_content_type );

        if( ( 0 != header.content_size ) | ( 0 != header.attached_binary_size ) )
        {
            logger().error( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                format_to(
                    out,
                    "[{};cid:{}] heartbeat response package with "
                    "nonzero content: content_size={}, attached_binary_size={}",
                    this->remote_endpoint_str(),
                    this->underlying_connection_id(),
                    header.content_size,
                    header.attached_binary_size );
            } );
            shutdown_and_terminate( connection_shutdown_context_t{
                entry_shutdown_reason::invalid_heartbeat_package } );
            return package_handling_result::invalid_package;
        }

        m_pkg_input.skip_bytes( header.advertized_header_size() );

        logger().trace( OPIO_SRC_LOCATION, [ & ]( auto out ) {
            format_to( out,
                       "[{};cid:{}] heartbeat reply package came",
                       this->remote_endpoint_str(),
                       this->underlying_connection_id() );
        } );

        return package_handling_result::fully_consumed;
    }

    /**
     * @brief Handles unknown type of package.
     *
     * @param header The header of the package at the head of the input stream.
     */
    [[nodiscard]] package_handling_result handle_unknown_pkg_content_type(
        pkg_header_t header )
    {
        logger().error( OPIO_SRC_LOCATION, [ & ]( auto out ) {
            format_to( out,
                       "[{};cid:{}] unknown pkg_content_type value: {}",
                       this->remote_endpoint_str(),
                       this->underlying_connection_id(),
                       header.pkg_content_type );
        } );
        shutdown_and_terminate( connection_shutdown_context_t{
            entry_shutdown_reason::unknown_pkg_content_type } );
        return package_handling_result::invalid_package;
    }

    /**
     * @brief Handle connection shutdown reason.
     */
    void handle_shutdown( opio::net::tcp::connection_shutdown_reason reason )
    {
        if( !m_connection_is_active ) [[unlikely]]
        {
            logger().trace( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                format_to( out,
                           "[{};cid:{}] skip connection shutdown handler: "
                           "already disconnected",
                           this->remote_endpoint_str(),
                           this->underlying_connection_id(),
                           reason );
            } );
            return;
        }

        logger().debug( OPIO_SRC_LOCATION, [ & ]( auto out ) {
            format_to( out,
                       "[{};cid:{}] connection shutdowned: {}",
                       this->remote_endpoint_str(),
                       this->underlying_connection_id(),
                       reason );
        } );

        terminate( connection_shutdown_context_t{ reason } );
        m_connection_is_active = false;
    }

private:
    void on_check_heartbeat()
    {
        if( !m_connection_is_active ) [[unlikely]]
        {
            // We already disconnected.
            logger().warn( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                format_to( out,
                           "[{};cid:{}] skip heartbeat check: "
                           "already disconnected",
                           this->remote_endpoint_str(),
                           this->underlying_connection_id() );
            } );
            return;
        }

        using namespace std::chrono;

        const auto since_last_input =
            std::chrono::steady_clock::now() - m_last_input_at;

        const auto initiate_heartbeat_timeout = get_initiate_heartbeat_timeout();

        if( since_last_input < initiate_heartbeat_timeout )
        {
            // Similar for both HB strategies (modern and legacy)
            logger().trace( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                format_to(
                    out,
                    "[{};cid:{}] false heartbeat timeout accured, "
                    "passed since last input: {} msec ({} allowed), "
                    "will reschedule",
                    this->remote_endpoint_str(),
                    this->underlying_connection_id(),
                    duration_cast< milliseconds >( since_last_input ).count(),
                    duration_cast< milliseconds >( initiate_heartbeat_timeout )
                        .count() );
            } );

            // A little desync happened
            // e.g. an input appeared in roughly the same time as
            // the delayed message was sent (think of a case parsing
            // was taking too long while new input came on the edge of
            // timeout schedule).
            schedule_next_heartbeat_check( initiate_heartbeat_timeout
                                           - since_last_input );
            return;
        }

        auto send_heartbeat_request = [ this ] {
            const auto ping_req_header =
                pkg_header_t::make( pkg_content_heartbeat_request );

            this->underlying_connection()->schedule_send( net::simple_buffer_t{
                &ping_req_header, ping_req_header.advertized_header_size() } );

            ++m_heartbeat_sent_count;
        };

        if( 0 != m_heartbeat_sent_count
            && since_last_input >= m_cfg.heartbeat.await_heartbeat_reply_timeout )
        {
            logger().error( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                format_to(
                    out,
                    "[{};cid:{}] no reply to heartbeat, no input for "
                    "{} msec (max allowed: {} msec)",
                    this->remote_endpoint_str(),
                    this->underlying_connection_id(),
                    duration_cast< milliseconds >( since_last_input ).count(),
                    duration_cast< milliseconds >(
                        m_cfg.heartbeat.await_heartbeat_reply_timeout )
                        .count() );
            } );

            shutdown_and_terminate( connection_shutdown_context_t{
                entry_shutdown_reason::hearbeat_reply_timeout } );
            return;
        }

        // It is the first time we heve timeout of no input from client.

        logger().debug( OPIO_SRC_LOCATION, [ & ]( auto out ) {
            format_to( out,
                       "[{};cid:{}] heartbeat timeout occured, "
                       "since last input: {}",
                       this->remote_endpoint_str(),
                       this->underlying_connection_id(),
                       duration_cast< milliseconds >( since_last_input ).count() );
        } );

        // We are should initiate heartbeat request:
        send_heartbeat_request();

        // And schedule check to a point at which
        // we would close connection if no input happened.
        if( m_cfg.heartbeat.await_heartbeat_reply_timeout > since_last_input )
        {
            schedule_next_heartbeat_check(
                m_cfg.heartbeat.await_heartbeat_reply_timeout - since_last_input );
        }
        else
        {
            logger().warn( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                format_to(
                    out,
                    "[{};cid:{}] looks like asio loop is overloaded "
                    "the very first timeout event already exceeds "
                    "await_heartbeat_reply_timeout ({} msec) "
                    "will give peer at least initiate_heartbeat_timeout "
                    "({} msec) to reply on heartbeat request",
                    this->remote_endpoint_str(),
                    this->underlying_connection_id(),
                    duration_cast< milliseconds >( since_last_input ).count(),
                    duration_cast< milliseconds >(
                        m_cfg.heartbeat.await_heartbeat_reply_timeout )
                        .count(),
                    duration_cast< milliseconds >(
                        m_cfg.heartbeat.initiate_heartbeat_timeout )
                        .count() );
            } );

            schedule_next_heartbeat_check(
                m_cfg.heartbeat.initiate_heartbeat_timeout );
        }
    }

    auto get_initiate_heartbeat_timeout() const noexcept
    {
        return m_cfg.heartbeat.initiate_heartbeat_timeout;
    }

    void schedule_next_heartbeat_check( std::chrono::steady_clock::duration delay )
    {
        m_heartbeat_timer.expires_after( delay );
        m_heartbeat_timer.async_wait(
            [ weak_self = this->weak_from_this() ]( const auto & ec ) {
                if( !ec )
                {
                    if( auto self = weak_self.lock(); self )
                    {
                        self->on_check_heartbeat();
                    }
                }
            } );
    }

    void schedule_next_heartbeat_check()
    {
        schedule_next_heartbeat_check( this->get_initiate_heartbeat_timeout() );
    }

    /**
     * @brief Update last input timestamp.
     */
    void update_last_input_at()
    {
        m_last_input_at        = std::chrono::steady_clock::now();
        m_heartbeat_sent_count = 0;
    }

    opio::net::tcp::connection_id_t m_connection_id{};

    /**
     * @brief Connection object associated with this entry.
     */
    underlying_connection_sptr_t m_connection;

    /**
     * @brief Tells if connection is active.
     *
     * That tells if we can send data through this connection
     */
    std::atomic< bool > m_connection_is_active = true;

    /**
     * @brief Strand for this connection object.
     */
    strand_t m_strand;

    /**
     * @brief Logger for this entry.
     */
    logger_t m_logger;

    [[no_unique_address]] buffer_driver_t m_buffer_driver;

    /**
     * @brief parameters to work with connections.
     */
    const entry_cfg_t m_cfg;

    /**
     * @brief An input stream for reading packages.
     *
     * This stream consumes incoming buffers and allows
     * to either read package headers or to Parse protobuf messages.
     * It cares about delimited buffers handling automatically.
     */
    pkg_input_t<> m_pkg_input;

    shutdown_handler_variant_t m_shutdown_handler;

    std::chrono::steady_clock::time_point m_last_input_at;

    using heartbeat_timer_t = opio::net::asio_ns::basic_waitable_timer<
        std::chrono::steady_clock,
        opio::net::asio_ns::wait_traits< std::chrono::steady_clock >,
        strand_t >;

    heartbeat_timer_t m_heartbeat_timer;

    // opio::net::asio_ns::steady_timer m_heartbeat_timer;

    /**
     * @brief Number of sent HB requests while no input from client.
     *
     * @since v0.10.1
     */
    std::uint32_t m_heartbeat_sent_count{};
};

//
// common_traits_base_t
//

/**
 * @brief Class defining common traits.
 */
template < typename Stats_Driver,
           typename Logger,
           protobuf_parsing_strategy Protobuf_Parsing_Strategy =
               protobuf_parsing_strategy::trivial >
struct common_traits_base_t
{
    using socket_t = opio::net::asio_ns::ip::tcp::socket;
    using socket_io_operation_watchdog_t =
        opio::net::asio_timer_operation_watchdog_t;
    using underlying_stats_driver_t = opio::net::noop_stats_driver_t;

    using logger_t       = Logger;
    using stats_driver_t = Stats_Driver;

    template < typename Traits >
    using underlying_connection_t = opio::net::tcp::connection_t< Traits >;

    template < typename Message >
    using protobuf_parsing_engine_t =
        impl::protobuf_parsing_engine_t< Protobuf_Parsing_Strategy, Message >;

    using locking_t = opio::net::noop_locking_t;
};

/**
 * @name A set of standard traits for various cases.
 */
/// @{
template < typename Stats_Driver,
           typename Logger,
           protobuf_parsing_strategy Protobuf_Parsing_Strategy =
               protobuf_parsing_strategy::trivial >
struct singlethread_traits_base_t
    : public common_traits_base_t< Stats_Driver,
                                   Logger,
                                   Protobuf_Parsing_Strategy >
{
    using underlying_connection_strand_t = opio::net::tcp::noop_strand_t;
    using strand_t                       = opio::net::tcp::noop_strand_t;
    using buffer_driver_t = opio::net::heterogeneous_buffer_driver_t;
};

template < typename Stats_Driver,
           typename Logger,
           protobuf_parsing_strategy Protobuf_Parsing_Strategy =
               protobuf_parsing_strategy::trivial >
struct multithread_traits_base_t
    : public common_traits_base_t< Stats_Driver,
                                   Logger,
                                   Protobuf_Parsing_Strategy >
{
    using underlying_connection_strand_t = opio::net::tcp::real_strand_t;
    using strand_t                       = opio::net::tcp::real_strand_t;
    using buffer_driver_t = opio::net::heterogeneous_buffer_driver_t;
};
/// @}

}  // namespace opio::proto_entry

namespace fmt
{

// Make it possible to use opio::proto_entry::entry_shutdown_reason enum
// as an argument for fmt format functions.
template <>
struct formatter< opio::proto_entry::entry_shutdown_reason >
{
    using shutdown_reason = opio::proto_entry::entry_shutdown_reason;
    template < class Parse_Context >
    constexpr auto parse( Parse_Context & ctx )
    {
        auto it  = std::begin( ctx );
        auto end = std::end( ctx );
        if( it != end && *it != '}' ) throw fmt::format_error( "invalid format" );
        return it;
    }

    template < class Format_Context >
    auto format( shutdown_reason reason, Format_Context & ctx )
    {
        switch( reason )
        {
            case shutdown_reason::underlying_connection:
                return format_to( ctx.out(), "underlying_connection" );
            case shutdown_reason::user_initiated:
                return format_to( ctx.out(), "user_initiated" );
            case shutdown_reason::exception_handling_input:
                return format_to( ctx.out(), "exception_handling_input" );
            case shutdown_reason::invalid_input_package:
                return format_to( ctx.out(), "invalid_input_package" );
            case shutdown_reason::unexpected_input_package_size:
                return format_to( ctx.out(), "unexpected_input_package_size" );
            case shutdown_reason::invalid_input_package_size:
                return format_to( ctx.out(), "invalid_input_package_size" );
            case shutdown_reason::invalid_heartbeat_package:
                return format_to( ctx.out(), "invalid_heartbeat_package" );
            case shutdown_reason::unknown_pkg_content_type:
                return format_to( ctx.out(), "unknown_pkg_content_type" );
            case shutdown_reason::hearbeat_reply_timeout:
                return format_to( ctx.out(), "hearbeat_reply_timeout" );
            default:
                return format_to( ctx.out(),
                                  "unknown_shutdown_reason({})",
                                  static_cast< int >( reason ) );
        }
    }
};

}  // namespace fmt
