#include <numeric>
#include <iostream>

#include <opio/net/tcp/connection.hpp>

#include <gtest/gtest.h>

#include <opio/test_utils/test_logger.hpp>

namespace /* anonymous */
{

using namespace ::opio::net;         // NOLINT
using namespace ::opio::net::tcp;    // NOLINT
using namespace ::opio::test_utils;  // NOLINT

// Some tagged types for tests:

struct test_logger_t : public opio::logger::noop_logger_t
{
    test_logger_t( int t )
        : tag{ t }
    {
    }

    int tag;
};

struct test_buffer_driver_t : public simple_buffer_driver_t
{
    test_buffer_driver_t()
        : tag{ 999 }  // NOLINT
    {
    }

    test_buffer_driver_t( int t )
        : tag{ t }
    {
    }

    int tag;
};

struct test_stats_driver_t : public noop_stats_driver_t
{
    test_stats_driver_t()
        : tag{ -128 }  // NOLINT
    {
    }

    test_stats_driver_t( int t )
        : tag{ t }
    {
    }

    int tag;
};

struct test_input_handler_t
{
    test_input_handler_t( int t )
        : tag{ t }
    {
    }

    template < typename Ctx >
    void operator()( [[maybe_unused]] Ctx & ctx )
    {
    }

    int tag;
};

struct connection_traits_st_t : public default_traits_st_t
{
    using logger_t        = test_logger_t;
    using buffer_driver_t = test_buffer_driver_t;
    using input_handler_t = test_input_handler_t;
    using stats_driver_t  = test_stats_driver_t;
};

using connection_t  = opio::net::tcp::connection_t< connection_traits_st_t >;
using ctor_params_t = typename connection_t::ctor_params_t;

TEST( OpioNetTcp, CtorParamsId )  // NOLINT
{
    {
        constexpr auto id = 101;
        auto params       = ctor_params_t{}.connection_id( id );
        EXPECT_EQ( params.connection_id(), id );
    }

    {
        constexpr auto id = 999;
        auto params       = ctor_params_t{}.connection_id( id );
        EXPECT_EQ( params.connection_id(), id );
    }
}

TEST( OpioNetTcp, CtorParamsCfg )  // NOLINT
{
    constexpr std::size_t sz   = 10099;
    constexpr std::size_t secs = 13;

    auto params = ctor_params_t{}.connection_cfg(
        connection_cfg_t{}.input_buffer_size( sz ).write_timeout_per_1mb(
            std::chrono::seconds( secs ) ) );

    EXPECT_EQ( params.connection_cfg().input_buffer_size(), sz );
    EXPECT_EQ( params.connection_cfg().write_timeout_per_1mb(),
               std::chrono::seconds( secs ) );
}

TEST( OpioNetTcp, CtorParamsLogger )  // NOLINT
{
    {
        auto params = ctor_params_t{}.logger( test_logger_t{ 111222 } );

        EXPECT_EQ( params.logger_giveaway().tag, 111222 );
    }

    {
        ctor_params_t params{};

        EXPECT_ANY_THROW( auto another_logger = params.logger_giveaway() );
    }
}

TEST( OpioNetTcp, CtorParamsBufferDriver )  // NOLINT
{
    {
        constexpr auto tag = 42;
        auto params        = ctor_params_t{}.buffer_driver( { tag } );

        EXPECT_EQ( params.buffer_driver_giveaway().tag, tag );
    }

    {
        ctor_params_t params{};

        EXPECT_EQ( params.buffer_driver_giveaway().tag, 999 );
    }
}

TEST( OpioNetTcp, CtorParamsInputHandler )  // NOLINT
{
    {
        auto params = ctor_params_t{}.input_handler( { 321 } );

        EXPECT_EQ( params.input_handler_giveaway().tag, 321 );
    }

    {
        ctor_params_t params{};

        EXPECT_ANY_THROW( [[maybe_unused]] auto another_input_handler =
                              params.input_handler_giveaway() );
    }
}

TEST( OpioNetTcp, CtorParamsShutdownHandler )  // NOLINT
{
    {
        auto params = ctor_params_t{}.shutdown_handler( []( auto /*reason*/ ) {} );

        auto h = params.shutdown_handler_giveaway();
        EXPECT_TRUE( h );
    }

    {
        ctor_params_t params{};

        auto h = params.shutdown_handler_giveaway();
        EXPECT_FALSE( h );
    }
}

TEST( OpioNetTcp, CtorParamsStatsDriver )  // NOLINT
{
    {
        auto params = ctor_params_t{}.stats_driver( { 42 } );

        EXPECT_EQ( params.stats_driver_giveaway().tag, 42 );
    }

    {
        ctor_params_t params{};

        EXPECT_EQ( params.stats_driver_giveaway().tag, -128 );
    }
}

}  // anonymous namespace
