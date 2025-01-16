#include <opio/proto_entry/entry_base.hpp>

#include <gtest/gtest.h>

namespace /* anonymous */
{

using namespace opio::proto_entry;  // NOLINT

class fake_consumer_t
{
public:
    explicit fake_consumer_t( int n )
        : m_secret{ n }
    {
    }

    fake_consumer_t( const fake_consumer_t & ) = delete;
    fake_consumer_t( fake_consumer_t && )      = delete;
    fake_consumer_t & operator=( const fake_consumer_t & ) = delete;
    fake_consumer_t & operator=( fake_consumer_t && ) = delete;

    int secret() const noexcept { return m_secret; }

private:
    int m_secret;
};

TEST( OpioProtoEntryExecuteForReference, WithInstance )  // NOLINT
{
    constexpr auto secret = 99;
    fake_consumer_t consumer{ secret };
    details::execute_for_reference( consumer, [ & ]( auto & c ) {
        EXPECT_EQ( &consumer, &c );
        EXPECT_EQ( consumer.secret(), c.secret() );
    } );
}

TEST( OpioProtoEntryExecuteForReference, WithPointer )  // NOLINT
{
    constexpr auto secret = 332;
    fake_consumer_t consumer{ secret };
    fake_consumer_t * consumer_ptr = &consumer;

    details::execute_for_reference( consumer_ptr, [ & ]( auto & c ) {
        EXPECT_EQ( &consumer, &c );
        EXPECT_EQ( consumer.secret(), c.secret() );
    } );
}

TEST( OpioProtoEntryExecuteForReference, WithUniquePointer )  // NOLINT
{
    constexpr auto secret = 133;
    auto consumer         = std::make_unique< fake_consumer_t >( secret );

    details::execute_for_reference( consumer, [ & ]( auto & c ) {
        EXPECT_EQ( consumer.get(), &c );
        EXPECT_EQ( consumer->secret(), c.secret() );
    } );
}

TEST( OpioProtoEntryExecuteForReference, WithSharedPointer )  // NOLINT
{
    constexpr auto secret = 4321;
    auto consumer         = std::make_shared< fake_consumer_t >( secret );

    details::execute_for_reference( consumer, [ & ]( auto & c ) {
        EXPECT_EQ( consumer.get(), &c );
        EXPECT_EQ( consumer->secret(), c.secret() );
    } );
}

TEST( OpioProtoEntryExecuteForReference, WithWeakPointer )  // NOLINT
{
    constexpr auto secret = 777;
    auto consumer         = std::make_shared< fake_consumer_t >( secret );
    std::weak_ptr< fake_consumer_t > wp{ consumer };

    details::execute_for_reference( wp, [ & ]( auto & c ) {
        EXPECT_EQ( consumer.get(), &c );
        EXPECT_EQ( consumer->secret(), c.secret() );
    } );
}

}  // anonymous namespace
