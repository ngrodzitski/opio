#include <opio/net/tcp/acceptor.hpp>

#include <opio/net/tcp/connector.hpp>
#include <opio/net/tcp/error_code.hpp>
#include <opio/exception.hpp>

#include <gtest/gtest.h>

#include <opio/test_utils/test_logger.hpp>

#include <tcp_test_utils.hpp>

namespace /* anonymous */
{

using namespace ::opio;              // NOLINT
using namespace ::opio::net;         // NOLINT
using namespace ::opio::net::tcp;    // NOLINT
using namespace ::opio::test_utils;  // NOLINT

TEST( OpioNetTcp, AcceptorOpenClose )  // NOLINT
{
    asio_ns::io_context ioctx( 1 );
    asio_ns::ip::tcp::endpoint ep{ asio_ns::ip::make_address( "127.0.0.1" ),
                                   make_random_port_value() };

    auto accept_happened           = 0;
    bool acceptor_opened_cb_called = false;
    bool acceptor_closed_cb_called = false;
    auto acceptor                  = make_acceptor(
        ioctx.get_executor(),
        ep,
        make_test_logger( "acceptor" ),
        [ & ]( [[maybe_unused]] auto socket ) { ++accept_happened; } );

    acceptor->open( [ & ]( const auto & ec ) {
        ASSERT_FALSE( ec );
        acceptor_opened_cb_called = true;
    } );
    ioctx.poll();

    acceptor->close( [ & ]( const auto & ec ) {
        ASSERT_FALSE( ec );
        acceptor_closed_cb_called = true;
    } );

    ioctx.run();
    ASSERT_EQ( accept_happened, 0 );
    ASSERT_TRUE( acceptor_opened_cb_called );
    ASSERT_TRUE( acceptor_closed_cb_called );
}

TEST( OpioNetTcp, AcceptorOpenCloseExceptionOnOpen1 )  // NOLINT
{
    asio_ns::io_context ioctx( 1 );
    asio_ns::ip::tcp::endpoint ep{ asio_ns::ip::make_address( "127.0.0.1" ),
                                   make_random_port_value() };

    auto acceptor = make_acceptor( ioctx.get_executor(),
                                   ep,
                                   make_test_logger( "acceptor" ),
                                   [ & ]( [[maybe_unused]] auto socket ) {} );

    acceptor->open( [ & ]( const auto & ec ) {
        if( !ec )
        {
            throw_exception( "check acceptor {} exception", "open" );
        }

        ASSERT_EQ( &( ec.category() ), &( tcp::details::error_category() ) );
        ASSERT_NE( ec.category().name(), "" );
        ASSERT_EQ( static_cast< error_codes >( ec.value() ),
                   error_codes::open_acceptor_failed_exception_happened );
        ASSERT_NE( ec.message(), "" );
    } );

    ioctx.poll();
    acceptor->close( [ & ]( const auto & ec ) { ASSERT_FALSE( ec ); } );

    ioctx.run();
}

TEST( OpioNetTcp, AcceptorOpenCloseExceptionOnOpen2 )  // NOLINT
{
    asio_ns::io_context ioctx( 1 );
    asio_ns::ip::tcp::endpoint ep{ asio_ns::ip::make_address( "127.0.0.1" ),
                                   make_random_port_value() };

    auto acceptor = make_acceptor( ioctx.get_executor(),
                                   ep,
                                   make_test_logger( "acceptor" ),
                                   [ & ]( [[maybe_unused]] auto socket ) {} );

    acceptor->open( [ & ]( const auto & ec ) { ASSERT_FALSE( ec ); } );
    acceptor->open( [ & ]( const auto & ec ) {
        ASSERT_TRUE( ec );

        ASSERT_EQ( &( ec.category() ), &( tcp::details::error_category() ) );
        ASSERT_NE( ec.category().name(), "" );
        ASSERT_EQ( static_cast< error_codes >( ec.value() ),
                   error_codes::open_acceptor_failed_already_started );
        ASSERT_NE( ec.message(), "" );
    } );

    ioctx.poll();
    acceptor->close( [ & ]( const auto & ec ) { ASSERT_FALSE( ec ); } );

    ioctx.run();
}

TEST( OpioNetTcp, AcceptorOpenCloseExceptionOnClose1 )  // NOLINT
{
    asio_ns::io_context ioctx( 1 );
    asio_ns::ip::tcp::endpoint ep{ asio_ns::ip::make_address( "127.0.0.1" ),
                                   make_random_port_value() };

    auto acceptor = make_acceptor( ioctx.get_executor(),
                                   ep,
                                   make_test_logger( "acceptor" ),
                                   [ & ]( [[maybe_unused]] auto socket ) {} );

    acceptor->open( [ & ]( const auto & ec ) { ASSERT_FALSE( ec ); } );
    ioctx.poll();

    acceptor->close( [ & ]( const auto & ec ) {
        if( !ec )
        {
            throw_exception( "check acceptor close exception" );
        }

        ASSERT_EQ( &( ec.category() ), &( tcp::details::error_category() ) );
        ASSERT_NE( ec.category().name(), "" );
        ASSERT_EQ( static_cast< error_codes >( ec.value() ),
                   error_codes::close_acceptor_failed_exception_happened );
        ASSERT_NE( ec.message(), "" );
    } );

    ioctx.run();
}

TEST( OpioNetTcp, AcceptorOpenCloseExceptionOnClose2 )  // NOLINT
{
    asio_ns::io_context ioctx( 1 );
    asio_ns::ip::tcp::endpoint ep{ asio_ns::ip::make_address( "127.0.0.1" ),
                                   make_random_port_value() };

    auto acceptor = make_acceptor( ioctx.get_executor(),
                                   ep,
                                   make_test_logger( "acceptor" ),
                                   [ & ]( [[maybe_unused]] auto socket ) {} );

    acceptor->open( [ & ]( const auto & ec ) { ASSERT_FALSE( ec ); } );
    ioctx.poll();

    acceptor->close( [ & ]( const auto & ec ) { ASSERT_FALSE( ec ); } );

    acceptor->close( [ & ]( const auto & ec ) {
        ASSERT_TRUE( ec );
        ASSERT_EQ( &( ec.category() ), &( tcp::details::error_category() ) );
        ASSERT_NE( ec.category().name(), "" );
        ASSERT_EQ( static_cast< error_codes >( ec.value() ),
                   error_codes::close_acceptor_failed_not_running );
        ASSERT_NE( ec.message(), "" );
    } );

    ioctx.poll();

    ioctx.run();
}

TEST( OpioNetTcp, AcceptorOpenDuplicate )  // NOLINT
{
    asio_ns::io_context ioctx( 1 );
    asio_ns::ip::tcp::endpoint ep{ asio_ns::ip::make_address( "127.0.0.1" ),
                                   make_random_port_value() };

    auto accept_happened             = 0;
    bool acceptor_opened_cb_called   = false;
    bool acceptor_opened_cb_called_2 = false;
    bool acceptor_closed_cb_called   = false;
    auto acceptor                    = make_acceptor(
        ioctx.get_executor(),
        ep,
        make_test_logger( "acceptor" ),
        [ & ]( [[maybe_unused]] auto socket ) { ++accept_happened; } );

    acceptor->open( [ & ]( const auto & ec ) {
        ASSERT_FALSE( ec );
        acceptor_opened_cb_called = true;
    } );
    acceptor->open( [ & ]( const auto & ec ) {
        ASSERT_TRUE( !!ec );
        acceptor_opened_cb_called_2 = true;
    } );
    ioctx.poll();

    acceptor->close( [ & ]( const auto & ec ) {
        ASSERT_FALSE( ec );
        acceptor_closed_cb_called = true;
    } );

    ioctx.run();
    ASSERT_EQ( accept_happened, 0 );
    ASSERT_TRUE( acceptor_opened_cb_called );
    ASSERT_TRUE( acceptor_opened_cb_called_2 );
    ASSERT_TRUE( acceptor_closed_cb_called );
}

TEST( OpioNetTcp, AcceptorCloseDuplicate )  // NOLINT
{
    asio_ns::io_context ioctx( 1 );
    asio_ns::ip::tcp::endpoint ep{ asio_ns::ip::make_address( "127.0.0.1" ),
                                   make_random_port_value() };

    auto accept_happened             = 0;
    bool acceptor_opened_cb_called   = false;
    bool acceptor_closed_cb_called   = false;
    bool acceptor_closed_cb_called_2 = false;
    auto acceptor                    = make_acceptor(
        ioctx.get_executor(),
        ep,
        make_test_logger( "acceptor" ),
        [ & ]( [[maybe_unused]] auto socket ) { ++accept_happened; } );

    acceptor->open( [ & ]( const auto & ec ) {
        ASSERT_FALSE( ec );
        acceptor_opened_cb_called = true;
    } );

    ioctx.poll();

    acceptor->close( [ & ]( const auto & ec ) {
        ASSERT_FALSE( ec );
        acceptor_closed_cb_called = true;
    } );

    acceptor->close( [ & ]( const auto & ec ) {
        ASSERT_TRUE( !!ec );
        acceptor_closed_cb_called_2 = true;
    } );

    ioctx.run();
    ASSERT_EQ( accept_happened, 0 );
    ASSERT_TRUE( acceptor_opened_cb_called );
    ASSERT_TRUE( acceptor_closed_cb_called );
    ASSERT_TRUE( acceptor_closed_cb_called_2 );
}

TEST( OpioNetTcp, AcceptorAccept1 )  // NOLINT
{
    asio_ns::io_context ioctx( 1 );
    const auto port = make_random_port_value();
    asio_ns::ip::tcp::endpoint ep{ asio_ns::ip::tcp::v4(), port };

    auto accept_happened  = 0;
    auto connect_happened = 0;
    auto acceptor         = make_acceptor( ioctx.get_executor(),
                                   ep,
                                   make_test_logger( "acceptor" ),
                                   [ & ]( auto socket ) {
                                       ASSERT_TRUE( socket.is_open() );
                                       ++accept_happened;
                                   } );

    acceptor->open();

    ioctx.poll();

    async_connect( ioctx.get_executor(),
                   tcp_resolver_query_t{ asio_ns::ip::tcp::v4(),
                                         "localhost",
                                         std::to_string( port ) },
                   make_test_logger( "connector" ),
                   [ & ]( const auto & ec, auto socket ) {
                       ++connect_happened;
                       ASSERT_FALSE( ec );
                       ASSERT_TRUE( socket.is_open() );
                   } );

    while( 0 == accept_happened )  // NOLINT
    {
        ioctx.poll();
    }

    acceptor->close();
    ioctx.run();
    ASSERT_EQ( accept_happened, 1 );
    ASSERT_EQ( connect_happened, 1 );
}

TEST( OpioNetTcp, AcceptorAccept10 )  // NOLINT
{
    asio_ns::io_context ioctx( 1 );
    const auto port = make_random_port_value();
    asio_ns::ip::tcp::endpoint ep{ asio_ns::ip::tcp::v4(), port };

    auto accept_happened  = 0;
    auto connect_happened = 0;
    auto acceptor         = make_acceptor( ioctx.get_executor(),
                                   ep,
                                   make_test_logger( "acceptor" ),
                                   [ & ]( auto socket ) {
                                       ASSERT_TRUE( socket.is_open() );
                                       ++accept_happened;
                                   } );

    acceptor->open();

    ioctx.poll();

    for( auto i = 0; i < 10; ++i )
    {
        async_connect( ioctx.get_executor(),
                       tcp_resolver_query_t{ asio_ns::ip::tcp::v4(),
                                             "localhost",
                                             std::to_string( port ) },
                       make_test_logger( "connector" ),
                       [ & ]( const auto & ec, auto socket ) {
                           ++connect_happened;
                           ASSERT_FALSE( ec );
                           ASSERT_TRUE( socket.is_open() );
                       } );
    }

    while( accept_happened < 10 )  // NOLINT
    {
        ioctx.poll();
    }

    acceptor->close();
    ioctx.run();
    ASSERT_EQ( accept_happened, 10 );
    ASSERT_EQ( connect_happened, 10 );
}

}  // anonymous namespace
