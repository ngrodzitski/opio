#include <numeric>
#include <iostream>

#define OPIO_NET_QUIK_SYNC_WRITE_HEURISTIC_SIZE 64  // NOLINT

#include <opio/net/tcp/connection.hpp>
#include <opio/net/tcp/utils.hpp>

#include <gtest/gtest.h>

#include <opio/test_utils/test_logger.hpp>
#include <tcp_test_utils.hpp>

namespace /* anonymous */
{

//
// simple_stats_driver_t
//

struct test_stats_driver_t : public opio::net::noop_stats_driver_t
{
    std::optional< std::thread::id > last_sync_write_tid;
    std::optional< std::thread::id > last_async_write_tid;

    template < typename... Args >
    constexpr void sync_write_started( [[maybe_unused]] Args &&... ) noexcept
    {
        last_sync_write_tid = std::this_thread::get_id();
    }

    template < typename... Args >
    constexpr void async_write_started( [[maybe_unused]] Args &&... ) noexcept
    {
        last_async_write_tid = std::this_thread::get_id();
    }
};

using namespace ::opio::net;         // NOLINT
using namespace ::opio::net::tcp;    // NOLINT
using namespace ::opio::test_utils;  // NOLINT

struct connection_traits_st_t : public default_traits_st_t
{
    using logger_t       = opio::logger::logger_t;
    using stats_driver_t = test_stats_driver_t;
    using input_handler_t =
        std::function< void( input_ctx_t< connection_traits_st_t > & ) >;
};

using connection_t = opio::net::tcp::connection_t< connection_traits_st_t >;
using buffer_t     = simple_buffer_t;

class OpioIpcTcpXXXSend : public ::testing::Test
{
protected:
    asio_ns::io_context ioctx;
    std::thread ioctx_thread;

    connection_t::sptr_t server_conn;
    connection_t::sptr_t client_conn;

    std::vector< std::string > client_input;

    void SetUp() override
    {
        opio::net::asio_ns::ip::tcp::socket server_socket{ ioctx };
        opio::net::asio_ns::ip::tcp::socket client_socket{ ioctx };

        connect_pair( ioctx, server_socket, client_socket );

        socket_options_cfg_t socket_options_cfg;
        socket_options_cfg.receive_buffer_size = 16 * 1024;
        socket_options_cfg.send_buffer_size    = 16 * 1024;

        set_socket_options( socket_options_cfg, server_socket );
        set_socket_options( socket_options_cfg, client_socket );

        connection_cfg_t cfg{};
        server_conn = make_connection< connection_t >(
            std::move( server_socket ),
            0,
            cfg,
            make_test_logger( "SERVER_CONN", logr::log_message_level::trace ),
            []( auto & /*ctx*/ ) {} );

        client_conn = make_connection< connection_t >(
            std::move( client_socket ),
            1,
            cfg,
            make_test_logger( "CLIENT_CONN", logr::log_message_level::trace ),
            [ this ]( auto & ctx ) {
                std::string s{ reinterpret_cast< const char * >(
                                   ctx.buf().data() ),
                               ctx.buf().size() };

                client_input.emplace_back( std::move( s ) );
            } );

        ioctx_thread = std::thread{ [ this ] {
            auto logger =
                make_test_logger( "ioctx_thread", logr::log_message_level::trace );

            logger.info( OPIO_SRC_LOCATION, "started" );
            asio_ns::executor_work_guard< noop_strand_t > work{
                ioctx.get_executor()
            };
            ioctx.restart();
            ioctx.run();
            logger.info( OPIO_SRC_LOCATION, "finished" );
        } };

        client_conn->start_reading();
    }

    void TearDown() override
    {
        ioctx.stop();
        ioctx_thread.join();
    }
};

// =============================================================================

TEST_F( OpioIpcTcpXXXSend, DispatchSendNonAsioThread )  // NOLINT
{
    std::string buf_content = "DispatchSend12345";
    buffer_t b{ buf_content.data(), buf_content.size() };

    const auto caller_thread_id = std::this_thread::get_id();
    server_conn->dispatch_send( std::move( b ) );

    std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );

    ASSERT_TRUE( server_conn->stats_driver().last_sync_write_tid );
    ASSERT_EQ( server_conn->stats_driver().last_sync_write_tid.value(),
               ioctx_thread.get_id() );
    ASSERT_NE( server_conn->stats_driver().last_sync_write_tid.value(),
               caller_thread_id );

    ASSERT_EQ( 1, client_input.size() );
    ASSERT_EQ( buf_content, client_input.back() );
}

TEST_F( OpioIpcTcpXXXSend, DispatchSendNonAsioThreadBigBuffer )  // NOLINT
{
    std::string buf_content = "DispatchSend12345";
    buf_content += buf_content;
    buf_content += buf_content;
    buf_content += buf_content;

    ASSERT_LE( OPIO_NET_QUIK_SYNC_WRITE_HEURISTIC_SIZE, buf_content.size() );

    buffer_t b{ buf_content.data(), buf_content.size() };

    const auto caller_thread_id = std::this_thread::get_id();
    server_conn->dispatch_send( std::move( b ) );

    std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );

    ASSERT_TRUE( server_conn->stats_driver().last_async_write_tid );
    ASSERT_EQ( server_conn->stats_driver().last_async_write_tid.value(),
               ioctx_thread.get_id() );
    ASSERT_NE( server_conn->stats_driver().last_async_write_tid.value(),
               caller_thread_id );

    ASSERT_EQ( 1, client_input.size() );
    ASSERT_EQ( buf_content, client_input.back() );
}

TEST_F( OpioIpcTcpXXXSend, DispatchSendAsioThread )  // NOLINT
{
    std::string buf_content = "DispatchSend12345";

    std::thread::id caller_thread_id;

    asio_ns::post( ioctx, [ & ] {
        caller_thread_id = std::this_thread::get_id();

        buffer_t b{ buf_content.data(), buf_content.size() };
        server_conn->dispatch_send( std::move( b ) );

        // Dispatch will be executed as function call
        // so write must already happen.
        ASSERT_TRUE( server_conn->stats_driver().last_sync_write_tid );
    } );

    std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );

    ASSERT_TRUE( server_conn->stats_driver().last_sync_write_tid );
    ASSERT_EQ( server_conn->stats_driver().last_sync_write_tid.value(),
               ioctx_thread.get_id() );
    ASSERT_EQ( server_conn->stats_driver().last_sync_write_tid.value(),
               caller_thread_id );

    ASSERT_EQ( 1, client_input.size() );
    ASSERT_EQ( buf_content, client_input.back() );
}

TEST_F( OpioIpcTcpXXXSend, DispatchSendAsioThreadBigBuffer )  // NOLINT
{
    std::string buf_content = "DispatchSend12345";
    buf_content += buf_content;
    buf_content += buf_content;
    buf_content += buf_content;

    ASSERT_LE( OPIO_NET_QUIK_SYNC_WRITE_HEURISTIC_SIZE, buf_content.size() );

    std::thread::id caller_thread_id;

    asio_ns::post( ioctx, [ & ] {
        caller_thread_id = std::this_thread::get_id();

        buffer_t b{ buf_content.data(), buf_content.size() };
        server_conn->dispatch_send( std::move( b ) );

        // Dispatch will be executed as function call
        // so write must already happen.
        ASSERT_TRUE( server_conn->stats_driver().last_async_write_tid );
    } );

    std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );

    ASSERT_TRUE( server_conn->stats_driver().last_async_write_tid );
    ASSERT_EQ( server_conn->stats_driver().last_async_write_tid.value(),
               ioctx_thread.get_id() );
    ASSERT_EQ( server_conn->stats_driver().last_async_write_tid.value(),
               caller_thread_id );

    ASSERT_EQ( 1, client_input.size() );
    ASSERT_EQ( buf_content, client_input.back() );
}

// =============================================================================

TEST_F( OpioIpcTcpXXXSend, PostSendNonAsioThread )  // NOLINT
{
    std::string buf_content = "PostSend123456789";
    buffer_t b{ buf_content.data(), buf_content.size() };

    const auto caller_thread_id = std::this_thread::get_id();
    server_conn->post_send( std::move( b ) );

    std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );

    ASSERT_TRUE( server_conn->stats_driver().last_sync_write_tid );
    ASSERT_EQ( server_conn->stats_driver().last_sync_write_tid.value(),
               ioctx_thread.get_id() );
    ASSERT_NE( server_conn->stats_driver().last_sync_write_tid.value(),
               caller_thread_id );

    ASSERT_EQ( 1, client_input.size() );
    ASSERT_EQ( buf_content, client_input.back() );
}

TEST_F( OpioIpcTcpXXXSend, PostSendNonAsioThreadBigBuffer )  // NOLINT
{
    std::string buf_content = "PostSend123456789";
    buf_content += buf_content;
    buf_content += buf_content;
    buf_content += buf_content;

    ASSERT_LE( OPIO_NET_QUIK_SYNC_WRITE_HEURISTIC_SIZE, buf_content.size() );

    buffer_t b{ buf_content.data(), buf_content.size() };

    const auto caller_thread_id = std::this_thread::get_id();
    server_conn->post_send( std::move( b ) );

    std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );

    ASSERT_TRUE( server_conn->stats_driver().last_async_write_tid );
    ASSERT_EQ( server_conn->stats_driver().last_async_write_tid.value(),
               ioctx_thread.get_id() );
    ASSERT_NE( server_conn->stats_driver().last_async_write_tid.value(),
               caller_thread_id );

    ASSERT_EQ( 1, client_input.size() );
    ASSERT_EQ( buf_content, client_input.back() );
}

TEST_F( OpioIpcTcpXXXSend, PostSendAsioThread )  // NOLINT
{
    std::string buf_content = "PostSend123456789";

    std::thread::id caller_thread_id;

    asio_ns::post( ioctx, [ & ] {
        caller_thread_id = std::this_thread::get_id();

        buffer_t b{ buf_content.data(), buf_content.size() };
        server_conn->post_send( std::move( b ) );

        // Post CB will not be executed yet (note asio runs on single thread).
        ASSERT_FALSE( server_conn->stats_driver().last_sync_write_tid );
    } );

    std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );

    ASSERT_TRUE( server_conn->stats_driver().last_sync_write_tid );
    ASSERT_EQ( server_conn->stats_driver().last_sync_write_tid.value(),
               ioctx_thread.get_id() );
    ASSERT_EQ( server_conn->stats_driver().last_sync_write_tid.value(),
               caller_thread_id );

    ASSERT_EQ( 1, client_input.size() );
    ASSERT_EQ( buf_content, client_input.back() );
}

TEST_F( OpioIpcTcpXXXSend, PostSendAsioThreadBigBuffer )  // NOLINT
{
    std::string buf_content = "PostSend123456789";
    buf_content += buf_content;
    buf_content += buf_content;
    buf_content += buf_content;

    ASSERT_LE( OPIO_NET_QUIK_SYNC_WRITE_HEURISTIC_SIZE, buf_content.size() );

    std::thread::id caller_thread_id;

    asio_ns::post( ioctx, [ & ] {
        caller_thread_id = std::this_thread::get_id();

        buffer_t b{ buf_content.data(), buf_content.size() };
        server_conn->post_send( std::move( b ) );

        ASSERT_FALSE( server_conn->stats_driver().last_async_write_tid );
    } );

    std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );

    ASSERT_TRUE( server_conn->stats_driver().last_async_write_tid );
    ASSERT_EQ( server_conn->stats_driver().last_async_write_tid.value(),
               ioctx_thread.get_id() );
    ASSERT_EQ( server_conn->stats_driver().last_async_write_tid.value(),
               caller_thread_id );

    ASSERT_EQ( 1, client_input.size() );
    ASSERT_EQ( buf_content, client_input.back() );
}

// =============================================================================

TEST_F( OpioIpcTcpXXXSend, AggressiveDispatchSendNonAsioThread )  // NOLINT
{
    std::string buf_content = "AggressiveDispatchSend";
    buffer_t b{ buf_content.data(), buf_content.size() };

    const auto caller_thread_id = std::this_thread::get_id();
    server_conn->aggressive_dispatch_send( std::move( b ) );

    std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );

    ASSERT_TRUE( server_conn->stats_driver().last_sync_write_tid );

#if defined( OPIO_NET_FORCE_DEFAULT_LOCKING_WITH_MUTEX )
    ASSERT_EQ( server_conn->stats_driver().last_sync_write_tid.value(),
               caller_thread_id );
#else   // defined(OPIO_NET_FORCE_DEFAULT_LOCKING_WITH_MUTEX)
    ASSERT_NE( server_conn->stats_driver().last_sync_write_tid.value(),
               caller_thread_id );
#endif  // defined(OPIO_NET_FORCE_DEFAULT_LOCKING_WITH_MUTEX)

    ASSERT_EQ( 1, client_input.size() );
    ASSERT_EQ( buf_content, client_input.back() );
}

TEST_F( OpioIpcTcpXXXSend,
        AggressiveDispatchSendNonAsioThreadBigBuffer )  // NOLINT
{
    std::string buf_content = "AggressiveDispatchSend";
    buf_content += buf_content;
    buf_content += buf_content;
    buf_content += buf_content;

    ASSERT_LE( OPIO_NET_QUIK_SYNC_WRITE_HEURISTIC_SIZE, buf_content.size() );

    buffer_t b{ buf_content.data(), buf_content.size() };

    const auto caller_thread_id = std::this_thread::get_id();
    server_conn->aggressive_dispatch_send( std::move( b ) );

    std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );

    ASSERT_TRUE( server_conn->stats_driver().last_async_write_tid );

#if defined( OPIO_NET_FORCE_DEFAULT_LOCKING_WITH_MUTEX )
    ASSERT_EQ( server_conn->stats_driver().last_async_write_tid.value(),
               caller_thread_id );
#else   // defined(OPIO_NET_FORCE_DEFAULT_LOCKING_WITH_MUTEX)
    ASSERT_NE( server_conn->stats_driver().last_async_write_tid.value(),
               caller_thread_id );
#endif  // defined(OPIO_NET_FORCE_DEFAULT_LOCKING_WITH_MUTEX)

    ASSERT_EQ( 1, client_input.size() );
    ASSERT_EQ( buf_content, client_input.back() );
}

TEST_F( OpioIpcTcpXXXSend, AggressiveDispatchSendAsioThread )  // NOLINT
{
    std::string buf_content = "AggressiveDispatchSend";

    std::thread::id caller_thread_id;

    asio_ns::post( ioctx, [ & ] {
        caller_thread_id = std::this_thread::get_id();

        buffer_t b{ buf_content.data(), buf_content.size() };
        server_conn->aggressive_dispatch_send( std::move( b ) );

        // Post CB will not be executed yet (note asio runs on single thread).
        ASSERT_TRUE( server_conn->stats_driver().last_sync_write_tid );
    } );

    std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );

    ASSERT_TRUE( server_conn->stats_driver().last_sync_write_tid );
    ASSERT_EQ( server_conn->stats_driver().last_sync_write_tid.value(),
               ioctx_thread.get_id() );
    ASSERT_EQ( server_conn->stats_driver().last_sync_write_tid.value(),
               caller_thread_id );

    ASSERT_EQ( 1, client_input.size() );
    ASSERT_EQ( buf_content, client_input.back() );
}

TEST_F( OpioIpcTcpXXXSend,
        AggressiveDispatchSendAsioThreadBigBuffer )  // NOLINT
{
    std::string buf_content = "AggressiveDispatchSend";
    buf_content += buf_content;
    buf_content += buf_content;
    buf_content += buf_content;

    ASSERT_LE( OPIO_NET_QUIK_SYNC_WRITE_HEURISTIC_SIZE, buf_content.size() );

    std::thread::id caller_thread_id;

    asio_ns::post( ioctx, [ & ] {
        caller_thread_id = std::this_thread::get_id();

        buffer_t b{ buf_content.data(), buf_content.size() };
        server_conn->aggressive_dispatch_send( std::move( b ) );

        ASSERT_TRUE( server_conn->stats_driver().last_async_write_tid );
    } );

    std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );

    ASSERT_TRUE( server_conn->stats_driver().last_async_write_tid );
    ASSERT_EQ( server_conn->stats_driver().last_async_write_tid.value(),
               ioctx_thread.get_id() );
    ASSERT_EQ( server_conn->stats_driver().last_async_write_tid.value(),
               caller_thread_id );

    ASSERT_EQ( 1, client_input.size() );
    ASSERT_EQ( buf_content, client_input.back() );
}

// =============================================================================

TEST_F( OpioIpcTcpXXXSend,
        AggressiveDispatchSendWithCBNonAsioThread )  // NOLINT
{
    std::string buf_content = "AggressiveDispatchSend";

    std::optional< std::thread::id > cb_tid;

    buffer_t b{ buf_content.data(), buf_content.size() };

    const auto caller_thread_id = std::this_thread::get_id();
    server_conn->aggressive_dispatch_send_with_cb(
        [ & ]( [[maybe_unused]] auto res ) {
            cb_tid = std::this_thread::get_id();
        },
        std::move( b ) );

    std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );

    ASSERT_TRUE( server_conn->stats_driver().last_sync_write_tid );
    ASSERT_TRUE( cb_tid );

#if defined( OPIO_NET_FORCE_DEFAULT_LOCKING_WITH_MUTEX )
    ASSERT_EQ( server_conn->stats_driver().last_sync_write_tid.value(),
               caller_thread_id );
    ASSERT_EQ( cb_tid, caller_thread_id );
#else   // defined(OPIO_NET_FORCE_DEFAULT_LOCKING_WITH_MUTEX)
    ASSERT_NE( server_conn->stats_driver().last_sync_write_tid.value(),
               caller_thread_id );
    ASSERT_NE( cb_tid, caller_thread_id );
#endif  // defined(OPIO_NET_FORCE_DEFAULT_LOCKING_WITH_MUTEX)

    ASSERT_EQ( 1, client_input.size() );
    ASSERT_EQ( buf_content, client_input.back() );
}

TEST_F( OpioIpcTcpXXXSend,
        AggressiveDispatchSendWithCBNonAsioThreadBigBuffer )  // NOLINT
{
    std::string buf_content = "AggressiveDispatchSend";
    buf_content += buf_content;
    buf_content += buf_content;
    buf_content += buf_content;

    std::optional< std::thread::id > cb_tid;
    ASSERT_LE( OPIO_NET_QUIK_SYNC_WRITE_HEURISTIC_SIZE, buf_content.size() );

    buffer_t b{ buf_content.data(), buf_content.size() };

    const auto caller_thread_id = std::this_thread::get_id();
    server_conn->aggressive_dispatch_send_with_cb(
        [ & ]( [[maybe_unused]] auto res ) {
            cb_tid = std::this_thread::get_id();
        },
        std::move( b ) );

    std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );

    ASSERT_TRUE( server_conn->stats_driver().last_async_write_tid );
    ASSERT_TRUE( cb_tid );

#if defined( OPIO_NET_FORCE_DEFAULT_LOCKING_WITH_MUTEX )
    ASSERT_EQ( server_conn->stats_driver().last_async_write_tid.value(),
               caller_thread_id );

#else   // defined(OPIO_NET_FORCE_DEFAULT_LOCKING_WITH_MUTEX)
    ASSERT_NE( server_conn->stats_driver().last_async_write_tid.value(),
               caller_thread_id );
#endif  // defined(OPIO_NET_FORCE_DEFAULT_LOCKING_WITH_MUTEX)

    // With "big" buffer an `async_write` operation is started
    // even for aggressive dispatch operations.
    // and completion token for it will be executed on asio thread
    // which means a CB attached to a given thread will also be executed
    // on asio thread
    ASSERT_NE( cb_tid, caller_thread_id );
    ASSERT_EQ( cb_tid, ioctx_thread.get_id() );

    ASSERT_EQ( 1, client_input.size() );
    ASSERT_EQ( buf_content, client_input.back() );
}

TEST_F( OpioIpcTcpXXXSend, AggressiveDispatchSendWithCBAsioThread )  // NOLINT
{
    std::string buf_content = "AggressiveDispatchSend";

    std::thread::id caller_thread_id;
    std::optional< std::thread::id > cb_tid;

    asio_ns::post( ioctx, [ & ] {
        caller_thread_id = std::this_thread::get_id();

        buffer_t b{ buf_content.data(), buf_content.size() };
        server_conn->aggressive_dispatch_send_with_cb(
            [ & ]( [[maybe_unused]] auto res ) {
                cb_tid = std::this_thread::get_id();
            },
            std::move( b ) );

        // Post CB will not be executed yet (note asio runs on single thread).
        ASSERT_TRUE( server_conn->stats_driver().last_sync_write_tid );
    } );

    std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );

    ASSERT_TRUE( server_conn->stats_driver().last_sync_write_tid );
    ASSERT_TRUE( cb_tid );

    ASSERT_EQ( server_conn->stats_driver().last_sync_write_tid.value(),
               ioctx_thread.get_id() );
    ASSERT_EQ( server_conn->stats_driver().last_sync_write_tid.value(),
               caller_thread_id );
    ASSERT_EQ( server_conn->stats_driver().last_sync_write_tid.value(), cb_tid );

    ASSERT_EQ( 1, client_input.size() );
    ASSERT_EQ( buf_content, client_input.back() );
}

TEST_F( OpioIpcTcpXXXSend,
        AggressiveDispatchSendWithCBAsioThreadBigBuffer )  // NOLINT
{
    std::string buf_content = "AggressiveDispatchSend";
    buf_content += buf_content;
    buf_content += buf_content;
    buf_content += buf_content;

    ASSERT_LE( OPIO_NET_QUIK_SYNC_WRITE_HEURISTIC_SIZE, buf_content.size() );

    std::thread::id caller_thread_id;
    std::optional< std::thread::id > cb_tid;

    asio_ns::post( ioctx, [ & ] {
        caller_thread_id = std::this_thread::get_id();

        buffer_t b{ buf_content.data(), buf_content.size() };
        server_conn->aggressive_dispatch_send_with_cb(
            [ & ]( [[maybe_unused]] auto res ) {
                cb_tid = std::this_thread::get_id();
            },
            std::move( b ) );

        ASSERT_TRUE( server_conn->stats_driver().last_async_write_tid );
    } );

    std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );

    ASSERT_TRUE( server_conn->stats_driver().last_async_write_tid );
    ASSERT_TRUE( cb_tid );

    ASSERT_EQ( server_conn->stats_driver().last_async_write_tid.value(),
               ioctx_thread.get_id() );
    ASSERT_EQ( server_conn->stats_driver().last_async_write_tid.value(),
               caller_thread_id );
    ASSERT_EQ( server_conn->stats_driver().last_async_write_tid.value(), cb_tid );

    ASSERT_EQ( 1, client_input.size() );
    ASSERT_EQ( buf_content, client_input.back() );
}

}  // anonymous namespace
