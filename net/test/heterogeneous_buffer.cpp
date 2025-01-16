#include <opio/net/heterogeneous_buffer.hpp>

#include <type_traits>

#include <gtest/gtest.h>

namespace /* anonymous */
{

using namespace opio;       // NOLINT
using namespace opio::net;  // NOLINT

TEST( OpioNet, HeteroBufferDefault )  // NOLINT
{
    heterogeneous_buffer_t hb{};
    auto buf = hb.make_asio_const_buffer();

    EXPECT_EQ( buf.data(), nullptr );
    EXPECT_EQ( buf.size(), 0 );
}

TEST( OpioNet, HeteroBufferCString )  // NOLINT
{
    const char * s1 = "0123456789";

    auto x = const_buffer_t{ s1, std::strlen( s1 ) };
    heterogeneous_buffer_t hb{ const_buffer_t{ s1, std::strlen( s1 ) } };
    auto buf = hb.make_asio_const_buffer();

    EXPECT_EQ( buf.data(), static_cast< const void * >( s1 ) );
    EXPECT_EQ( buf.size(), 10 );
    EXPECT_EQ( hb.get_size(), buf.size() );

    static const char s2[] = "012345678901234567890123456789";
    hb = heterogeneous_buffer_t{ const_buffer_t{ s2, std::strlen( s2 ) } };

    buf = hb.make_asio_const_buffer();
    EXPECT_EQ( buf.data(), static_cast< const void * >( s2 ) );
    EXPECT_EQ( buf.size(), 30 );

    const char * s3 = "qweasdzxcrtyfghvbnuiojklm,.";
    hb              = heterogeneous_buffer_t{ const_buffer_t{ s3, 16 } };

    buf = hb.make_asio_const_buffer();
    EXPECT_EQ( buf.data(), static_cast< const void * >( s3 ) );
    EXPECT_EQ( buf.size(), 16 );

    simple_buffer_t sb{};
    const auto extract_reusable_simple_buffer =
        hb.extract_reusable_simple_buffer( sb );

    ASSERT_FALSE( extract_reusable_simple_buffer );

    auto try_get_mutable = [ & ] {
        [[maybe_unused]] auto mb = hb.make_asio_mutable_buffer();
    };

    EXPECT_THROW( try_get_mutable(), exception_t );
}

TEST( OpioNet, HeteroBufferStdString )  // NOLINT
{
    const char * s1 = "0123456789";
    std::string str1{ s1 };

    heterogeneous_buffer_t hb{ std::move( str1 ) };

    auto buf = hb.make_asio_const_buffer();

    EXPECT_EQ( buf.size(), 10 );
    EXPECT_EQ( hb.get_size(), buf.size() );
    EXPECT_EQ( 0, std::memcmp( buf.data(), s1, buf.size() ) );

    const char * s2 = "012345678901234567890123456789";

    hb = heterogeneous_buffer_t{ s2 };

    buf = hb.make_asio_const_buffer();
    EXPECT_EQ( buf.size(), 30 );
    EXPECT_EQ( hb.get_size(), buf.size() );
    EXPECT_EQ( 0, std::memcmp( buf.data(), s2, buf.size() ) );

    const char * s3 =
        "\0x00\0x00\0x00"
        "012345678901234567890123456789 012345678901234567890123456789";
    std::string str3{ s3, 64 };

    hb = heterogeneous_buffer_t{ std::move( str3 ) };

    buf = hb.make_asio_const_buffer();
    EXPECT_EQ( buf.size(), 64 );
    EXPECT_EQ( hb.get_size(), buf.size() );
    EXPECT_EQ( 0, std::memcmp( buf.data(), s3, buf.size() ) );

    simple_buffer_t sb{};
    const auto extract_reusable_simple_buffer =
        hb.extract_reusable_simple_buffer( sb );

    ASSERT_FALSE( extract_reusable_simple_buffer );

    auto try_get_mutable = [ & ] {
        [[maybe_unused]] auto mb = hb.make_asio_mutable_buffer();
    };

    EXPECT_NO_THROW( try_get_mutable() );
}

TEST( OpioNet, HeteroBufferSimpleBuffer )  // NOLINT
{
    const char * s1 = "0123456789";
    auto buf1       = simple_buffer_t::make_from(
        { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' } );

    heterogeneous_buffer_t hb{ std::move( buf1 ) };

    auto buf = hb.make_asio_const_buffer();

    EXPECT_EQ( buf.size(), 10 );
    EXPECT_EQ( hb.get_size(), buf.size() );
    EXPECT_EQ( 0, std::memcmp( buf.data(), s1, buf.size() ) );

    simple_buffer_t sb{};
    const auto extract_reusable_simple_buffer =
        hb.extract_reusable_simple_buffer( sb );

    ASSERT_TRUE( extract_reusable_simple_buffer );

    ASSERT_EQ( sb.size(), buf.size() );
}

TEST( OpioNet, HeteroBufferSharedDatasizeable )  // NOLINT
{
    auto str = std::make_shared< std::string >( "01234567890123456789xy" );

    heterogeneous_buffer_t hb{ str };
    auto buf = hb.make_asio_const_buffer();

    EXPECT_EQ( buf.size(), str->size() );
    EXPECT_EQ( static_cast< const void * >( buf.data() ),
               static_cast< const void * >( str->data() ) );

    auto simple_buf =
        std::make_shared< simple_buffer_t >( simple_buffer_t::make_from(
            { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' } ) );

    hb  = heterogeneous_buffer_t{ simple_buf };
    buf = hb.make_asio_const_buffer();
    EXPECT_EQ( buf.size(), simple_buf->size() );
    EXPECT_EQ( static_cast< const void * >( buf.data() ),
               static_cast< const void * >( simple_buf->data() ) );

    simple_buffer_t sb{};
    auto extract_reusable_simple_buffer = hb.extract_reusable_simple_buffer( sb );

    ASSERT_FALSE( extract_reusable_simple_buffer );

    auto try_get_mutable = [ & ] {
        [[maybe_unused]] auto mb = hb.make_asio_mutable_buffer();
    };

    EXPECT_NO_THROW( try_get_mutable() );

    // Make hb the only owner of internal simple_buffer wrapped in shared ptr.
    simple_buf.reset();
    extract_reusable_simple_buffer = hb.extract_reusable_simple_buffer( sb );

    ASSERT_TRUE( extract_reusable_simple_buffer );
}

TEST( OpioNet, HeteroBufferSharedDatasizeableEmptySptr )  // NOLINT
{
    auto bad_approach = [] {
        std::shared_ptr< std::string > str_sptr{};
        heterogeneous_buffer_t hb{ str_sptr };
        return hb.get_size();
    };

    EXPECT_THROW( bad_approach(), exception_t );
}

using test_adjustable_content_buffer_t =
    adjustable_content_buffer_t< std::function<
        void( simple_buffer_t::value_type *, simple_buffer_t::size_type ) > >;

TEST( OpioNet, HeteroBufferAdjustableContentBuffer )  // NOLINT
{
    test_adjustable_content_buffer_t buf{
        simple_buffer_t::make_from( { 'a', 'b', 'c' } ),
        []( auto * data, auto size ) {
            for( ; size > 0; ++data, --size )
            {
                char c = static_cast< char >( *data );
                ++c;
                *data = static_cast< std::byte >( c );
            }
        }
    };

    heterogeneous_buffer_t hb{ std::move( buf ) };
    auto b = hb.make_asio_const_buffer();

    EXPECT_EQ( b.size(), 3 );

    EXPECT_EQ( static_cast< const char * >( b.data() )[ 0 ], 'b' );
    EXPECT_EQ( static_cast< const char * >( b.data() )[ 1 ], 'c' );
    EXPECT_EQ( static_cast< const char * >( b.data() )[ 2 ], 'd' );

    b = hb.make_asio_const_buffer();

    EXPECT_EQ( static_cast< const char * >( b.data() )[ 0 ], 'c' );
    EXPECT_EQ( static_cast< const char * >( b.data() )[ 1 ], 'd' );
    EXPECT_EQ( static_cast< const char * >( b.data() )[ 2 ], 'e' );
}

using test_nonowning_adjustable_content_buffer_t = adjustable_content_buffer_t<
    std::function< void( simple_buffer_t::value_type *,
                         simple_buffer_t::size_type ) >,
    std::span< std::byte > >;

TEST( OpioNet, HeteroBufferNonOwningAdjustableContentBuffer )  // NOLINT
{
    auto simple_buf = simple_buffer_t::make_from( { 'a', 'b', 'c' } );
    test_nonowning_adjustable_content_buffer_t buf{
        { simple_buf.data(), simple_buf.size() },
        []( auto * data, auto size ) {
            for( ; size > 0; ++data, --size )
            {
                char c = static_cast< char >( *data );
                ++c;
                *data = static_cast< std::byte >( c );
            }
        }
    };

    heterogeneous_buffer_t hb{ std::move( buf ) };
    auto b = hb.make_asio_const_buffer();

    EXPECT_EQ( b.size(), 3 );
    EXPECT_EQ( hb.get_size(), b.size() );

    EXPECT_EQ( static_cast< const char * >( b.data() )[ 0 ], 'b' );
    EXPECT_EQ( static_cast< const char * >( b.data() )[ 1 ], 'c' );
    EXPECT_EQ( static_cast< const char * >( b.data() )[ 2 ], 'd' );

    b = hb.make_asio_const_buffer();

    EXPECT_EQ( static_cast< const char * >( b.data() )[ 0 ], 'c' );
    EXPECT_EQ( static_cast< const char * >( b.data() )[ 1 ], 'd' );
    EXPECT_EQ( static_cast< const char * >( b.data() )[ 2 ], 'e' );

    simple_buffer_t sb{};
    const auto extract_reusable_simple_buffer =
        hb.extract_reusable_simple_buffer( sb );

    ASSERT_FALSE( extract_reusable_simple_buffer );
}

//
// custom_buffer_t
//

struct custom_buffer_t
{
    custom_buffer_t( int & counter, std::string str )
        : m_str{ std::move( str ) }
        , m_counter{ counter }
    {
        ++m_counter;
    }

    ~custom_buffer_t() { --m_counter; }

    const char * data() const { return m_str.data(); }
    std::size_t size() const { return m_str.size(); }

    char * data() { return m_str.data(); }
    std::size_t size() { return m_str.size(); }

    std::string m_str;
    int & m_counter;
};

TEST( OpioNet, HeteroBufferCustomDataSizeableType )  // NOLINT
{
    int bufs_count = 0;

    {
        const char * s1 = "01234567890123456789xy";
        auto custom     = std::make_shared< custom_buffer_t >( bufs_count, s1 );

        EXPECT_EQ( 1, bufs_count );

        heterogeneous_buffer_t hb{ std::move( custom ) };
        EXPECT_EQ( 1, bufs_count );

        auto buf = hb.make_asio_const_buffer();
        EXPECT_EQ( buf.size(), 22 );
        EXPECT_EQ( 0, std::memcmp( buf.data(), s1, buf.size() ) );

        {
            heterogeneous_buffer_t hb2{ std::move( hb ) };

            EXPECT_EQ( 1, bufs_count );

            buf = hb2.make_asio_const_buffer();
            EXPECT_EQ( buf.size(), 22 );
            EXPECT_EQ( 0, std::memcmp( buf.data(), s1, buf.size() ) );

            hb = std::move( hb2 );
        }

        const char * s2 =
            "01234567890123456789xy01234567890123456789xy01234567890123456789xy";

        custom = std::make_shared< custom_buffer_t >( bufs_count, s2 );
        EXPECT_EQ( 2, bufs_count );

        hb = heterogeneous_buffer_t{ custom };
        EXPECT_EQ( 1, bufs_count );

        buf = hb.make_asio_const_buffer();
        EXPECT_EQ( buf.size(), 66 );
        EXPECT_EQ( 0, std::memcmp( buf.data(), s2, buf.size() ) );
    }

    EXPECT_EQ( 0, bufs_count );
}

TEST( OpioNet, HeteroBufferVector )  // NOLINT
{
    std::vector< heterogeneous_buffer_t > v1;
    std::vector< heterogeneous_buffer_t > v2;

    v1.push_back( const_buffer_t{ "123", 3 } );
    v1.emplace_back( const_buffer_t{ "qwe", 3 } );

    v1.push_back( { "@123" } );
    v1.emplace_back( "@qwe" );

    v1.push_back( std::string{ "!!!" } );
    v1.emplace_back( std::string{ "***" } );

    v1.push_back( simple_buffer_t::make_from( { '0', '1', '2', '3', '4' } ) );
    v1.emplace_back( simple_buffer_t::make_from( { '5', '6', '7', '8', '9' } ) );

    v1.push_back( std::make_shared< std::string >( "shared - !!!" ) );
    v1.emplace_back( std::make_shared< std::string >( "shared - ***" ) );

    v1.push_back( std::make_shared< simple_buffer_t >(
        simple_buffer_t::make_from( { 's', '-', '0', '1', '2', '3', '4' } ) ) );
    v1.emplace_back( std::make_shared< simple_buffer_t >(
        simple_buffer_t::make_from( { 's', '-', '5', '6', '7', '8', '9' } ) ) );

    EXPECT_EQ( v1.size(), 12 );

    auto make_string = []( auto & buf ) {
        auto b = buf.make_asio_const_buffer();
        return std::string{ static_cast< const char * >( b.data() ), b.size() };
    };

    EXPECT_EQ( make_string( v1[ 0 ] ), "123" );
    EXPECT_EQ( make_string( v1[ 1 ] ), "qwe" );

    EXPECT_EQ( make_string( v1[ 2 ] ), "@123" );
    EXPECT_EQ( make_string( v1[ 3 ] ), "@qwe" );

    EXPECT_EQ( make_string( v1[ 4 ] ), "!!!" );
    EXPECT_EQ( make_string( v1[ 5 ] ), "***" );

    EXPECT_EQ( make_string( v1[ 6 ] ), "01234" );
    EXPECT_EQ( make_string( v1[ 7 ] ), "56789" );

    EXPECT_EQ( make_string( v1[ 8 ] ), "shared - !!!" );
    EXPECT_EQ( make_string( v1[ 9 ] ), "shared - ***" );

    EXPECT_EQ( make_string( v1[ 10 ] ), "s-01234" );
    EXPECT_EQ( make_string( v1[ 11 ] ), "s-56789" );

    v2 = std::move( v1 );
    EXPECT_EQ( v2.size(), 12 );
    EXPECT_EQ( v1.size(), 0 );

    EXPECT_EQ( make_string( v2[ 0 ] ), "123" );
    EXPECT_EQ( make_string( v2[ 1 ] ), "qwe" );

    EXPECT_EQ( make_string( v2[ 2 ] ), "@123" );
    EXPECT_EQ( make_string( v2[ 3 ] ), "@qwe" );

    EXPECT_EQ( make_string( v2[ 4 ] ), "!!!" );
    EXPECT_EQ( make_string( v2[ 5 ] ), "***" );

    EXPECT_EQ( make_string( v2[ 6 ] ), "01234" );
    EXPECT_EQ( make_string( v2[ 7 ] ), "56789" );

    EXPECT_EQ( make_string( v2[ 8 ] ), "shared - !!!" );
    EXPECT_EQ( make_string( v2[ 9 ] ), "shared - ***" );

    EXPECT_EQ( make_string( v2[ 10 ] ), "s-01234" );
    EXPECT_EQ( make_string( v2[ 11 ] ), "s-56789" );
}

TEST( OpioNetTcp, NocopyBufferWrapperConstructors )  // NOLINT
{
    {
        heterogeneous_buffer_t xxx{ simple_buffer_t::make_from(
            { '1', '2', '3' } ) };

        EXPECT_EQ( 3, xxx.make_asio_const_buffer().size() );
    }
    {
        heterogeneous_buffer_t xxx{ std::string{
            "012345678901234567890123456789" } };

        EXPECT_EQ( 30, xxx.make_asio_const_buffer().size() );
    }
    {
        heterogeneous_buffer_t xxx{ std::make_shared< std::string >(
            "012345678901234567890123456789" ) };

        EXPECT_EQ( 30, xxx.make_asio_const_buffer().size() );
    }
}

}  // anonymous namespace
