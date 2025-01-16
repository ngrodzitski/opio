set(test_prj _utest.opio.proto_entry)

project(${test_prj})

# ==============================================================================
# Run protobuf files generation
add_library(utest_proto OBJECT "${CMAKE_CURRENT_LIST_DIR}/utest.proto")

target_link_libraries(utest_proto PUBLIC protobuf::libprotobuf)
set(utest_generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")

file(MAKE_DIRECTORY ${utest_generated_dir})

target_include_directories(utest_proto
                          PUBLIC
                          ${utest_generated_dir}
)

protobuf_generate(
    LANGUAGE cpp
    TARGET utest_proto
    IMPORT_DIRS "${CMAKE_CURRENT_LIST_DIR}"
    PROTOC_OUT_DIR "${utest_generated_dir}")

protobuf_generate(
    LANGUAGE python
    TARGET utest_proto
    IMPORT_DIRS "${CMAKE_CURRENT_LIST_DIR}"
    PROTOC_OUT_DIR "${utest_generated_dir}")

# Disable static analysis for generated files.
set_target_properties(utest_proto PROPERTIES CXX_CLANG_TIDY "")
set_target_properties(utest_proto PROPERTIES CXX_CPPCHECK "")
# ==============================================================================

proto_entry_generate_protocol_entry(
    CLIENT_SERVER_ROLE  server
    TARGET_NAME      generated_utest_proto_spec
    OUTPUT_DIR       ${utest_generated_dir}/opio/proto_entry/utest
    OUTPUT_NAMESPACE "opio::proto_entry::utest"
    GENERATED_FILES  generated_utest_proto_spec_headers
    INPUT_PACKAGE    "utest_pb2"
    PY_ADD_SYS_PATH  ${utest_generated_dir}
)
add_dependencies(generated_utest_proto_spec utest_proto)

proto_entry_generate_protocol_entry(
    CLIENT_SERVER_ROLE  client
    TARGET_NAME      generated_utest_client_proto_spec
    OUTPUT_DIR       ${utest_generated_dir}/opio/proto_entry/utest_client
    OUTPUT_NAMESPACE "opio::proto_entry::utest_client"
    GENERATED_FILES  generated_utest_client_proto_spec_headers
    INPUT_PACKAGE    "utest_pb2"
    PY_ADD_SYS_PATH  ${utest_generated_dir}
)
add_dependencies(generated_utest_client_proto_spec utest_proto)

list(APPEND  unittests_srcfiles
    entry.cpp
    pkg_header.cpp
    pkg_input.cpp
    utils.cpp
    cfg.cpp
    message_carrier.cpp
    impl_protobuf_parsing_engines.cpp
    execute_for_reference.cpp
    entry_msg_type_lut.cpp
    cfg_json.cpp
)

if (NOT MSVC)
    # On msvc the approach to make socket blocked doesn't work.
    list(APPEND  unittests_srcfiles ext_back_pressure.cpp)
endif ()

list(APPEND unittests_deps
                      GTest::gmock_main
                      opio::proto_entry
                      logr::logr_spdlog
                      opio::test_utils
                      opio::utest.net.reusable_utils
                      utest_proto
)

include(GoogleTest)
# ==============================================================================
add_executable(${test_prj} ${unittests_srcfiles})
add_dependencies( ${test_prj}
                  generated_utest_proto_spec
                  generated_utest_client_proto_spec
)
target_include_directories( ${test_prj}
                            PUBLIC
                            ${PROJECT_BINARY_DIR}/include
                            ${utest_generated_dir}
)
target_link_libraries(${test_prj} PRIVATE ${unittests_deps})

add_dependencies(${test_prj}
                 generated_utest_proto_spec
                 generated_utest_client_proto_spec
)

if (MSVC)
    target_compile_options(${test_prj} PRIVATE /bigobj)
    target_compile_definitions(${test_prj} PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(${test_prj} PRIVATE /bigobj)
endif ()

# Distinguisg tests for CI ([Debug, Release, ...] x [devtoolset, other gcc, clang],,,)
# If the variable is defined test report would also include this tag
# as a part of the test name.
gtest_discover_tests(${test_prj}
                     TEST_PREFIX "${OPIO_TEST_PREFIX}")
