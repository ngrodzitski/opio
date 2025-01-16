set(test_prj _utest.logger)

project(${test_prj})

list(APPEND  unittests_srcfiles
    cfg_json.cpp
)

add_executable(${test_prj} ${unittests_srcfiles})

if (WIN32)
    target_compile_definitions(${test_prj} PRIVATE _CRT_SECURE_NO_WARNINGS)
endif ()

target_link_libraries(${test_prj}
                      PRIVATE
                      GTest::gtest_main
                      opio::logger

                      opio::test_utils
)

include(GoogleTest)

# Distinguishing tests for CI ([Debug, Release, ...] x [devtoolset, other gcc, clang],,,)
# If the variable is defined test report would also include this tag
# as a part of the test name.
gtest_discover_tests(${test_prj}
                     TEST_PREFIX "${OPIO_TEST_PREFIX}")
