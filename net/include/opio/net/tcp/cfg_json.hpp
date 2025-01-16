/**
 * @file Contains integration of ipc cfg with json (throuth json_dto).
 */

#pragma once

#include <stdexcept>
#include <sstream>
#include <string>
#include <regex>

#include <json_dto/pub.hpp>

#include <opio/net/tcp/cfg.hpp>

namespace json_dto
{

//
// read_json_value()
//

/**
 * @brief Reader customization for asio_ns::ip::tcp.
 *
 * @see
 * https://github.com/Stiffstream/json_dto#overloading-of-read_json_value-and-write_json_value
 */
template <>
inline void read_json_value( opio::net::asio_ns::ip::tcp & v,
                             const rapidjson::Value & object )
{
    try
    {
        std::string str;
        read_json_value( str, object );

        if( str == "v4" )
        {
            v = opio::net::asio_ns::ip::tcp::v4();
        }
        else if( str == "v6" )
        {
            v = opio::net::asio_ns::ip::tcp::v6();
        }
        else
        {
            throw std::runtime_error{ fmt::format( "unknown protocol '{}'",
                                                   str ) };
        }
    }
    catch( const std::exception & ex )
    {
        throw std::runtime_error{ fmt::format( "unable to read asio_ns::ip::tcp: ",
                                               ex.what() ) };
    }
}

//
// write_json_value()
//

/**
 * @brief Writer customization for asio_ns::ip::tcp.
 *
 * @see
 * https://github.com/Stiffstream/json_dto#overloading-of-read_json_value-and-write_json_value
 */
template <>
inline void write_json_value( const opio::net::asio_ns::ip::tcp & v,
                              rapidjson::Value & object,
                              rapidjson::MemoryPoolAllocator<> & allocator )
{
    std::string representation =
        v == opio::net::asio_ns::ip::tcp::v4() ? "v4" : "v5";
    write_json_value( representation, object, allocator );
}

/**
 * @brief Tcp socket options.
 */
template < typename Json_Io >
void json_io( Json_Io & io, opio::net::tcp::socket_options_cfg_t & cfg )
{
    io & json_dto::optional( "no_delay", cfg.no_delay, std::nullopt )
        & json_dto::optional( "keep_alive", cfg.keep_alive, std::nullopt )
        & json_dto::optional( "linger", cfg.linger, std::nullopt )
        & json_dto::optional(
            "receive_buffer_size", cfg.receive_buffer_size, std::nullopt )
        & json_dto::optional(
            "send_buffer_size", cfg.send_buffer_size, std::nullopt );
}

/**
 * @brief Introduce tcp endpoint parameters.
 */
template < typename Json_Io >
void json_io( Json_Io & io, opio::net::tcp::tcp_endpoint_cfg_t & cfg )
{
    io & json_dto::mandatory( "port", cfg.port )
        & json_dto::optional(
            "host", cfg.host, opio::net::tcp::tcp_endpoint_cfg_t::default_host )
        & json_dto::optional(
            "protocol", cfg.protocol, opio::net::asio_ns::ip::tcp::v4() )
        & json_dto::optional_no_default( "socket_options", cfg.socket_options );
}

} /* namespace json_dto */
