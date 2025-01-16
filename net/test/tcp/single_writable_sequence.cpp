#include <numeric>
#include <iostream>

#include <opio/net/tcp/connection.hpp>
#include <opio/net/heterogeneous_buffer.hpp>

#include <gtest/gtest.h>

#include <opio/test_utils/test_logger.hpp>

namespace /* anonymous */
{

using namespace ::opio::net;         // NOLINT
using namespace ::opio::net::tcp;    // NOLINT
using namespace ::opio::test_utils;  // NOLINT

using buffer_t = opio::net::simple_buffer_t;

TEST( OpioNetTcp, RawConnectionCfgMakeWriteTimeourPerBuffer )  // NOLINT
{
    connection_cfg_t cfg;
    cfg.write_timeout_per_1mb( std::chrono::milliseconds( 100 ) );
    EXPECT_EQ( cfg.make_write_timeout_per_buffer( 100 ),
               std::chrono::milliseconds( 100 ) );
    EXPECT_EQ( cfg.make_write_timeout_per_buffer( 1024 ),
               std::chrono::milliseconds( 100 ) );
    EXPECT_EQ( cfg.make_write_timeout_per_buffer( 1024 * 1024 - 1 ),
               std::chrono::milliseconds( 100 ) );
    EXPECT_EQ( cfg.make_write_timeout_per_buffer( 1024 * 1024 ),
               std::chrono::milliseconds( 100 ) );
    EXPECT_EQ( cfg.make_write_timeout_per_buffer( 1024 * 1024 + 1 ),
               std::chrono::milliseconds( 200 ) );

    EXPECT_EQ( cfg.make_write_timeout_per_buffer( 1024 * 1024 * 2 - 1 ),
               std::chrono::milliseconds( 200 ) );
    EXPECT_EQ( cfg.make_write_timeout_per_buffer( 1024 * 1024 * 2 ),
               std::chrono::milliseconds( 200 ) );
    EXPECT_EQ( cfg.make_write_timeout_per_buffer( 1024 * 1024 * 2 + 1 ),
               std::chrono::milliseconds( 300 ) );

    EXPECT_EQ( cfg.make_write_timeout_per_buffer( 1024 * 1024 * 10 - 1 ),
               std::chrono::seconds( 1 ) );
    EXPECT_EQ( cfg.make_write_timeout_per_buffer( 1024 * 1024 * 10 ),
               std::chrono::seconds( 1 ) );
    EXPECT_EQ( cfg.make_write_timeout_per_buffer( 1024 * 1024 * 10 + 1 ),
               std::chrono::seconds( 1 ) + std::chrono::milliseconds( 100 ) );
}

TEST( OpioNetTcp, ReasonableMaxIOVLen )  // NOLINT
{
    // Should better fail if IOV is less than 16 for a given platform.
    // It requires a concious thinking then.
    ASSERT_GE( opio::net::tcp::details::reasonable_max_iov_len(), 16 );
    ASSERT_LE( opio::net::tcp::details::reasonable_max_iov_len(), 64 );
    ASSERT_EQ( opio::net::tcp::details::reasonable_max_iov_len() % 2, 0 );
}

auto make_sample_buffer = []( std::size_t size, auto x ) {
    return buffer_t{ size, static_cast< std::byte >( x ) };
};

auto make_sv = []( auto buf ) {
    return std::string_view{ static_cast< const char * >( buf.data() ),
                             buf.size() };
};

TEST( OpioNetTcpSingleWritableSequence, Basic )  // NOLINT
{
    opio::net::tcp::details::single_writable_sequence_t< simple_buffer_driver_t >
        seq;
    std::vector< asio_ns::const_buffer > all_bufs;
    for( auto i = 0; i < opio::net::tcp::details::reasonable_max_iov_len(); ++i )
    {
        ASSERT_TRUE( seq.can_append_buffer() ) << "When i = " << i;
        auto buf       = make_sample_buffer( i + 100, 0xFF );
        const void * d = buf.data();
        const auto s   = buf.size();
        all_bufs.emplace_back( d, s );

        seq.append_buffer( std::move( buf ) );

        const auto asio_bufs = seq.asio_bufs();
        ASSERT_EQ( asio_bufs.bufs.size(), all_bufs.size() ) << "When i = " << i;

        ASSERT_TRUE( std::equal( begin( all_bufs ),
                                 end( all_bufs ),
                                 std::begin( asio_bufs.bufs ),
                                 []( auto x, auto y ) {
                                     return x.data() == y.data()
                                            && x.size() == y.size();
                                 } ) )
            << "When i = " << i;

        EXPECT_EQ( asio_bufs.total_size,
                   std::accumulate( std::begin( asio_bufs.bufs ),
                                    std::end( asio_bufs.bufs ),
                                    std::size_t{ 0 },
                                    []( auto memo, auto item ) {
                                        return memo + item.size();
                                    } ) )
            << "When i = " << i;
    }

    ASSERT_FALSE( seq.can_append_buffer() );
}

TEST( OpioNetTcpSingleWritableSequence,
      ConcatSmallBuffersNotHappen )  // NOLINT
{
    simple_buffer_driver_t buffer_driver{};
    opio::net::tcp::details::single_writable_sequence_t< simple_buffer_driver_t,
                                                         10 >
        seq;

    seq.append_buffer( make_sample_buffer( 10, 0xFF ) );
    seq.append_buffer( make_sample_buffer( 1, 0xFF ) );
    seq.append_buffer( make_sample_buffer( 10, 0xFF ) );
    seq.append_buffer( make_sample_buffer( 2, 0xFF ) );
    seq.append_buffer( make_sample_buffer( 9, 0xFF ) );

    for( auto i = 5; i < opio::net::tcp::details::reasonable_max_iov_len(); ++i )
    {
        seq.append_buffer( make_sample_buffer( i, 0xFF ) );
    }

    ASSERT_FALSE( seq.can_append_buffer() );
    seq.concat_small_buffers( buffer_driver );

    // Any adjacent buffers together are larger than 10 (the limit for concat)
    ASSERT_FALSE( seq.can_append_buffer() );
}

TEST( OpioNetTcpSingleWritableSequence,
      SimpleConcatSmallBuffers2Bufs )  // NOLINT
{
    simple_buffer_driver_t buffer_driver{};
    opio::net::tcp::details::single_writable_sequence_t< simple_buffer_driver_t,
                                                         10 >
        seq;

    seq.append_buffer( make_sample_buffer( 4, 0xFF ) );
    seq.append_buffer( make_sample_buffer( 5, 0xFF ) );

    ASSERT_EQ( 2, seq.asio_bufs().bufs.size() );
    ASSERT_EQ( 4, seq.asio_bufs().bufs[ 0 ].size() );
    ASSERT_EQ( 5, seq.asio_bufs().bufs[ 1 ].size() );
    ASSERT_EQ( 9, seq.asio_bufs().total_size );

    seq.concat_small_buffers( buffer_driver );

    ASSERT_EQ( 1, seq.asio_bufs().bufs.size() );
    ASSERT_EQ( 9, seq.asio_bufs().bufs[ 0 ].size() );
    ASSERT_EQ( 9, seq.asio_bufs().total_size );
}

TEST( OpioNetTcpSingleWritableSequence,
      SimpleConcatSmallBuffers3Bufs )  // NOLINT
{
    simple_buffer_driver_t buffer_driver{};
    opio::net::tcp::details::single_writable_sequence_t< simple_buffer_driver_t,
                                                         10 >
        seq;

    seq.append_buffer( make_sample_buffer( 4, 'a' ) );
    seq.append_buffer( make_sample_buffer( 2, 'b' ) );
    seq.append_buffer( make_sample_buffer( 4, 'c' ) );

    ASSERT_EQ( 3, seq.asio_bufs().bufs.size() );
    ASSERT_EQ( 4, seq.asio_bufs().bufs[ 0 ].size() );
    ASSERT_EQ( 2, seq.asio_bufs().bufs[ 1 ].size() );
    ASSERT_EQ( 4, seq.asio_bufs().bufs[ 2 ].size() );
    ASSERT_EQ( 10, seq.asio_bufs().total_size );

    seq.concat_small_buffers( buffer_driver );

    ASSERT_EQ( 1, seq.asio_bufs().bufs.size() );
    ASSERT_EQ( 10, seq.asio_bufs().bufs[ 0 ].size() );
    ASSERT_EQ( 10, seq.asio_bufs().total_size );

    const char expected_buf[] = "aaaabbcccc";

    auto appeared_buf = make_sv( seq.asio_bufs().bufs[ 0 ] );
    ASSERT_EQ( expected_buf, appeared_buf );
}

TEST( OpioNetTcpSingleWritableSequence,
      SimpleConcatSmallBuffers32Bufs )  // NOLINT
{
    simple_buffer_driver_t buffer_driver{};
    opio::net::tcp::details::single_writable_sequence_t< simple_buffer_driver_t,
                                                         1024 >
        seq;

    for( auto i = 0; i < opio::net::tcp::details::reasonable_max_iov_len(); ++i )
    {
        seq.append_buffer( make_sample_buffer( 16, 'a' + i ) );
    }

    ASSERT_EQ( opio::net::tcp::details::reasonable_max_iov_len(),
               seq.asio_bufs().bufs.size() );

    seq.concat_small_buffers( buffer_driver );

    ASSERT_EQ( 1, seq.asio_bufs().bufs.size() );
}

// NOLINTNEXTLINE
TEST( OpioNetTcpSingleWritableSequence, ConcatSmallBuffers3BufsIntheMiddle )
{
    simple_buffer_driver_t buffer_driver{};
    opio::net::tcp::details::single_writable_sequence_t< simple_buffer_driver_t,
                                                         10 >
        seq;

    seq.append_buffer( make_sample_buffer( 9, 0xFF ) );
    seq.append_buffer( make_sample_buffer( 4, 0xFF ) );
    seq.append_buffer( make_sample_buffer( 2, 0xFF ) );
    seq.append_buffer( make_sample_buffer( 4, 0xFF ) );
    seq.append_buffer( make_sample_buffer( 1, 0xFF ) );

    ASSERT_EQ( 5, seq.asio_bufs().bufs.size() );
    ASSERT_EQ( 20, seq.asio_bufs().total_size );

    seq.concat_small_buffers( buffer_driver );

    ASSERT_EQ( 3, seq.asio_bufs().bufs.size() );
    ASSERT_EQ( 9, seq.asio_bufs().bufs[ 0 ].size() );
    ASSERT_EQ( 10, seq.asio_bufs().bufs[ 1 ].size() );
    ASSERT_EQ( 1, seq.asio_bufs().bufs[ 2 ].size() );
    ASSERT_EQ( 20, seq.asio_bufs().total_size );
}

// NOLINTNEXTLINE
TEST( OpioNetTcpSingleWritableSequence, ConcatSmallBuffersVarying )
{
    simple_buffer_driver_t buffer_driver{};
    opio::net::tcp::details::single_writable_sequence_t< simple_buffer_driver_t,
                                                         10 >
        seq;

    seq.append_buffer( make_sample_buffer( 9, 'a' ) );  // ...

    seq.append_buffer( make_sample_buffer( 4, 'b' ) );  // Group 1
    seq.append_buffer( make_sample_buffer( 1, 'b' ) );  // Group 1
    seq.append_buffer( make_sample_buffer( 4, 'b' ) );  // Group 1

    seq.append_buffer( make_sample_buffer( 3, 'c' ) );  // Group 2
    seq.append_buffer( make_sample_buffer( 3, 'c' ) );  // Group 2
    seq.append_buffer( make_sample_buffer( 1, 'c' ) );  // Group 2
    seq.append_buffer( make_sample_buffer( 1, 'c' ) );  // Group 2
    seq.append_buffer( make_sample_buffer( 1, 'c' ) );  // Group 2

    seq.append_buffer( make_sample_buffer( 11, 'd' ) );  // ...

    seq.append_buffer( make_sample_buffer( 4, 'e' ) );  // Group 3
    seq.append_buffer( make_sample_buffer( 4, 'e' ) );  // Group 3

    seq.append_buffer( make_sample_buffer( 5, 'f' ) );  // Group 4
    seq.append_buffer( make_sample_buffer( 2, 'f' ) );  // Group 4

    seq.append_buffer( make_sample_buffer( 4, 'g' ) );  // ...

    ASSERT_EQ( 15, seq.asio_bufs().bufs.size() );

    seq.concat_small_buffers( buffer_driver );

    ASSERT_EQ( 7, seq.asio_bufs().bufs.size() );
    ASSERT_EQ( "aaaaaaaaa", make_sv( seq.asio_bufs().bufs[ 0 ] ) );
    ASSERT_EQ( "bbbbbbbbb", make_sv( seq.asio_bufs().bufs[ 1 ] ) );
    ASSERT_EQ( "ccccccccc", make_sv( seq.asio_bufs().bufs[ 2 ] ) );
    ASSERT_EQ( "ddddddddddd", make_sv( seq.asio_bufs().bufs[ 3 ] ) );
    ASSERT_EQ( "eeeeeeee", make_sv( seq.asio_bufs().bufs[ 4 ] ) );
    ASSERT_EQ( "fffffff", make_sv( seq.asio_bufs().bufs[ 5 ] ) );
    ASSERT_EQ( "gggg", make_sv( seq.asio_bufs().bufs[ 6 ] ) );
}

//
// utest_call_count_buffer_t
//

/*
 * A fake buffer type to count the number of invokation of
 * `data()` mem-function.
 */
struct utest_call_count_buffer_t
{
    const void * data() const
    {
        const_cast< utest_call_count_buffer_t & >( *this ).counter++;
        return &counter;
    }

    void * data()
    {
        counter++;
        return &counter;
    }

    std::size_t size() const { return sizeof( counter ); }

    int counter{};
};

// NOLINTNEXTLINE
TEST( OpioNetTcpSingleWritableSequence, NumberOfCallsToData )
{
    // heterogeneous_buffer_driver_t buffer_driver{};
    opio::net::tcp::details::
        single_writable_sequence_t< heterogeneous_buffer_driver_t, 10 >
            seq;

    auto buf1 = std::make_shared< utest_call_count_buffer_t >();
    auto buf2 = std::make_shared< utest_call_count_buffer_t >();

    seq.append_buffer( buf1 );  // Group 1
    seq.append_buffer( buf2 );  // Group 1

    auto bufs_seq = seq.asio_bufs();
    ASSERT_EQ( 2, bufs_seq.bufs.size() );
    ASSERT_EQ( 1, buf1->counter );
    ASSERT_EQ( 1, buf2->counter );
}

}  // anonymous namespace
