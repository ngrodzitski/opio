#include <type_traits>

#include <opio/proto_entry/impl/protobuf_parsing_engines.hpp>

#include <opio/proto_entry/utest/entry.hpp>

#include <gmock/gmock.h>

namespace /* anonymous */
{

using namespace opio::proto_entry;  // NOLINT

TEST( OpioProtoEntryImplProtobufParsingEngine, Enum2TypeLUT )  // NOLINT
{
    using msg_t = opio::proto_entry::utest::YyyRequest;

    const auto trivial_parsing_engine_lut_as_expected = std::is_same_v<
        impl::protobuf_parsing_engine_t< protobuf_parsing_strategy::trivial,
                                         msg_t >,
        impl::protobuf_trivial_parsing_engine_t< msg_t > >;

    EXPECT_TRUE( trivial_parsing_engine_lut_as_expected );

    const auto with_arena_parsing_engine_lut_as_expected = std::is_same_v<
        impl::protobuf_parsing_engine_t< protobuf_parsing_strategy::with_arena,
                                         msg_t >,
        impl::protobuf_with_arena_parsing_engine_t< msg_t > >;

    EXPECT_TRUE( with_arena_parsing_engine_lut_as_expected );
}

class OpioProtoEntryImplProtobufParsingEngines : public testing::Test
{
protected:
    opio::net::simple_buffer_t buf;

    static inline constexpr std::uint32_t req_id_value = 1010;

    void SetUp() override
    {
        utest::YyyRequest msg;
        msg.set_req_id( req_id_value );

        buf.resize_drop_data( msg.ByteSizeLong() );

        msg.SerializeToArray( buf.data(),
                              static_cast< int >( msg.ByteSizeLong() ) );
    }
};

TEST_F( OpioProtoEntryImplProtobufParsingEngines, TrivialParsing )  // NOLINT
{
    impl::protobuf_parsing_engine_t< protobuf_parsing_strategy::trivial,
                                     utest::YyyRequest >
        engine{};

    google::protobuf::io::ArrayInputStream input{
        buf.data(), static_cast< int >( buf.size() )
    };
    auto parse_res = engine.parse_package( input );
    ASSERT_TRUE( parse_res );

    EXPECT_EQ( parse_res->message().req_id(), req_id_value );

    auto msg = parse_res->carry_message();
    EXPECT_EQ( msg->req_id(), req_id_value );
}

TEST_F( OpioProtoEntryImplProtobufParsingEngines, WithArenaParsing )  // NOLINT
{
    impl::protobuf_parsing_engine_t< protobuf_parsing_strategy::with_arena,
                                     utest::YyyRequest >
        engine{};

    google::protobuf::io::ArrayInputStream input{
        buf.data(), static_cast< int >( buf.size() )
    };
    auto parse_res = engine.parse_package( input );
    ASSERT_TRUE( parse_res );

    EXPECT_EQ( parse_res->message().req_id(), req_id_value );

    auto msg = parse_res->carry_message();
    EXPECT_EQ( msg->req_id(), req_id_value );
}

}  // anonymous namespace
