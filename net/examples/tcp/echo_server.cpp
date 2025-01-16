/**
 * @file
 *
 * A simple example that shows the usage raw connectio for echo server.
 */

#include <CLI/CLI.hpp>

#include <fmt/ostream.h>

#include <opio/net/asio_include.hpp>
#include <opio/net/tcp/connection.hpp>
#include <opio/net/tcp/acceptor.hpp>

#include "../logger.hpp"

struct connection_traits_st_t : public opio::net::tcp::default_traits_st_t
{
    using strand_t        = opio::net::tcp::noop_strand_t;
    using logger_t        = console_logger_t;
    using input_handler_t = std::function< void(
        opio::net::tcp::input_ctx_t< connection_traits_st_t > & ) >;
};

using connection_cfg_t = opio::net::tcp::connection_cfg_t;
using connection_t     = opio::net::tcp::connection_t< connection_traits_st_t >;
namespace asio_ns      = opio::net::asio_ns;

int main( int argc, char * argv[] )
{
    try
    {
        std::uint16_t port = 3344;
        std::string addr   = "127.0.0.1";

        CLI::App app{ "_example.opio.net.tcp.echo_server echo server" };

        app.add_option( "--port,-p", port, "port to run a server on" )
            ->required( false );
        app.add_option( "--address,-a", port, "adress to run server on" )
            ->required( false );

        CLI11_PARSE( app, argc, argv );

        if( addr == "localhost" )
        {
            addr = "127.0.0.1";
        }
        else if( addr == "ip6-localhost" )
        {
            addr = "::1";
        }

        const asio_ns::ip::tcp::endpoint ep{ asio_ns::ip::make_address( addr ),
                                             port };

        asio_ns::io_context ioctx( 1 );
        auto acceptor = opio::net::tcp::make_acceptor(
            ioctx.get_executor(),
            ep,
            make_logger( "acceptor" ),
            [ id_counter = 0 ]( auto socket ) mutable {
                auto log_name = fmt::format( "{}", socket.remote_endpoint() );
                connection_t::make(
                    std::move( socket ),
                    id_counter++,
                    connection_cfg_t{},
                    make_logger( std::move( log_name ) ),
                    {},
                    [ & ]( auto & ctx ) {
                        ctx.connection().schedule_send( std::move( ctx.buf() ) );
                    },
                    {},
                    {},
                    {} )
                    ->start_reading();
            } );
        acceptor->open();
        ioctx.run();
    }
    catch( const std::exception & ex )
    {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
