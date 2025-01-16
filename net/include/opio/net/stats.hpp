#pragma once

namespace opio::net
{

//
// noop_stats_driver_t
//

/**
 * @brief Default tratis class for IO statistics concept.
 *
 * Acts as a default definition of a IO statisctics.
 * The implementation does nothing and allows compiler to totally eliminate itself.
 */
struct noop_stats_driver_t
{
    /**
     * @brief Increment incoming traffic counters (async read from socket).
     */
    template < typename... Args >
    constexpr void inc_bytes_rx_async( Args &&... ) const noexcept
    {
    }

    /**
     * @brief Increment incoming traffic counters (sync read from socket).
     *
     * @note This is not used yet as library doesn't do sync reads.
     */
    template < typename... Args >
    constexpr void inc_bytes_rx_sync( Args &&... ) const noexcept
    {
    }

    /**
     * @brief Increment outgoing traffic counters (async write to socket).
     */
    template < typename... Args >
    constexpr void inc_bytes_tx_async( Args &&... ) const noexcept
    {
    }

    /**
     * @brief Increment outgoing traffic counters (sync write to socket).
     */
    template < typename... Args >
    constexpr void inc_bytes_tx_sync( Args &&... ) const noexcept
    {
    }

    /**
     * @brief Experience a would_block error on sync_write.
     */
    template < typename... Args >
    constexpr void hit_would_block_event( Args &&... ) const noexcept
    {
    }

    /**
     * @brief An sync write operation started.
     */
    template < typename... Args >
    constexpr void sync_write_started( Args &&... ) const noexcept
    {
    }

    /**
     * @brief An sync write operation finished.
     */
    template < typename... Args >
    constexpr void sync_write_finished( Args &&... ) const noexcept
    {
    }

    /**
     * @brief An async write operation started.
     */
    template < typename... Args >
    constexpr void async_write_started( Args &&... ) const noexcept
    {
    }

    /**
     * @brief An async write operation finished.
     */
    template < typename... Args >
    constexpr void async_write_finished( Args &&... ) const noexcept
    {
    }
};

}  // namespace opio::net
