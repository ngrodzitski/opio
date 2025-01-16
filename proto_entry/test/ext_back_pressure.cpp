#include <opio/proto_entry/ext/back_pressure.hpp>

#include <opio/proto_entry/utest/entry.hpp>
#include <opio/test_utils/test_logger.hpp>
#include "test_utils.hpp"

#include <gmock/gmock.h>

namespace /* anonymous */
{

// Use Gtest routimes without namspaces.
using namespace ::testing;               // NOLINT
namespace asio_ns = opio::net::asio_ns;  // NOLINT
using namespace ::opio::test_utils;      // NOLINT

// For test we actually don't care about specific entry type
// because back-pressure uses sending raw bytes.
using sample_bp_client_base_t =
    opio::proto_entry::utest::core_entry_singlethread_t< message_consumer_mock_t *,
                                                         opio::logger::logger_t >;
using sample_bp_client_t =
    opio::proto_entry::ext::bp_entry_t< sample_bp_client_base_t, int >;

TEST( OpioProtoEntryExt, BackPressure )  // NOLINT
{
    asio_ns::io_context ioctx{};
    asio_ns::executor_work_guard< asio_ns::any_io_executor > work{
        ioctx.get_executor()
    };

    asio_ns::ip::tcp::socket server_socket{ ioctx };
    asio_ns::ip::tcp::socket client_socket{ ioctx };

    connect_pair( ioctx, server_socket, client_socket );

    opio::net::tcp::socket_options_cfg_t socket_cfg{};
    socket_cfg.receive_buffer_size = 16 * 1024;
    socket_cfg.send_buffer_size    = 16 * 1024;
    socket_cfg.no_delay            = true;

    message_consumer_mock_t message_consumer;

    auto bp_entry = sample_bp_client_t::make(
        std::move( server_socket ), [ & ]( auto & params ) {
            params.logger( make_test_logger( "ENTRY" ) )
                .message_consumer( &message_consumer );
        } );
    bp_entry->underlying_connection()->update_socket_options( socket_cfg );
    set_socket_options( socket_cfg, client_socket );

    ioctx.run_for( std::chrono::milliseconds( 30 ) );

    bp_entry->bp_send_raw_buf(
        42, opio::net::simple_buffer_t::make_from( { 'h', 'e', 'l', 'l', 'o' } ) );

    ioctx.run_for( std::chrono::milliseconds( 200 ) );

    {
        std::array< char, 16 > buf{};
        const auto n =
            asio_ns::read( client_socket,
                           asio_ns::mutable_buffer{ buf.data(), buf.size() },
                           asio_ns::transfer_at_least( 5 ) );

        ASSERT_EQ( 5, n );
        EXPECT_EQ( std::string{ "hello" }, buf.data() );
    }

    constexpr std::size_t write_blocking_buf_size =
        // Input buffer of remote client's socket doubled as OS
        // alocates twice the requested amount;
        2 * ( 16 * 1024 )

        // Onput buffer of remote client's socket doubled
        // as OS alocates twice the requested amount;
        + 2 * ( 16 * 1024 )

        // A little extra to make write operation wait.
        // On CI we have this not qute predictable, so we add a lot
        // instead of just adding `+1`.
        + 5 * ( 16 * 1024 );

    // Make write operation stuck.
    bp_entry->schedule_send_raw_bufs( opio::net::simple_buffer_t(
        write_blocking_buf_size, static_cast< std::byte >( '@' ) ) );
    ioctx.run_for( std::chrono::milliseconds( 10 ) );

    // Yeach letter would be used as values for bytes in a given "stream".
    const std::string letters = "abcdefghijklmnop";

    // When debugging it might be a good idea to reduce the number of stream to 2:
    // const std::string letters = "ab";

    auto send_multi = [ & ]( std::size_t len ) {
        for( auto i = 0UL; i < letters.size(); ++i )
        {
            bp_entry->bp_send_raw_buf(
                i,
                opio::net::simple_buffer_t(
                    len, static_cast< std::byte >( letters[ i ] ) ) );
        }
    };

    send_multi( 4 );
    ioctx.run_for( std::chrono::milliseconds( 10 ) );

    // ==========================================
    // This would be skipped:
    send_multi( 5 );
    ioctx.run_for( std::chrono::milliseconds( 10 ) );
    send_multi( 6 );
    ioctx.run_for( std::chrono::milliseconds( 10 ) );
    send_multi( 7 );
    ioctx.run_for( std::chrono::milliseconds( 10 ) );
    send_multi( 8 );
    ioctx.run_for( std::chrono::milliseconds( 10 ) );
    send_multi( 9 );
    ioctx.run_for( std::chrono::milliseconds( 10 ) );
    // ==========================================

    // That would make an effect on the socket
    send_multi( 10 );
    ioctx.run_for( std::chrono::milliseconds( 10 ) );

    // Unblock write
    std::thread t{ [ & ] {
        std::vector< char > buf{};
        buf.resize( write_blocking_buf_size );
        const auto n = asio_ns::read(
            client_socket, asio_ns::mutable_buffer{ buf.data(), buf.size() } );

        ASSERT_EQ( write_blocking_buf_size, n );
        const bool block_buffer_content_check = std::all_of(
            buf.begin(), buf.end(), []( auto c ) { return c == '@'; } );
        EXPECT_TRUE( block_buffer_content_check );
    } };

    ioctx.run_for( std::chrono::milliseconds( 1000 ) );
    t.join();

    auto read_and_check_multi = [ & ]( std::size_t len ) {
        opio::net::simple_buffer_t buf( len, static_cast< std::byte >( '1' ) );
        for( auto i = 0UL; i < letters.size(); ++i )
        {
            const auto n = asio_ns::read(
                client_socket, asio_ns::mutable_buffer{ buf.data(), buf.size() } );

            ASSERT_EQ( len, n );
            const bool buffer_content_check =
                std::all_of( begin( buf ), end( buf ), [ & ]( auto c ) {
                    return c == static_cast< std::byte >( letters[ i ] );
                } );
            EXPECT_TRUE( buffer_content_check )
                << "len=" << len << "; buf = [" << buf.make_string_view() << "]";
        }
    };

    read_and_check_multi( 4 );
    read_and_check_multi( 10 );

    bp_entry->close();
    work.reset();
    ioctx.run();
}

}  // anonymous namespace
