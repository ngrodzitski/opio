#include <fstream>

#include <CLI/CLI.hpp>

#include <fmt/format.h>
#include <fmt/chrono.h>

#include <logr/logr.hpp>
#include <logr/ostream_backend.hpp>

#include <opio/net/tcp/acceptor.hpp>
#include <opio/net/tcp/connector.hpp>

#include <opio/proto_entry/cfg_json.hpp>

#include <opio/proto_entry/examples/ping_pong/sender/entry.hpp>
#include <opio/proto_entry/examples/ping_pong/receiver/entry.hpp>

namespace /* anonymous */
{

namespace asio_ns = opio::net::asio_ns;

using logger_t = logr::ostream_logger_t<>;

//
// make_logger()
//

[[nodiscard]] inline logger_t make_logger()
{
    return logger_t{ std::cout, logr::log_message_level::trace };
}
//
//  app_cfg_t
//

/**
 * @brief Application configuration.
 */
struct app_cfg_t
{
    opio::proto_entry::entry_full_cfg_t entry;

    //! Entry point for json_dto.
    template < typename Json_Io >
    void json_io( Json_Io & io )
    {
        io & json_dto::mandatory( "entry", entry );
    }
};

//
// read_config_from_file()
//

/**
 * @brief Reads configuration from file.
 *
 * @param  path  Path to file.
 *
 * @return       Parsed configuration.
 */
app_cfg_t read_config_from_file( const std::string & path )
{
    std::ifstream fin{ path.c_str(), std::ios::binary };
    return json_dto::from_stream< app_cfg_t,
                                  rapidjson::kParseCommentsFlag
                                      | rapidjson::kParseTrailingCommasFlag >(
        fin );
}

using request_t        = opio::proto_entry::examples::ping_pong::PingRequest;
using reply_t          = opio::proto_entry::examples::ping_pong::PongReply;
using multi_send_msg_t = opio::proto_entry::examples::ping_pong::MultiSendMsg;

class ping_server_t;

//
// ping_server_t
//

class ping_server_t : public std::enable_shared_from_this< ping_server_t >
{
    using acceptor_t      = opio::net::tcp::acceptor_t< logger_t >;
    using acceptor_sptr_t = std::shared_ptr< acceptor_t >;

public:
    using entry_t = opio::proto_entry::examples::ping_pong::receiver::
        entry_singlethread_t< std::weak_ptr< ping_server_t >, logger_t >;
    using connection_sptr_t = entry_t::sptr_t;

    ping_server_t( opio::net::asio_ns::io_context & io_ctx,
                   opio::proto_entry::entry_full_cfg_t cfg )
        : m_io_ctx{ io_ctx }
        , m_cfg{ std::move( cfg ) }
    {
    }

    void start()
    {
        m_logger.info( OPIO_SRC_LOCATION, "starting ping-pong acceeptor" );
        m_acceptor = opio::net::tcp::make_acceptor(
            m_io_ctx.get_executor(),
            m_cfg.endpoint.make_endpoint(),
            m_cfg.endpoint.socket_options,
            m_logger,
            [ self = shared_from_this() ]( auto socket ) {
                self->on_client_connected( std::move( socket ) );
            } );
        m_logger.info( OPIO_SRC_LOCATION, "starting ping-pong acceeptor" );
        m_acceptor->open();
    }

    void stop()
    {
        m_logger.info( OPIO_SRC_LOCATION, "finish ping-pong acceptor" );
        m_acceptor->close();

        for( auto & c : m_connections )
        {
            m_logger.debug( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                format_to( out,
                           "Connection: {}",
                           static_cast< const void * >( c.second.get() ) );
            } );

            c.second->close();
        }

        m_connections.clear();
    }

    void on_message( typename entry_t::template message_carrier_t< request_t > req,
                     entry_t & client )
    {
        client.logger().info( OPIO_SRC_LOCATION, [ & ]( auto out ) {
            format_to( out, "Received ping: {}", req->ShortDebugString() );
        } );

        reply_t reply;
        reply.set_req_id( req->req_id() );
        reply.set_payload( req->payload() );
        client.send( std::move( reply ) );

        if( ++m_ping_count % 4 == 0 )
        {
            initiate_multisend();
        }
    }

private:
    [[nodiscard]] static std::uint64_t make_now_ts() noexcept
    {
        using namespace std::chrono;
        return duration_cast< nanoseconds >(
                   system_clock::now().time_since_epoch() )
            .count();
    }

    void initiate_multisend()
    {
        multi_send_msg_t msg{};

        msg.set_seq_num( m_ping_count );
        msg.set_message( fmt::format( "Wow, you all guys in total made {} pings",
                                      m_ping_count ) );

        const auto ts = make_now_ts();
        auto attached_buffer =
            std::make_shared< opio::net::simple_buffer_t >( &ts, sizeof( ts ) );

        auto package_buf = std::make_shared< opio::net::simple_buffer_t >(
            opio::proto_entry::examples::ping_pong::receiver::make_package_image(
                msg, attached_buffer->size() ) );

        std::for_each(
            begin( m_connections ), end( m_connections ), [ & ]( auto & client ) {
                ::opio::proto_entry::get_entry_ptr( client )
                    ->schedule_send_raw_bufs( package_buf, attached_buffer );
            } );
    }

    void on_client_connected( opio::net::asio_ns::ip::tcp::socket socket )
    {
        auto entry = entry_t::make( std::move( socket ), [ & ]( auto & params ) {
            // Next line is due to msvc c++17 limitation
            // it can't recognize `this` in the following expression
            // in lambda caprure.
            auto * self = this;
            params.entry_config( m_cfg.make_short_cfg() )
                .logger( make_logger() )
                .underlying_connection_cfg( opio::net::tcp::connection_cfg_t{} )
                .message_consumer( this->weak_from_this() )
                .shutdown_handler( [ wp = self->weak_from_this() ]( auto id ) {
                    if( auto self = wp.lock(); self )
                    {
                        self->m_connections.erase( id );
                    }
                } );
        } );

        m_connections[ entry->underlying_connection_id() ] = entry;

        m_logger.info( OPIO_SRC_LOCATION, [ & ]( auto out ) {
            format_to( out,
                       "client connected: id={}",
                       entry->underlying_connection_id() );
        } );
    }

    opio::net::asio_ns::io_context & m_io_ctx;
    const opio::proto_entry::entry_full_cfg_t m_cfg;
    acceptor_sptr_t m_acceptor;
    logger_t m_logger = make_logger();

    std::map< opio::net::tcp::connection_id_t, connection_sptr_t > m_connections;

    std::size_t m_ping_count{};
};

//
// ping_client_t
//

class ping_client_t : public std::enable_shared_from_this< ping_client_t >
{
public:
    using entry_t = opio::proto_entry::examples::ping_pong::sender::
        entry_singlethread_t< std::weak_ptr< ping_client_t >, logger_t >;
    using connection_sptr_t = std::shared_ptr< entry_t >;

    ping_client_t( opio::net::asio_ns::io_context & io_ctx,
                   opio::proto_entry::entry_full_cfg_t cfg )
        : m_io_ctx{ io_ctx }
        , m_cfg{ std::move( cfg ) }
        , m_operation_timer{ m_io_ctx }
    {
    }

    void connect()
    {
        m_logger.info( OPIO_SRC_LOCATION, "start ping-pong client" );
        async_connect(
            m_io_ctx.get_executor(),
            m_cfg.endpoint.make_query(),
            m_cfg.endpoint.socket_options,
            m_logger,
            [ self = shared_from_this() ]( auto ec, auto socket ) {
                if( !ec )
                {
                    self->m_connection = entry_t::make(
                        std::move( socket ), [ &self ]( auto & params ) {
                            params.connection_id( 0 )
                                .entry_config( self->m_cfg.make_short_cfg() )
                                .logger( make_logger() )
                                .message_consumer( self->weak_from_this() )
                                .shutdown_handler( [ wp = self->weak_from_this() ](
                                                       [[maybe_unused]] auto r ) {
                                    if( auto self = wp.lock(); self )
                                    {
                                        self->connect();
                                    }
                                } );
                        } );
                }
            } );
    }

    void stop()
    {
        m_logger.info( OPIO_SRC_LOCATION, "finish ping-pong client" );
        m_connection->close();
        m_connection.reset();
    }

    void schedule_next_timer()
    {
        m_operation_timer.expires_after( std::chrono::seconds( 10 ) );
        m_operation_timer.async_wait( [ log = m_logger, wp = weak_from_this() ](
                                          const auto & ec ) mutable {
            if( auto self = wp.lock(); self )
            {
                self->m_logger.info( OPIO_SRC_LOCATION, "Timer event" );
                if( !ec )
                {
                    if( self->m_connection )
                    {
                        using request_t =
                            opio::proto_entry::examples::ping_pong::PingRequest;

                        request_t req;
                        req.set_req_id( self->m_req_id_counter++ );
                        req.set_payload( "##########***********" );

                        self->m_connection->send( std::move( req ) );
                        self->m_logger.info( OPIO_SRC_LOCATION,
                                             "ping_request sent" );
                    }
                    else
                    {
                        self->connect();
                    }

                    self->schedule_next_timer();
                }
            }
        } );
    }

    void on_message( [[maybe_unused]]
                     typename entry_t::template message_carrier_t< reply_t > rep,
                     entry_t & client )
    {
        client.logger().info( OPIO_SRC_LOCATION, [ & ]( auto out ) {
            format_to( out, "Received pong: {}", rep->ShortDebugString() );
        } );
    }

    void on_message(
        [[maybe_unused]]
        typename entry_t::template message_carrier_t< multi_send_msg_t > msg,
        entry_t & client )
    {
        std::uint64_t ts{};

        if( msg.attached_buffer().size() == sizeof( ts ) )
        {
            std::memcpy( &ts, msg.attached_buffer().data(), sizeof( ts ) );
        }

        client.logger().info( OPIO_SRC_LOCATION, [ & ]( auto out ) {
            format_to( out,
                       "Received multi msg: {}, ts=0x{:X}",
                       msg->ShortDebugString(),
                       ts );
        } );
    }

private:
    opio::net::asio_ns::io_context & m_io_ctx;

    const opio::proto_entry::entry_full_cfg_t m_cfg;

    logger_t m_logger = make_logger();

    connection_sptr_t m_connection;

    asio_ns::steady_timer m_operation_timer;
    std::uint32_t m_req_id_counter{ 1 };
};

//
// run_ioctx()
//

//! Run asio event loop.
void run_ioctx( asio_ns::io_context & ioctx )
{
    auto break_signals =
        std::make_unique< asio_ns::signal_set >( ioctx, SIGINT, SIGTERM );

    break_signals->async_wait( [ & ]( auto ec, [[maybe_unused]] auto signal ) {
        if( !ec )
        {
            ioctx.stop();
        }
    } );

    ioctx.run();
}

void start_server( const app_cfg_t & cfg )
{
    auto logger = make_logger();

    logger.info( OPIO_SRC_LOCATION, "Starting ping-pong server" );

    asio_ns::io_context ioctx;

    auto server = std::make_shared< ping_server_t >( ioctx, cfg.entry );

    server->start();
    run_ioctx( ioctx );
    server->stop();

    // Nicer logging: let all queued events to complete.
    ioctx.restart();
    server.reset();

    ioctx.run();
}

void start_client( const app_cfg_t & cfg )
{
    auto logger = make_logger();

    logger.info( OPIO_SRC_LOCATION, "Starting ping-pong client" );

    asio_ns::io_context ioctx;

    auto client = std::make_shared< ping_client_t >( ioctx, cfg.entry );

    client->connect();
    client->schedule_next_timer();
    run_ioctx( ioctx );
    client->stop();

    // Nicer logging: let all queued events to complete.
    ioctx.restart();
    client.reset();
    ioctx.run();
}

}  // anonymous namespace

int main( int argc, const char * argv[] )
{
    try
    {
        CLI::App app{ "opio proto_entry ping-pong example" };

        std::string cfg_file_path;
        bool is_server = false;
        bool is_client = false;

        app.add_option( "--config", cfg_file_path, "app configuration" )
            ->required( true );

        auto * server_opt = app.add_flag( "--server", is_server, "Start server" );

        auto * client_opt = app.add_flag( "--client", is_client, "Start client" );

        server_opt->excludes( client_opt );
        client_opt->excludes( server_opt );

        CLI11_PARSE( app, argc, argv );

        if( !is_server && !is_client )
        {
            throw std::runtime_error{ "--server/--client must be specified" };
        }

        const auto cfg = read_config_from_file( cfg_file_path );

        if( is_server )
        {
            start_server( cfg );
        }
        else
        {
            start_client( cfg );
        }
    }
    catch( const std::exception & ex )
    {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
}
