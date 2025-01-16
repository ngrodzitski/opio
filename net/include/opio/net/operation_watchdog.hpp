#pragma once

#include <chrono>
#include <memory>
#include <cstdint>

#include <opio/net/asio_include.hpp>

namespace opio::net
{

//
// noop_operation_watchdog_t
//

/**
 * @brief A noop operation watchdog.
 *
 * Acts as one of a customization point for `connection_t<Traits>` class.
 * Effectively collapses any watchdog activity to nothing by
 * declaring all relevant functions as constexr functions with empty
 * implementations which makes it easy for compiler to optimize them out.
 */
struct noop_operation_watchdog_t
{
    using timeout_event_key_t = std::uint64_t;

    template < typename... Args >
    constexpr noop_operation_watchdog_t( Args &&... ) noexcept
    {
    }

    template < typename... Args >
    constexpr void start_watch_operation( Args &&... ) const noexcept
    {
    }

    template < typename... Args >
    constexpr void cancel_watch_operation( Args &&... ) const noexcept
    {
    }

    timeout_event_key_t timeout_key() const noexcept { return 0; };
};

//
// asio_timer_operation_watchdog_t
//

/**
 * @brief Watchdog based on asio timers.
 *
 * Acts as one of a customization point for `connection_t<Traits>` class.
 * Uses asio timer routine to perform check scheduling.
 */
class asio_timer_operation_watchdog_t final
{
public:
    using timeout_event_key_t = std::uint64_t;

    asio_timer_operation_watchdog_t( asio_ns::any_io_executor executor )
        : m_operation_timer{ std::move( executor ) }
    {
    }

    template < typename Executor >
    asio_timer_operation_watchdog_t( Executor & executor )
        : m_operation_timer{ executor }
    {
    }

    asio_timer_operation_watchdog_t( const asio_timer_operation_watchdog_t & ) =
        delete;
    asio_timer_operation_watchdog_t( asio_timer_operation_watchdog_t && ) =
        default;
    asio_timer_operation_watchdog_t & operator    =(
        const asio_timer_operation_watchdog_t & ) = delete;
    asio_timer_operation_watchdog_t & operator    =(
        asio_timer_operation_watchdog_t && ) = default;

    ~asio_timer_operation_watchdog_t()
    {
        try
        {
            cancel_watch_operation();
        }
        catch( ... )
        {
        }
    }

    /**
     * @brief Start watching operation.
     */
    template < typename Callback >
    void start_watch_operation( std::chrono::steady_clock::duration timeout,
                                Callback cb )
    {
        m_operation_timer.expires_after( timeout );
        m_operation_timer.async_wait(
            // Memo `timeout_event_key` to make sure we do not
            // trigger on cancelled timer (watch_id_source
            // holding a counter would heve a different value)
            [ timeout_key = ++m_timeout_key,
              timeout_cb  = std::move( cb ) ]( const auto & ec ) {
                if( !ec )
                {
                    timeout_cb( timeout_key );
                }
            } );
    }

    /**
     * @brief Get any scheduled check.
     */
    void cancel_watch_operation()
    {
        m_operation_timer.cancel();
        ++m_timeout_key;
    }

    /**
     * @brief Get current timeout key.
     */
    [[nodiscard]] timeout_event_key_t timeout_key() const noexcept
    {
        return m_timeout_key;
    };

private:
    asio_ns::steady_timer m_operation_timer;
    timeout_event_key_t m_timeout_key{};
};

}  // namespace opio::net
