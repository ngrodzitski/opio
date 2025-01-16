#include <opio/net/try_make_addr.hpp>

#include <opio/test_utils/test_logger.hpp>

#include <gtest/gtest.h>

namespace /* anonymous */
{

// NOLINTNEXTLINE
using namespace ::opio::net;
using namespace ::opio::test_utils;

// NOLINTNEXTLINE
TEST( OpioUtils, TryMakeAddr )
{
    auto logger = make_test_logger(
        ::testing::UnitTest::GetInstance()->current_test_info()->name() );

    {
        logger.debug( OPIO_SRC_LOCATION, ">>>>>>>>>> TEST \"lo\"" );
        const auto a = try_make_addr( "lo" );

        ASSERT_TRUE( static_cast< bool >( a ) );
        EXPECT_EQ( a->to_string(), "127.0.0.1" );
        logger.debug( OPIO_SRC_LOCATION, "<<<<<<<<<< TEST \"lo\"" );
    }

    {
        logger.debug( OPIO_SRC_LOCATION, ">>>>>>>>>> TEST \"localhost\"" );
        const auto a = try_make_addr( "localhost" );

        ASSERT_TRUE( static_cast< bool >( a ) );
        EXPECT_EQ( a->to_string(), "127.0.0.1" );
        logger.debug( OPIO_SRC_LOCATION, "<<<<<<<<<< TEST \"localhost\"" );
    }

    {
        logger.debug( OPIO_SRC_LOCATION, ">>>>>>>>>> TEST \"192.0.0.12\"" );
        const auto a = try_make_addr( "192.0.0.12" );

        ASSERT_TRUE( static_cast< bool >( a ) );
        EXPECT_EQ( a->to_string(), "192.0.0.12" );
        logger.debug( OPIO_SRC_LOCATION, "<<<<<<<<<< TEST \"192.0.0.12\"" );
    }

    {
        logger.debug( OPIO_SRC_LOCATION, ">>>>>>>>>> TEST \"SHULD_NOT_EXIST\"" );
        const auto a = try_make_addr( "SHULD_NOT_EXIST" );

        ASSERT_FALSE( static_cast< bool >( a ) );
        logger.debug( OPIO_SRC_LOCATION, "<<<<<<<<<< TEST \"SHULD_NOT_EXIST\"" );
    }
}

}  // anonymous namespace
