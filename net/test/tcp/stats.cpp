#include <numeric>
#include <iostream>

#define OPIO_NET_QUIK_SYNC_WRITE_HEURISTIC_SIZE 100

#include <opio/net/tcp/connection.hpp>

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

    template < typename Connection >
    constexpr void hit_would_block_event(
        [[maybe_unused]] std::size_t n,
        [[maybe_unused]] Connection & con ) const noexcept
    {
    }

    std::size_t sync_write_size_started{};
    std::size_t sync_write_size_finished{};

    std::size_t async_write_size_started{};
    std::size_t async_write_size_finished{};

    template < typename Connection >
    void sync_write_started( std::size_t n,
                             [[maybe_unused]] Connection & con ) noexcept
    {
        sync_write_size_started += n;
    }

    template < typename Connection >
    void sync_write_finished( std::size_t n,
                              [[maybe_unused]] Connection & con ) noexcept
    {
        sync_write_size_finished += n;
    }

    template < typename Connection >
    void async_write_started( std::size_t n,
                              [[maybe_unused]] Connection & con ) noexcept
    {
        async_write_size_started += n;
    }

    template < typename Connection >
    void async_write_finished( std::size_t n,
                               [[maybe_unused]] Connection & con ) noexcept
    {
        async_write_size_finished += n;
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

TEST( OpioNetTcp, StatsDriver )  // NOLINT
{
    connection_cfg_t cfg{};

    constexpr std::size_t size_buf1 = 39;
    constexpr std::size_t size_buf2 = 61;

    constexpr std::size_t size_buf3 = 175;
    constexpr std::size_t size_buf4 = 150;

    buffer_t buf1( size_buf1, static_cast< std::byte >( '*' ) );
    buffer_t buf2( size_buf2, static_cast< std::byte >( '*' ) );
    buffer_t buf3( size_buf3, static_cast< std::byte >( '*' ) );
    buffer_t buf4( size_buf4, static_cast< std::byte >( '*' ) );

    asio_ns::io_context ioctx( 1 );

    opio::net::asio_ns::ip::tcp::socket s1{ ioctx };
    opio::net::asio_ns::ip::tcp::socket s2{ ioctx };

    connect_pair( ioctx, s1, s2 );

    auto server_conn = make_connection(
        std::move( s1 ),
        0,
        cfg,
        make_test_logger( "SERVER_CONN" ),
        [ & ]( auto & ctx ) {
            ctx.log().info( [ & ]( auto out ) {
                format_to( out, "received buf of size {}", ctx.buf().size() );
            } );
        } );
    EXPECT_EQ( server_conn->connection_id(), 0 );
    server_conn->start_reading();

    auto client_conn = make_connection( std::move( s2 ),
                                        1,
                                        cfg,
                                        make_test_logger( "client_conn" ),
                                        [ & ]( [[maybe_unused]] auto & ctx ) {} );
    EXPECT_EQ( client_conn->connection_id(), 1 );
    client_conn->start_reading();
    client_conn->schedule_send( std::move( buf1 ), std::move( buf2 ) );
    client_conn->schedule_send( std::move( buf3 ), std::move( buf4 ) );

    ioctx.run_for( std::chrono::milliseconds( 200 ) );
    server_conn->shutdown();
    ioctx.run();

    EXPECT_EQ( size_buf1 + size_buf2,
               client_conn->stats_driver().output_bytes_sync );
    EXPECT_EQ( size_buf1 + size_buf2,
               client_conn->stats_driver().sync_write_size_started );
    EXPECT_EQ( size_buf1 + size_buf2,
               client_conn->stats_driver().sync_write_size_finished );

    EXPECT_EQ( size_buf3 + size_buf4,
               client_conn->stats_driver().output_bytes_async );
    EXPECT_EQ( size_buf3 + size_buf4,
               client_conn->stats_driver().async_write_size_started );
    EXPECT_EQ( size_buf3 + size_buf4,
               client_conn->stats_driver().async_write_size_finished );

    EXPECT_EQ( 0, client_conn->stats_driver().input_bytes_sync );
    EXPECT_EQ( 0, client_conn->stats_driver().input_bytes_async );

    EXPECT_EQ( 0, server_conn->stats_driver().input_bytes_sync );
    EXPECT_EQ( size_buf1 + size_buf2 + size_buf3 + size_buf4,
               server_conn->stats_driver().input_bytes_async );
    EXPECT_EQ( 0, server_conn->stats_driver().output_bytes_sync );
    EXPECT_EQ( 0, server_conn->stats_driver().output_bytes_async );
}

}  // anonymous namespace
