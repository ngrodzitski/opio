#include <opio/net/tcp/connector.hpp>

#include <gtest/gtest.h>

#include <opio/test_utils/test_logger.hpp>

namespace /* anonymous */
{

using namespace opio::net;         // NOLINT
using namespace opio::net::tcp;    // NOLINT
using namespace opio::test_utils;  // NOLINT

TEST( OpioNetTcp, ConnectorOk )  // NOLINT
{
    asio_ns::io_context ioctx( 1 );
    asio_ns::ip::tcp::endpoint ep{ asio_ns::ip::make_address( "127.0.0.1" ),
                                   12999 };
    asio_ns::ip::tcp::acceptor acceptor{ ioctx, ep };

    bool accept_happened  = false;
    bool connect_happened = false;

    acceptor.async_accept( [ & ]( const auto & ec, auto socket ) {
        ASSERT_TRUE( !ec );
        asio_ns::error_code ignored_ec;

        socket.shutdown( asio_ns::ip::tcp::socket::shutdown_both, ignored_ec );
        socket.close( ignored_ec );
        accept_happened = true;
    } );

    ioctx.poll();

    async_connect( ioctx.get_executor(),
                   "localhost",
                   12999,
                   make_test_logger( "connector" ),
                   [ & ]( const auto & ec, auto socket ) {
                       connect_happened = true;
                       ASSERT_TRUE( !ec );
                       ASSERT_TRUE( socket.is_open() );
                   } );
    ioctx.run();
    ASSERT_TRUE( accept_happened );
    ASSERT_TRUE( connect_happened );
}

TEST( OpioNetTcp, ConnectorSocketOptions )  // NOLINT
{
    asio_ns::io_context ioctx( 1 );
    asio_ns::ip::tcp::endpoint ep{ asio_ns::ip::make_address( "127.0.0.1" ),
                                   21999 };
    asio_ns::ip::tcp::acceptor acceptor{ ioctx, ep };

    bool accept_happened  = false;
    bool connect_happened = false;

    acceptor.async_accept( [ & ]( const auto & ec, auto socket ) {
        ASSERT_TRUE( !ec );
        asio_ns::error_code ignored_ec;

        socket.shutdown( asio_ns::ip::tcp::socket::shutdown_both, ignored_ec );
        socket.close( ignored_ec );
        accept_happened = true;
    } );

    ioctx.poll();

    socket_options_cfg_t socket_options_cfg{};
    socket_options_cfg.receive_buffer_size = 8320;

    async_connect(
        ioctx.get_executor(),
        tcp_resolver_query_t{ asio_ns::ip::tcp::v4(), "localhost", "21999" },
        socket_options_cfg,
        make_test_logger( "connector" ),
        [ & ]( const auto & ec, auto socket ) {
            connect_happened = true;
            ASSERT_TRUE( !ec );
            ASSERT_TRUE( socket.is_open() );

            asio_ns::socket_base::receive_buffer_size option;
            socket.get_option( option );
            EXPECT_EQ( option.value(), 8320 );
        } );
    ioctx.run();
    ASSERT_TRUE( accept_happened );
    ASSERT_TRUE( connect_happened );
}

TEST( OpioNetTcp, ConnectorResolveFailed )  // NOLINT
{
    asio_ns::io_context ioctx( 1 );

    bool handler_called = false;
    async_connect( ioctx.get_executor(),
                   tcp_resolver_query_t{
                       asio_ns::ip::tcp::v4(), "very--weird--host--name", "2999" },
                   make_test_logger( "connector" ),
                   [ & ]( const auto & ec, auto socket ) {
                       ASSERT_FALSE( !ec );
                       ASSERT_FALSE( socket.is_open() );
                       handler_called = true;
                   } );
    ioctx.run();
    ASSERT_TRUE( handler_called );
}

TEST( OpioNetTcp, ConnectorConnectFailed )  // NOLINT
{
    asio_ns::io_context ioctx( 1 );

    bool handler_called = false;
    async_connect(
        ioctx.get_executor(),
        tcp_resolver_query_t{ asio_ns::ip::tcp::v4(), "localhost", "2444" },
        make_test_logger( "connector" ),
        [ & ]( const auto & ec, [[maybe_unused]] auto socket ) {
            ASSERT_FALSE( !ec );
            handler_called = true;
        } );
    ioctx.run();
    ASSERT_TRUE( handler_called );
}

}  // anonymous namespace
