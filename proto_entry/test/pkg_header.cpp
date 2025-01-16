#include <cstring>

#include <opio/proto_entry/pkg_header.hpp>

#include <gtest/gtest.h>

namespace /* anonymous */
{

using namespace opio::proto_entry;  // NOLINT

TEST( OpioProtoEntry, PkgHeaderBinaryProperties )  // NOLINT
{
    ASSERT_EQ( sizeof( pkg_header_t ), pkg_header_t::image_size_dwords * 4 )
        << "pkg_header_t struct is mapped to binary, "
           "so we expect a specific size exactly";

    const std::uint16_t x = 0x0001;
    std::uint8_t b0       = 0;
    std::memcpy( &b0, &x, 1 );
    ASSERT_EQ( b0, 1 ) << "!!! Platform must be Little Endian...\n"
                          "Current implementation considers 4b integer values "
                          "that constitute pkg header are not translated to BE "
                          "and so the platform must be LE ";
}

}  // anonymous namespace
