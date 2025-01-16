#include <numeric>
#include <iostream>
#include <array>

#include <opio/net/heterogeneous_buffer.hpp>
#include <opio/net/tcp/connection.hpp>

#include <gtest/gtest.h>

#include <opio/test_utils/test_logger.hpp>
#include <tcp_test_utils.hpp>

namespace /* anonymous */
{

using namespace ::opio::net;         // NOLINT
using namespace ::opio::net::tcp;    // NOLINT
using namespace ::opio::test_utils;  // NOLINT

struct connection_traits_st_t : public default_traits_st_t
{
    using buffer_driver_t = heterogeneous_buffer_driver_t;
    using logger_t        = opio::logger::logger_t;
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

TEST( OpioNetTcp, HeteroBufRawConnectionMinimalPingPong )  // NOLINT
{
    connection_cfg_t cfg{};

    buffer_t etalon_buf1 = simple_buffer_t::make_from( { 'a', 'b', 'c', 'd' } );
    buffer_t etalon_buf2 = simple_buffer_t::make_from( { '0', '1', '2', '3' } );
    buffer_t etalon_data_all = etalon_buf1.make_copy();
    etalon_data_all.resize( etalon_data_all.size() + etalon_buf2.size() );
    std::copy( begin( etalon_buf2 ),
               end( etalon_buf2 ),
               etalon_data_all.offset_data( etalon_buf1.size() ) );

    asio_ns::io_context ioctx( 1 );

    opio::net::asio_ns::ip::tcp::socket s1{ ioctx };
    opio::net::asio_ns::ip::tcp::socket s2{ ioctx };

    connect_pair( ioctx, s1, s2 );

    std::string srv_input{};
    std::string cli_input{};

    auto server_conn = make_connection(
        std::move( s1 ),
        0,
        cfg,
        make_test_logger( "SERVER_CONN" ),
        [ & ]( auto & ctx ) {
            ctx.log().info( [ & ]( auto out ) {
                format_to( out, "received: [{}]", ctx.buf().make_string_view() );
            } );
            srv_input += ctx.buf().make_string_view();
            ctx.connection().schedule_send( std::move( ctx.buf() ) );
        } );
    EXPECT_EQ( server_conn->connection_id(), 0 );
    server_conn->start_reading();

    auto client_conn = make_connection(
        std::move( s2 ),
        1,
        cfg,
        make_test_logger( "client_conn" ),
        [ & ]( auto & ctx ) {
            ctx.log().info( [ & ]( auto out ) {
                format_to( out, "received: [{}]", ctx.buf().make_string_view() );
            } );
            cli_input += ctx.buf().make_string_view();

            if( cli_input.size() >= etalon_data_all.size() )
            {
                ctx.connection().shutdown();
            }
        } );
    EXPECT_EQ( client_conn->connection_id(), 1 );
    client_conn->start_reading();
    client_conn->schedule_send( std::move( etalon_buf1 ),
                                std::move( etalon_buf2 ) );

    ioctx.run();
    ASSERT_EQ( etalon_data_all.make_string_view(), srv_input );
    ASSERT_EQ( etalon_data_all.make_string_view(), cli_input );
}

TEST( OpioNetTcp, HeteroBufMultipleBuffs )  // NOLINT
{
    connection_cfg_t cfg{};

    asio_ns::io_context ioctx( 1 );

    bool server_input_handler_happened             = false;
    bool client_input_handler_happened             = false;
    std::size_t server_expected_received_data_size = 0;
    std::string received_data{};

    opio::net::asio_ns::ip::tcp::socket s1{ ioctx };
    opio::net::asio_ns::ip::tcp::socket s2{ ioctx };

    connect_pair( ioctx, s1, s2 );

    auto server_conn = make_connection(
        std::move( s1 ),
        0,
        cfg,
        make_test_logger( "SERVER_CONN" ),
        [ & ]( auto & ctx ) {
            received_data.append( ctx.buf().make_string_view() );

            if( server_expected_received_data_size == received_data.size() )
            {
                EXPECT_EQ( received_data,
                           "!@#$%^&*()"
                           "123"
                           "1234"
                           "12345"
                           "012345678901234567890123456789"
                           "shared-01234567890"
                           "1234" );
                ctx.connection().shutdown();
            }

            server_input_handler_happened = true;
        } );
    server_conn->start_reading();

    auto client_conn =
        make_connection( std::move( s2 ),
                         1,
                         cfg,
                         make_test_logger( "client_conn" ),
                         [ & ]( [[maybe_unused]] auto & input_ctx ) {
                             client_input_handler_happened = true;
                         } );
    client_conn->start_reading();

    ioctx.poll();

    client_conn->schedule_send(
        const_buffer_t{ "!@#$%^&*()", 10 },
        simple_buffer_t::make_from( { '1', '2', '3' } ),
        "1234",
        std::array< char, 5 >{ '1', '2', '3', '4', '5' },
        std::string{ "012345678901234567890123456789" },
        std::make_shared< std::string >( "shared-01234567890" ),
        std::make_shared< simple_buffer_t >(
            simple_buffer_t::make_from( { '1', '2', '3', '4' } ) ) );

    server_expected_received_data_size = 10 + 3 + 4 + 5 + 30 + 18 + 4;

    ioctx.run();
    EXPECT_TRUE( server_input_handler_happened );
    EXPECT_FALSE( client_input_handler_happened );
}

}  // anonymous namespace
