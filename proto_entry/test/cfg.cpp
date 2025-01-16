#include <opio/proto_entry/cfg_json.hpp>

#include <gtest/gtest.h>

namespace /* anonymous */
{

using namespace opio::proto_entry;       // NOLINT
namespace asio_ns = opio::net::asio_ns;  // NOLINT

TEST( OpioProtoEntry, CfgMakeShortCfg )  // NOLINT
{
    entry_full_cfg_t cfg;

    cfg.max_valid_package_size             = 900;   // NOLINT
    cfg.initiate_heartbeat_timeout_msec    = 2500;  // NOLINT
    cfg.await_heartbeat_reply_timeout_msec = 3200;  // NOLINT

    const auto s = cfg.make_short_cfg();

    EXPECT_EQ( cfg.max_valid_package_size, s.max_valid_package_size );
    EXPECT_EQ( cfg.initiate_heartbeat_timeout_msec,
               std::chrono::duration_cast< std::chrono::milliseconds >(
                   s.heartbeat.initiate_heartbeat_timeout )
                   .count() );
    EXPECT_EQ( cfg.initiate_heartbeat_timeout_msec
                   + cfg.await_heartbeat_reply_timeout_msec,
               std::chrono::duration_cast< std::chrono::milliseconds >(
                   s.heartbeat.await_heartbeat_reply_timeout )
                   .count() );
}

TEST( OpioProtoEntry, CfgMakeUnderlyingConnectionCfg )  // NOLINT
{
    entry_full_cfg_t cfg;

    cfg.input_buffer_size          = 123;   // NOLINT
    cfg.write_timeout_per_1mb_msec = 9999;  // NOLINT

    const auto s = cfg.make_underlying_connection_cfg();

    EXPECT_EQ( cfg.input_buffer_size, s.input_buffer_size() );
    EXPECT_EQ( cfg.write_timeout_per_1mb_msec,
               std::chrono::duration_cast< std::chrono::milliseconds >(
                   s.write_timeout_per_1mb() )
                   .count() );
}

}  // anonymous namespace
