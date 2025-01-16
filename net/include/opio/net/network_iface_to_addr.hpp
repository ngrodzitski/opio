#pragma once

#include <string_view>

#include <opio/expected_include.hpp>
#include <opio/exception.hpp>

#include <opio/net/asio_include.hpp>

namespace opio::net
{

//
// network_iface_to_addr()
//

/**
 * @brief For a given network interface finds an ipv4 inet address.
 *
 * @return  A ipv4 address of the network interface if found,
 *          otherwise return error.
 */
[[nodiscard]] expected_t< asio_ns::ip::address, exception_t >
network_iface_to_addr( std::string_view iface_name );

}  // namespace opio::net
