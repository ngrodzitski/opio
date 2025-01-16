#include <opio/net/tcp/cfg_json.hpp>

#include <gtest/gtest.h>

namespace /* anonymous */
{
using namespace ::opio::net;       // NOLINT
using namespace ::opio::net::tcp;  // NOLINT

TEST( OpioNetTcp, CfgSocketOptions )  // NOLINT
{
    {
        const auto cfg = json_dto::from_json< socket_options_cfg_t >( "{}" );

        EXPECT_FALSE( cfg.no_delay );
        EXPECT_FALSE( cfg.keep_alive );
        EXPECT_FALSE( cfg.linger );
        EXPECT_FALSE( cfg.receive_buffer_size );
        EXPECT_FALSE( cfg.send_buffer_size );
    }

    {
        const auto cfg = json_dto::from_json< socket_options_cfg_t >( R"-({
            "no_delay" : true,
            "keep_alive": true,
            "linger": 5,
            "receive_buffer_size": 4096,
            "send_buffer_size": 512
        })-" );

        ASSERT_TRUE( cfg.no_delay );
        ASSERT_TRUE( cfg.keep_alive );
        ASSERT_TRUE( cfg.linger );
        ASSERT_TRUE( cfg.receive_buffer_size );
        ASSERT_TRUE( cfg.send_buffer_size );

        EXPECT_TRUE( cfg.no_delay.value() );
        EXPECT_TRUE( cfg.keep_alive.value() );
        EXPECT_EQ( cfg.linger.value(), 5 );
        EXPECT_EQ( cfg.receive_buffer_size, 4096 );
        EXPECT_EQ( cfg.send_buffer_size, 512 );
        EXPECT_EQ( cfg.send_buffer_size, 512 );
    }
}

TEST( OpioNetTcp, CfgSocketOptionsReadFail )  // NOLINT
{
    {
        // Invalid no_delay type.
        auto code = []() {
            [[maybe_unused]] const auto cfg =
                json_dto::from_json< socket_options_cfg_t >( R"-({
                    "no_delay" : ""
                })-" );
        };

        EXPECT_ANY_THROW( code() );
    }
    {
        // Invalid keep_alive type.
        auto code = []() {
            [[maybe_unused]] const auto cfg =
                json_dto::from_json< socket_options_cfg_t >( R"-({
                    "keep_alive": 12
                })-" );
        };

        EXPECT_ANY_THROW( code() );
    }
    {
        // Invalid linger type.
        auto code = []() {
            [[maybe_unused]] const auto cfg =
                json_dto::from_json< socket_options_cfg_t >( R"-({
                    "linger": "22"
                })-" );
        };

        EXPECT_ANY_THROW( code() );
    }
    {
        // Invalid receive_buffer_size type
        auto code = []() {
            [[maybe_unused]] const auto cfg =
                json_dto::from_json< socket_options_cfg_t >( R"-({
                    "receive_buffer_size": "1234"
                })-" );
        };

        EXPECT_ANY_THROW( code() );
    }
    {
        // Invalid send_buffer_size type
        auto code = []() {
            [[maybe_unused]] const auto cfg =
                json_dto::from_json< socket_options_cfg_t >( R"-({
                    "send_buffer_size": false
                })-" );
        };

        EXPECT_ANY_THROW( code() );
    }
}

TEST( OpioNetTcp, CfgEnpoint )  // NOLINT
{
    {
        const auto cfg = json_dto::from_json< tcp_endpoint_cfg_t >( R"-({
            "port" : 1234,
            "host" : "jazz2",
            "protocol" : "v4",
            "socket_options": {
                "no_delay" : true,
                "keep_alive": true,
                "linger": 50,
                "receive_buffer_size": 8192,
                "send_buffer_size": 8192
            }

        })-" );

        EXPECT_EQ( cfg.port, 1234 );
        EXPECT_EQ( cfg.host, "jazz2" );
        EXPECT_EQ( cfg.protocol, asio_ns::ip::tcp::v4() );

        EXPECT_TRUE( cfg.socket_options.no_delay.value() );
        EXPECT_TRUE( cfg.socket_options.keep_alive.value() );
        EXPECT_EQ( cfg.socket_options.linger.value(), 50 );
        EXPECT_EQ( cfg.socket_options.receive_buffer_size, 8192 );
        EXPECT_EQ( cfg.socket_options.send_buffer_size, 8192 );
    }

    {
        const auto cfg = json_dto::from_json< tcp_endpoint_cfg_t >( R"-({
            "port" : 1122,
            "host" : "funk7"
        })-" );

        EXPECT_EQ( cfg.port, 1122 );
        EXPECT_EQ( cfg.host, "funk7" );
        EXPECT_EQ( cfg.protocol, asio_ns::ip::tcp::v4() );
        EXPECT_TRUE( cfg.socket_options.is_empty() );
    }

    {
        const auto cfg = json_dto::from_json< tcp_endpoint_cfg_t >( R"-({
            "port" : 33322,
            "host" : "::1",
            "protocol" : "v6"
        })-" );

        EXPECT_EQ( cfg.port, 33322 );
        EXPECT_EQ( cfg.host, "::1" );
        EXPECT_EQ( cfg.protocol, asio_ns::ip::tcp::v6() );

        EXPECT_TRUE( cfg.socket_options.is_empty() );
    }
}

TEST( OpioNetTcp, CfgEnpointReadFail )  // NOLINT
{
    {
        // Invalid port type.
        auto code = []() {
            [[maybe_unused]] const auto cfg =
                json_dto::from_json< tcp_endpoint_cfg_t >( R"-({
                    "port" : "123",
                    "host" : "jazz2",
                    "protocol" : "v4"
                })-" );
        };

        EXPECT_ANY_THROW( code() );
    }
    {
        // Invalid port value.
        auto code = []() {
            [[maybe_unused]] const auto cfg =
                json_dto::from_json< tcp_endpoint_cfg_t >( R"-({
                    "port" : 99999,
                    "host" : "jazz2",
                    "protocol" : "v4"
                })-" );
        };

        EXPECT_ANY_THROW( code() );
    }
    {
        // Invalid host type.
        auto code = []() {
            [[maybe_unused]] const auto cfg =
                json_dto::from_json< tcp_endpoint_cfg_t >( R"-({
                    "port" : 1234,
                    "host" : 11,
                    "protocol" : "v4"
                })-" );
        };

        EXPECT_ANY_THROW( code() );
    }
    {
        // Invalid protocol value.
        auto code = []() {
            [[maybe_unused]] const auto cfg =
                json_dto::from_json< tcp_endpoint_cfg_t >( R"-({
                    "port" : 1234,
                    "host" : "localhost",
                    "protocol" : "v8"
                })-" );
        };

        EXPECT_ANY_THROW( code() );
    }
    {
        // Invalid protocol value (not a string).
        auto code = []() {
            [[maybe_unused]] const auto cfg =
                json_dto::from_json< tcp_endpoint_cfg_t >( R"-({
                    "port" : 1234,
                    "host" : "localhost",
                    "protocol" : 4
                })-" );
        };

        EXPECT_ANY_THROW( code() );
    }
    {
        // Port is mandatory parameter.
        auto code = []() {
            [[maybe_unused]] const auto cfg =
                json_dto::from_json< tcp_endpoint_cfg_t >( R"-({
                    "host" : "localhost",
                    "protocol" : "v6"
                })-" );
        };

        EXPECT_ANY_THROW( code() );
    }
}

}  // anonymous namespace
