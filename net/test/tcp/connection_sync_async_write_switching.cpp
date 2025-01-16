#include <numeric>
#include <iostream>

#define OPIO_NET_QUIK_SYNC_WRITE_HEURISTIC_SIZE 4 * 1024  // NOLINT

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
    std::size_t input_bytes_async{};
    std::size_t output_bytes_async{};

    std::size_t input_bytes_sync{};
    std::size_t output_bytes_sync{};

    auto bytes_rx() const noexcept { return input_bytes_async + input_bytes_sync; }

    template < typename Connection >
    void inc_bytes_rx_async( std::size_t n,
                             [[maybe_unused]] Connection & con ) noexcept
    {
        input_bytes_async += n;
    }
    template < typename Connection >
    void inc_bytes_rx_sync( std::size_t n,
                            [[maybe_unused]] Connection & con ) noexcept
    {
        input_bytes_sync += n;
    }

    template < typename Connection >
    void inc_bytes_tx_async( std::size_t n,
                             [[maybe_unused]] Connection & con ) noexcept
    {
        output_bytes_async += n;
    }
    template < typename Connection >
    void inc_bytes_tx_sync( std::size_t n,
                            [[maybe_unused]] Connection & con ) noexcept
    {
        output_bytes_sync += n;
    }

    template < typename... Args >
    constexpr void inc_bytes_rx_async( Args &&... ) noexcept
    {
    }

    template < typename... Args >
    constexpr void inc_bytes_rx_sync( Args &&... ) noexcept
    {
    }

    template < typename... Args >
    constexpr void inc_bytes_tx_async( Args &&... ) noexcept
    {
    }

    template < typename... Args >
    constexpr void inc_bytes_tx_sync( Args &&... ) noexcept
    {
    }

    int would_block_events{};

    template < typename Connection >
    void hit_would_block_event( [[maybe_unused]] std::size_t n,
                                [[maybe_unused]] Connection & con ) noexcept
    {
        ++would_block_events;
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

using connection_cfg_t = opio::net::tcp::connection_cfg_t;
using connection_t     = opio::net::tcp::connection_t< connection_traits_st_t >;
using buffer_t         = opio::net::simple_buffer_t;

template < typename Input_Handler >
connection_t::sptr_t make_connection( connection_traits_st_t::socket_t socket,
                                      opio::net::tcp::connection_id_t id,
                                      const opio::net::tcp::connection_cfg_t & cfg,
                                      opio::logger::logger_t logger,
                                      Input_Handler input_handler )
{
    return connection_t::make( std::move( socket ), [ & ]( auto & params ) {
        params.connection_id( id )
            .connection_cfg( cfg )
            .logger( std::move( logger ) )
            .input_handler( std::move( input_handler ) );
    } );
}

// Note it is a prime, and in the end we will check that
// the total data sent is an exact multiple of it.
constexpr std::size_t single_buf_size = 1117;

void do_write( asio_ns::io_context * ioctx,
               connection_t * client_conn,
               connection_t * server_conn )
{
    asio_ns::post( ioctx->get_executor(), [ = ] {
        client_conn->logger().info( "Will schedule yet another piece of data" );

        asio_ns::post( ioctx->get_executor(), [ = ] {
            client_conn->schedule_send_with_cb(
                [ = ]( auto res ) {
                    if( send_buffers_result::success == res
                        && 0 == client_conn->stats_driver().would_block_events )
                    {
                        do_write( ioctx, client_conn, server_conn );
                    }
                },
                buffer_t( single_buf_size, static_cast< std::byte >( '#' ) ),
                buffer_t( single_buf_size, static_cast< std::byte >( '$' ) ),
                buffer_t( single_buf_size, static_cast< std::byte >( '%' ) ) );
        } );

        asio_ns::defer( ioctx->get_executor(), [ = ] {
            if( client_conn->stats_driver().would_block_events )
            {
                client_conn->logger().info( "Hit would_block event" );
                server_conn->start_reading();
            }
            else
            {
                client_conn->logger().info( "NO would_block events yet" );
            }
        } );
    } );
}

TEST( OpioNetTcp, SyncWriteWouldBlockSwitchToAsyncWrite )  // NOLINT
{
    connection_cfg_t cfg{};

    socket_options_cfg_t socket_options_cfg;
    socket_options_cfg.receive_buffer_size = 16 * 1024;
    socket_options_cfg.send_buffer_size    = 16 * 1024;

    asio_ns::io_context ioctx( 1 );
    asio_ns::executor_work_guard< opio::net::tcp::noop_strand_t > work{
        ioctx.get_executor()
    };

    opio::net::asio_ns::ip::tcp::socket s1{ ioctx };
    opio::net::asio_ns::ip::tcp::socket s2{ ioctx };

    connect_pair( ioctx, s1, s2 );

    auto server_conn = make_connection(
        std::move( s1 ),
        0,
        cfg,
        make_test_logger( "SERVER_CONN", logr::log_message_level::debug ),
        [ & ]( [[maybe_unused]] auto & ctx ) {} );

    auto client_conn = make_connection(
        std::move( s2 ),
        1,
        cfg,
        make_test_logger( "client_conn", logr::log_message_level::debug ),
        [ & ]( [[maybe_unused]] auto & ctx ) {} );

    ioctx.run_for( std::chrono::milliseconds( 200 ) );

    do_write( &ioctx, client_conn.get(), server_conn.get() );

    ioctx.run_for( std::chrono::seconds( 1 ) );

    ASSERT_LE( 1, client_conn->stats_driver().would_block_events );

    EXPECT_LT( 0, server_conn->stats_driver().bytes_rx() );
    EXPECT_EQ( 0, server_conn->stats_driver().bytes_rx() % single_buf_size );
}

}  // anonymous namespace
