#pragma once

#include <string>
#include <optional>
#include <cstdint>

#include <opio/net/asio_include.hpp>

namespace opio::net::udp
{

//
// udp_receiver_cfg_t
//

/**
 * @brief Defines udp multicast client
 */
struct udp_receiver_cfg_t
{
    /**
     * @brief Listen on address or network iface.
     */
    std::string listen_on;

    std::string multicast_address;
    std::uint16_t multicast_port;
};

}  // namespace opio::net::udp
