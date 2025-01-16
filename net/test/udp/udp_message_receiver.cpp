#include <opio/net/udp/udp_message_receiver.hpp>

#include <opio/test_utils/test_logger.hpp>

#include <gtest/gtest.h>

namespace /* anonymous */
{

using namespace ::opio::net;         // NOLINT
using namespace ::opio::net::udp;    // NOLINT
using namespace ::opio::test_utils;  // NOLINT

// NOLINTNEXTLINE
TEST( OpioNet, UdpMessageReceiver )
{
    using msg_receiver_t =
        udp_message_receiver_t< std::function< void( udp_raw_message_t ) > >;

    asio_ns::io_context ioctx;

    auto logger = make_test_logger( "UDP" );

    logger.debug( OPIO_SRC_LOCATION, "Creating msg_receiver_t" );

    const asio_ns::ip::udp::endpoint multicast_ep{
        asio::ip::make_address( "224.0.131.132" ), 30001
    };

    std::vector< std::string > received_messages;

    auto msg_receiver = std::make_shared< msg_receiver_t >(
        ioctx,
        "",
        multicast_ep.address(),
        multicast_ep.port(),
        logger,
        [ & ]( auto s ) {
            received_messages.emplace_back(
                reinterpret_cast< const char * >( s.raw_data().data() ),
                s.raw_data().size() );

            logger.info( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                format_to( out,
                           "received ptr={}, sz={}",
                           static_cast< const void * >( s.raw_data().data() ),
                           s.raw_data().size() );
            } );
        } );

    logger.debug( OPIO_SRC_LOCATION, "msg_receiver_t instance created" );

    logger.debug( OPIO_SRC_LOCATION, ">>>>>>>>>> start_listening()" );
    msg_receiver->start_listening();
    logger.debug( OPIO_SRC_LOCATION, "<<<<<<<<<< start_listening()" );

    ioctx.run_for( std::chrono::milliseconds( 10 ) );

    logger.debug( OPIO_SRC_LOCATION, "sending a multicast message" );
    asio_ns::ip::udp::socket sender_socket{ ioctx, multicast_ep.protocol() };

    std::string_view msg1 = "Hello TEST!";
    std::string_view msg2 = "SECOND Hello TEST!";
    sender_socket.send_to( asio_ns::const_buffer( msg1.data(), msg1.size() ),
                           multicast_ep );
    sender_socket.send_to( asio_ns::const_buffer( msg2.data(), msg2.size() ),
                           multicast_ep );

    ioctx.run_for( std::chrono::milliseconds( 15 ) );

    logger.debug( OPIO_SRC_LOCATION, ">>>>>>>>>> stop_listening()" );
    msg_receiver->stop_listening();
    logger.debug( OPIO_SRC_LOCATION, "<<<<<<<<<< stop_listening()" );

    ioctx.run_for( std::chrono::milliseconds( 10 ) );

    ASSERT_EQ( 2, received_messages.size() );
    EXPECT_EQ( msg1, received_messages[ 0 ] );
    EXPECT_EQ( msg2, received_messages[ 1 ] );

    ioctx.run();
}

}  // anonymous namespace
