#include <opio/net/tcp/utils.hpp>

#include <opio/net/tcp/acceptor.hpp>
#include <opio/net/tcp/connector.hpp>

#include <opio/test_utils/test_logger.hpp>
#include <tcp_test_utils.hpp>

#include <gtest/gtest.h>

namespace /* anonymous */
{

using namespace opio::net;           // NOLINT
using namespace opio::net::tcp;      // NOLINT
using namespace ::opio::test_utils;  // NOLINT

template < typename Test_Body >
void make_socket_and_run_test( const socket_options_cfg_t & cfg,
                               Test_Body test_body )
{
    asio_ns::io_context ioctx( 1 );
    const auto port = make_random_port_value();
    asio_ns::ip::tcp::endpoint ep{ asio_ns::ip::make_address( "127.0.0.1" ),
                                   port };
    auto acceptor = make_acceptor( ioctx.get_executor(),
                                   ep,
                                   cfg,
                                   make_test_logger( "acceptor" ),
                                   [ & ]( auto socket ) { test_body( socket ); } );

    acceptor->open();
    ioctx.poll();

    async_connect( ioctx.get_executor(),
                   tcp_resolver_query_t{ asio_ns::ip::tcp::v4(),
                                         "localhost",
                                         std::to_string( port ) },
                   make_test_logger( "connector" ),
                   [ & ]( [[maybe_unused]] const auto & ec,
                          [[maybe_unused]] auto soc ) { acceptor->close(); } );
    ioctx.run();
}

TEST( OpioNetTcp, UtilsSetSocketOptionsNoDelay )  // NOLINT
{
    {
        socket_options_cfg_t cfg{};
        cfg.no_delay = true;
        make_socket_and_run_test( cfg, []( auto & s ) {
            asio_ns::ip::tcp::no_delay option;
            s.get_option( option );
            EXPECT_TRUE( option.value() );
        } );
    }

    {
        socket_options_cfg_t cfg{};
        cfg.no_delay = false;
        make_socket_and_run_test( cfg, []( auto & s ) {
            asio_ns::ip::tcp::no_delay option;
            s.get_option( option );
            EXPECT_FALSE( option.value() );
        } );
    }
}

TEST( OpioNetTcp, UtilsSetSocketOptionsKeepAlive )  // NOLINT
{
    {
        socket_options_cfg_t cfg{};
        cfg.keep_alive = true;

        make_socket_and_run_test( cfg, []( auto & s ) {
            asio_ns::socket_base::keep_alive option;
            s.get_option( option );
            EXPECT_TRUE( option.value() );
        } );
    }

    {
        socket_options_cfg_t cfg{};
        cfg.keep_alive = false;

        make_socket_and_run_test( cfg, []( auto & s ) {
            asio_ns::socket_base::keep_alive option;
            s.get_option( option );
            EXPECT_FALSE( option.value() );
        } );
    }
}

TEST( OpioNetTcp, UtilsSetSocketOptionsLinger )  // NOLINT
{
    socket_options_cfg_t cfg{};
    cfg.linger = 31;

    make_socket_and_run_test( cfg, []( auto & s ) {
        asio_ns::socket_base::linger option;
        s.get_option( option );
        EXPECT_TRUE( option.enabled() );
        EXPECT_EQ( option.timeout(), 31 );
    } );
}

TEST( OpioNetTcp, UtilsSetSocketOptionsReceiveBufferSize )  // NOLINT
{
    socket_options_cfg_t cfg{};
    cfg.receive_buffer_size = 2127;

    make_socket_and_run_test( cfg, []( auto & s ) {
        asio_ns::socket_base::receive_buffer_size option;
        s.get_option( option );
        EXPECT_EQ( option.value(), 2127 );
    } );
}

TEST( OpioNetTcp, UtilsSetSocketOptionsSendBufferSize )  // NOLINT
{
    socket_options_cfg_t cfg{};
    cfg.send_buffer_size = 4129;

    make_socket_and_run_test( cfg, []( auto & s ) {
        asio_ns::socket_base::send_buffer_size option;
        s.get_option( option );
        EXPECT_EQ( option.value(), 4129 );
    } );
}

}  // anonymous namespace
