#include <chrono>

#include <opio/net/asio_include.hpp>
#include <opio/net/tcp/connection.hpp>

#include <opio/test_utils/test_logger.hpp>

//
// connect_pair()
//

/**
 * @brief Connects two sockets.
 *
 * @return Server port number used for connecting sockets.
 */
std::uint16_t connect_pair( opio::net::asio_ns::io_context & ioctx,
                            opio::net::asio_ns::ip::tcp::socket & s1,
                            opio::net::asio_ns::ip::tcp::socket & s2 );

//
// make_connection()
//

template < typename Connection, typename Input_Handler, typename Socket >
auto make_connection( Socket socket,
                      opio::net::tcp::connection_id_t id,
                      const opio::net::tcp::connection_cfg_t & cfg,
                      opio::logger::logger_t logger,
                      Input_Handler input_handler )
{
    return Connection::make( std::move( socket ), [ & ]( auto & params ) {
        params.connection_id( id )
            .connection_cfg( cfg )
            .logger( std::move( logger ) )
            .input_handler( std::move( input_handler ) );
    } );
}

//
// msec_from_x_to_now()
//

inline auto msec_from_x_to_now(
    std::chrono::steady_clock::time_point starting_from )
{
    return std::chrono::duration_cast< std::chrono::milliseconds >(
               std::chrono::steady_clock::now() - starting_from )
        .count();
}

//
// adjust_for_msvc_if_necessary()
//

inline std::uint64_t adjust_for_msvc_if_necessary( std::uint64_t n )
{
#if defined( OPIO_NET_ASIO_WINDOWS ) \
    || defined( OPIO_NET_CI_ADJUST_FOR_FLAKY_SLOW_RUNNERS )
    return n > 100 ? n + n / 2 : n * 2;
#else   // defined( OPIO_ASIO_WINDOWS ) || defined(
        // OPIO_NET_CI_ADJUST_FOR_FLAKY_SLOW_RUNNERS )
    return n;
#endif  // defined( OPIO_ASIO_WINDOWS ) || defined(
        // OPIO_NET_CI_ADJUST_FOR_FLAKY_SLOW_RUNNERS )
}

//
// make_random_port_value()
//

/**
 * @brief Creates random port number.
 *
 * Used to minimize test fails due to "port already in use".
 */
std::uint16_t make_random_port_value();
