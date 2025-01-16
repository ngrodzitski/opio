#include <opio/proto_entry/message_carrier.hpp>

#include <gtest/gtest.h>

#include "utest.pb.h"

namespace /* anonymous */
{

using namespace opio::net;          // NOLINT
using namespace opio::proto_entry;  // NOLINT

TEST( OpioProtoEntryMessageCarrier, TrivialProxy )  // NOLINT
{
    namespace proto = opio::proto_entry::utest;

    proto::XxxRequest msg;

    msg.set_req_id( 42 );
    msg.set_aaa( 100 );
    msg.set_bbb( 0xFF000000 );
    msg.set_ccc( 0xDEADBEEF );

    msg.add_strings( "0123456789012345678901234567890123456789" );
    msg.add_strings( "0123456789 0123456789 0123456789 0123456789" );

    trivial_proxy_message_carrier_t msg_carrier{ std::move( msg ) };

    ASSERT_EQ( msg_carrier->req_id(), 42 );
    ASSERT_EQ( msg_carrier->aaa(), 100 );
    ASSERT_EQ( msg_carrier->bbb(), 0xFF000000 );
    ASSERT_EQ( msg_carrier->ccc(), 0xDEADBEEF );
    ASSERT_EQ( ( *msg_carrier ).strings_size(), 2 );
    ASSERT_EQ( ( *msg_carrier ).strings( 0 ),
               "0123456789012345678901234567890123456789" );
    ASSERT_EQ( ( *msg_carrier ).strings( 1 ),
               "0123456789 0123456789 0123456789 0123456789" );

    ASSERT_EQ( msg_carrier.attached_buffer().size(), 0 );
}

TEST( OpioProtoEntryMessageCarrier, TrivialProxyWithAttachedBuffer )  // NOLINT
{
    namespace proto = opio::proto_entry::utest;

    proto::XxxRequest msg;

    msg.set_req_id( 42 );
    msg.set_aaa( 100 );
    msg.set_bbb( 0xFF000000 );
    msg.set_ccc( 0xDEADBEEF );

    msg.add_strings( "0123456789012345678901234567890123456789" );
    msg.add_strings( "0123456789 0123456789 0123456789 0123456789" );

    auto attached_bin = simple_buffer_t::make_from(
        { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' } );

    trivial_proxy_message_carrier_t msg_carrier{ std::move( msg ),
                                                 std::move( attached_bin ) };

    ASSERT_EQ( msg_carrier->req_id(), 42 );
    ASSERT_EQ( msg_carrier->aaa(), 100 );
    ASSERT_EQ( msg_carrier->bbb(), 0xFF000000 );
    ASSERT_EQ( msg_carrier->ccc(), 0xDEADBEEF );
    ASSERT_EQ( ( *msg_carrier ).strings_size(), 2 );
    ASSERT_EQ( ( *msg_carrier ).strings( 0 ),
               "0123456789012345678901234567890123456789" );
    ASSERT_EQ( ( *msg_carrier ).strings( 1 ),
               "0123456789 0123456789 0123456789 0123456789" );

    ASSERT_EQ( msg_carrier.attached_buffer().size(), 10 );
    ASSERT_EQ( msg_carrier.attached_buffer().make_string_view(), "0123456789" );

    auto bin = std::move( msg_carrier.attached_buffer() );
    ASSERT_EQ( bin.size(), 10 );
    ASSERT_EQ( bin.make_string_view(), "0123456789" );
    ASSERT_EQ( msg_carrier.attached_buffer().size(), 0 );
}

TEST( OpioProtoEntryMessageCarrier, WithArena )  // NOLINT
{
    namespace proto = opio::proto_entry::utest;

    auto arena = std::make_unique< google::protobuf::Arena >();

    auto * msg =
        google::protobuf::Arena::CreateMessage< proto::XxxRequest >( arena.get() );

    msg->set_req_id( 42 );
    msg->set_aaa( 100 );
    msg->set_bbb( 0xFF000000 );
    msg->set_ccc( 0xDEADBEEF );

    msg->add_strings( "0123456789012345678901234567890123456789" );
    msg->add_strings( "0123456789 0123456789 0123456789 0123456789" );

    with_arena_message_carrier_t msg_carrier{ msg, std::move( arena ) };
    ASSERT_EQ( msg, msg_carrier.get() );

    ASSERT_EQ( msg_carrier->req_id(), 42 );
    decltype( msg_carrier ) msg_carrier_second{ std::move( msg_carrier ) };
    ASSERT_EQ( msg, msg_carrier_second.get() );
    ASSERT_EQ( nullptr, msg_carrier.get() );

    msg_carrier = std::move( msg_carrier_second );
    ASSERT_EQ( msg, msg_carrier.get() );
    ASSERT_EQ( nullptr, msg_carrier_second.get() );

    ASSERT_EQ( msg_carrier->aaa(), 100 );
    ASSERT_EQ( msg_carrier->bbb(), 0xFF000000 );
    ASSERT_EQ( msg_carrier->ccc(), 0xDEADBEEF );
    ASSERT_EQ( ( *msg_carrier ).strings_size(), 2 );
    ASSERT_EQ( ( *msg_carrier ).strings( 0 ),
               "0123456789012345678901234567890123456789" );
    ASSERT_EQ( ( *msg_carrier ).strings( 1 ),
               "0123456789 0123456789 0123456789 0123456789" );

    ASSERT_EQ( msg_carrier.attached_buffer().size(), 0 );
}

TEST( OpioProtoEntryMessageCarrier, WithArenaWithAttachedBuffer )  // NOLINT
{
    namespace proto = opio::proto_entry::utest;

    auto arena = std::make_unique< google::protobuf::Arena >();

    auto * msg =
        google::protobuf::Arena::CreateMessage< proto::XxxRequest >( arena.get() );

    msg->set_req_id( 42 );
    msg->set_aaa( 100 );
    msg->set_bbb( 0xFF000000 );
    msg->set_ccc( 0xDEADBEEF );

    msg->add_strings( "0123456789012345678901234567890123456789" );
    msg->add_strings( "0123456789 0123456789 0123456789 0123456789" );

    auto attached_bin = simple_buffer_t::make_from(
        { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' } );
    with_arena_message_carrier_t msg_carrier{ msg,
                                              std::move( arena ),
                                              std::move( attached_bin ) };
    ASSERT_EQ( msg, msg_carrier.get() );

    ASSERT_EQ( msg_carrier->req_id(), 42 );
    decltype( msg_carrier ) msg_carrier_second{ std::move( msg_carrier ) };
    ASSERT_EQ( msg, msg_carrier_second.get() );
    ASSERT_EQ( nullptr, msg_carrier.get() );

    msg_carrier = std::move( msg_carrier_second );
    ASSERT_EQ( msg, msg_carrier.get() );
    ASSERT_EQ( nullptr, msg_carrier_second.get() );

    ASSERT_EQ( msg_carrier->aaa(), 100 );
    ASSERT_EQ( msg_carrier->bbb(), 0xFF000000 );
    ASSERT_EQ( msg_carrier->ccc(), 0xDEADBEEF );
    ASSERT_EQ( ( *msg_carrier ).strings_size(), 2 );
    ASSERT_EQ( ( *msg_carrier ).strings( 0 ),
               "0123456789012345678901234567890123456789" );
    ASSERT_EQ( ( *msg_carrier ).strings( 1 ),
               "0123456789 0123456789 0123456789 0123456789" );

    ASSERT_EQ( msg_carrier.attached_buffer().size(), 10 );
    ASSERT_EQ( msg_carrier.attached_buffer().make_string_view(), "0123456789" );

    auto bin = std::move( msg_carrier.attached_buffer() );
    ASSERT_EQ( bin.size(), 10 );
    ASSERT_EQ( bin.make_string_view(), "0123456789" );
    ASSERT_EQ( msg_carrier.attached_buffer().size(), 0 );
}

}  // anonymous namespace
