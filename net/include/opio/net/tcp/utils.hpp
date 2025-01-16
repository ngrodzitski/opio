/**
 * @file
 *
 * This header file contains some utility functions used by library implementation.
 */

#pragma once

#include <opio/net/asio_include.hpp>

#include <opio/net/tcp/cfg.hpp>

namespace opio::net::tcp
{

//
// set_socket_options()
//

/**
 * @brief Set socket option based on a given config.
 *
 * @tparam Socket Type of socket.
 *
 * @param cfg     Socket options config.
 * @param socket  Target socket.
 */
template < typename Socket >
void set_socket_options( const socket_options_cfg_t & cfg, Socket & socket )
{
    // auto & s = socket.lowest_layer();

    if( cfg.no_delay )
    {
        asio_ns::ip::tcp::no_delay option{ *cfg.no_delay };
        socket.set_option( option );
    }

    if( cfg.keep_alive )
    {
        asio_ns::socket_base::keep_alive option{ *cfg.keep_alive };
        socket.set_option( option );
    }

    if( cfg.linger )
    {
        asio_ns::socket_base::linger option{ true, *cfg.linger };
        socket.set_option( option );
    }

    if( cfg.receive_buffer_size )
    {
        asio_ns::socket_base::receive_buffer_size option{
            *cfg.receive_buffer_size
        };
        socket.set_option( option );
    }

    if( cfg.send_buffer_size )
    {
        asio_ns::socket_base::send_buffer_size option{ *cfg.send_buffer_size };
        socket.set_option( option );
    }
}

}  // namespace opio::net::tcp
