#include <type_traits>
#include <cstring>

#include <opio/net/buffer.hpp>

#include <gtest/gtest.h>

namespace /* anonymous */
{

using namespace opio::net;  // NOLINT

TEST( OpioNetBuffer, FmtIntergatorIsTriviallyCopyable )  // NOLINT
{
    // This class should be very well optimized.
    ASSERT_TRUE( std::is_trivially_copyable_v< buffer_fmt_integrator_t > );
}

TEST( OpioNetBuffer, SimpleBufferCtorDefault )  // NOLINT
{
    simple_buffer_t b{};
    EXPECT_EQ( b.data(), nullptr );
    EXPECT_EQ( b.size(), 0 );
    EXPECT_EQ( b.capacity(), 0 );
}

TEST( OpioNetBuffer, SimpleBufferCtorWithSize )  // NOLINT
{
    constexpr std::size_t sz = 89;
    simple_buffer_t b{ sz };
    EXPECT_NE( b.data(), nullptr );
    EXPECT_EQ( b.size(), sz );
    EXPECT_GE( b.capacity(), sz );
}

TEST( OpioNetBuffer, SimpleBufferCtorWithSizeByteValue )  // NOLINT
{
    constexpr std::size_t sz = 21;
    constexpr std::byte value{ 0x33 };

    simple_buffer_t b{ sz, value };
    EXPECT_NE( b.data(), nullptr );
    EXPECT_EQ( b.size(), sz );
    const auto c = std::count( begin( b ), end( b ), value );
    EXPECT_EQ( c, sz );
}

TEST( OpioNetBuffer, SimpleBufferCtorWithSrcSize )  // NOLINT
{
    std::string s{ "1234567890" };

    simple_buffer_t b{ s.data(), s.size() };

    EXPECT_NE( b.data(), nullptr );
    EXPECT_EQ( b.size(), s.size() );
    EXPECT_EQ( s, b.make_string_view() );
}

TEST( OpioNetBuffer, SimpleBufferCtorMove )  // NOLINT
{
    std::string s{ "1234567890" };

    simple_buffer_t b{ s.data(), s.size() };
    auto * p = b.data();
    auto n   = b.size();
    auto cap = b.capacity();

    simple_buffer_t b2{ std::move( b ) };

    EXPECT_EQ( b.data(), nullptr );
    EXPECT_EQ( b.size(), 0 );
    EXPECT_EQ( b.capacity(), 0 );

    EXPECT_EQ( b2.data(), p );
    EXPECT_EQ( b2.size(), n );
    EXPECT_EQ( b2.capacity(), cap );

    EXPECT_EQ( s, b2.make_string_view() );
}

TEST( OpioNetBuffer, SimpleBufferMoveAssign )  // NOLINT
{
    std::string s{ "1234567890" };

    simple_buffer_t b{ s.data(), s.size() };
    auto * p = b.data();
    auto n   = b.size();
    auto cap = b.capacity();

    simple_buffer_t b2{};

    EXPECT_NE( b.data(), nullptr );
    EXPECT_NE( b.size(), 0 );
    EXPECT_GE( b.capacity(), b.size() );

    b2 = std::move( b );

    EXPECT_EQ( b.data(), nullptr );
    EXPECT_EQ( b.size(), 0 );
    EXPECT_EQ( b.capacity(), 0 );

    EXPECT_EQ( b2.data(), p );
    EXPECT_EQ( b2.size(), n );
    EXPECT_EQ( b2.capacity(), cap );
    EXPECT_EQ( s, b2.make_string_view() );
}

TEST( OpioNetBuffer, SimpleBufferMakeCopy )  // NOLINT
{
    std::string s{ "1234567890" };

    simple_buffer_t b{ s.data(), s.size() };
    auto b2 = b.make_copy();

    EXPECT_NE( b.data(), b2.data() );
    EXPECT_EQ( b.make_string_view(), s );
    EXPECT_EQ( b2.make_string_view(), s );
}

TEST( OpioNetBuffer, SimpleBufferShrinkSize )  // NOLINT
{
    std::string s{ "1234567890qqqqqqqqq" };
    constexpr std::size_t sz = 10;

    simple_buffer_t b{ s.data(), s.size() };
    b.shrink_size( sz );

    EXPECT_NE( b.data(), nullptr );
    EXPECT_EQ( b.size(), sz );
    EXPECT_GE( b.capacity(), s.size() );

    EXPECT_EQ( b.make_string_view(), "1234567890" );
}

TEST( OpioNetBuffer, SimpleBufferResize )  // NOLINT
{
    std::string s{ "1234567890" };

    simple_buffer_t b{ s.data(), s.size() };
    b.resize( b.size() * 3 );

    EXPECT_NE( b.data(), nullptr );
    EXPECT_EQ( b.size(), 3 * s.size() );
    EXPECT_GE( b.capacity(), 3 * s.size() );

    auto check_data = b.make_string_view();
    check_data.remove_suffix( 2 * s.size() );
    EXPECT_EQ( check_data, s );

    b.resize( s.size() );
    EXPECT_NE( b.data(), nullptr );
    EXPECT_EQ( b.size(), s.size() );
    EXPECT_GE( b.capacity(), s.size() );
}

TEST( OpioNetBuffer, SimpleBufferResizeDropData )  // NOLINT
{
    std::string s{ "!@#1234567890&*()" };

    simple_buffer_t b{ s.data(), s.size() };
    b.resize_drop_data( b.size() * 3 );

    EXPECT_NE( b.data(), nullptr );
    EXPECT_EQ( b.size(), 3 * s.size() );
    EXPECT_GE( b.capacity(), 3 * s.size() );

    // There is a hypothetical probability the the bytes laying there
    // are the same as data in `s`.
    //
    // TODO: Technically we might be reading from uninitialized memory
    //       because we never tauch buffer after resizing it with data dropped.
    auto check_data = b.make_string_view();
    check_data.remove_suffix( 2 * s.size() );
    EXPECT_NE( check_data, s );
}

TEST( OpioNetBuffer, SimpleBufferOffsetData )  // NOLINT
{
    std::string s{ "!@#1234567890&*()" };

    simple_buffer_t b{ s.data(), s.size() };
    EXPECT_EQ( b.offset_data( 0 ), b.data() );
    EXPECT_EQ( *b.offset_data( 2 ), static_cast< std::byte >( s[ 2 ] ) );
    EXPECT_EQ( *b.offset_data( 3 ), static_cast< std::byte >( s[ 3 ] ) );
    EXPECT_EQ( *b.offset_data( 15 ), static_cast< std::byte >( s[ 15 ] ) );
}

TEST( OpioNetBuffer, ResizeWithDoubleCapacityGrowth )  // NOLINT
{
    {
        simple_buffer_t b{ 100 };

        ASSERT_EQ( b.capacity(), 100 );

        b.resize_with_double_capacity_growth( 101 );
        ASSERT_EQ( b.size(), 101 );
        ASSERT_EQ( b.capacity(), 200 );
    }

    {
        simple_buffer_t b{ 100 };

        ASSERT_EQ( b.capacity(), 100 );

        b.resize_with_double_capacity_growth( 210 );
        ASSERT_EQ( b.size(), 210 );
        ASSERT_EQ( b.capacity(), 210 );
    }
}

}  // anonymous namespace
