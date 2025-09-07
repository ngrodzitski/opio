#include <opio/proto_entry/utest/entry.hpp>

#include <gtest/gtest.h>

namespace /* anonymous */
{

using namespace ::testing;          // NOLINT
using namespace opio::proto_entry;  // NOLINT

// NOLINTNEXTLINE
TEST( OpioProtoEntryMetaProgrammingHelpers, MsgTypesLut )
{
    using lut_t = utest::msg_type_lut_t< utest::XxxRequest >;
    EXPECT_EQ( lut_t::enum_value, utest::MessageType::XXX_REQUEST );
    EXPECT_EQ( lut_t::class_name, "::opio::proto_entry::utest::XxxRequest" );
    EXPECT_EQ( lut_t::class_name_short, "XxxRequest" );
    EXPECT_EQ( lut_t::enum_name,
               "::opio::proto_entry::utest::MessageType::XXX_REQUEST" );
    EXPECT_EQ( lut_t::enum_name_short, "XXX_REQUEST" );
}

// NOLINTNEXTLINE
TEST( OpioProtoEntryMetaProgrammingHelpers, EnumValueLut )
{
    {
        using lut_t = utest::enum_value_lut_t< utest::MessageType::XXX_REQUEST >;

        bool res = std::is_same_v< lut_t::msg_type_t, utest::XxxRequest >;
        EXPECT_TRUE( res );
    }
    {
        using lut_t = utest::enum_value_lut_t< utest::MessageType::BOTH_WAY >;

        bool res = std::is_same_v< lut_t::msg_type_t, utest::BothWayMessage >;
        EXPECT_TRUE( res );
    }
    {
        using lut_t = utest::enum_value_lut_t< utest::MessageType::YYY_REPLY >;

        bool res = std::is_same_v< lut_t::msg_type_t, utest::YyyReply >;
        EXPECT_TRUE( res );
    }
}

// Adds a string name of the type and provide
// a bool return value to indicate a "break" condition.
template < size_t I >
void make_msg_type_str( std::string & out )
{
    using msg_t = typename utest::enum_value_lut_t<
        utest::incoming_enums_list[ I ] >::msg_type_t;
    using lut_t = utest::msg_type_lut_t< msg_t >;

    out += lut_t::class_name_short;
    out += ", ";
}

// A sample algo to traverse the types of incoming messages.
template < size_t... ix >
std::string test_incoming_msg_arr( std::integer_sequence< std::size_t, ix... > )
{
    std::string res{};

    auto run = [ & ] { return ( make_msg_type_str< ix >( res ), ... ); };

    run();

    return res;
}

// NOLINTNEXTLINE
TEST( OpioProtoEntryMetaProgrammingHelpers, LambdaOverIncoming )
{
    auto s = test_incoming_msg_arr(
        std::make_integer_sequence< std::size_t,
                                    utest::msg_types_count_incoming >{} );

    EXPECT_EQ( s, "BothWayMessage, XxxRequest, YyyRequest, ZzzRequest, " );
}

}  // anonymous namespace
