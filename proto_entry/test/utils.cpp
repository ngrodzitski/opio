#include <opio/proto_entry/utils.hpp>

#include <opio/proto_entry/utest/entry.hpp>

#include <gtest/gtest.h>

namespace /* anonymous */
{

using namespace opio::proto_entry;  // NOLINT

TEST( OpioProtoEntryUtils, MakePackageImage )  // NOLINT
{
    opio::proto_entry::utest::YyyReply msg;
    msg.set_req_id( 101 );

    auto image = opio::proto_entry::utest::make_package_image( msg );

    ASSERT_LT( sizeof( pkg_header_t ), image.size() );

    pkg_header_t header;  // NOLINT

    std::memcpy( &header, image.data(), sizeof( header ) );

    EXPECT_EQ( header.pkg_content_type, opio::proto_entry::pkg_content_message );
    EXPECT_EQ( header.content_specific_value,
               opio::proto_entry::utest::YYY_REPLY );

    opio::proto_entry::utest::YyyReply msg2;
    msg2.ParseFromArray( image.offset_data( sizeof( header ) ),
                         static_cast< int >( header.advertized_header_size() ) );

    EXPECT_EQ( msg.req_id(), msg2.req_id() );
}

}  // anonymous namespace
