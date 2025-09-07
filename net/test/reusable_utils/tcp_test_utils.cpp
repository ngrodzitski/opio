#include "tcp_test_utils.hpp"

#include <chrono>

//
// connect_pair()
//

std::uint16_t connect_pair( opio::net::asio_ns::io_context & ioctx,
                            opio::net::asio_ns::ip::tcp::socket & s1,
                            opio::net::asio_ns::ip::tcp::socket & s2 )
{
    int tries = 10;
    while( 0 < tries-- )
    {
        try
        {
            const auto port = make_random_port_value();
            opio::net::asio_ns::ip::tcp::endpoint ep{
                opio::net::asio_ns::ip::make_address( "127.0.0.1" ), port
            };
            opio::net::asio_ns::ip::tcp::acceptor acceptor{ ioctx, ep };

            acceptor.async_accept( [ & ]( [[maybe_unused]] auto ec, auto socket ) {
                assert( !ec );
                s1 = std::move( socket );
            } );

            ioctx.poll();
            s2.connect( ep );
#if defined( OPIO_NET_ASIO_WINDOWS )
            std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
#endif  // !defined( OPIO_NET_ASIO_WINDOWS )

            ioctx.poll();

#if defined( OPIO_NET_ASIO_WINDOWS )
            std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
#endif  // !defined( OPIO_NET_ASIO_WINDOWS )

            ioctx.restart();

            // Return port on success.
            return port;
        }
        catch( ... )
        {
        }
    }

    // We tries 10 times and we fail... smth. wrong give up.
    throw std::runtime_error{ "Can't connect 2 sockets..." };
}

//
// make_random_port_value()
//

std::uint16_t make_random_port_value()
{
    std::uint16_t res = 20'000;

    res += ( std::rand()
             + std::chrono::duration_cast< std::chrono::microseconds >(
                   std::chrono::steady_clock::now().time_since_epoch() )
                   .count() )
           % 42'000;

    return res;
}
