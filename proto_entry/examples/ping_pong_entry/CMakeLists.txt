project(opio.proto_entry.examples.ping_pong_entry)

# ==============================================================================
# Run protobuf files generation
add_library(proto_entry_example_ping_pong_proto OBJECT
            "${CMAKE_CURRENT_LIST_DIR}/opio/proto_entry/examples/ping_pong/ping_pong.proto"
)

target_link_libraries(proto_entry_example_ping_pong_proto PUBLIC protobuf::libprotobuf)
set(ping_pong_generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")

file(MAKE_DIRECTORY ${ping_pong_generated_dir})

target_include_directories(proto_entry_example_ping_pong_proto
                          PUBLIC
                          ${ping_pong_generated_dir}
)

protobuf_generate(
    LANGUAGE cpp
    TARGET proto_entry_example_ping_pong_proto
    IMPORT_DIRS "${CMAKE_CURRENT_LIST_DIR}"
    PROTOC_OUT_DIR "${ping_pong_generated_dir}")

protobuf_generate(
    LANGUAGE python
    TARGET proto_entry_example_ping_pong_proto
    IMPORT_DIRS "${CMAKE_CURRENT_LIST_DIR}"
    PROTOC_OUT_DIR "${ping_pong_generated_dir}")


# Disable static analysis for generated files.
set_target_properties(proto_entry_example_ping_pong_proto
    PROPERTIES CXX_CLANG_TIDY "")
set_target_properties(proto_entry_example_ping_pong_proto
    PROPERTIES CXX_CPPCHECK "")
# ==============================================================================

proto_entry_generate_protocol_entry(
    CLIENT_SERVER_ROLE  client
    TARGET_NAME         generated_ping_pong_proto_entry_sender
    OUTPUT_DIR          ${ping_pong_generated_dir}/opio/proto_entry/examples/ping_pong/sender
    OUTPUT_NAMESPACE    "opio::proto_entry::examples::ping_pong::sender"
    GENERATED_FILES     generated_ping_pong_proto_entry_sender_headers
    INPUT_PACKAGE       "opio.proto_entry.examples.ping_pong.ping_pong_pb2"
    PY_ADD_SYS_PATH     ${ping_pong_generated_dir}
)
add_dependencies(generated_ping_pong_proto_entry_sender proto_entry_example_ping_pong_proto)

proto_entry_generate_protocol_entry(
    CLIENT_SERVER_ROLE  server
    TARGET_NAME         generated_ping_pong_proto_entry_receiver
    OUTPUT_DIR          ${ping_pong_generated_dir}/opio/proto_entry/examples/ping_pong/receiver
    OUTPUT_NAMESPACE    "opio::proto_entry::examples::ping_pong::receiver"
    GENERATED_FILES     generated_ping_pong_proto_entry_receiver_headers
    INPUT_PACKAGE       "opio.proto_entry.examples.ping_pong.ping_pong_pb2"
    PY_ADD_SYS_PATH     ${ping_pong_generated_dir}
)
add_dependencies(generated_ping_pong_proto_entry_receiver proto_entry_example_ping_pong_proto)
add_executable(_example.opio.proto_entry.ping_pong main.cpp)

add_dependencies(_example.opio.proto_entry.ping_pong
                 generated_ping_pong_proto_entry_sender
                 generated_ping_pong_proto_entry_receiver
)

target_include_directories( _example.opio.proto_entry.ping_pong
                            PRIVATE
                            ${ping_pong_generated_dir}
)

target_link_libraries(
    _example.opio.proto_entry.ping_pong
    PRIVATE CLI11::CLI11
            opio::logger
            opio::proto_entry
            proto_entry_example_ping_pong_proto )

if (MSVC)
    target_compile_options(_example.opio.proto_entry.ping_pong PRIVATE /bigobj)
endif ()
