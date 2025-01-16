#pragma once

#include <string_view>

#include <opio/expected_include.hpp>
#include <opio/exception.hpp>

#include <opio/net/asio_include.hpp>

namespace opio::net
{

//
// try_make_addr()
//

/**
 * @brief For a given string which might be network interface or
 *        address alias or ip-addr string tries to figure out
 *        an ip addr.
 *
 * @return  ip-addr object if success and an error otherwise.
 */
[[nodiscard]] ::opio::expected_t< asio_ns::ip::address, ::opio::exception_t >
try_make_addr( std::string_view iface_or_addr_str );

}  // namespace opio::net
