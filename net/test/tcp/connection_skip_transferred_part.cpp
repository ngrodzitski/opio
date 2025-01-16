#include <numeric>
#include <iostream>

#include <opio/net/tcp/connection.hpp>

#include <gtest/gtest.h>

namespace /* anonymous */
{

using namespace ::opio::net;       // NOLINT
using namespace ::opio::net::tcp;  // NOLINT

class OpioIpcTcpDetailsSkipTransferredPart : public ::testing::Test
{
protected:
    void SetUp() override
    {
        sample_data.assign( 500, '!' );
        bufs_container.emplace_back( sample_data.data(), 100 );
        bufs_container.emplace_back( sample_data.data(), 200 );
        bufs_container.emplace_back( sample_data.data(), 300 );
        bufs_container.emplace_back( sample_data.data(), 200 );
        bufs_container.emplace_back( sample_data.data(), 100 );

        initial_span = { bufs_container.data(), bufs_container.size() };
    }

    std::string sample_data;
    std::vector< asio_ns::const_buffer > bufs_container;
    details::buf_descriptors_span_t initial_span;
};

#define OPIO_IPC_UTEST_CMP_BUF( x, y )       \
    EXPECT_EQ( ( x ).data(), ( y ).data() ); \
    EXPECT_EQ( ( x ).size(), ( y ).size() );

TEST_F( OpioIpcTcpDetailsSkipTransferredPart, ZeroTransferred )  // NOLINT
{
    auto res = details::skip_transferred_part( initial_span, 0 );
    ASSERT_EQ( res.size(), initial_span.size() );
    OPIO_IPC_UTEST_CMP_BUF( res[ 0 ], initial_span[ 0 ] );
    OPIO_IPC_UTEST_CMP_BUF( res[ 1 ], initial_span[ 1 ] );
    OPIO_IPC_UTEST_CMP_BUF( res[ 2 ], initial_span[ 2 ] );
    OPIO_IPC_UTEST_CMP_BUF( res[ 3 ], initial_span[ 3 ] );
    OPIO_IPC_UTEST_CMP_BUF( res[ 4 ], initial_span[ 4 ] );
}

TEST_F( OpioIpcTcpDetailsSkipTransferredPart,
        TransferedPartOfFirstBuf )  // NOLINT
{
    auto res = details::skip_transferred_part( initial_span, 42 );
    ASSERT_EQ( res.size(), 5 );

    EXPECT_EQ( res[ 0 ].data(), &sample_data[ 42 ] );
    EXPECT_EQ( res[ 0 ].size(), 58 );

    // The rest is as before:
    OPIO_IPC_UTEST_CMP_BUF( res[ 1 ], initial_span[ 1 ] );
    OPIO_IPC_UTEST_CMP_BUF( res[ 2 ], initial_span[ 2 ] );
    OPIO_IPC_UTEST_CMP_BUF( res[ 3 ], initial_span[ 3 ] );
    OPIO_IPC_UTEST_CMP_BUF( res[ 4 ], initial_span[ 4 ] );
}

TEST_F( OpioIpcTcpDetailsSkipTransferredPart,
        TransferedFullFirstBuf )  // NOLINT
{
    auto res = details::skip_transferred_part( initial_span, 100 );
    ASSERT_EQ( res.size(), 4 );

    OPIO_IPC_UTEST_CMP_BUF( res[ 0 ], initial_span[ 1 ] );
    OPIO_IPC_UTEST_CMP_BUF( res[ 1 ], initial_span[ 2 ] );
    OPIO_IPC_UTEST_CMP_BUF( res[ 2 ], initial_span[ 3 ] );
    OPIO_IPC_UTEST_CMP_BUF( res[ 3 ], initial_span[ 4 ] );
}

TEST_F( OpioIpcTcpDetailsSkipTransferredPart,
        TransferedBeforeMiddleOfSecondBuf )  // NOLINT
{
    auto res = details::skip_transferred_part( initial_span, 111 );
    ASSERT_EQ( res.size(), 4 );

    OPIO_IPC_UTEST_CMP_BUF( res[ 0 ], initial_span[ 1 ] );
    EXPECT_EQ( res[ 0 ].data(), &sample_data[ 11 ] );
    EXPECT_EQ( res[ 0 ].size(), 189 );

    OPIO_IPC_UTEST_CMP_BUF( res[ 1 ], initial_span[ 2 ] );
    OPIO_IPC_UTEST_CMP_BUF( res[ 2 ], initial_span[ 3 ] );
    OPIO_IPC_UTEST_CMP_BUF( res[ 3 ], initial_span[ 4 ] );
}

TEST_F( OpioIpcTcpDetailsSkipTransferredPart,
        TransferedMiddleOfSecondBuf )  // NOLINT
{
    auto res = details::skip_transferred_part( initial_span, 200 );
    ASSERT_EQ( res.size(), 4 );

    OPIO_IPC_UTEST_CMP_BUF( res[ 0 ], initial_span[ 1 ] );
    EXPECT_EQ( res[ 0 ].data(), &sample_data[ 100 ] );
    EXPECT_EQ( res[ 0 ].size(), 100 );

    OPIO_IPC_UTEST_CMP_BUF( res[ 1 ], initial_span[ 2 ] );
    OPIO_IPC_UTEST_CMP_BUF( res[ 2 ], initial_span[ 3 ] );
    OPIO_IPC_UTEST_CMP_BUF( res[ 3 ], initial_span[ 4 ] );
}

TEST_F( OpioIpcTcpDetailsSkipTransferredPart,
        TransferedAfterMiddleOfSecondBuf )  // NOLINT
{
    auto res = details::skip_transferred_part( initial_span, 299 );
    ASSERT_EQ( res.size(), 4 );

    OPIO_IPC_UTEST_CMP_BUF( res[ 0 ], initial_span[ 1 ] );
    EXPECT_EQ( res[ 0 ].data(), &sample_data[ 199 ] );
    EXPECT_EQ( res[ 0 ].size(), 1 );

    OPIO_IPC_UTEST_CMP_BUF( res[ 1 ], initial_span[ 2 ] );
    OPIO_IPC_UTEST_CMP_BUF( res[ 2 ], initial_span[ 3 ] );
    OPIO_IPC_UTEST_CMP_BUF( res[ 3 ], initial_span[ 4 ] );
}

TEST_F( OpioIpcTcpDetailsSkipTransferredPart,
        TransferedFullSecondBuf )  // NOLINT
{
    auto res = details::skip_transferred_part( initial_span, 300 );
    ASSERT_EQ( res.size(), 3 );

    OPIO_IPC_UTEST_CMP_BUF( res[ 0 ], initial_span[ 2 ] );
    OPIO_IPC_UTEST_CMP_BUF( res[ 1 ], initial_span[ 3 ] );
    OPIO_IPC_UTEST_CMP_BUF( res[ 2 ], initial_span[ 4 ] );
}

TEST_F( OpioIpcTcpDetailsSkipTransferredPart,
        TransferedUpToLastBuf )  // NOLINT
{
    auto res = details::skip_transferred_part( initial_span, 800 );
    ASSERT_EQ( res.size(), 1 );

    OPIO_IPC_UTEST_CMP_BUF( res[ 0 ], initial_span[ 4 ] );
}

TEST_F( OpioIpcTcpDetailsSkipTransferredPart,
        TransferedUpToLastBufPlus )  // NOLINT
{
    auto res = details::skip_transferred_part( initial_span, 801 );
    ASSERT_EQ( res.size(), 1 );
}

TEST_F( OpioIpcTcpDetailsSkipTransferredPart,
        TransferedUpToLastAlmostFullBuf )  // NOLINT
{
    auto res = details::skip_transferred_part( initial_span, 899 );
    ASSERT_EQ( res.size(), 1 );

    EXPECT_EQ( res[ 0 ].data(), &sample_data[ 99 ] );
    EXPECT_EQ( res[ 0 ].size(), 1 );
}

}  // anonymous namespace
