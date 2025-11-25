/**
 * @file
 *
 * This header file contains a class that adds a locking mechanics to
 * asio executors.
 */

#pragma once

#include <thread>

#include <opio/net/asio_include.hpp>

namespace opio::net
{

//
// noop_lock_t
//

struct noop_lock_t
{
public:
    constexpr void lock() const noexcept {}
    constexpr void unlock() const noexcept {}
};

//
// noop_lock_guard_t
//

struct noop_lock_guard_t
{
public:
    template < typename Lock >
    explicit constexpr noop_lock_guard_t( [[maybe_unused]] Lock && lock ) noexcept
    {
    }
};

//
// noop_locking_t
//

struct noop_locking_t
{
    static constexpr bool noop_lock = true;

    using lock_t       = noop_lock_t;
    using lock_guard_t = noop_lock_guard_t;
};

//
// mutex_locking_t
//

struct mutex_locking_t
{
    static constexpr bool noop_lock = false;

    using lock_t       = std::mutex;
    using lock_guard_t = std::lock_guard< lock_t >;
};

}  // namespace opio::net
