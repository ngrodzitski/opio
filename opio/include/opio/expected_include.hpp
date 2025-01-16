/**
 * @file
 *
 * Contains an adoption for std::expected (c++23).
 */
#pragma once

#include <nonstd/expected.hpp>

namespace opio
{

template < typename T, typename E >
using expected_t = nonstd::expected< T, E >;

using nonstd::make_unexpected;

}  // namespace opio
