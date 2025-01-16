#include <numeric>
#include <iostream>

#define OPIO_NET_QUIK_SYNC_WRITE_HEURISTIC_SIZE 0  // NOLINT

#include <opio/net/tcp/connection.hpp>

#include <gtest/gtest.h>

#include <opio/test_utils/test_logger.hpp>
#include <tcp_test_utils.hpp>

namespace /* anonymous */
{

// TODO: Think of making this test on windows
// Setting `receive_buffer_size` and `send_buffer_size`
// Doesn't work the same way on windows and the write operation succeeds
// which breaks test mechanics assumptions and we can't get timeout executed.
#if !defined( OPIO_ASIO_WINDOWS )

using namespace ::opio::net;         // NOLINT
using namespace ::opio::net::tcp;    // NOLINT
using namespace ::opio::test_utils;  // NOLINT

struct connection_traits_st_t : public default_traits_st_t
{
    using logger_t             = opio::logger::logger_t;
    using operation_watchdog_t = asio_timer_operation_watchdog_t;
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

TEST( OpioNetTcp, WriteOperationTimeout )  // NOLINT
{
    connection_cfg_t cfg{};
    cfg.write_timeout_per_1mb( std::chrono::milliseconds( 300 ) );

    socket_options_cfg_t socket_cfg{};
    socket_cfg.receive_buffer_size = 16 * 1024;
    socket_cfg.send_buffer_size    = 16 * 1024;

    asio_ns::io_context ioctx{};
    asio_ns::executor_work_guard< asio_ns::any_io_executor > work{
        ioctx.get_executor()
    };

    opio::net::asio_ns::ip::tcp::socket s1{ ioctx };
    opio::net::asio_ns::ip::tcp::socket s2{ ioctx };

    connect_pair( ioctx, s1, s2 );

    auto server_conn = make_connection( std::move( s1 ),
                                        0,
                                        cfg,
                                        make_test_logger( "SERVER_CONN" ),
                                        [ & ]( [[maybe_unused]] auto & ctx ) {

                                        } );
    server_conn->update_socket_options( socket_cfg );
    auto client_conn = make_connection( std::move( s2 ),
                                        1,
                                        cfg,
                                        make_test_logger( "client_conn" ),
                                        [ & ]( [[maybe_unused]] auto & ctx ) {} );
    client_conn->update_socket_options( socket_cfg );

    // give it some time to connect:
    ioctx.run_for( std::chrono::milliseconds( 200 ) );

    simple_buffer_t buf{
        2 * ( 16 * 1024 )  // Input buffer of remote client's socket doubled as OS
                           // alocates twice the requested amount;
            + 2 * ( 16 * 1024 )  // Onput buffer of remote client's socket doubled
                                 // as OS alocates twice the requested amount;

            // A little extra to make write operation wait.
            // On CI we have this not qute predictable, so
            + 5 * ( 16 * 1024 ),  // we add a lot
        static_cast< std::byte >( '@' )
    };

    std::optional< connection_shutdown_reason > shutdown_reason;

    server_conn->reset_shutdown_handler( [ & ]( auto reason ) {
        shutdown_reason = reason;
        ioctx.stop();
    } );

    const auto started_write_at = std::chrono::steady_clock::now();
    server_conn->schedule_send( std::move( buf ) );
    ioctx.run_for( std::chrono::seconds( 1 ) );

    ASSERT_TRUE( static_cast< bool >( shutdown_reason ) );
    EXPECT_EQ( connection_shutdown_reason::write_timeout, *shutdown_reason );

    const auto delay = std::chrono::duration_cast< std::chrono::milliseconds >(
                           std::chrono::steady_clock::now() - started_write_at )
                           .count();
    EXPECT_LE( 300, delay );
    EXPECT_GE( 320, delay );
}

#endif  // !defined(OPIO_ASIO_WINDOWS)

}  // anonymous namespace
