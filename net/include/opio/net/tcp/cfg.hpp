/**
 * @file A vocabulary for config types for tcp routines.
 */

#pragma once

#include <string>
#include <cstdint>
#include <optional>

#include <opio/net/asio_include.hpp>

namespace opio::net::tcp
{

//
//  socket_options_cfg_t
//

/**
 * @brief Tcp socket options.
 */
struct socket_options_cfg_t
{
    std::optional< bool > no_delay;
    std::optional< bool > keep_alive;
    std::optional< int > linger;
    std::optional< int > receive_buffer_size;
    std::optional< int > send_buffer_size;

    /**
     * @brief Tells if option cfg is empty.
     */
    [[nodiscard]] bool is_empty() const noexcept
    {
        const bool has_any_setting =
            no_delay.has_value() || keep_alive.has_value() || linger.has_value()
            || receive_buffer_size.has_value() || send_buffer_size.has_value();

        return !has_any_setting;
    }
};

//
// tcp_resolver_query_t
//

#if OPIO_ASIO_VERSION < 103300  // Asio < 1.33.0

using tcp_resolver_query_t = asio_ns::ip::tcp::resolver::query;

#else
/**
 * @brief A substitute for `asio::ip::tcp::resolver::query`
 *        that was removed in later versions.
 */
struct tcp_resolver_query_t
{
    net::asio_ns::ip::tcp protocol{ net::asio_ns::ip::tcp::v4() };
    std::string host;
    std::string port;

    std::string host_name() const { return host; }
    std::string service_name() const { return port; }
};
#endif

//
//  tcp_endpoint_cfg_t
//

/**
 * @brief Tcp endpoint parameters.
 */
struct tcp_endpoint_cfg_t
{
    inline static net::asio_ns::ip::tcp default_protocol =
        net::asio_ns::ip::tcp::v4();
    net::asio_ns::ip::tcp protocol{ default_protocol };

    static constexpr std::string_view default_host{ "localhost" };
    std::string host{ default_host };

    std::uint16_t port{};
    socket_options_cfg_t socket_options{};

    tcp_resolver_query_t make_query() const
    {
        std::string target_host = host.empty() ? "127.0.0.1" : host;
        return { protocol, target_host, std::to_string( port ) };
    }

    asio_ns::ip::tcp::endpoint make_endpoint() const
    {
        if( host.empty() )
        {
            return { protocol, port };
        }

        if( host == "localhost" )
        {
            return { asio_ns::ip::make_address( "127.0.0.1" ), port };
        }

        if( host == "ip6-localhost" )
        {
            return { asio_ns::ip::make_address( "::1" ), port };
        }

        return { asio_ns::ip::make_address( host ), port };
    }

    std::string get_real_host() const noexcept
    {
        if( host.empty() || host == "127.0.0.1" || host == "0.0.0.0" )
            return asio_ns::ip::host_name();

        return host;
    }
};

}  // namespace opio::net::tcp

namespace fmt
{

template <>
struct formatter< opio::net::tcp::socket_options_cfg_t >
{
    template < class Parse_Context >
    constexpr auto parse( Parse_Context & ctx ) const
    {
        auto it  = std::begin( ctx );
        auto end = std::end( ctx );
        if( it != end && *it != '}' ) throw fmt::format_error( "invalid format" );
        return it;
    }

    template < class Format_Context >
    auto format( const opio::net::tcp::socket_options_cfg_t & options_cfg,
                 Format_Context & ctx ) const
    {
        if( options_cfg.is_empty() )
        {
            return fmt::format_to( ctx.out(), "<empty>" );
        }

        auto out = fmt::format_to( ctx.out(), "[" );

        auto format_if_necessary = [ &out, is_first = true ](
                                       std::string_view name,
                                       const auto & op ) mutable {
            if( !op )
            {
                return;
            }

            if( is_first )
            {
                is_first = false;
                out      = fmt::format_to( out, "{}: {}", name, *op );
                return;
            }

            out = fmt::format_to( out, ", {}: {}", name, *op );
        };

        format_if_necessary( "no_delay", options_cfg.no_delay );
        format_if_necessary( "keep_alive", options_cfg.keep_alive );
        format_if_necessary( "linger", options_cfg.linger );
        format_if_necessary( "receive_buffer_size",
                             options_cfg.receive_buffer_size );
        format_if_necessary( "send_buffer_size", options_cfg.send_buffer_size );

        return fmt::format_to( out, "]" );
    }
};

}  // namespace fmt
