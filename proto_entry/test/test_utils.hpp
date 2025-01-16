#include <chrono>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <opio/net/asio_include.hpp>

#include <opio/proto_entry/utest/entry.hpp>

#include <tcp_test_utils.hpp>

//
// message_consumer_mock_t
//

/**
 * @brief A mockable message_consumer.
 */
class message_consumer_mock_t
{
public:
    // A proxy for mock methods.
    // Intended to skip Entry parameter.
    template < typename Entry, typename Message_Carrier >
    void on_message( Message_Carrier msg, [[maybe_unused]] Entry & e )
    {
        if( msg.attached_buffer().size() > 0 )
        {
            on_message_with_attached_bin( std::move( *msg ),
                                          std::move( msg.attached_buffer() ) );
        }
        else
        {
            on_message( std::move( *msg ) );
        }
    }

    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    MOCK_METHOD( void, on_message, ( opio::proto_entry::utest::XxxRequest ) );

    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    MOCK_METHOD( void, on_message, ( opio::proto_entry::utest::YyyRequest ) );

    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    MOCK_METHOD( void, on_message, ( opio::proto_entry::utest::ZzzRequest ) );

    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    MOCK_METHOD( void, on_message, ( opio::proto_entry::utest::BothWayMessage ) );

    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    MOCK_METHOD( void,
                 on_message_with_attached_bin,
                 ( ::opio::proto_entry::utest::XxxRequest,
                   ::opio::net::simple_buffer_t ) );

    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    MOCK_METHOD( void,
                 on_message_with_attached_bin,
                 ( ::opio::proto_entry::utest::YyyRequest,
                   ::opio::net::simple_buffer_t ) );

    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    MOCK_METHOD( void,
                 on_message_with_attached_bin,
                 ( ::opio::proto_entry::utest::ZzzRequest,
                   ::opio::net::simple_buffer_t ) );

    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    MOCK_METHOD( void,
                 on_message_with_attached_bin,
                 ( ::opio::proto_entry::utest::BothWayMessage,
                   ::opio::net::simple_buffer_t ) );
};

//
// client_message_consumer_mock_t
//

class client_message_consumer_mock_t
{
public:
    // A proxy for mock methods.
    // Intended to skip Entry parameter.
    template < typename Entry, typename Message_Carrier >
    void on_message( Message_Carrier msg, [[maybe_unused]] Entry & e )
    {
        on_message( std::move( *msg ) );
    }

    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    MOCK_METHOD( void, on_message, ( opio::proto_entry::utest::XxxReply ) );

    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    MOCK_METHOD( void, on_message, ( opio::proto_entry::utest::YyyReply ) );

    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    MOCK_METHOD( void, on_message, ( opio::proto_entry::utest::ZzzReply ) );

    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    MOCK_METHOD( void, on_message, ( opio::proto_entry::utest::BothWayMessage ) );
};
