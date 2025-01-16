#pragma once

#include <variant>

#include <opio/net/tcp/cfg.hpp>
#include <opio/net/tcp/connection.hpp>

namespace opio::proto_entry
{

namespace details
{

inline constexpr std::uint32_t default_reconnect_timeout_msec             = 10000;
inline constexpr std::uint32_t default_initiate_heartbeat_timeout_msec    = 10000;
inline constexpr std::uint32_t default_await_heartbeat_reply_timeout_msec = 20000;
inline constexpr std::uint32_t default_max_valid_package_size = 100 * 1024 * 1024;
inline constexpr std::size_t default_input_buffer_size        = 256 * 1024;
inline constexpr std::uint32_t default_write_timeout_per_1mb_msec = 1000;

}  // namespace details

//
// heartbeat_params_t
//

/**
 * @brief Default HB params.
 */
struct heartbeat_params_t
{
    /**
     * @brief A timeout after which to initiate heartbeat request.
     */
    std::chrono::steady_clock::duration initiate_heartbeat_timeout{
        std::chrono::milliseconds(
            details::default_initiate_heartbeat_timeout_msec )
    };

    /**
     * @brief The amount of time to wait for heartbeat reply.
     *
     * When heartbeat request is sent, connection will wait
     * for reply (not necessarly heartbeat reply any incoming bytes count,
     * and connection will be treated as alive). If no incoming activity
     * happens after heartbeat request, connection is considered dead
     * and will be terminated.
     */

    std::chrono::steady_clock::duration await_heartbeat_reply_timeout{
        std::chrono::milliseconds(
            2 * details::default_initiate_heartbeat_timeout_msec )
    };

    std::uint64_t client_app_id{};
};

//
// entry_cfg_t
//

/**
 * @brief A short version of entry's configuration params.
 *
 * Aggregated parameter which affet entry's behavior directly.
 */
struct entry_cfg_t
{
    std::uint32_t max_valid_package_size{
        details::default_max_valid_package_size
    };

    heartbeat_params_t heartbeat;
};

//
//  entry_full_cfg_t
//

/**
 * @brief A full version of proto entry configuration.
 */
struct entry_full_cfg_t
{
    /**
     * @brief Endpoint of the connection.
     *
     * This is either a server endpoint for server role connection
     * or this is a remote endpoint for client connection.
     */
    opio::net::tcp::tcp_endpoint_cfg_t endpoint;

    /**
     * @brief Reconnect timeout (applies to client entry only).
     */
    std::uint32_t reconnect_timeout_msec = details::default_reconnect_timeout_msec;

    /**
     * @brief A timeout after which to initiate heartbeat request.
     */
    std::uint32_t initiate_heartbeat_timeout_msec =
        details::default_initiate_heartbeat_timeout_msec;

    /**
     * @brief The amount of time to wait for heartbeat reply.
     *
     * When heartbeat request is sent, connection will wait
     * for reply (not necessarly heartbeat reply any incoming bytes count,
     * and connection will be treated as alive). If no incoming activity
     * happens after heartbeat request, connection is considered dead
     * and will be terminated.
     */
    std::uint32_t await_heartbeat_reply_timeout_msec =
        details::default_await_heartbeat_reply_timeout_msec;

    /**
     * @brief The maximum allowed package size.
     *
     * If connection faces package header announcing package size
     * which is greater than this value connection will treat it as an error
     * and will terminate.
     */
    std::uint32_t max_valid_package_size = details::default_max_valid_package_size;

    /**
     * @brief Input buffer size.
     *
     * The read operation from socket by default will be supplied with a
     * buffer of a given size.
     */
    std::uint32_t input_buffer_size = details::default_input_buffer_size;

    /**
     * @brief Write operation timeout.
     *
     * The read operation from socket will by default will be supplied with a
     * buffer of a given size.
     */
    std::uint32_t write_timeout_per_1mb_msec =
        details::default_write_timeout_per_1mb_msec;

    [[nodiscard]] opio::net::tcp::connection_cfg_t make_underlying_connection_cfg()
        const noexcept
    {
        opio::net::tcp::connection_cfg_t res;

        res.input_buffer_size( input_buffer_size );
        res.write_timeout_per_1mb(
            std::chrono::milliseconds( write_timeout_per_1mb_msec ) );

        return res;
    }

    [[nodiscard]] entry_cfg_t make_short_cfg() const noexcept
    {
        return entry_cfg_t{
            max_valid_package_size,
            heartbeat_params_t{
                std::chrono::milliseconds( initiate_heartbeat_timeout_msec ),
                std::chrono::milliseconds( initiate_heartbeat_timeout_msec
                                           + await_heartbeat_reply_timeout_msec ) }
        };
    }
};

}  // namespace opio::proto_entry
