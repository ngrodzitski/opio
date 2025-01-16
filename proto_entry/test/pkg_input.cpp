#include <opio/proto_entry/pkg_input.hpp>

#include <gtest/gtest.h>

#include "utest.pb.h"

namespace /* anonymous */
{

using namespace opio::proto_entry;  // NOLINT

TEST( OpioProtoEntryPkgInput, OwnRoutinesCtor )  // NOLINT
{
    pkg_input_t input{};

    ASSERT_EQ( input.size(), 0 );
}

inline auto make_buffer( std::size_t n, char c )
{
    opio::net::simple_buffer_t buf{ n };
    std::memset( buf.data(), c, n );
    return buf;
}

inline auto make_buffer( std::size_t n, std::byte c )
{
    return make_buffer( n, static_cast< char >( c ) );
}

TEST( OpioProtoEntryPkgInput,                         // NOLINT
      OwnRoutinesSkipBytesWithinSingleBufferAtOnce )  // NOLINT
{
    pkg_input_t< opio::net::simple_buffer_t, 4 > input{};

    input.append( make_buffer( 100, '1' ) );
    ASSERT_EQ( input.size(), 100 );

    input.skip_bytes( 1 );
    ASSERT_EQ( input.size(), 99 );

    input.skip_bytes( 10 );
    ASSERT_EQ( input.size(), 89 );

    input.skip_bytes( 59 );
    ASSERT_EQ( input.size(), 30 );
}

TEST( OpioProtoEntryPkgInput,                         // NOLINT
      OwnRoutinesSkipBytesEntireSingleBufferAtOnce )  // NOLINT
{
    pkg_input_t< opio::net::simple_buffer_t, 4 > input{};

    input.append( make_buffer( 100, '1' ) );
    input.append( make_buffer( 100, '2' ) );
    ASSERT_EQ( input.size(), 200 );

    input.skip_bytes( 100 );
    ASSERT_EQ( input.size(), 100 );

    input.skip_bytes( 100 );
    ASSERT_EQ( input.size(), 0 );
}

TEST( OpioProtoEntryPkgInput,
      OwnRoutinesSkipBytesEntireFirstSomeSecond )  // NOLINT
{
    {
        pkg_input_t< opio::net::simple_buffer_t, 4 > input{};

        input.append( make_buffer( 100, '1' ) );
        input.append( make_buffer( 100, '2' ) );
        ASSERT_EQ( input.size(), 200 );

        input.skip_bytes( 101 );
        ASSERT_EQ( input.size(), 99 );

        input.skip_bytes( 99 );
        ASSERT_EQ( input.size(), 0 );
    }
    {
        pkg_input_t< opio::net::simple_buffer_t, 4 > input{};

        input.append( make_buffer( 100, '1' ) );
        input.append( make_buffer( 100, '2' ) );
        ASSERT_EQ( input.size(), 200 );

        input.skip_bytes( 142 );
        ASSERT_EQ( input.size(), 58 );

        input.skip_bytes( 58 );
        ASSERT_EQ( input.size(), 0 );
    }
    {
        pkg_input_t< opio::net::simple_buffer_t, 4 > input{};

        input.append( make_buffer( 100, '1' ) );
        input.append( make_buffer( 100, '2' ) );
        ASSERT_EQ( input.size(), 200 );

        input.skip_bytes( 200 );
        ASSERT_EQ( input.size(), 0 );
    }
}

TEST( OpioProtoEntryPkgInput,                           // NOLINT
      OwnRoutinesSkipBytesEntireFirstSecondSomeThird )  // NOLINT
{
    {
        pkg_input_t< opio::net::simple_buffer_t, 4 > input{};

        input.append( make_buffer( 100, '1' ) );
        input.append( make_buffer( 100, '2' ) );
        input.append( make_buffer( 100, '3' ) );
        ASSERT_EQ( input.size(), 300 );

        input.skip_bytes( 222 );
        ASSERT_EQ( input.size(), 78 );

        input.skip_bytes( 78 );
        ASSERT_EQ( input.size(), 0 );
    }
    {
        pkg_input_t< opio::net::simple_buffer_t, 4 > input{};

        input.append( make_buffer( 100, '1' ) );
        input.append( make_buffer( 100, '2' ) );
        input.append( make_buffer( 100, '3' ) );
        ASSERT_EQ( input.size(), 300 );

        input.skip_bytes( 300 );
        ASSERT_EQ( input.size(), 0 );
    }
}

TEST( OpioProtoEntryPkgInput,                       // NOLINT
      OwnRoutinesAppendMoreBufsThenQueueCapacity )  // NOLINT
{
    pkg_input_t< opio::net::simple_buffer_t, 4 > input{};

    ASSERT_EQ( input.size(), 0 );

    input.append( make_buffer( 100, '1' ) );
    ASSERT_EQ( input.size(), 100 );

    input.append( make_buffer( 1000, '2' ) );
    ASSERT_EQ( input.size(), 1100 );

    input.append( make_buffer( 10, '3' ) );
    ASSERT_EQ( input.size(), 1110 );

    input.append( make_buffer( 1, '4' ) );
    ASSERT_EQ( input.size(), 1111 );

    // That one is appended to the last buffer in the queue.
    input.append( make_buffer( 1, '5' ) );
    ASSERT_EQ( input.size(), 1112 );

    input.append( make_buffer( 10, '6' ) );
    ASSERT_EQ( input.size(), 1122 );

    input.append( make_buffer( 100, '7' ) );
    ASSERT_EQ( input.size(), 1222 );

    input.append( make_buffer( 1000, '8' ) );
    ASSERT_EQ( input.size(), 2222 );
}

TEST( OpioProtoEntryPkgInput, OwnRoutinesRingQueue1 )  // NOLINT
{
    pkg_input_t< opio::net::simple_buffer_t, 4 > input{};

    ASSERT_EQ( input.size(), 0 );

    input.append( make_buffer( 100, '1' ) );
    input.append( make_buffer( 1000, '2' ) );
    input.append( make_buffer( 10, '3' ) );
    input.append( make_buffer( 1, '4' ) );
    ASSERT_EQ( input.size(), 1111 );

    input.skip_bytes( 100 );  // Exhaust first buffer in the queue.
    ASSERT_EQ( input.size(), 1011 );
    input.append( make_buffer( 100, '5' ) );
    ASSERT_EQ( input.size(), 1111 );

    input.skip_bytes( 1000 );  // Exhaust 2nd buffer in the queue.
    ASSERT_EQ( input.size(), 111 );
    input.append( make_buffer( 1000, '6' ) );
    ASSERT_EQ( input.size(), 1111 );

    input.skip_bytes( 10 );  // Exhaust 3rd buffer in the queue.
    ASSERT_EQ( input.size(), 1101 );
    input.append( make_buffer( 10, '7' ) );
    ASSERT_EQ( input.size(), 1111 );

    input.skip_bytes( 1 );  // Exhaust 4th buffer in the queue.
    ASSERT_EQ( input.size(), 1110 );
    input.append( make_buffer( 1, '8' ) );
    ASSERT_EQ( input.size(), 1111 );

    input.skip_bytes( 1100 );  // Exhaust 5/6th buffer in the queue.
    ASSERT_EQ( input.size(), 11 );
}

TEST( OpioProtoEntryPkgInput, OwnRoutinesRingQueue2 )  // NOLINT
{
    pkg_input_t< opio::net::simple_buffer_t, 4 > input{};

    ASSERT_EQ( input.size(), 0 );

    input.append( make_buffer( 100, 'x' ) );
    input.append( make_buffer( 100, 'x' ) );

    char c = '0';
    for( int i = 0; i < 30; ++i )
    {
        ASSERT_EQ( input.size(), 200 );
        input.append( make_buffer( 100, c++ ) );
        ASSERT_EQ( input.size(), 300 );

        input.skip_bytes( 100 );  // Exhaust one buffer.
        ASSERT_EQ( input.size(), 200 );
    }
}

TEST( OpioProtoEntryPkgInput, OwnRoutinesViewPackageHeader )  // NOLINT
{
    pkg_input_t input{};

    auto header = pkg_header_t::make( pkg_content_message, 0, 0x00FFAA11 );
    opio::net::simple_buffer_t buf{ &header, sizeof( header ) };
    input.append( std::move( buf ) );

    const auto h = input.view_pkg_header();

    ASSERT_EQ( header.pkg_content_type, h.pkg_content_type );
    ASSERT_EQ( header.header_size_dwords, h.header_size_dwords );
    ASSERT_EQ( header.content_specific_value, h.content_specific_value );
    ASSERT_EQ( header.content_size, h.content_size );
    ASSERT_EQ( header.attached_binary_size, h.attached_binary_size );
}

TEST( OpioProtoEntryPkgInput,
      OwnRoutinesViewPackageHeaderDelimited )  // NOLINT
{
    pkg_input_t input{};

    auto header = pkg_header_t::make( pkg_content_message, 0, 0x00FFAA11 );
    opio::net::simple_buffer_t buf{ &header, sizeof( header ) };

    for( auto c : buf )
    {
        input.append( make_buffer( 1, c ) );
    }

    const auto h = input.view_pkg_header();

    ASSERT_EQ( header.pkg_content_type, h.pkg_content_type );
    ASSERT_EQ( header.header_size_dwords, h.header_size_dwords );
    ASSERT_EQ( header.content_specific_value, h.content_specific_value );
    ASSERT_EQ( header.content_size, h.content_size );
    ASSERT_EQ( header.attached_binary_size, h.attached_binary_size );
}

TEST( OpioProtoEntryPkgInput, ZCBufNext )  // NOLINT
{
    {
        pkg_input_t input{};

        ASSERT_EQ( input.size(), 0 );

        auto b                 = make_buffer( 100, 'x' );
        const void * orig_data = b.data();

        input.append( std::move( b ) );

        const void * data{};
        int size{};
        ASSERT_TRUE( input.Next( &data, &size ) );
        ASSERT_EQ( input.size(), 100 );

        EXPECT_EQ( data, orig_data );
        EXPECT_EQ( size, 100 );

        ASSERT_FALSE( input.Next( &data, &size ) );
        ASSERT_EQ( input.size(), 0 );

        ASSERT_EQ( input.ByteCount(), 100 );
    }

    {
        pkg_input_t input{};

        ASSERT_EQ( input.size(), 0 );

        auto b                  = make_buffer( 100, 'x' );
        const void * orig_data1 = b.data();
        input.append( std::move( b ) );

        b                       = make_buffer( 200, 'y' );
        const void * orig_data2 = b.data();
        input.append( std::move( b ) );

        const void * data{};
        int size{};
        ASSERT_TRUE( input.Next( &data, &size ) );
        ASSERT_EQ( input.ByteCount(), 100 );
        ASSERT_EQ( input.size(), 300 );

        EXPECT_EQ( data, orig_data1 );
        EXPECT_EQ( size, 100 );

        ASSERT_TRUE( input.Next( &data, &size ) );
        ASSERT_EQ( input.ByteCount(), 300 );
        ASSERT_EQ( input.size(), 200 );

        EXPECT_EQ( data, orig_data2 );
        EXPECT_EQ( size, 200 );

        ASSERT_FALSE( input.Next( &data, &size ) );
        ASSERT_EQ( input.size(), 0 );

        ASSERT_EQ( input.ByteCount(), 300 );
    }
}

TEST( OpioProtoEntryPkgInput, ZCBufBackUp )  // NOLINT
{
    {
        pkg_input_t input{};

        ASSERT_EQ( input.size(), 0 );

        auto b                 = make_buffer( 100, 'x' );
        const void * orig_data = b.data();

        input.append( std::move( b ) );

        const void * data{};
        int size{};
        ASSERT_EQ( input.ByteCount(), 0 );
        ASSERT_TRUE( input.Next( &data, &size ) );
        ASSERT_EQ( input.ByteCount(), 100 );
        ASSERT_EQ( input.size(), 100 );

        EXPECT_EQ( data, orig_data );
        EXPECT_EQ( size, 100 );

        input.BackUp( 99 );
        ASSERT_EQ( input.ByteCount(), 1 );
        ASSERT_EQ( input.size(), 99 );

        ASSERT_TRUE( input.Next( &data, &size ) );

        EXPECT_EQ( data,
                   &static_cast< const char * >( orig_data )[ 1 ] );  // NOLINT
        EXPECT_EQ( size, 99 );
    }
}

TEST( OpioProtoEntryPkgInput, ZCBufBackUpZero )  // NOLINT
{
    pkg_input_t input{};

    ASSERT_EQ( input.size(), 0 );

    auto b                 = make_buffer( 100, 'x' );
    const void * orig_data = b.data();

    input.append( std::move( b ) );

    const void * data{};
    int size{};
    ASSERT_EQ( input.ByteCount(), 0 );
    ASSERT_TRUE( input.Next( &data, &size ) );
    ASSERT_EQ( input.ByteCount(), 100 );
    ASSERT_EQ( input.size(), 100 );

    EXPECT_EQ( data, orig_data );
    EXPECT_EQ( size, 100 );

    input.BackUp( 0 );
    ASSERT_EQ( input.ByteCount(), 100 );
    ASSERT_EQ( input.size(), 0 );

    ASSERT_FALSE( input.Next( &data, &size ) );
}

TEST( OpioProtoEntryPkgInput, ZCBufSkip )  // NOLINT
{
    {
        pkg_input_t input{};

        ASSERT_EQ( input.size(), 0 );

        auto b                 = make_buffer( 100, 'x' );
        const void * orig_data = b.data();

        input.append( std::move( b ) );
        input.Skip( 0 );

        const void * data{};
        int size{};

        ASSERT_TRUE( input.Next( &data, &size ) );
        EXPECT_EQ( data, orig_data );
        EXPECT_EQ( size, 100 );
    }
    {
        pkg_input_t input{};

        ASSERT_EQ( input.size(), 0 );

        auto b                 = make_buffer( 100, 'x' );
        const void * orig_data = b.data();

        input.append( std::move( b ) );
        input.Skip( 10 );

        const void * data{};
        int size{};

        ASSERT_TRUE( input.Next( &data, &size ) );
        EXPECT_EQ( data,
                   &static_cast< const char * >( orig_data )[ 10 ] );  // NOLINT
        EXPECT_EQ( size, 90 );
    }
}

TEST( OpioProtoEntryPkgInput, ZCBufSkipAfterNext )  // NOLINT
{
    pkg_input_t input{};

    ASSERT_EQ( input.size(), 0 );

    input.append( make_buffer( 100, 'x' ) );

    auto b                  = make_buffer( 100, 'y' );
    const void * orig_data2 = b.data();
    input.append( std::move( b ) );

    const void * data{};
    int size{};
    ASSERT_TRUE( input.Next( &data, &size ) );

    input.Skip( 99 );
    ASSERT_EQ( input.ByteCount(), 199 );
    ASSERT_EQ( input.size(), 1 );

    ASSERT_TRUE( input.Next( &data, &size ) );

    EXPECT_EQ( data, &static_cast< const char * >( orig_data2 )[ 99 ] );  // NOLINT
    EXPECT_EQ( size, 1 );

    input.Skip( 0 );
    ASSERT_EQ( input.size(), 0 );
    ASSERT_FALSE( input.Next( &data, &size ) );
}

TEST( OpioProtoEntryPkgInput, ZCBufReadMessage )  // NOLINT
{
    namespace proto = opio::proto_entry::utest;

    proto::XxxRequest msg;

    msg.set_req_id( 42 );
    msg.set_aaa( 100 );
    msg.set_bbb( 0xFF000000 );
    msg.set_ccc( 0xDEADBEEF );

    msg.add_strings( "0123456789012345678901234567890123456789" );
    msg.add_strings( "0123456789 0123456789 0123456789 0123456789" );
    msg.add_strings( "--0123456789012345678901234567890123456789" );
    msg.add_strings( "++0123456789012345678901234567890123456789" );
    msg.add_strings( "##0123456789012345678901234567890123456789" );
    msg.add_strings( "987654321098765432109876543210" );
    msg.add_strings( "zxcasdqwe" );
    msg.add_strings( "opio::proto_entry::utest 1" );
    msg.add_strings( "opio::proto_entry::utest 2" );
    msg.add_strings( "opio::proto_entry::utest 3" );

    opio::net::simple_buffer_t buf( msg.ByteSizeLong() );
    ASSERT_TRUE( msg.SerializeToArray( buf.data(), buf.size() ) );

    pkg_input_t input{};
    input.append( std::move( buf ) );

    EXPECT_LT( 0, input.size() );

    proto::XxxRequest new_msg;
    ASSERT_TRUE( new_msg.ParseFromZeroCopyStream( &input ) );

    ASSERT_EQ( new_msg.req_id(), 42 );
    ASSERT_EQ( new_msg.aaa(), 100 );
    ASSERT_EQ( new_msg.bbb(), 0xFF000000 );
    ASSERT_EQ( new_msg.ccc(), 0xDEADBEEF );
    ASSERT_EQ( new_msg.strings_size(), 10 );
    ASSERT_EQ( new_msg.strings( 0 ), "0123456789012345678901234567890123456789" );
    ASSERT_EQ( new_msg.strings( 9 ), "opio::proto_entry::utest 3" );
}

TEST( OpioProtoEntryPkgInput, ZCBufReadMessageDelimited )  // NOLINT
{
    namespace proto = opio::proto_entry::utest;

    proto::XxxRequest msg;

    msg.set_req_id( 42 );
    msg.set_aaa( 100 );
    msg.set_bbb( 0xFF000000 );
    msg.set_ccc( 0xDEADBEEF );

    msg.add_strings( "0123456789012345678901234567890123456789" );
    msg.add_strings( "0123456789 0123456789 0123456789 0123456789" );
    msg.add_strings( "--0123456789012345678901234567890123456789" );
    msg.add_strings( "++0123456789012345678901234567890123456789" );
    msg.add_strings( "##0123456789012345678901234567890123456789" );
    msg.add_strings( "987654321098765432109876543210" );
    msg.add_strings( "zxcasdqwe" );
    msg.add_strings( "opio::proto_entry::utest 1" );
    msg.add_strings( "opio::proto_entry::utest 2" );
    msg.add_strings( "opio::proto_entry::utest 3" );

    opio::net::simple_buffer_t buf( msg.ByteSizeLong() );
    ASSERT_TRUE( msg.SerializeToArray( buf.data(), buf.size() ) );

    opio::net::simple_buffer_t buf2{ buf.offset_data( buf.size() / 2 ),
                                     buf.size() - ( buf.size() / 2 ) };

    buf.resize( buf.size() / 2 );
    pkg_input_t input{};
    input.append( std::move( buf ) );
    input.append( std::move( buf2 ) );

    EXPECT_LT( 0, input.size() );
    EXPECT_EQ( msg.ByteSizeLong(), input.size() );

    proto::XxxRequest new_msg;
    ASSERT_TRUE( new_msg.ParseFromZeroCopyStream( &input ) );

    ASSERT_EQ( new_msg.req_id(), 42 );
    ASSERT_EQ( new_msg.aaa(), 100 );
    ASSERT_EQ( new_msg.bbb(), 0xFF000000 );
    ASSERT_EQ( new_msg.ccc(), 0xDEADBEEF );
    ASSERT_EQ( new_msg.strings_size(), 10 );
    ASSERT_EQ( new_msg.strings( 0 ), "0123456789012345678901234567890123456789" );
    ASSERT_EQ( new_msg.strings( 9 ), "opio::proto_entry::utest 3" );
}

TEST( OpioProtoEntryPkgInput, ZCBufReadMessageExtreemlyDelimited )  // NOLINT
{
    namespace proto = opio::proto_entry::utest;

    proto::XxxRequest msg;

    msg.set_req_id( 42 );
    msg.set_aaa( 100 );
    msg.set_bbb( 0xFF000000 );
    msg.set_ccc( 0xDEADBEEF );

    msg.add_strings( "0123456789012345678901234567890123456789" );
    msg.add_strings( "0123456789 0123456789 0123456789 0123456789" );
    msg.add_strings( "--0123456789012345678901234567890123456789" );
    msg.add_strings( "++0123456789012345678901234567890123456789" );
    msg.add_strings( "##0123456789012345678901234567890123456789" );
    msg.add_strings( "987654321098765432109876543210" );
    msg.add_strings( "zxcasdqwe" );
    msg.add_strings( "opio::proto_entry::utest 1" );
    msg.add_strings( "opio::proto_entry::utest 2" );
    msg.add_strings( "opio::proto_entry::utest 3" );

    opio::net::simple_buffer_t buf( msg.ByteSizeLong() );
    ASSERT_TRUE( msg.SerializeToArray( buf.data(), buf.size() ) );

    // The ring buffer should allow to have a 1b buffer
    // for every byte in the serialized image
    pkg_input_t< opio::net::simple_buffer_t, 512 > input{};
    for( auto c : buf )
    {
        input.append( make_buffer( 1, c ) );
    }

    EXPECT_LT( 0, input.size() );

    proto::XxxRequest new_msg;
    ASSERT_TRUE( new_msg.ParseFromZeroCopyStream( &input ) );

    ASSERT_EQ( new_msg.req_id(), 42 );
    ASSERT_EQ( new_msg.aaa(), 100 );
    ASSERT_EQ( new_msg.bbb(), 0xFF000000 );
    ASSERT_EQ( new_msg.ccc(), 0xDEADBEEF );
    ASSERT_EQ( new_msg.strings_size(), 10 );
    ASSERT_EQ( new_msg.strings( 0 ), "0123456789012345678901234567890123456789" );
    ASSERT_EQ( new_msg.strings( 9 ), "opio::proto_entry::utest 3" );
}

TEST( OpioProtoEntryPkgInput, ReadBufferSimple )  // NOLINT
{
    pkg_input_t< opio::net::simple_buffer_t, 512 > input{};

    opio::net::simple_buffer_t buffer{ 100 };
    for( auto i = 0UL; i < buffer.size(); ++i )
    {
        buffer.data()[ i ] = static_cast< std::byte >( 100 + i );
    }

    input.append( buffer.make_copy() );

    const std::size_t head_size = 64;
    const std::size_t tail_size = buffer.size() - head_size;

    opio::net::simple_buffer_t read_buf{ head_size };

    input.read_buffer( read_buf.data(), read_buf.size() );

    ASSERT_EQ( input.size(), tail_size );
    for( auto i = 0UL; i < read_buf.size(); ++i )
    {
        EXPECT_EQ( *( read_buf.data() + i ), *( buffer.data() + i ) )
            << "i = " << i;
    }
}

TEST( OpioProtoEntryPkgInput, ReadBufferDelimited )  // NOLINT
{
    pkg_input_t< opio::net::simple_buffer_t, 512 > input{};

    opio::net::simple_buffer_t buffer{ 120 };
    for( auto i = 0UL; i < buffer.size(); ++i )
    {
        buffer.data()[ i ] = static_cast< std::byte >( 100 + i );
    }

    auto add_buf = [ &, n = 0UL ]( std::size_t s ) mutable {
        input.append( opio::net::simple_buffer_t{ buffer.data() + n, s } );
        n += s;
    };

    add_buf( 4 );
    add_buf( 3 );
    add_buf( 2 );
    add_buf( 1 );

    add_buf( 1 );
    add_buf( 2 );
    add_buf( 3 );
    add_buf( 4 );

    add_buf( 14 );
    add_buf( 13 );
    add_buf( 12 );
    add_buf( 11 );

    add_buf( 11 );
    add_buf( 12 );
    add_buf( 13 );
    add_buf( 14 );

    const std::size_t head_size = 64;
    const std::size_t tail_size = buffer.size() - head_size;

    opio::net::simple_buffer_t read_buf{ head_size };

    input.read_buffer( read_buf.data(), read_buf.size() );

    EXPECT_EQ( input.size(), tail_size );
    for( auto i = 0UL; i < read_buf.size(); ++i )
    {
        EXPECT_EQ( *( read_buf.data() + i ), *( buffer.data() + i ) )
            << "i = " << i;
    }
}

}  // anonymous namespace
