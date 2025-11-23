/**
 * @file
 *
 * This header file contains acceptor_t class and some auxiliary routines.
 */

#pragma once

#include <opio/log.hpp>

#include <opio/net/asio_include.hpp>
#include <opio/net/tcp/error_code.hpp>
#include <opio/net/tcp/utils.hpp>

namespace opio::net::tcp
{

/**
 * @brief A CB type to handle new connection.
 */
using on_accept_cb_t = std::function< void( asio_ns::ip::tcp::socket ) >;

/**
 * @brief A CB type to handle acceptor open(), close() events.
 */
using on_openclose_cb_t = std::function< void( const asio_ns::error_code & ) >;

//
// acceptor_t
//

/**
 * @brief Class providing socket acceptor service.
 *
 * Acts as a context class for accepting incoming tcp connections which
 * runs on asio event loop. The class uses asio acceptor
 * to abstract low level details of starting a server (bind, listen, accept).
 * For accepting new connection an async interface of asio acceptor is used.
 *
 * This calss is intended to be used in a form of shared pointer.
 * So it passes the shared pointer to itslef to callbacks
 *
 * @todo Should consider the case of asio running multiple threads and
 *       user calls `open()/close()` many times, which currently is not
 *       accounted.
 */
template < class Logger >
class acceptor_t : public std::enable_shared_from_this< acceptor_t< Logger > >
{
public:
    acceptor_t( asio_ns::any_io_executor executor,
                asio_ns::ip::tcp::endpoint endpoint,
                const socket_options_cfg_t & socket_options_cfg,
                Logger logger,
                on_accept_cb_t on_accept_cb )
        : m_executor{ std::move( executor ) }
        , m_socket{ m_executor }
        , m_acceptor{ m_executor }
        , m_endpoint{ std::move( endpoint ) }
        , m_socket_options_cfg{ socket_options_cfg }
        , m_logger{ std::move( logger ) }
        , m_on_accept_cb{ std::move( on_accept_cb ) }
    {
        assert( m_on_accept_cb );
    }

    acceptor_t( asio_ns::any_io_executor executor,
                asio_ns::ip::tcp::endpoint endpoint,
                Logger logger,
                on_accept_cb_t on_accept_cb )
        : acceptor_t{ std::move( executor ),
                      std::move( endpoint ),
                      socket_options_cfg_t{},
                      std::move( logger ),
                      std::move( on_accept_cb ) }
    {
    }

    /**
     * @brief Open server for given acceptor.
     *
     * Starts listening and accepting connections.
     *
     * @param cb  CB to be called with the result of operation.
     */
    void open( on_openclose_cb_t cb = {} )
    {
        asio_ns::post( m_executor,
                       [ wp = this->weak_from_this(), cb = std::move( cb ) ] {
                           if( auto self = wp.lock(); self )
                           {
                               self->open_impl( std::move( cb ) );
                           }
                       } );
    }

    /**
     * @brief Close server for given acceptor.
     *
     * Stops listening and accepting connections.
     *
     * @param cb  CB to be called with the result of operation.
     */
    void close( on_openclose_cb_t cb = {} )
    {
        asio_ns::post( m_executor,
                       [ wp = this->weak_from_this(), cb = std::move( cb ) ] {
                           if( auto self = wp.lock(); self )
                           {
                               self->close_impl( std::move( cb ) );
                           }
                       } );
    }

    /**
     * @brief Get an endpoint associated with this acceptor.
     */
    [[nodiscard]] const asio_ns::ip::tcp::endpoint & endpoint() const noexcept
    {
        return m_endpoint;
    }

private:
    /**
     * @brief A helper routine to call user provided open/close handler.
     *
     * @param cb  A target CB.
     *
     * @note CB parameter is received by values, which is a deliberate choice,
     *       that also helps us to make sure we call the callback only once.
     */
    void call_openclose_cb( on_openclose_cb_t cb, const asio_ns::error_code & ec )
    {
        try
        {
            if( cb )
            {
                cb( ec );
            }
        }
        catch( const std::exception & ex )
        {
            m_logger.error( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                format_to( out,
                           "Failed to run open/close callback with ec={}; {}",
                           fmt_integrator( ec ),
                           ex.what() );
            } );
        }
    }

    /**
     * @brief An imlementaion of open operation executed on ASIO context.
     */
    void open_impl( on_openclose_cb_t cb )
    {
        try
        {
            if( m_acceptor.is_open() )
            {
                const auto ep = m_acceptor.local_endpoint();
                m_logger.warn( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                    format_to( out, "Server already started on {}", ep );
                } );
                call_openclose_cb(
                    std::move( cb ),
                    make_std_compaible_error(
                        error_codes::open_acceptor_failed_already_started ) );
            }
            else
            {
                m_logger.trace( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                    format_to( out, "Opening server on {}", m_endpoint );
                } );
                m_acceptor.open( m_endpoint.protocol() );
                asio_ns::ip::tcp::acceptor::reuse_address reuse_address_option(
                    true );
                m_acceptor.set_option( reuse_address_option );

                m_acceptor.bind( m_endpoint );

                m_acceptor.listen( asio_ns::socket_base::max_listen_connections );

                accept_next();

                m_logger.debug( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                    format_to( out, "Server is opened on {}", m_endpoint );
                } );
                call_openclose_cb( std::move( cb ), {} );
            }
        }
        catch( const std::exception & ex )
        {
            m_logger.error( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                format_to( out, "Open server endpoint failed: {}", ex.what() );
            } );

            call_openclose_cb(
                std::move( cb ),
                make_std_compaible_error(
                    error_codes::open_acceptor_failed_exception_happened ) );
        }
    }

    /**
     * @brief An imlementaion of close operation executed on ASIO context.
     */
    void close_impl( on_openclose_cb_t cb )
    {
        try
        {
            if( m_acceptor.is_open() )
            {
                const auto ep = m_acceptor.local_endpoint();

                m_logger.trace( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                    format_to( out, "closing server on {}", ep );
                } );

                m_acceptor.close();

                m_logger.debug( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                    format_to( out, "server closed on {}", ep );
                } );
                call_openclose_cb( std::move( cb ), {} );
            }
            else
            {
                m_logger.warn( OPIO_SRC_LOCATION, "Server is not running" );
                call_openclose_cb(
                    std::move( cb ),
                    make_std_compaible_error(
                        error_codes::close_acceptor_failed_not_running ) );
            }
        }
        catch( const std::exception & ex )
        {
            m_logger.error( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                format_to( out, "Close server endpoint failed: {}", ex.what() );
            } );
            call_openclose_cb(
                std::move( cb ),
                make_std_compaible_error(
                    error_codes::close_acceptor_failed_exception_happened ) );
        }
    }

    /**
     * @brief Start a next iteration of accepting ne connection.
     */
    void accept_next()
    {
        m_acceptor.async_accept(
            m_socket, [ wp = this->weak_from_this() ]( const auto & ec ) {
                if( auto self = wp.lock(); self )
                {
                    self->on_connection( ec );
                }
            } );
    }

    /**
     * @brief Handle new incoming connection.
     *
     * @param ec      Result of accepting new socket.
     * @param socket  Socket instance containing new connection.
     */
    void on_connection( const asio_ns::error_code & ec )
    {
        asio_ns::ip::tcp::socket socket{ std::move( m_socket ) };
        try
        {
            if( !ec )
            {
                m_logger.trace( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                    format_to( out,
                               "Accept connection from: {}",
                               socket.remote_endpoint() );
                } );
                set_socket_options( m_socket_options_cfg, socket );
                m_on_accept_cb( std::move( socket ) );
            }
            else if( ec == asio_ns::error::operation_aborted )
            {
                m_logger.trace( OPIO_SRC_LOCATION,
                                "Accepting connection aborted" );
                return;
            }
            else
            {
                m_logger.error( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                    format_to( out,
                               "Accept connection failed: ec={}",
                               fmt_integrator( ec ) );
                } );
            }
        }
        catch( const std::exception & ex )
        {
            m_logger.error( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                format_to( out,
                           "Failed to handle accept result with ec={}; {}",
                           fmt_integrator( ec ),
                           ex.what() );
            } );
        }
        accept_next();
    }

    asio_ns::any_io_executor m_executor;
    asio_ns::ip::tcp::socket m_socket;
    asio_ns::ip::tcp::acceptor m_acceptor;
    const asio_ns::ip::tcp::endpoint m_endpoint;

    const socket_options_cfg_t m_socket_options_cfg;
    Logger m_logger;
    on_accept_cb_t m_on_accept_cb;
};

/**
 * @brief A factory for creating an instance of acceptor.
 *
 * @param  executor            Executor where to run acceptor operations.
 * @param  endpoint            TCP endpoint where to start the server
 *                             (accept new connections).
 * @param  socket_options_cfg  Socket options configuration.
 * @param  logger              Loger assigned to the instance of acceptor.
 * @param  on_accept_cb        CB to handle new connections.
 *
 * @return A shared pointer to acceptor.
 */
template < typename Logger >
auto make_acceptor( asio_ns::any_io_executor executor,
                    asio_ns::ip::tcp::endpoint endpoint,
                    const socket_options_cfg_t & socket_options_cfg,
                    Logger logger,
                    on_accept_cb_t on_accept_cb )
{
    return std::make_shared< acceptor_t< Logger > >( std::move( executor ),
                                                     std::move( endpoint ),
                                                     socket_options_cfg,
                                                     std::move( logger ),
                                                     std::move( on_accept_cb ) );
}

/**
 * @brief A factory for creating an instance of acceptor.
 *
 * @param  executor      Executor where to run acceptor operations.
 * @param  endpoint      TCP endpoint where to start the server
 *                       (accept new connections).
 * @param  logger        Loger assigned to the instance of acceptor.
 * @param  on_accept_cb  CB to handle new connections.
 *
 * @return A shared pointer to acceptor.
 */
template < typename Logger >
auto make_acceptor( asio_ns::any_io_executor executor,
                    asio_ns::ip::tcp::endpoint endpoint,
                    Logger logger,
                    on_accept_cb_t on_accept_cb )
{
    return std::make_shared< acceptor_t< Logger > >( std::move( executor ),
                                                     std::move( endpoint ),
                                                     std::move( logger ),
                                                     std::move( on_accept_cb ) );
}

}  // namespace opio::net::tcp
