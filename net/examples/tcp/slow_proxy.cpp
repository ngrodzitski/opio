/**
 * @file
 *
 * A simple example that acts as a proxy.
 *
 * Can be used to emulate "slow" client.
 */

#include <fstream>
#include <map>
#include <string>

#include <json_dto/pub.hpp>
#include <json_dto/validators.hpp>

#include <opio/net/asio_include.hpp>
#include <opio/net/tcp/connection.hpp>
#include <opio/net/tcp/acceptor.hpp>
#include <opio/net/tcp/cfg.hpp>
#include <opio/net/tcp/cfg_json.hpp>

#include "../logger.hpp"

namespace /* anonymous */
{

struct server_cfg_t
{
    opio::net::tcp::tcp_endpoint_cfg_t endpoint;
    opio::net::tcp::socket_options_cfg_t socket_options;

    template < typename Json_Io >
    void json_io( Json_Io & io )
    {
        io & json_dto::mandatory( "endpoint", endpoint )
            & json_dto::optional( "socket_options",
                                  socket_options,
                                  opio::net::tcp::socket_options_cfg_t{} );
    }
};

struct app_cfg_t
{
    server_cfg_t self_server;
    server_cfg_t target_server;

    std::uint32_t kb_per_sec{ 1 };
    int log_level{};

    std::uint16_t limit_after_timeout_sec{ 0 };

    template < typename Json_Io >
    void json_io( Json_Io & io )
    {
        io & json_dto::mandatory( "self_server", self_server )
            & json_dto::mandatory( "target_server", target_server )
            & json_dto::mandatory(
                "kb_per_sec",
                kb_per_sec,
                json_dto::min_max_constraint< std::uint32_t >( 1, 1024 * 1024 ) )
            & json_dto::optional( "log_level",
                                  log_level,
                                  2,
                                  json_dto::min_max_constraint< int >( 0, 5 ) )
            & json_dto::optional(
                "limit_after_timeout_sec", limit_after_timeout_sec, 0 );
    }

    auto make_log_level() const noexcept
    {
        return static_cast< logr::log_message_level >( log_level );
    }
};

auto read_config_from_json_file( const std::string & path )
{
    std::ifstream fin( path.c_str(), std::ios::binary );

    return json_dto::from_stream< app_cfg_t,
                                  rapidjson::kParseCommentsFlag
                                      | rapidjson::kParseTrailingCommasFlag >(
        fin );
}

}  // anonymous namespace

struct connection_traits_st_t : public opio::net::tcp::default_traits_st_t
{
    using strand_t             = opio::net::tcp::noop_strand_t;
    using logger_t             = console_logger_t;
    using input_handler_t      = std::function< void(
        opio::net::tcp::input_ctx_t< connection_traits_st_t > & ) >;
    using operation_watchdog_t = opio::net::asio_timer_operation_watchdog_t;
};

using connection_cfg_t  = opio::net::tcp::connection_cfg_t;
using connection_t      = opio::net::tcp::connection_t< connection_traits_st_t >;
using connection_sptr_t = typename connection_t::sptr_t;
namespace asio_ns       = opio::net::asio_ns;

//
// session_t
//

class session_t : public std::enable_shared_from_this< session_t >
{
private:
    session_t( const app_cfg_t & app_cfg,
               asio_ns::any_io_executor executor,
               connection_sptr_t client,
               connection_sptr_t server )
        : m_kb_per_sec{ app_cfg.kb_per_sec }
        , m_limit_after{ std::chrono::steady_clock::now()
                         + std::chrono::seconds(
                             app_cfg.limit_after_timeout_sec ) }
        , m_client{ executor, std::move( client ) }
        , m_server{ executor, std::move( server ) }
        , m_logger{ make_logger(
              fmt::format( "ses({})", m_client.con->remote_endpoint_str() ),
              app_cfg.make_log_level() ) }
    {
        m_logger.info( [ & ]( auto out ) {
            format_to( out,
                       "Create session {} => {} @{}",
                       m_client.con->remote_endpoint_str(),
                       m_server.con->remote_endpoint_str(),
                       static_cast< void * >( this ) );
        } );
    }

public:
    ~session_t()
    {
        m_logger.info( [ & ]( auto out ) {
            format_to( out,
                       "Destroy session {} => {} @{}",
                       m_client.con->remote_endpoint_str(),
                       m_server.con->remote_endpoint_str(),
                       static_cast< void * >( this ) );
        } );
    }

    using sptr_t             = std::shared_ptr< session_t >;
    using session_registry_r = std::map< std::string, sptr_t >;

    static sptr_t make( session_registry_r & registry,
                        const app_cfg_t & app_cfg,
                        asio_ns::any_io_executor executor,
                        asio_ns::ip::tcp::socket client_socket,
                        asio_ns::ip::tcp::socket server_socket )
    {
        static std::uint64_t id_counter{};
        auto empty_input_handler = [ & ]( [[maybe_unused]] auto & ctx ) {};

        const auto read_buf_len = 1024 * app_cfg.kb_per_sec / 16;

        std::shared_ptr< session_t > ses{ new session_t{
            app_cfg,
            executor,
            connection_t::make(
                std::move( client_socket ),
                id_counter++,
                connection_cfg_t{}.input_buffer_size( read_buf_len ),
                make_logger( "cli", app_cfg.make_log_level() ),
                {},
                empty_input_handler,
                {},
                { executor },
                {} ),
            connection_t::make(
                std::move( server_socket ),
                id_counter++,
                connection_cfg_t{}.input_buffer_size( read_buf_len ),
                make_logger( "srv", app_cfg.make_log_level() ),
                {},
                empty_input_handler,
                {},
                { executor },
                {} ) } };

        auto w = ses->weak_from_this();
        assert( w.lock() );
        ses->m_client.con->reset_input_handler( [ w ]( auto & ctx ) {
            if( auto s = w.lock(); s )
            {
                s->schedule_next_read_from( ctx.buf().size(), s->m_client );
                s->m_server.con->schedule_send( std::move( ctx.buf() ) );
            }
        } );
        ses->m_server.con->reset_input_handler( [ w ]( auto & ctx ) {
            if( auto s = w.lock(); s )
            {
                s->schedule_next_read_from( ctx.buf().size(), s->m_server );
                s->m_client.con->schedule_send( std::move( ctx.buf() ) );
            }
        } );

        std::string registry_key = ses->m_client.con->remote_endpoint_str();

        registry[ registry_key ] = ses;

        auto shutdown_handler = [ &registry,
                                  registry_key,
                                  w = ses->weak_from_this() ](
                                    [[maybe_unused]] auto res ) {
            if( opio::net::tcp::connection_shutdown_reason::user_initiated == res )
            {
                return;
            }

            if( auto s = w.lock(); s )
            {
                s->close();
                registry.erase( registry_key );
            }
        };

        ses->m_client.con->reset_shutdown_handler( shutdown_handler );
        ses->m_server.con->reset_shutdown_handler( shutdown_handler );

        ses->m_client.con->start_reading();
        ses->m_server.con->start_reading();

        return ses;
    }

    void close()
    {
        m_client.con->shutdown();
        m_server.con->shutdown();

        m_logger.info( [ & ]( auto out ) {
            format_to( out,
                       "finish session {} => {}",
                       m_client.con->remote_endpoint_str(),
                       m_server.con->remote_endpoint_str() );
        } );
    }

private:
    struct connection_ctx_t
    {
        connection_ctx_t( asio_ns::any_io_executor executor, connection_sptr_t c )
            : next_read{ executor }
            , con( std::move( c ) )
        {
        }

        asio_ns::steady_timer next_read;
        connection_sptr_t con;
    };

    void schedule_next_read_from( std::uint32_t size_just_read,
                                  connection_ctx_t & ctx )
    {
        if( std::chrono::steady_clock::now() <= m_limit_after )
        {
            return;
        }

        ctx.con->stop_reading();

        const auto usec_to_wait =
            ( size_just_read * 1000 * 1000 ) / ( m_kb_per_sec * 1024 );
        ctx.next_read.expires_after( std::chrono::microseconds( usec_to_wait ) );

        ctx.next_read.async_wait(
            [ w = weak_from_this(), &ctx ]( const auto & ec ) {
                if( !ec )
                {
                    if( auto s = w.lock(); s )
                    {
                        ctx.con->start_reading();
                    }
                }
            } );
    }

    const std::uint32_t m_kb_per_sec;
    const std::chrono::time_point< std::chrono::steady_clock > m_limit_after;

    connection_ctx_t m_client;
    connection_ctx_t m_server;

    console_logger_t m_logger;
};

int main( int argc, char * argv[] )
{
    try
    {
        if( 2 != argc )
        {
            throw std::runtime_error{
                "config file path should be provided as a first argument"
            };
        }

        const auto app_cfg = read_config_from_json_file( argv[ 1 ] );

        asio_ns::io_context ioctx( 1 );

        auto app_log = make_logger( "_example.opio.net.tcp.slow_proxy",
                                    app_cfg.make_log_level() );

        session_t::session_registry_r sessions;

        auto acceptor = opio::net::tcp::make_acceptor(
            ioctx.get_executor(),
            app_cfg.self_server.endpoint.make_endpoint(),
            make_logger( "acceptor", app_cfg.make_log_level() ),
            [ & ]( auto client_socket ) {
                asio_ns::ip::tcp::socket server_socket{ ioctx.get_executor() };

                app_log.info( "connecting to target server" );
                asio_ns::error_code ec;
                server_socket.connect(
                    app_cfg.target_server.endpoint.make_endpoint(), ec );

                if( ec )
                {
                    app_log.error( [ & ]( auto out ) {
                        format_to( out,
                                   "Unable to connect to server: {}",
                                   opio::net::fmt_integrator( ec ) );
                    } );
                    return;
                }

                session_t::make( sessions,
                                 app_cfg,
                                 ioctx.get_executor(),
                                 std::move( client_socket ),
                                 std::move( server_socket ) );
            } );

        app_log.info( "Starting acceptor" );
        acceptor->open();

        auto break_signals =
            std::make_unique< asio_ns::signal_set >( ioctx, SIGINT, SIGTERM );

        break_signals->async_wait( [ & ]( auto ec, [[maybe_unused]] auto signal ) {
            if( !ec )
            {
                ioctx.stop();
            }
        } );

        app_log.info( "Starting running asio io context" );
        ioctx.run();

        ioctx.restart();

        acceptor->close();

        for( auto & s : sessions )
        {
            s.second->close();
        }

        ioctx.run();
    }
    catch( const std::exception & ex )
    {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
