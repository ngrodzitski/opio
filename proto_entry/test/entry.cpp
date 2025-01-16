#include <opio/proto_entry/entry_base.hpp>

#include <opio/net/tcp/connector.hpp>
#include <opio/net/tcp/acceptor.hpp>
#include <opio/net/buffer.hpp>

#include <opio/proto_entry/utest/entry.hpp>

#include <gmock/gmock.h>

#include <opio/test_utils/test_logger.hpp>

#include "test_utils.hpp"

namespace /* anonymous */
{

/**
 * @brief Create a raw buffer for a given package message.
 */
template < typename Message >
opio::net::simple_buffer_t convert_to_raw_buffer( const Message & pkg )
{
    using opio::proto_entry::pkg_header_t;
    using opio::net::simple_buffer_t;
    simple_buffer_t pkg_buf;
    pkg_buf.resize( sizeof( pkg_header_t ) + pkg.ByteSizeLong() );
    auto h =
        pkg_header_t::make( static_cast< std::uint32_t >( pkg.ByteSizeLong() ),
                            opio::proto_entry::pkg_content_message );
    std::memcpy( pkg_buf.data(), &h, sizeof( h ) );
    pkg.SerializeToArray( &pkg_buf.data()[ sizeof( h ) ],
                          static_cast< int >( pkg.ByteSizeLong() ) );

    return pkg_buf;
}

// Use Gtest routimes without namspaces.
using namespace ::testing;               // NOLINT
using namespace ::opio::proto_entry;     // NOLINT
using namespace ::opio::test_utils;      // NOLINT
namespace asio_ns = opio::net::asio_ns;  // NOLINT

template < typename Message_Consumer >
using test_entry_t =
    opio::proto_entry::utest::entry_singlethread_t< Message_Consumer,
                                                    opio::logger::logger_t >;

// NOLINTNEXTLINE
struct test_connection_traits_t : public opio::net::tcp::default_traits_st_t
{
    using logger_t = opio::logger::logger_t;

    using input_ctx_t = opio::net::tcp::input_ctx_t< test_connection_traits_t >;
    using input_handler_t = std::function< void( input_ctx_t & ) >;
};

using test_connection_t = opio::net::tcp::connection_t< test_connection_traits_t >;

/**
 * @brief Runs a give io context for a while.
 */
void run_ioctx_for( asio_ns::io_context & ioctx,
                    std::chrono::steady_clock::duration delay )
{
    asio_ns::steady_timer test_timer{ ioctx.get_executor() };
    test_timer.expires_after( delay );
    test_timer.async_wait(
        [ &ioctx ]( [[maybe_unused]] auto ec ) { ioctx.stop(); } );
    ioctx.run();
    ioctx.restart();
}

class OpioProtoEntryConnectionCreateAndCloseFixture
    : public ::testing::TestWithParam< int >
{
};

// NOLINTNEXTLINE
TEST_P( OpioProtoEntryConnectionCreateAndCloseFixture, NoTraffic )
{
    // Created an entry and immediatelly shut it down.
    const auto started_at = std::chrono::steady_clock::now();

    asio_ns::io_context ioctx( 1 );
    constexpr std::uint16_t port = 40001;
    asio_ns::ip::tcp::endpoint ep{
        asio_ns::ip::address::from_string( "127.0.0.1" ), port
    };

    asio_ns::ip::tcp::socket client_socket{ ioctx };
    StrictMock< message_consumer_mock_t > message_consumer;

    using entry_t = test_entry_t< decltype( message_consumer ) * >;
    typename entry_t::sptr_t entry;

    int shutdown_handler_count = 0;

    auto acceptor = opio::net::tcp::make_acceptor(
        ioctx.get_executor(),
        ep,
        make_test_logger( "acceptor" ),
        // NOLINTNEXTLINE(readability-function-cognitive-complexity)
        [ & ]( auto socket ) {
            // NOLINTNEXTLINE(readability-function-cognitive-complexity)
            entry = entry_t::make( std::move( socket ), [ & ]( auto & params ) {
                params.connection_id( 111 )
                    .logger( make_test_logger( "ENTRY" ) )
                    .message_consumer( &message_consumer )
                    // NOLINTNEXTLINE(readability-function-cognitive-complexity)
                    .shutdown_handler( [ & ]( auto id, auto ctx ) {
                        ++shutdown_handler_count;
                        EXPECT_EQ( id, 111 );

                        if( 0 == GetParam() )
                        {
                            EXPECT_EQ(
                                ctx.reason,
                                entry_shutdown_reason::underlying_connection );
                            ASSERT_TRUE( ctx.underlying_reason );
                            EXPECT_EQ(
                                *ctx.underlying_reason,
                                opio::net::tcp::connection_shutdown_reason::eof );
                        }
                        else
                        {
                            EXPECT_EQ( ctx.reason,
                                       entry_shutdown_reason::user_initiated );
                        }
                    } );
            } );
            EXPECT_EQ( entry->underlying_connection_id(), 111 );

            if( client_socket.is_open() )
            {
                ioctx.stop();
            }
        } );
    acceptor->open();

    ioctx.poll();

    opio::net::tcp::async_connect( ioctx.get_executor(),
                                   "localhost",
                                   port,
                                   make_test_logger( "connector" ),
                                   [ & ]( const auto & ec, auto socket ) {
                                       EXPECT_FALSE( static_cast< bool >( ec ) );
                                       client_socket = std::move( socket );

                                       if( entry )
                                       {
                                           ioctx.stop();
                                       }
                                   } );

    ioctx.run();
    acceptor->close();

    if( 0 == GetParam() )
    {
        client_socket.close();
    }
    else
    {
        entry->close();
    }

    ioctx.restart();
    ioctx.run();

    EXPECT_EQ( 1, shutdown_handler_count );

    EXPECT_GT( adjust_for_msvc_if_necessary( 100 ),
               msec_from_x_to_now( started_at ) );
}

// NOLINTNEXTLINE
TEST_P( OpioProtoEntryConnectionCreateAndCloseFixture,
        HeartbeatAndThenExplicitClose )  // NOLINT
{
    // Created an entry waits heartbeat message
    // and then closes connections (using entry or remote peer).

    asio_ns::io_context ioctx( 1 );
    constexpr std::uint16_t port = 40003;
    asio_ns::ip::tcp::endpoint ep{
        asio_ns::ip::address::from_string( "127.0.0.1" ), port
    };

    asio_ns::ip::tcp::socket client_socket{ ioctx };
    StrictMock< message_consumer_mock_t > message_consumer;

    using entry_t = test_entry_t< decltype( message_consumer ) * >;
    typename entry_t::sptr_t entry;

    auto acceptor = opio::net::tcp::make_acceptor(
        ioctx.get_executor(),
        ep,
        make_test_logger( "acceptor" ),
        [ & ]( auto socket ) {
            entry = entry_t::make( std::move( socket ), [ & ]( auto & params ) {
                entry_cfg_t cfg{};
                cfg.heartbeat.initiate_heartbeat_timeout =
                    std::chrono::milliseconds( 50 );
                cfg.heartbeat.await_heartbeat_reply_timeout =
                    std::chrono::milliseconds( 100 );

                params.entry_config( cfg )
                    .logger( make_test_logger( "ENTRY" ) )
                    .message_consumer( &message_consumer );
            } );

            if( client_socket.is_open() )
            {
                ioctx.stop();
            }
        } );
    acceptor->open();

    ioctx.poll();

    opio::net::tcp::async_connect( ioctx.get_executor(),
                                   "localhost",
                                   port,
                                   make_test_logger( "connector" ),
                                   [ & ]( const auto & ec, auto socket ) {
                                       EXPECT_FALSE( static_cast< bool >( ec ) );
                                       client_socket = std::move( socket );
                                       if( entry )
                                       {
                                           ioctx.stop();
                                       }
                                   } );

    ioctx.run();
    acceptor->close();

    const auto started_at = std::chrono::steady_clock::now();

    ioctx.restart();
    run_ioctx_for( ioctx, std::chrono::milliseconds( 55 ) );

    // Heartbeat Must happen!

    std::array< char, 16 > buf{};

    const auto n = client_socket.read_some(
        asio_ns::mutable_buffer{ buf.data(), buf.size() } );

    const auto h = pkg_header_t::make( pkg_content_heartbeat_request );

    ASSERT_EQ( h.advertized_header_size(), n );

    ASSERT_EQ( 0, std::memcmp( buf.data(), &h, h.advertized_header_size() ) );

    if( 0 == GetParam() )
    {
        client_socket.close();
    }
    else
    {
        entry->close();
    }

    ioctx.run();

    EXPECT_GT( adjust_for_msvc_if_necessary( 75 ),
               msec_from_x_to_now( started_at ) );
}

// NOLINTNEXTLINE
INSTANTIATE_TEST_CASE_P( OpioProtoEntryConnectionCreateAndClose,
                         OpioProtoEntryConnectionCreateAndCloseFixture,
                         ::testing::Values( 0, 1 ) );

class OpioProtoEntryConnectionHBFixture : public ::testing::TestWithParam< int >
{
};

INSTANTIATE_TEST_CASE_P( OpioProtoEntryConnectionHB,
                         OpioProtoEntryConnectionHBFixture,
                         ::testing::Values( 0 ) );

// NOLINTNEXTLINE
TEST_P( OpioProtoEntryConnectionHBFixture, NoResponseOnHeartbeat )
{
    // Created an entry and immediatelly shuts it down.
    const auto started_at = std::chrono::steady_clock::now();

    asio_ns::io_context ioctx( 1 );
    asio_ns::ip::tcp::endpoint ep{
        asio_ns::ip::address::from_string( "127.0.0.1" ), 40001
    };

    asio_ns::ip::tcp::socket client_socket{ ioctx };
    StrictMock< message_consumer_mock_t > message_consumer;

    using entry_t = test_entry_t< decltype( message_consumer ) * >;
    typename entry_t::sptr_t entry;

    int shutdown_handler_count = 0;

    auto acceptor = opio::net::tcp::make_acceptor(
        ioctx.get_executor(),
        ep,
        make_test_logger( "acceptor" ),
        [ & ]( auto socket ) {
            entry = entry_t::make( std::move( socket ), [ & ]( auto & params ) {
                entry_cfg_t cfg{};
                cfg.heartbeat =
                    heartbeat_params_t{ std::chrono::milliseconds( 50 ),
                                        std::chrono::milliseconds( 200 ) };

                params.entry_config( cfg )
                    .logger( make_test_logger( "ENTRY" ) )
                    .message_consumer( &message_consumer )
                    .shutdown_handler( [ & ]( [[maybe_unused]] auto id,
                                              auto ctx ) {
                        ++shutdown_handler_count;
                        EXPECT_EQ( ctx.reason,
                                   entry_shutdown_reason::hearbeat_reply_timeout );
                    } );
            } );

            if( client_socket.is_open() )
            {
                ioctx.stop();
            }
        } );
    acceptor->open();

    ioctx.poll();

    opio::net::tcp::async_connect( ioctx.get_executor(),
                                   "localhost",
                                   40001,
                                   make_test_logger( "connector" ),
                                   [ & ]( const auto & ec, auto socket ) {
                                       EXPECT_FALSE( static_cast< bool >( ec ) );
                                       client_socket = std::move( socket );
                                       if( entry )
                                       {
                                           ioctx.stop();
                                       }
                                   } );

    ioctx.run();
    acceptor->close();

    ioctx.restart();
    run_ioctx_for( ioctx, std::chrono::milliseconds( 55 ) );

    // Heartbeat Must happen!

    std::array< char, 64 > buf{};

    auto n = client_socket.read_some(
        asio_ns::mutable_buffer{ buf.data(), buf.size() } );

    ASSERT_EQ( sizeof( pkg_header_t ), n );

    run_ioctx_for( ioctx, std::chrono::milliseconds( 250 ) );

    // Disconnect must happen:
    EXPECT_EQ( 1, shutdown_handler_count );

    asio_ns::error_code ec;
    n = client_socket.read_some( asio_ns::mutable_buffer{ buf.data(), buf.size() },
                                 ec );
    if( 0 == GetParam() )
    {
        EXPECT_TRUE( static_cast< bool >( ec ) );
        ASSERT_EQ( 0, n );
    }

    ioctx.run();

    EXPECT_GT( adjust_for_msvc_if_necessary( 400 ),
               msec_from_x_to_now( started_at ) );
}

// NOLINTNEXTLINE
TEST_P( OpioProtoEntryConnectionHBFixture, HasResponseOnHeartbeat )
{
    const auto started_at = std::chrono::steady_clock::now();

    asio_ns::io_context ioctx( 1 );
    asio_ns::ip::tcp::endpoint ep{
        asio_ns::ip::address::from_string( "127.0.0.1" ), 40001
    };

    asio_ns::ip::tcp::socket client_socket{ ioctx };
    StrictMock< message_consumer_mock_t > message_consumer;

    using entry_t = test_entry_t< decltype( message_consumer ) * >;
    typename entry_t::sptr_t entry;

    int shutdown_handler_count = 0;

    auto acceptor = opio::net::tcp::make_acceptor(
        ioctx.get_executor(),
        ep,
        make_test_logger( "acceptor" ),
        [ & ]( auto socket ) {
            entry = entry_t::make( std::move( socket ), [ & ]( auto & params ) {
                entry_cfg_t cfg{};
                cfg.heartbeat.initiate_heartbeat_timeout =
                    std::chrono::milliseconds( 50 );
                cfg.heartbeat.await_heartbeat_reply_timeout =
                    std::chrono::milliseconds( 150 );

                params.entry_config( cfg )
                    .logger( make_test_logger( "ENTRY" ) )
                    .message_consumer( &message_consumer )
                    .shutdown_handler( [ & ]( [[maybe_unused]] auto id ) {
                        ++shutdown_handler_count;
                    } );
            } );

            if( client_socket.is_open() )
            {
                ioctx.stop();
            }
        } );
    acceptor->open();

    ioctx.poll();

    opio::net::tcp::async_connect( ioctx.get_executor(),
                                   "localhost",
                                   40001,
                                   make_test_logger( "connector" ),
                                   [ & ]( const auto & ec, auto socket ) {
                                       EXPECT_FALSE( static_cast< bool >( ec ) );
                                       client_socket = std::move( socket );
                                       if( entry )
                                       {
                                           ioctx.stop();
                                       }
                                   } );

    ioctx.run();
    acceptor->close();

    ioctx.restart();
    run_ioctx_for( ioctx, std::chrono::milliseconds( 51 ) );

    // Heartbeat Must happen!

    std::array< char, 16 > buf{};

    auto n = client_socket.read_some(
        asio_ns::mutable_buffer{ buf.data(), buf.size() } );

    EXPECT_GE( adjust_for_msvc_if_necessary( 60 ),
               msec_from_x_to_now( started_at ) );

    ASSERT_EQ( sizeof( pkg_header_t ), n );

    auto h = pkg_header_t::make( pkg_content_heartbeat_reply );

    client_socket.send( asio_ns::const_buffer{ &h, sizeof( h ) } );

    EXPECT_GE( adjust_for_msvc_if_necessary( 60 ),
               msec_from_x_to_now( started_at ) );

    run_ioctx_for( ioctx, std::chrono::milliseconds( 50 ) );

    EXPECT_GT( adjust_for_msvc_if_necessary( 111 ),
               msec_from_x_to_now( started_at ) );

    EXPECT_EQ( 0, shutdown_handler_count );

    client_socket.close();

    EXPECT_GT( adjust_for_msvc_if_necessary( 112 ),
               msec_from_x_to_now( started_at ) );

    ioctx.run();

    EXPECT_GT( adjust_for_msvc_if_necessary( 115 ),
               msec_from_x_to_now( started_at ) );
}

// NOLINTNEXTLINE
TEST( OpioProtoEntryConnection, HandleHeartbeatRequest )
{
    const auto started_at = std::chrono::steady_clock::now();

    asio_ns::io_context ioctx( 1 );
    asio_ns::ip::tcp::endpoint ep{
        asio_ns::ip::address::from_string( "127.0.0.1" ), 40001
    };

    asio_ns::ip::tcp::socket client_socket{ ioctx };
    StrictMock< message_consumer_mock_t > message_consumer;

    using entry_t = test_entry_t< decltype( message_consumer ) * >;
    typename entry_t::sptr_t entry;

    int shutdown_handler_count = 0;

    auto acceptor = opio::net::tcp::make_acceptor(
        ioctx.get_executor(),
        ep,
        make_test_logger( "acceptor" ),
        [ & ]( auto socket ) {
            entry = entry_t::make( std::move( socket ), [ & ]( auto & params ) {
                params.logger( make_test_logger( "ENTRY" ) )
                    .message_consumer( &message_consumer )
                    .shutdown_handler( [ & ]( [[maybe_unused]] auto id ) {
                        ++shutdown_handler_count;
                    } );
            } );

            if( client_socket.is_open() )
            {
                ioctx.stop();
            }
        } );
    acceptor->open();

    ioctx.poll();

    opio::net::tcp::async_connect( ioctx.get_executor(),
                                   "localhost",
                                   40001,
                                   make_test_logger( "connector" ),
                                   [ & ]( const auto & ec, auto socket ) {
                                       EXPECT_FALSE( static_cast< bool >( ec ) );
                                       client_socket = std::move( socket );
                                       if( entry )
                                       {
                                           ioctx.stop();
                                       }
                                   } );

    ioctx.run();
    acceptor->close();

    ioctx.restart();
    run_ioctx_for( ioctx, std::chrono::milliseconds( 5 ) );

    auto h = pkg_header_t::make( pkg_content_heartbeat_request );
    client_socket.send( asio_ns::const_buffer{ &h, sizeof( h ) } );

    run_ioctx_for( ioctx, std::chrono::milliseconds( 5 ) );
    EXPECT_EQ( 0, shutdown_handler_count );

    std::array< char, 32 > buf{};

    auto n = client_socket.read_some(
        asio_ns::mutable_buffer{ buf.data(), buf.size() } );

    ASSERT_EQ( sizeof( h ), n );

    memcpy( &h, buf.data(), sizeof( h ) );

    EXPECT_EQ( sizeof( h ), h.advertized_header_size() );
    EXPECT_EQ( pkg_content_heartbeat_reply, h.pkg_content_type );
    EXPECT_EQ( 0, h.content_specific_value );
    EXPECT_EQ( 0, h.content_size );
    EXPECT_EQ( 0, h.attached_binary_size );

    client_socket.close();

    ioctx.run();

    EXPECT_GT( adjust_for_msvc_if_necessary( 50 ),
               msec_from_x_to_now( started_at ) );
}

// NOLINTNEXTLINE
TEST( OpioProtoEntryExtendedHeader, IgnoreExtensionPart )
{
    asio_ns::io_context ioctx( 1 );
    asio_ns::ip::tcp::endpoint ep{
        asio_ns::ip::address::from_string( "127.0.0.1" ), 40001
    };

    asio_ns::ip::tcp::socket client_socket{ ioctx };
    StrictMock< message_consumer_mock_t > message_consumer;

    using entry_t = test_entry_t< decltype( message_consumer ) * >;
    typename entry_t::sptr_t entry;

    int shutdown_handler_count = 0;

    auto acceptor = opio::net::tcp::make_acceptor(
        ioctx.get_executor(),
        ep,
        make_test_logger( "acceptor" ),
        [ & ]( auto socket ) {
            entry = entry_t::make( std::move( socket ), [ & ]( auto & params ) {
                params.logger( make_test_logger( "ENTRY" ) )
                    .message_consumer( &message_consumer )
                    .shutdown_handler( [ & ]( [[maybe_unused]] auto id ) {
                        ++shutdown_handler_count;
                    } );
            } );

            if( client_socket.is_open() )
            {
                ioctx.stop();
            }
        } );
    acceptor->open();

    ioctx.poll();

    opio::net::tcp::async_connect( ioctx.get_executor(),
                                   "localhost",
                                   40001,
                                   make_test_logger( "connector" ),
                                   [ & ]( const auto & ec, auto socket ) {
                                       EXPECT_FALSE( static_cast< bool >( ec ) );
                                       client_socket = std::move( socket );
                                       if( entry )
                                       {
                                           ioctx.stop();
                                       }
                                   } );

    ioctx.run();
    acceptor->close();

    const auto started_at = std::chrono::steady_clock::now();
    ioctx.restart();
    run_ioctx_for( ioctx, std::chrono::milliseconds( 5 ) );

    EXPECT_CALL( message_consumer, on_message( An< utest::BothWayMessage >() ) )
        // NOLINTNEXTLINE(readability-function-cognitive-complexity)
        .WillOnce( Invoke( [ & ]( auto received_msg ) {
            EXPECT_EQ( received_msg.some_string(), "Hello Unit tests!" );
        } ) );

    {
        utest::BothWayMessage message{};
        message.set_some_string( "Hello Unit tests!" );

        opio::net::simple_buffer_driver_t buffer_driver;

        auto images = make_separate_package_image(
            static_cast< std::uint16_t >( utest::BOTH_WAY ),
            message,
            buffer_driver,
            0ULL,
            32 );

        ASSERT_EQ( images.first.size(), sizeof( pkg_header_t ) + 32 );

        client_socket.send( images.first.make_asio_const_buffer() );
        client_socket.send( images.second.make_asio_const_buffer() );

        run_ioctx_for( ioctx, std::chrono::milliseconds( 5 ) );

        EXPECT_EQ( 0, shutdown_handler_count ) << "Should continue to operate";
    }

    run_ioctx_for( ioctx, std::chrono::milliseconds( 5 ) );

    EXPECT_EQ( 0, shutdown_handler_count ) << "Should continue to operate";

    client_socket.close();

    ioctx.run();

    EXPECT_GT( adjust_for_msvc_if_necessary( 50 ),
               msec_from_x_to_now( started_at ) );
}

class OpioProtoEntryBadPackagesFixture : public ::testing::TestWithParam< int >
{
};

// NOLINTNEXTLINE
TEST_P( OpioProtoEntryBadPackagesFixture, BadPackageAndDisconnect )
{
    asio_ns::io_context ioctx( 1 );
    asio_ns::ip::tcp::endpoint ep{
        asio_ns::ip::address::from_string( "127.0.0.1" ), 40001
    };

    asio_ns::ip::tcp::socket client_socket{ ioctx };
    StrictMock< message_consumer_mock_t > message_consumer;
    Sequence utest_calls_seq;

    using entry_t = test_entry_t< decltype( message_consumer ) * >;
    typename entry_t::sptr_t entry;

    int shutdown_handler_count = 0;

    auto acceptor = opio::net::tcp::make_acceptor(
        ioctx.get_executor(),
        ep,
        make_test_logger( "acceptor" ),
        [ & ]( auto socket ) {
            entry = entry_t::make( std::move( socket ), [ & ]( auto & params ) {
                entry_cfg_t cfg{};
                cfg.max_valid_package_size = 256;

                params.entry_config( cfg )
                    .logger( make_test_logger( "ENTRY" ) )
                    .message_consumer( &message_consumer )
                    .shutdown_handler( [ & ]( [[maybe_unused]] auto id ) {
                        ++shutdown_handler_count;
                    } );
            } );

            if( client_socket.is_open() )
            {
                ioctx.stop();
            }
        } );
    acceptor->open();

    ioctx.poll();

    opio::net::tcp::async_connect( ioctx.get_executor(),
                                   "localhost",
                                   40001,
                                   make_test_logger( "connector" ),
                                   [ & ]( const auto & ec, auto socket ) {
                                       EXPECT_FALSE( static_cast< bool >( ec ) );
                                       client_socket = std::move( socket );
                                       if( entry )
                                       {
                                           ioctx.stop();
                                       }
                                   } );

    ioctx.run();
    acceptor->close();

    ioctx.restart();

    const auto started_at = std::chrono::steady_clock::now();
    run_ioctx_for( ioctx, std::chrono::milliseconds( 5 ) );

    if( 0 == GetParam() )
    {
        // Unknown package_content.
        auto h = pkg_header_t::make( 0xF0 );
        client_socket.send( asio_ns::const_buffer{ &h, sizeof( h ) } );
    }
    else if( 1 == GetParam() )
    {
        // Another unknown package_content.
        auto h = pkg_header_t::make( 42 );
        client_socket.send( asio_ns::const_buffer{ &h, sizeof( h ) } );
    }
    else if( 2 == GetParam() )
    {
        // HUUUge size.
        auto h = pkg_header_t::make(
            opio::proto_entry::pkg_content_message, 0, 0xFFFFFFFFUL );
        client_socket.send( asio_ns::const_buffer{ &h, sizeof( h ) } );
    }
    else if( 3 == GetParam() )
    {
        // Size just above the limit.
        auto h =
            pkg_header_t::make( opio::proto_entry::pkg_content_message, 0, 257 );
        client_socket.send( asio_ns::const_buffer{ &h, sizeof( h ) } );
    }
    else if( 4 == GetParam() )
    {
        // Heartbeat requeset with not zero size.
        auto h = pkg_header_t::make(
            opio::proto_entry::pkg_content_heartbeat_request, 0, 123 );
        client_socket.send( asio_ns::const_buffer{ &h, sizeof( h ) } );
    }
    else if( 5 == GetParam() )
    {
        // Heartbeat reply with not zero size.
        auto h = pkg_header_t::make(
            opio::proto_entry::pkg_content_heartbeat_reply, 0, 123 );
        client_socket.send( asio_ns::const_buffer{ &h, sizeof( h ) } );
    }
    else if( 6 == GetParam() )
    {
        // Heartbeat requeset with not zero size.
        auto h = pkg_header_t::make(
            opio::proto_entry::pkg_content_heartbeat_request, 0, 0, 99 );
        client_socket.send( asio_ns::const_buffer{ &h, sizeof( h ) } );
    }
    else if( 7 == GetParam() )
    {
        // Heartbeat reply with not zero size.
        auto h = pkg_header_t::make(
            opio::proto_entry::pkg_content_heartbeat_reply, 0, 0, 99 );
        client_socket.send( asio_ns::const_buffer{ &h, sizeof( h ) } );
    }
    else if( 8 == GetParam() )
    {
        // Package with bad payload:
        const std::uint64_t payload = 0x8FFFFFFF'7777777FUL;
        auto h                      = pkg_header_t::make(
            opio::proto_entry::pkg_content_message, 0, sizeof( payload ) );

        client_socket.send( asio_ns::const_buffer{ &h, sizeof( h ) } );
        client_socket.send( asio_ns::const_buffer{ &payload, sizeof( payload ) } );
    }
    else if( 9 == GetParam() )
    {
        // Package is fine (message is fine), but advertised
        // content size is greater then a protobuf image,
        // so a few bytes will be not consumed from protobuf stream.

        utest::YyyRequest msg;
        msg.set_req_id( 1040 );

        // pkg.add_type(  );

        opio::net::simple_buffer_t pkg_buf;
        pkg_buf.resize( sizeof( pkg_header_t ) + msg.ByteSizeLong() + 10 );

        auto h = pkg_header_t::make(
            opio::proto_entry::pkg_content_message,
            static_cast< std::uint16_t >( utest::MessageType::YYY_REQUEST ),
            static_cast< std::uint32_t >( msg.ByteSizeLong() ) + 1 );

        std::memcpy( pkg_buf.data(), &h, sizeof( h ) );
        msg.SerializeToArray( &pkg_buf.data()[ sizeof( h ) ],
                              static_cast< int >( msg.ByteSizeLong() ) );

        client_socket.send(
            asio_ns::const_buffer{ pkg_buf.data(), pkg_buf.size() } );
    }
    else
    {
        ASSERT_TRUE( false ) << "Unknown test case...";
    }

    run_ioctx_for( ioctx, std::chrono::milliseconds( 5 ) );

    // Disconnect must happen!

    EXPECT_EQ( 1, shutdown_handler_count );

    {
        std::array< char, 16 > buf{};

        asio_ns::error_code ec;
        auto n = client_socket.read_some(
            asio_ns::mutable_buffer{ buf.data(), buf.size() }, ec );

        EXPECT_TRUE( static_cast< bool >( ec ) );
        ASSERT_EQ( 0, n );
    }

    client_socket.close();

    ioctx.run();

    EXPECT_GT( adjust_for_msvc_if_necessary( 50 ),
               msec_from_x_to_now( started_at ) );
}

// NOLINTNEXTLINE
INSTANTIATE_TEST_CASE_P( OpioProtoEntryBadPackages,
                         OpioProtoEntryBadPackagesFixture,
                         ::testing::Values( 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ) );

class OpioProtoEntryMessageTrafficLocalToRemoteFixture
    : public ::testing::TestWithParam< utest::MessageType >
{
};

TEST_P( OpioProtoEntryMessageTrafficLocalToRemoteFixture, Message )  // NOLINT
{
    const auto started_at = std::chrono::steady_clock::now();

    asio_ns::io_context ioctx( 1 );
    asio_ns::ip::tcp::endpoint ep{
        asio_ns::ip::address::from_string( "127.0.0.1" ), 40001
    };

    asio_ns::ip::tcp::socket client_socket{ ioctx };
    StrictMock< message_consumer_mock_t > message_consumer;
    Sequence utest_calls_seq;

    using entry_t = test_entry_t< decltype( message_consumer ) * >;
    typename entry_t::sptr_t entry;

    int shutdown_handler_count = 0;

    auto acceptor = opio::net::tcp::make_acceptor(
        ioctx.get_executor(),
        ep,
        make_test_logger( "acceptor" ),
        [ & ]( auto socket ) {
            entry = entry_t::make( std::move( socket ), [ & ]( auto & params ) {
                params.logger( make_test_logger( "ENTRY" ) )
                    .message_consumer( &message_consumer )
                    .shutdown_handler( [ & ]( [[maybe_unused]] auto id ) {
                        ++shutdown_handler_count;
                    } );
            } );

            if( client_socket.is_open() )
            {
                ioctx.stop();
            }
        } );
    acceptor->open();

    ioctx.poll();

    opio::net::tcp::async_connect( ioctx.get_executor(),
                                   "localhost",
                                   40001,
                                   make_test_logger( "connector" ),
                                   [ & ]( const auto & ec, auto socket ) {
                                       EXPECT_FALSE( static_cast< bool >( ec ) );
                                       client_socket = std::move( socket );
                                       if( entry )
                                       {
                                           ioctx.stop();
                                       }
                                   } );

    ioctx.run();
    ioctx.restart();

    acceptor->close();
    run_ioctx_for( ioctx, std::chrono::milliseconds( 10 ) );

    utest::YyyReply msg1;
    utest::XxxReply msg2;
    std::uint32_t msg_image_size{};
    // =========================================================================
    if( utest::MessageType::YYY_REPLY == GetParam() )
    {
        msg1.set_req_id( 1040 );
        msg1.add_strings( "qwerty" );
        msg1.add_strings( "123456" );
        msg1.add_strings( "sample protobuf message" );
        msg1.add_strings( "for unittest" );
        msg1.set_error( "ErrorData" );

        // ACTUAL SEND:
        entry->send( msg1 );
        msg_image_size = msg1.ByteSizeLong();
    }
    else if( utest::MessageType::XXX_REPLY == GetParam() )
    {
        msg2.set_req_id( 12345 );
        msg2.set_error( "qwerty123456 sample protobuf message for unittest...." );

        // ACTUAL SEND:
        entry->send( msg2 );
        msg_image_size = msg2.ByteSizeLong();
    }
    else
    {
        ASSERT_TRUE( false ) << "Unknown test case...";
    }
    // =========================================================================

    run_ioctx_for( ioctx, std::chrono::milliseconds( 5 ) );

    pkg_header_t h{};
    auto n = client_socket.read_some( asio_ns::mutable_buffer{ &h, sizeof( h ) } );

    ASSERT_EQ( sizeof( h ), n );
    ASSERT_EQ( pkg_content_message, h.pkg_content_type );
    ASSERT_EQ( msg_image_size, h.content_size );
    ASSERT_EQ( 0UL, h.attached_binary_size );

    utest::YyyReply received_msg1;
    utest::XxxReply received_msg2;

    opio::net::simple_buffer_t buf{ h.content_size };

    n = client_socket.read_some(
        asio_ns::mutable_buffer{ buf.data(), buf.size() } );

    ASSERT_EQ( buf.size(), n );

    if( utest::MessageType::YYY_REPLY == GetParam() )
    {
        ASSERT_TRUE( received_msg1.ParseFromArray( buf.data(), buf.size() ) );
    }
    else
    {
        ASSERT_TRUE( received_msg2.ParseFromArray( buf.data(), buf.size() ) );
    }

    // =========================================================================
    if( utest::MessageType::YYY_REPLY == GetParam() )
    {
        EXPECT_EQ( received_msg1.req_id(), msg1.req_id() );
        ASSERT_FALSE( received_msg1.error().empty() );
        EXPECT_EQ( received_msg1.error(), msg1.error() );
        ASSERT_EQ( received_msg1.strings_size(), msg1.strings_size() );
        ASSERT_EQ( received_msg1.strings_size(), 4 );
        EXPECT_EQ( received_msg1.strings( 0 ), msg1.strings( 0 ) );
        EXPECT_EQ( received_msg1.strings( 1 ), msg1.strings( 1 ) );
        EXPECT_EQ( received_msg1.strings( 2 ), msg1.strings( 2 ) );
        EXPECT_EQ( received_msg1.strings( 3 ), msg1.strings( 3 ) );
    }
    else if( utest::MessageType::XXX_REPLY == GetParam() )
    {
        EXPECT_EQ( received_msg2.req_id(), msg2.req_id() );
        EXPECT_EQ( received_msg2.error(), msg2.error() );
    }
    // =========================================================================

    EXPECT_GT( adjust_for_msvc_if_necessary( 50 ),
               msec_from_x_to_now( started_at ) );
}

// NOLINTNEXTLINE
INSTANTIATE_TEST_CASE_P( OpioProtoEntryMessageTrafficLocalToRemote,
                         OpioProtoEntryMessageTrafficLocalToRemoteFixture,
                         ::testing::Values( utest::MessageType::YYY_REPLY,
                                            utest::MessageType::XXX_REPLY ) );

class OpioProtoEntryMessageTrafficRemoteToLocalFixture
    : public ::testing::TestWithParam<
          std::pair< utest::MessageType, std::string > >
{
};

TEST_P( OpioProtoEntryMessageTrafficRemoteToLocalFixture, Message )  // NOLINT
{
    utest::MessageType test_type = GetParam().first;
    bool is_delimeted_sends      = GetParam().second == "BufDelimeted";

    asio_ns::io_context ioctx( 1 );
    asio_ns::ip::tcp::endpoint ep{
        asio_ns::ip::address::from_string( "127.0.0.1" ), 40001
    };

    asio_ns::ip::tcp::socket client_socket{ ioctx };
    StrictMock< message_consumer_mock_t > message_consumer;
    Sequence utest_calls_seq;

    using entry_t = test_entry_t< decltype( message_consumer ) * >;
    typename entry_t::sptr_t entry;

    int shutdown_handler_count = 0;

    auto acceptor = opio::net::tcp::make_acceptor(
        ioctx.get_executor(),
        ep,
        make_test_logger( "acceptor" ),
        [ & ]( auto socket ) {
            entry = entry_t::make( std::move( socket ), [ & ]( auto & params ) {
                params.logger( make_test_logger( "ENTRY" ) )
                    .message_consumer( &message_consumer )
                    .shutdown_handler( [ & ]( [[maybe_unused]] auto id ) {
                        ++shutdown_handler_count;
                    } );
            } );

            if( client_socket.is_open() )
            {
                ioctx.stop();
            }
        } );
    acceptor->open();

    ioctx.poll();

    opio::net::tcp::socket_options_cfg_t socket_options_cfg{};

    if( is_delimeted_sends )
    {
        socket_options_cfg.no_delay = true;
    }

    opio::net::tcp::async_connect( ioctx.get_executor(),
                                   "localhost",
                                   40001,
                                   socket_options_cfg,
                                   make_test_logger( "connector" ),
                                   [ & ]( const auto & ec, auto socket ) {
                                       EXPECT_FALSE( static_cast< bool >( ec ) );
                                       client_socket = std::move( socket );
                                       if( entry )
                                       {
                                           ioctx.stop();
                                       }
                                   } );

    ioctx.run();
    ioctx.restart();

    acceptor->close();

    const auto started_at = std::chrono::steady_clock::now();
    run_ioctx_for( ioctx, std::chrono::milliseconds( 5 ) );

    std::vector< opio::net::simple_buffer_t > packages;

    std::uint32_t id_counter = 2022;

    // NOLINTNEXTLINE(readability-function-cognitive-complexity)
    auto make_XxxRequest = [ & ]() {
        utest::XxxRequest msg;
        msg.set_req_id( id_counter++ );
        msg.set_aaa( 911 + id_counter );
        msg.set_bbb( 123456 + id_counter );
        msg.set_ccc( 0xFFFFFFFFUL - id_counter );

        msg.add_strings( "for unittest" );
        msg.add_strings( std::to_string( id_counter ) );

        EXPECT_CALL( message_consumer, on_message( An< utest::XxxRequest >() ) )
            .InSequence( utest_calls_seq )
            // NOLINTNEXTLINE(readability-function-cognitive-complexity)
            .WillOnce( Invoke( [ &, msg = msg ]( auto received_msg ) {
                EXPECT_EQ( msg.req_id(), received_msg.req_id() );
                EXPECT_EQ( msg.aaa(), received_msg.aaa() );
                EXPECT_EQ( msg.bbb(), received_msg.bbb() );
                EXPECT_EQ( msg.ccc(), received_msg.ccc() );

                ASSERT_EQ( msg.strings_size(), received_msg.strings_size() );
                ASSERT_EQ( msg.strings_size(), 2 );

                EXPECT_EQ( msg.strings( 0 ), received_msg.strings( 0 ) );
                EXPECT_EQ( msg.strings( 1 ), received_msg.strings( 1 ) );
            } ) );

        packages.push_back( utest::make_package_image( msg ) );
    };

    auto make_YyyRequest = [ & ]() {
        utest::YyyRequest msg;
        msg.set_req_id( id_counter++ );

        EXPECT_CALL( message_consumer, on_message( An< utest::YyyRequest >() ) )
            .InSequence( utest_calls_seq )
            .WillOnce( Invoke( [ &, msg = msg ]( auto received_msg ) {
                EXPECT_EQ( msg.req_id(), received_msg.req_id() );
            } ) );

        packages.push_back( utest::make_package_image( msg ) );
    };

    // NOLINTNEXTLINE(readability-function-cognitive-complexity)
    auto make_ZzzRequest = [ & ]() {
        utest::ZzzRequest msg;
        msg.set_req_id( id_counter++ );
        msg.add_numbers( ~id_counter );
        msg.add_numbers( 12345 );
        msg.add_numbers( id_counter );

        msg.add_dnumbers( 3.14 );
        msg.add_dnumbers( 2.71828 );

        auto attached_bin = ::opio::net::simple_buffer_t::make_from(
            { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' } );

        EXPECT_CALL(
            message_consumer,
            on_message_with_attached_bin( An< utest::ZzzRequest >(),
                                          An< ::opio::net::simple_buffer_t >() ) )
            .InSequence( utest_calls_seq )
            // NOLINTNEXTLINE(readability-function-cognitive-complexity)
            .WillOnce( Invoke( [ &, msg = msg ]( auto received_msg, auto sb ) {
                EXPECT_EQ( msg.req_id(), received_msg.req_id() );

                ASSERT_EQ( msg.numbers_size(), received_msg.numbers_size() );
                ASSERT_EQ( 3, received_msg.numbers_size() );
                EXPECT_EQ( msg.numbers( 0 ), received_msg.numbers( 0 ) );
                EXPECT_EQ( msg.numbers( 1 ), received_msg.numbers( 1 ) );
                EXPECT_EQ( msg.numbers( 2 ), received_msg.numbers( 2 ) );

                ASSERT_EQ( msg.dnumbers_size(), received_msg.dnumbers_size() );
                ASSERT_EQ( 2, received_msg.dnumbers_size() );
                EXPECT_DOUBLE_EQ( msg.dnumbers( 0 ), received_msg.dnumbers( 0 ) );
                EXPECT_DOUBLE_EQ( msg.dnumbers( 1 ), received_msg.dnumbers( 1 ) );

                EXPECT_EQ( sb.size(), 10 );
                EXPECT_EQ( sb.make_string_view(), "0123456789" );
            } ) );

        packages.push_back(
            utest::make_package_image( msg, attached_bin.size() ) );
        packages.push_back( std::move( attached_bin ) );
    };

    // NOLINTNEXTLINE(readability-function-cognitive-complexity)
    auto make_BothWayMessage = [ & ]() {
        utest::BothWayMessage msg;
        msg.set_some_string( fmt::format( "{:064b}", id_counter++ ) );

        EXPECT_CALL( message_consumer,
                     on_message( An< utest::BothWayMessage >() ) )
            .InSequence( utest_calls_seq )
            .WillOnce( Invoke( [ &, msg = msg ]( auto received_msg ) {
                EXPECT_EQ( msg.some_string(), received_msg.some_string() );
            } ) );

        packages.push_back( utest::make_package_image( msg ) );
    };

    // =========================================================================
    if( utest::MessageType::XXX_REQUEST == test_type )
    {
        make_XxxRequest();
    }
    else if( utest::MessageType::YYY_REQUEST == test_type )
    {
        make_YyyRequest();
        make_XxxRequest();
        make_XxxRequest();
        make_YyyRequest();
        make_YyyRequest();
    }
    else if( utest::MessageType::ZZZ_REQUEST == test_type )
    {
        make_ZzzRequest();
        make_YyyRequest();
        make_XxxRequest();

        make_ZzzRequest();
        make_XxxRequest();
    }
    else if( utest::MessageType::BOTH_WAY == test_type )
    {
        make_BothWayMessage();
        make_XxxRequest();
        make_BothWayMessage();
        make_BothWayMessage();
        make_YyyRequest();
        make_BothWayMessage();
        make_BothWayMessage();
        make_BothWayMessage();
        make_ZzzRequest();
        make_BothWayMessage();
        make_BothWayMessage();
        make_BothWayMessage();
        make_BothWayMessage();

        make_BothWayMessage();
        make_BothWayMessage();

        make_BothWayMessage();
        make_BothWayMessage();
        make_BothWayMessage();

        make_BothWayMessage();
        make_BothWayMessage();
        make_BothWayMessage();
        make_BothWayMessage();
        make_BothWayMessage();
    }
    else
    {
        ASSERT_TRUE( false ) << "Unknown test case...";
    }
    // =========================================================================

    run_ioctx_for( ioctx, std::chrono::milliseconds( 1 ) );

    for( auto & pkg_buf : packages )
    {
        if( is_delimeted_sends )
        {
            std::for_each( begin( pkg_buf ), end( pkg_buf ), [ & ]( auto b ) {
                // Send by 1 byte.
                client_socket.send( asio_ns::const_buffer{ &b, 1 } );
                run_ioctx_for( ioctx, std::chrono::milliseconds( 1 ) );
            } );
        }
        else
        {
            // Send the whole package.
            client_socket.send( pkg_buf.make_asio_const_buffer() );

            run_ioctx_for( ioctx, std::chrono::milliseconds( 1 ) );
        }
    }

    entry->close();
    ioctx.run();

    const auto n =
#if defined( OPIO_ASIO_WINDOWS )
        is_delimeted_sends ? 10'000 : 300
#else   // defined( OPIO_ASIO_WINDOWS )
        is_delimeted_sends ? 1500 : 100
#endif  // !defined(OPIO_ASIO_WINDOWS)
        ;
    EXPECT_GT( n, msec_from_x_to_now( started_at ) );
}

// NOLINTNEXTLINE
INSTANTIATE_TEST_SUITE_P(
    OpioProtoEntryMessageTrafficRemoteToLocal,
    OpioProtoEntryMessageTrafficRemoteToLocalFixture,
    ::testing::Values(
        std::make_pair( utest::MessageType::XXX_REQUEST, "XXX_REQUEST_BufNormal" ),
        std::make_pair( utest::MessageType::XXX_REQUEST,
                        "XXX_REQUEST_BufDelimeted" ),
        std::make_pair( utest::MessageType::YYY_REQUEST, "YYY_REQUEST_BufNormal" ),
        std::make_pair( utest::MessageType::YYY_REQUEST,
                        "YYY_REQUEST_BufDelimeted" ),
        std::make_pair( utest::MessageType::ZZZ_REQUEST, "ZZZ_REQUEST_BufNormal" ),
        std::make_pair( utest::MessageType::ZZZ_REQUEST,
                        "ZZZ_REQUEST_BufDelimeted" ),
        std::make_pair( utest::MessageType::BOTH_WAY, "BOTH_WAY_BufNormal" ),
        std::make_pair( utest::MessageType::BOTH_WAY, "BOTH_WAY_BufDelimeted" ) ),
    []( const auto & p ) { return p.param.second; } );

TEST( OpioProtoEntryGeneratedCode, EntrySharedFromThis )  // NOLINT
{
    asio_ns::io_context ioctx( 1 );
    asio_ns::ip::tcp::endpoint ep{
        asio_ns::ip::address::from_string( "127.0.0.1" ), 40001
    };

    asio_ns::ip::tcp::socket client_socket{ ioctx };
    StrictMock< message_consumer_mock_t > message_consumer;

    using entry_t = test_entry_t< decltype( message_consumer ) * >;
    typename entry_t::sptr_t entry;

    auto acceptor = opio::net::tcp::make_acceptor(
        ioctx.get_executor(),
        ep,
        make_test_logger( "acceptor" ),
        [ & ]( auto socket ) {
            entry = entry_t::make( std::move( socket ), [ & ]( auto & params ) {
                params.logger( make_test_logger( "ENTRY" ) )
                    .message_consumer( &message_consumer );
            } );

            if( client_socket.is_open() )
            {
                ioctx.stop();
            }
        } );
    acceptor->open();

    ioctx.poll();

    opio::net::tcp::async_connect( ioctx.get_executor(),
                                   "localhost",
                                   40001,
                                   make_test_logger( "connector" ),
                                   [ & ]( const auto & ec, auto socket ) {
                                       EXPECT_FALSE( static_cast< bool >( ec ) );
                                       client_socket = std::move( socket );
                                       if( entry )
                                       {
                                           ioctx.stop();
                                       }
                                   } );

    ioctx.run();
    acceptor->close();

    typename entry_t::sptr_t esptr = entry->shared_from_this();

    ASSERT_TRUE( static_cast< bool >( esptr ) );

    ioctx.restart();
    client_socket.close();

    ioctx.run();
}

TEST( OpioProtoEntry, ScheduleSendRawBufs )  // NOLINT
{
    asio_ns::io_context ioctx( 1 );
    asio_ns::ip::tcp::endpoint ep{
        asio_ns::ip::address::from_string( "127.0.0.1" ), 40001
    };

    asio_ns::ip::tcp::socket client_socket{ ioctx };
    StrictMock< message_consumer_mock_t > message_consumer;

    using entry_t = test_entry_t< decltype( message_consumer ) * >;
    typename entry_t::sptr_t entry;

    auto acceptor = opio::net::tcp::make_acceptor(
        ioctx.get_executor(),
        ep,
        make_test_logger( "acceptor" ),
        [ & ]( auto socket ) {
            entry = entry_t::make( std::move( socket ), [ & ]( auto & params ) {
                params.logger( make_test_logger( "ENTRY" ) )
                    .message_consumer( &message_consumer );
            } );

            if( client_socket.is_open() )
            {
                ioctx.stop();
            }
        } );
    acceptor->open();

    ioctx.poll();

    opio::net::tcp::async_connect( ioctx.get_executor(),
                                   "localhost",
                                   40001,
                                   make_test_logger( "connector" ),
                                   [ & ]( const auto & ec, auto socket ) {
                                       EXPECT_FALSE( static_cast< bool >( ec ) );
                                       client_socket = std::move( socket );
                                       if( entry )
                                       {
                                           ioctx.stop();
                                       }
                                   } );

    ioctx.run();
    acceptor->close();

    auto buf1 =
        opio::net::simple_buffer_t::make_from( { 'H', 'e', 'l', 'l', 'o' } );
    auto buf2 =
        opio::net::simple_buffer_t::make_from( { ' ', ' ', ' ', ' ', ' ' } );
    auto buf3 =
        opio::net::simple_buffer_t::make_from( { 'B', 'u', 'f', 'f', '!' } );

    bool after_write_cb_called = false;
    entry->schedule_send_raw_bufs_with_cb(
        [ & ]( auto res ) {
            after_write_cb_called = true;
            ASSERT_EQ( opio::net::tcp::send_buffers_result::success, res );
        },
        buf1.make_copy(),
        buf2.make_copy(),
        buf3.make_copy() );
    entry->schedule_send_raw_bufs(
        std::move( buf1 ), std::move( buf2 ), std::move( buf3 ) );

    ioctx.restart();
    run_ioctx_for( ioctx, std::chrono::milliseconds( 5 ) );

    std::array< char, 32 > buf{};

    const auto n = client_socket.read_some(
        asio_ns::mutable_buffer{ buf.data(), buf.size() } );

    ASSERT_EQ( 30, n );
    ASSERT_EQ( 0, memcmp( "Hello     Buff!Hello     Buff!", buf.data(), 30 ) );

    client_socket.close();

    ioctx.run();

    EXPECT_TRUE( after_write_cb_called );
}

TEST( OpioProtoEntry, ScheduleSendRawBufsWhenClosed )  // NOLINT
{
    asio_ns::io_context ioctx( 1 );
    asio_ns::ip::tcp::endpoint ep{
        asio_ns::ip::address::from_string( "127.0.0.1" ), 40001
    };

    asio_ns::ip::tcp::socket client_socket{ ioctx };
    StrictMock< message_consumer_mock_t > message_consumer;

    using entry_t = test_entry_t< decltype( message_consumer ) * >;
    typename entry_t::sptr_t entry;

    auto acceptor = opio::net::tcp::make_acceptor(
        ioctx.get_executor(),
        ep,
        make_test_logger( "acceptor" ),
        [ & ]( auto socket ) {
            entry = entry_t::make( std::move( socket ), [ & ]( auto & params ) {
                params.logger( make_test_logger( "ENTRY" ) )
                    .message_consumer( &message_consumer );
            } );

            if( client_socket.is_open() )
            {
                ioctx.stop();
            }
        } );
    acceptor->open();

    ioctx.poll();

    opio::net::tcp::async_connect( ioctx.get_executor(),
                                   "localhost",
                                   40001,
                                   make_test_logger( "connector" ),
                                   [ & ]( const auto & ec, auto socket ) {
                                       EXPECT_FALSE( static_cast< bool >( ec ) );
                                       client_socket = std::move( socket );
                                       if( entry )
                                       {
                                           ioctx.stop();
                                       }
                                   } );

    ioctx.run();
    acceptor->close();

    entry->close();

    ioctx.restart();
    run_ioctx_for( ioctx, std::chrono::milliseconds( 5 ) );

    entry->schedule_send_raw_bufs(
        opio::net::simple_buffer_t::make_from( { 'H', 'e', 'l', 'l', 'o' } ) );

    run_ioctx_for( ioctx, std::chrono::milliseconds( 5 ) );

    std::array< char, 16 > buf{};

    asio_ns::error_code ec;
    client_socket.read_some( asio_ns::mutable_buffer{ buf.data(), buf.size() },
                             ec );

    ASSERT_TRUE( static_cast< bool >( ec ) );

    ioctx.run();
}

//
//  connection_mock_t
//

struct connection_mock_t
{
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    MOCK_METHOD( const opio::net::tcp::connection_cfg_t &, cfg, () );

    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    MOCK_METHOD( opio::net::simple_buffer_driver_t &, buffer_driver, () );
};

//
// input_ctx_mock_t
//

// NOLINTNEXTLINE(altera-struct-pack-align)
struct input_ctx_mock_t
{
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    MOCK_METHOD( connection_mock_t &, connection, () );

    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    MOCK_METHOD( opio::net::simple_buffer_t &, buf, () );

    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    MOCK_METHOD( void, next_read_buffer, ( opio::net::simple_buffer_t ) );
};

//
// OpioProtoEntryRawBytesHandler
//

class OpioProtoEntryRawBytesHandler : public testing::Test
{
protected:
    opio::net::tcp::connection_cfg_t cfg{};
    opio::net::simple_buffer_driver_t buffer_driver{};
    connection_mock_t connection{};

    opio::net::simple_buffer_t buf;
    StrictMock< input_ctx_mock_t > ctx{};

    void SetUp() override
    {
        cfg.input_buffer_size( 32 );

        EXPECT_CALL( connection, cfg() ).WillRepeatedly( ReturnRef( cfg ) );
        EXPECT_CALL( connection, buffer_driver() )
            .WillRepeatedly( ReturnRef( buffer_driver ) );

        EXPECT_CALL( ctx, connection() ).WillRepeatedly( ReturnRef( connection ) );
        EXPECT_CALL( ctx, buf() ).WillRepeatedly( ReturnRef( buf ) );
    }
};

TEST_F( OpioProtoEntryRawBytesHandler, ReadBufSizeNoAdjustments )  // NOLINT
{
    using entry_t             = test_entry_t< message_consumer_mock_t >;
    using raw_bytes_handler_t = typename entry_t::raw_bytes_handler_t;

    asio_ns::io_context ioctx( 1 );

    raw_bytes_handler_t handler{ ioctx.get_executor(), {} };

    buf.resize( 16 );
    handler( ctx );

    buf.resize( 24 );
    handler( ctx );

    buf.resize( 31 );
    handler( ctx );

    buf.resize( 30 );
    handler( ctx );

    buf.resize( 8 );
    handler( ctx );
}

// NOLINTNEXTLINE
TEST_F( OpioProtoEntryRawBytesHandler, ReadBufSizeGrowAndShrink )
{
    using entry_t             = test_entry_t< message_consumer_mock_t >;
    using raw_bytes_handler_t = typename entry_t::raw_bytes_handler_t;

    asio_ns::io_context ioctx( 1 );

    raw_bytes_handler_t handler{ ioctx.get_executor(), {} };

    // Force to grow.
    buf.resize( 32 );
    EXPECT_CALL( ctx, next_read_buffer( _ ) ).WillOnce( []( auto buf ) {
        EXPECT_EQ( 64, buf.size() );
    } );
    handler( ctx );

    // CauseForce to grow.
    buf.resize( 64 );
    EXPECT_CALL( ctx, next_read_buffer( _ ) ).WillOnce( []( auto buf ) {
        EXPECT_EQ( 128, buf.size() );
    } );
    handler( ctx );

    // CauseForce to grow.
    buf.resize( 128 );
    EXPECT_CALL( ctx, next_read_buffer( _ ) ).WillOnce( []( auto buf ) {
        EXPECT_EQ( 256, buf.size() );
    } );
    handler( ctx );

    // CauseForce to grow.
    buf.resize( 256 );
    EXPECT_CALL( ctx, next_read_buffer( _ ) ).WillOnce( []( auto buf ) {
        EXPECT_EQ( 512, buf.size() );
    } );
    handler( ctx );

    // Must remain the same.
    buf.resize( 257 );
    EXPECT_CALL( ctx, next_read_buffer( _ ) ).WillOnce( []( auto buf ) {
        EXPECT_EQ( 512, buf.size() );
    } );
    handler( ctx );

    // Must shrink.
    buf.resize( 256 );
    EXPECT_CALL( ctx, next_read_buffer( _ ) ).WillOnce( []( auto buf ) {
        EXPECT_EQ( 256, buf.size() );
    } );
    handler( ctx );

    // Must remain the same as previous time.
    buf.resize( 255 );
    EXPECT_CALL( ctx, next_read_buffer( _ ) ).WillOnce( []( auto buf ) {
        EXPECT_EQ( 256, buf.size() );
    } );
    handler( ctx );

    // Must shrink (2 steps).
    buf.resize( 63 );
    EXPECT_CALL( ctx, next_read_buffer( _ ) ).WillOnce( []( auto buf ) {
        EXPECT_EQ( 64, buf.size() );
    } );
    handler( ctx );

    // Must remain growed.
    buf.resize( 32 );
    EXPECT_CALL( ctx, next_read_buffer( _ ) ).WillOnce( []( auto buf ) {
        EXPECT_EQ( 64, buf.size() );
    } );
    handler( ctx );

    buf.resize( 31 );
    handler( ctx );
}

TEST( OpioProtoEntry, HandleHeartBeatWhenDisconnected )  // NOLINT
{
    const auto started_at = std::chrono::steady_clock::now();

    asio_ns::io_context ioctx( 1 );
    asio_ns::ip::tcp::endpoint ep{
        asio_ns::ip::address::from_string( "127.0.0.1" ), 40001
    };

    asio_ns::ip::tcp::socket client_socket{ ioctx };
    StrictMock< message_consumer_mock_t > message_consumer;

    using entry_t = test_entry_t< decltype( message_consumer ) * >;
    typename entry_t::sptr_t entry;

    auto acceptor = opio::net::tcp::make_acceptor(
        ioctx.get_executor(),
        ep,
        make_test_logger( "acceptor" ),
        [ & ]( auto socket ) {
            entry = entry_t::make( std::move( socket ), [ & ]( auto & params ) {
                params.logger( make_test_logger( "ENTRY" ) )
                    .message_consumer( &message_consumer );
            } );
        } );
    acceptor->open();

    ioctx.poll();

    opio::net::tcp::async_connect( ioctx.get_executor(),
                                   "localhost",
                                   40001,
                                   make_test_logger( "connector" ),
                                   [ & ]( const auto & ec, auto socket ) {
                                       EXPECT_FALSE( static_cast< bool >( ec ) );
                                       client_socket = std::move( socket );
                                   } );

    run_ioctx_for( ioctx, std::chrono::milliseconds( 100 ) );
    acceptor->close();
    run_ioctx_for( ioctx, std::chrono::milliseconds( 5 ) );

    for( auto i = 0; i < 100; ++i )
    {
        entry->schedule_send_raw_bufs(
            opio::net::simple_buffer_t::make_from( { 'H', 'e', 'l', 'l', 'o' } ) );
    }

    ioctx.post( [ & ] {
        const auto ping_req_header =
            pkg_header_t::make( pkg_content_heartbeat_request );

        auto buf =
            opio::net::simple_buffer_t{ &ping_req_header,
                                        ping_req_header.advertized_header_size() };

        asio_ns::write( client_socket,
                        asio_ns::const_buffer{ buf.data(), buf.size() } );
        asio_ns::write( client_socket,
                        asio_ns::const_buffer{ buf.data(), buf.size() } );
        asio_ns::write( client_socket,
                        asio_ns::const_buffer{ buf.data(), buf.size() } );
        client_socket.close();
        for( auto i = 0; i < 100; ++i )
        {
            entry->schedule_send_raw_bufs( opio::net::simple_buffer_t::make_from(
                { 'H', 'e', 'l', 'l', 'o' } ) );
        }
    } );

    run_ioctx_for( ioctx, std::chrono::milliseconds( 200 ) );

    EXPECT_GT( adjust_for_msvc_if_necessary( 1000 ),
               msec_from_x_to_now( started_at ) );
}

#define OPIO_PROTO_ENTRY_TEST_PROTOBUF_PARSING_STRATEGY \
    opio::proto_entry::protobuf_parsing_strategy::trivial

// NOLINTNEXTLINE
#define OPIO_PROTO_ENTRY_TEST_CONSUMER_TEST_TYPE( name ) default_##name
// NOLINTNEXTLINE
#define OPIO_PROTO_ENTRY_TEST_CONSUMER_TEST_NAME( name ) name##Default
// NOLINTNEXTLINE
#include "entry_message_consumer_tests.ipp"
#undef OPIO_PROTO_ENTRY_TEST_CONSUMER_TEST_TYPE
#undef OPIO_PROTO_ENTRY_TEST_CONSUMER_TEST_NAME

#undef OPIO_PROTO_ENTRY_TEST_PROTOBUF_PARSING_STRATEGY

#define OPIO_PROTO_ENTRY_TEST_PROTOBUF_PARSING_STRATEGY \
    opio::proto_entry::protobuf_parsing_strategy::with_arena

// NOLINTNEXTLINE
#define OPIO_PROTO_ENTRY_TEST_CONSUMER_TEST_TYPE( name ) with_arena_##name
// NOLINTNEXTLINE
#define OPIO_PROTO_ENTRY_TEST_CONSUMER_TEST_NAME( name ) name##WithArena
// NOLINTNEXTLINE
#include "entry_message_consumer_tests.ipp"
#undef OPIO_PROTO_ENTRY_TEST_CONSUMER_TEST_TYPE
#undef OPIO_PROTO_ENTRY_TEST_CONSUMER_TEST_NAME

#undef OPIO_PROTO_ENTRY_TEST_PROTOBUF_PARSING_STRATEGY
}  // anonymous namespace
