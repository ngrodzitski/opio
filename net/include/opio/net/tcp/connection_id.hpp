/**
 * @file
 *
 * This header file contains connector_t class and some auxiliary routines.
 */

#pragma once

#include <cstdint>

namespace opio::net::tcp
{

/**
 * @brief A type for connection id, which uniquely identify connection instance.
 */
using connection_id_t = std::uint64_t;

}  // namespace opio::net::tcp
