set(test_prj _utest.net)

project(${test_prj})

add_library(utest.net.reusable_utils
            STATIC
            reusable_utils/tcp_test_utils.hpp
            reusable_utils/tcp_test_utils.cpp
)
add_library(opio::utest.net.reusable_utils ALIAS utest.net.reusable_utils)
target_include_directories(utest.net.reusable_utils
                           PUBLIC
                           ${CMAKE_CURRENT_SOURCE_DIR}/reusable_utils
)
target_link_libraries(utest.net.reusable_utils
                      PUBLIC
                      opio::net
                      opio::test_utils
)

list(APPEND  unittests_srcfiles
    buffer.cpp
    heterogeneous_buffer.cpp
    network_iface_to_addr.cpp
    operation_watchdog.cpp
    try_make_addr.cpp

    udp/cfg_json.cpp
    udp/udp_message_receiver.cpp

    tcp/cfg_json.cpp
    tcp/cfg.cpp
    tcp/utils.cpp
    tcp/acceptor.cpp
    tcp/connector.cpp
    tcp/connection.cpp
    tcp/connection_ctor_params.cpp
    tcp/connection_hetero_buffer.cpp
    tcp/connection_skip_transferred_part.cpp
    tcp/connection_sync_async_write_switching.cpp
    tcp/connection_sync_write_heuristic_eq_0.cpp
    tcp/connection_write_timeout.cpp
    tcp/connection_xxx_send.cpp
    tcp/single_writable_sequence.cpp
    tcp/stats.cpp
)

add_executable(${test_prj} ${unittests_srcfiles})

if (WIN32)
    target_compile_definitions(${test_prj} PRIVATE _CRT_SECURE_NO_WARNINGS)
endif ()

list(APPEND unittests_deps
            GTest::gtest_main
            opio::net
            opio::test_utils
            opio::utest.net.reusable_utils
)

target_link_libraries(${test_prj} PRIVATE ${unittests_deps})

add_executable(${test_prj}.default_mutex_lock ${unittests_srcfiles})
target_link_libraries(${test_prj}.default_mutex_lock PRIVATE ${unittests_deps})
target_compile_definitions(${test_prj}.default_mutex_lock
                           PUBLIC
                           OPIO_NET_FORCE_DEFAULT_LOCKING_WITH_MUTEX)

include(GoogleTest)

# Distinguishing tests for CI ([Debug, Release, ...] x [devtoolset, other gcc, clang],,,)
# If the variable is defined test report would also include this tag
# as a part of the test name.
gtest_discover_tests(${test_prj}
                     TEST_PREFIX "${OPIO_TEST_PREFIX}")


gtest_discover_tests(${test_prj}.default_mutex_lock
                     TEST_PREFIX "With_mutex_lock-${OPIO_TEST_PREFIX}")
