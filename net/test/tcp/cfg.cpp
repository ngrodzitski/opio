#include <opio/net/tcp/cfg.hpp>

#include <gtest/gtest.h>

namespace /* anonymous */
{
using namespace ::opio::net;       // NOLINT
using namespace ::opio::net::tcp;  // NOLINT

TEST( OpioNetTcp, CfgSocketOptionsIsEmpty )  // NOLINT
{
    {
        const socket_options_cfg_t cfg{};

        EXPECT_TRUE( cfg.is_empty() );
    }

    {
        socket_options_cfg_t cfg{};
        cfg.no_delay            = true;
        cfg.keep_alive          = true;
        cfg.linger              = 5;     // NOLINT
        cfg.receive_buffer_size = 4096;  // NOLINT
        cfg.send_buffer_size    = 512;   // NOLINT

        EXPECT_FALSE( cfg.is_empty() );
    }

    {
        socket_options_cfg_t cfg{};
        cfg.keep_alive          = true;
        cfg.receive_buffer_size = 4096;  // NOLINT

        EXPECT_FALSE( cfg.is_empty() );
    }
}

TEST( OpioNetTcp, CfgSocketOptionsFmt )  // NOLINT
{
    {
        const socket_options_cfg_t cfg{};

        EXPECT_EQ( "<empty>", fmt::format( "{}", cfg ) );
    }

    {
        socket_options_cfg_t cfg{};
        cfg.no_delay                  = true;
        cfg.keep_alive                = true;
        cfg.linger                    = 5;     // NOLINT
        cfg.receive_buffer_size       = 4096;  // NOLINT
        cfg.send_buffer_size          = 512;   // NOLINT
        std::string_view expected_res = "["
                                        "no_delay: true, "
                                        "keep_alive: true, "
                                        "linger: 5, "
                                        "receive_buffer_size: 4096, "
                                        "send_buffer_size: 512"
                                        "]";

        EXPECT_EQ( expected_res, fmt::format( "{}", cfg ) );
    }

    {
        socket_options_cfg_t cfg{};
        cfg.keep_alive          = true;
        cfg.receive_buffer_size = 4096;  // NOLINT

        EXPECT_EQ( "["
                   "keep_alive: true, "
                   "receive_buffer_size: 4096"
                   "]",
                   fmt::format( "{}", cfg ) );
    }
}

TEST( OpioNetTcp, CfgEndpointMakeQuery )  // NOLINT
{
    {
        tcp_endpoint_cfg_t cfg{};
        cfg.port = 8089;
        ASSERT_EQ( cfg.make_query().host_name(), "localhost" );
        ASSERT_EQ( cfg.make_query().service_name(), "8089" );
    }
    {
        tcp_endpoint_cfg_t cfg{};
        cfg.port = 8090;
        cfg.host = "";
        ASSERT_EQ( cfg.make_query().host_name(), "127.0.0.1" );
        ASSERT_EQ( cfg.make_query().service_name(), "8090" );
    }
    {
        tcp_endpoint_cfg_t cfg{};
        cfg.port = 8091;
        cfg.host = "192.168.100.11";
        ASSERT_EQ( cfg.make_query().host_name(), "192.168.100.11" );
        ASSERT_EQ( cfg.make_query().service_name(), "8091" );
    }
}

TEST( OpioNetTcp, CfgEndpointMakeEndpoint )  // NOLINT
{
    {
        tcp_endpoint_cfg_t cfg{};
        cfg.port = 8089;
        ASSERT_EQ( cfg.make_endpoint().port(), 8089 );
        ASSERT_EQ( cfg.make_endpoint().protocol(), asio_ns::ip::tcp::v4() );
    }
    {
        tcp_endpoint_cfg_t cfg{};
        cfg.port = 8090;
        cfg.host = "";
        ASSERT_EQ( cfg.make_endpoint().port(), 8090 );
        ASSERT_EQ( cfg.make_endpoint().protocol(), asio_ns::ip::tcp::v4() );
    }
    {
        tcp_endpoint_cfg_t cfg{};
        cfg.port = 8091;
        cfg.host = "192.168.100.11";
        ASSERT_EQ( cfg.make_endpoint().port(), 8091 );
        ASSERT_EQ( cfg.make_endpoint().protocol(), asio_ns::ip::tcp::v4() );
    }
    {
        tcp_endpoint_cfg_t cfg{};
        cfg.port = 8092;
        cfg.host = "ip6-localhost";
        ASSERT_EQ( cfg.make_endpoint().port(), 8092 );
        ASSERT_EQ( cfg.make_endpoint().protocol(), asio_ns::ip::tcp::v6() );
    }
    {
        tcp_endpoint_cfg_t cfg{};
        cfg.port = 8093;
        cfg.host = "::1";
        ASSERT_EQ( cfg.make_endpoint().port(), 8093 );
        ASSERT_EQ( cfg.make_endpoint().protocol(), asio_ns::ip::tcp::v6() );
    }
    {
        tcp_endpoint_cfg_t cfg{};
        cfg.port = 8094;
        cfg.host = "fe80::5fea:8bb5:c2ce:84c8";
        ASSERT_EQ( cfg.make_endpoint().port(), 8094 );
        ASSERT_EQ( cfg.make_endpoint().protocol(), asio_ns::ip::tcp::v6() );
    }
}

}  // anonymous namespace
