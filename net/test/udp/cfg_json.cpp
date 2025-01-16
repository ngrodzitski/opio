#include <opio/net/udp/cfg_json.hpp>

#include <opio/test_utils/test_read_config.hpp>

#include <gtest/gtest.h>

namespace /* anonymous */
{

// NOLINTNEXTLINE
using namespace ::opio::net::udp;

// NOLINTNEXTLINE
TEST( OpioCfg, UdpReceiverCfg )
{
    {
        const auto cfg = opio::test_utils::test_read_config< udp_receiver_cfg_t >(
            R"-({
                "listen_on" : "192.168.178.12",
                "multicast_address" : "204.0.11.130",
                "multicast_port" : 4321,
            })-" );

        EXPECT_EQ( cfg.listen_on, "192.168.178.12" );
        EXPECT_EQ( cfg.multicast_address, "204.0.11.130" );
        EXPECT_EQ( cfg.multicast_port, 4321 );
    }
}

}  // anonymous namespace
