#include <opio/net/operation_watchdog.hpp>

#include <thread>

#include <gtest/gtest.h>

namespace /* anonymous */
{

using namespace opio::net;  // NOLINT

TEST( OpioNet, OperationWatchdogTriggerFirst )  // NOLINT
{
    asio_ns::io_context ioctx;
    asio_timer_operation_watchdog_t wd{ ioctx };
    wd.start_watch_operation( std::chrono::milliseconds( 10 ), [ & ]( auto k ) {
        ASSERT_EQ( k, wd.timeout_key() );
    } );
    ioctx.run();
}

TEST( OpioNet, OperationWatchdogNoTrigger )  // NOLINT
{
    asio_ns::io_context ioctx;
    asio_timer_operation_watchdog_t wd{ ioctx };
    wd.start_watch_operation( std::chrono::milliseconds( 10 ), []( auto /*k*/ ) {
        std::runtime_error{ "SHOULD NOT BE CALLED!" };
    } );
    auto original_key = wd.timeout_key();
    ioctx.poll();
    std::this_thread::sleep_for( std::chrono::milliseconds( 3 ) );
    ASSERT_EQ( original_key, wd.timeout_key() );
    wd.cancel_watch_operation();
    ioctx.run();
    ASSERT_NE( original_key, wd.timeout_key() );
}

TEST( OpioNet, OperationWatchdogTriggerSecond )  // NOLINT
{
    asio_ns::io_context ioctx;
    asio_timer_operation_watchdog_t wd{ ioctx };

    wd.start_watch_operation( std::chrono::milliseconds( 10 ), []( auto /*k*/ ) {
        std::runtime_error{ "SHOULD NOT BE CALLED!" };
    } );
    auto first_key = wd.timeout_key();

    ioctx.poll();
    std::this_thread::sleep_for( std::chrono::milliseconds( 3 ) );
    ASSERT_EQ( first_key, wd.timeout_key() );
    wd.cancel_watch_operation();

    wd.start_watch_operation( std::chrono::milliseconds( 10 ), [ & ]( auto k ) {
        ASSERT_EQ( k, wd.timeout_key() );
    } );
    auto second_key = wd.timeout_key();

    ASSERT_NE( first_key, second_key );
    ioctx.run();
}

}  // anonymous namespace
