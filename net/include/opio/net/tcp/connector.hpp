/**
 * @file
 *
 * This header file contains connector_t class and some auxiliary routines.
 */

#pragma once

#include <opio/net/asio_include.hpp>
#include <opio/log.hpp>
#include <opio/net/tcp/utils.hpp>

namespace opio::net::tcp
{

/**
 * @brief A CB type to handle result of connecting to server.
 */
using on_connection_cb_t =
    std::function< void( const asio_ns::error_code &, asio_ns::ip::tcp::socket ) >;

//
// connector_t
//

/**
 * @brief Class performing connection operation.
 *
 * Acts as a context class which runs on asio event loop.
 * It performs async connection to server which includes endpoint resolution and
 * if that succeeds then connecting tries to connect an endpoint resolved on
 * the first step.
 */
template < class Logger >
class connector_t : public std::enable_shared_from_this< connector_t< Logger > >
{
public:
    connector_t( asio_ns::any_io_executor executor,
                 tcp_resolver_query_t query,
                 const socket_options_cfg_t & socket_options_cfg,
                 Logger logger,
                 on_connection_cb_t on_connection_cb )
        : m_query{ std::move( query ) }
        , m_resolver{ executor }
        , m_socket{ executor }
        , m_socket_options_cfg{ socket_options_cfg }
        , m_logger{ std::move( logger ) }
        , m_on_connection_cb{ std::move( on_connection_cb ) }
    {
        assert( m_on_connection_cb );
    }

    connector_t( asio_ns::any_io_executor executor,
                 tcp_resolver_query_t query,
                 Logger logger,
                 on_connection_cb_t on_connection_cb )
        : connector_t{ std::move( executor ),
                       std::move( query ),
                       socket_options_cfg_t{},
                       std::move( logger ),
                       on_connection_cb }
    {
        assert( m_on_connection_cb );
    }

    void connect()
    {
        m_resolver.async_resolve(
#if OPIO_ASIO_VERSION >= 103300  // Asio >= 1.33.0
            m_query.protocol,
#endif
            m_query.host_name(),
            m_query.service_name(),
            [ self = this->shared_from_this() ]( const auto & ec,
                                                 auto resolve_res ) {
                self->handle_resolve( ec, std::move( resolve_res ) );
            } );
    }

    /**
     * @brief Get this connector resolution query used by resolver
     *        to find an endpoint.
     */
    [[nodiscard]] const tcp_resolver_query_t & query() const noexcept
    {
        return m_query;
    }

private:
    /**
     * @brief Handle result of resolving server endpoint.
     *
     * @param ec       Result of resolution operation.
     * @param results  Endpoints resolved for a given query.
     */
    void handle_resolve( const asio_ns::error_code & ec,
                         asio_ns::ip::tcp::resolver::results_type results )
    {
        if( !ec )
        {
            const auto ep = std::cbegin( results )->endpoint();

            m_logger.trace( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                format_to( out,
                           "Resolve '{}:{}': {}",
                           m_query.host_name(),
                           m_query.service_name(),
                           ep );
            } );

            // To set the options we must first open the socket,
            // so that there would be  a real handle behind asio socket object:
            m_socket.open( ep.protocol() );
            set_socket_options( m_socket_options_cfg, m_socket );

            // We use socket.async_connect(CB) rather then
            // asio_ns::async_connect( sock, CB )
            // because we want our options to be considered.
            // asio_ns::async_connect( sock, CB ) - creates a new socket
            // and doesn't account for options we set before.
            m_socket.async_connect(
                *std::begin( results ),
                [ self = this->shared_from_this(), ep ]( const auto & ec ) {
                    self->handle_connect( ec, ep );
                } );
        }
        else
        {
            m_logger.error( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                format_to( out,
                           "Unable to resolve '{}:{}': ec={}",
                           m_query.host_name(),
                           m_query.service_name(),
                           fmt_integrator( ec ) );
            } );
            m_on_connection_cb( ec, std::move( m_socket ) );
        }
    }

    /**
     * @brief Handle result of "connect to server endpoint" operation.
     *
     * If connection succeeds then a cannected socket live in @c m_socket.
     *
     * @param ec        Result of connect operation.
     * @param endpoint  An endpoint to which connection was made.
     */
    void handle_connect(
        const asio_ns::error_code & ec,
        [[maybe_unused]] const asio_ns::ip::tcp::endpoint & endpoint )
    {
        if( !ec )
        {
            m_logger.trace( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                format_to( out,
                           "Connect '{}:{}': {}",
                           m_query.host_name(),
                           m_query.service_name(),
                           endpoint );
            } );
        }
        else
        {
            m_logger.error( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                format_to( out,
                           "Unable to connect '{}:{}': ec={}",
                           m_query.host_name(),
                           m_query.service_name(),
                           fmt_integrator( ec ) );
            } );
        }

        try
        {
            // In any case (connection succeed or connection failed)
            // We should report the status to CB provided by user.
            m_on_connection_cb( ec, std::move( m_socket ) );
        }
        catch( const std::exception & ex )
        {
            m_logger.error( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                format_to( out, "Callback failed: {}", ex.what() );
            } );
        }
    }

    tcp_resolver_query_t m_query;
    asio_ns::ip::tcp::resolver m_resolver;
    asio_ns::ip::tcp::socket m_socket;

    const socket_options_cfg_t m_socket_options_cfg;
    [[no_unique_address]] Logger m_logger;
    on_connection_cb_t m_on_connection_cb;
};

//
// async_connect()
//

/**
 * @brief Async connect to a target specified by qyery.
 */
template < typename Logger >
void async_connect( asio_ns::any_io_executor executor,
                    tcp_resolver_query_t query,
                    const socket_options_cfg_t & socket_options_cfg,
                    Logger logger,
                    on_connection_cb_t on_connection_cb )
{
    if( !on_connection_cb )
    {
        throw std::runtime_error{ "invalid on-connection callback" };
    }

    std::make_shared< connector_t< Logger > >( executor,
                                               std::move( query ),
                                               socket_options_cfg,
                                               std::move( logger ),
                                               std::move( on_connection_cb ) )
        ->connect();
}

/**
 * @brief Async connect to a target specified by qyery.
 */
template < typename Logger >
void async_connect( asio_ns::any_io_executor executor,
                    tcp_resolver_query_t query,
                    Logger logger,
                    on_connection_cb_t on_connection_cb )
{
    async_connect( std::move( executor ),
                   std::move( query ),
                   socket_options_cfg_t{},
                   std::move( logger ),
                   std::move( on_connection_cb ) );
}

/**
 * @brief Async connect to a target specified by host&port.
 */
template < typename Logger >
void async_connect( asio_ns::any_io_executor executor,
                    std::string_view host,
                    std::uint16_t port,
                    const socket_options_cfg_t & socket_options_cfg,
                    Logger logger,
                    on_connection_cb_t on_connection_cb )
{
    async_connect( std::move( executor ),
                   tcp_resolver_query_t{ asio_ns::ip::tcp::v4(),
                                         std::string( host ),
                                         std::to_string( port ) },
                   socket_options_cfg,
                   std::move( logger ),
                   std::move( on_connection_cb ) );
}

/**
 * @brief Async connect to a target specified by host&port.
 */
template < typename Logger >
void async_connect( asio_ns::any_io_executor executor,
                    std::string_view host,
                    std::uint16_t port,
                    Logger logger,
                    on_connection_cb_t on_connection_cb )
{
    async_connect( std::move( executor ),
                   host,
                   port,
                   socket_options_cfg_t{},
                   std::move( logger ),
                   std::move( on_connection_cb ) );
}

}  // namespace opio::net::tcp
