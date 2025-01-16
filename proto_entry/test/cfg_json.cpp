#include <opio/proto_entry/cfg_json.hpp>

#include <gtest/gtest.h>

namespace /* anonymous */
{

using namespace opio::proto_entry;       // NOLINT
namespace asio_ns = opio::net::asio_ns;  // NOLINT

TEST( OpioProtoEntry, Cfg )  // NOLINT
{
    const auto cfg = json_dto::from_json< entry_full_cfg_t >( R"-({
        "endpoint" : {
            "port" : 1234,
            "host" : "jazz2",
            "protocol" : "v4"
        },
        "socket_options" : {},
        "reconnect_timeout_msec" : 1111,
        "initiate_heartbeat_timeout_msec" : 9999,
        "await_heartbeat_reply_timeout_msec" : 7777,
        "max_valid_package_size" : 8000000,
        "input_buffer_size" :      8000000,
        "write_timeout_per_1mb_msec" : 3333
    })-" );

    EXPECT_EQ( cfg.endpoint.port, 1234 );
    EXPECT_EQ( cfg.endpoint.host, "jazz2" );
    EXPECT_EQ( cfg.endpoint.protocol, asio_ns::ip::tcp::v4() );

    EXPECT_EQ( cfg.reconnect_timeout_msec, 1111 );
    EXPECT_EQ( cfg.initiate_heartbeat_timeout_msec, 9999 );
    EXPECT_EQ( cfg.await_heartbeat_reply_timeout_msec, 7777 );
    EXPECT_EQ( cfg.max_valid_package_size, 8000000 );
    EXPECT_EQ( cfg.input_buffer_size, 8000000 );
    EXPECT_EQ( cfg.write_timeout_per_1mb_msec, 3333 );
}

TEST( OpioProtoEntry, CfgEmpty )  // NOLINT
{
    const auto cfg = json_dto::from_json< entry_full_cfg_t >( R"-({
        "endpoint" : {
            "port" : 1234,
            "host" : "jazz2",
            "protocol" : "v4"
        }
    })-" );

    EXPECT_EQ( cfg.endpoint.port, 1234 );
    EXPECT_EQ( cfg.endpoint.host, "jazz2" );
    EXPECT_EQ( cfg.endpoint.protocol, asio_ns::ip::tcp::v4() );

    EXPECT_EQ( cfg.reconnect_timeout_msec,
               details::default_reconnect_timeout_msec );
    EXPECT_EQ( cfg.initiate_heartbeat_timeout_msec,
               details::default_initiate_heartbeat_timeout_msec );
    EXPECT_EQ( cfg.await_heartbeat_reply_timeout_msec,
               details::default_await_heartbeat_reply_timeout_msec );
    EXPECT_EQ( cfg.max_valid_package_size,
               details::default_max_valid_package_size );
    EXPECT_EQ( cfg.input_buffer_size, details::default_input_buffer_size );
    EXPECT_EQ( cfg.write_timeout_per_1mb_msec,
               details::default_write_timeout_per_1mb_msec );
}

}  // anonymous namespace
