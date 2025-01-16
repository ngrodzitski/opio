cmake_minimum_required(VERSION 3.14)

if (NOT OPIO_LIBRARY_NAME)
    set(OPIO_LIBRARY_NAME proto_entry)
endif ()

# ====================================================================
# Declare project
# ====================================================================

set(TARGET_PROJECT opio${OPIO_LIBRARY_NAME})

project(${TARGET_PROJECT} LANGUAGES CXX VERSION ${OPIO_VERSION})

# ====================================================================
# Set sources
# ====================================================================

list(APPEND  TARGET_PUBLIC_HEADERS
        include/opio/proto_entry/cfg.hpp
        include/opio/proto_entry/cfg_json.hpp
        include/opio/proto_entry/entry_base.hpp
        include/opio/proto_entry/pkg_header.hpp
        include/opio/proto_entry/pkg_input.hpp
        include/opio/proto_entry/utils.hpp
        include/opio/proto_entry/std_entry_shortcuts_factory.hpp
        include/opio/proto_entry/message_carrier.hpp

        include/opio/proto_entry/impl/protobuf_parsing_engines.hpp
        include/opio/proto_entry/impl/self_contained_protobuf_arena.hpp
)

list(APPEND  target_src
        src/opio/proto_entry/entry_base.cpp
)

include(${CMAKE_CURRENT_LIST_DIR}/cheetah_wrapper.cmake)

# ====================================================================
# TARGET
# ====================================================================
add_library(${TARGET_PROJECT} STATIC ${TARGET_PUBLIC_HEADERS}
                                     ${target_src})
add_library(opio::${OPIO_LIBRARY_NAME} ALIAS ${TARGET_PROJECT})
set_target_properties(
    ${TARGET_PROJECT} PROPERTIES
    VERSION ${OPIO_VERSION}
    SOVERSION ${OPIO_VERSION_MAJOR}.${OPIO_VERSION_MINOR}
)

set(PROJECT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
message(STATUS "${TARGET_PROJECT} include_dir: ${PROJECT_INCLUDE_DIR}")

target_include_directories(${TARGET_PROJECT} PUBLIC
        $<BUILD_INTERFACE:${TARGET_BUILD_INCLUDE_DIR}>
        $<BUILD_INTERFACE:${CONFIG_HEADER_INCLUDE_PATH}>
        $<INSTALL_INTERFACE:include>)

target_include_directories(${TARGET_PROJECT}
                           PUBLIC
                           $<BUILD_INTERFACE:${PROJECT_INCLUDE_DIR}>
                           $<INSTALL_INTERFACE:include>
)

target_link_libraries(${TARGET_PROJECT}
                      PUBLIC
                      protobuf::libprotobuf
                      json-dto::json-dto
                      opio::net
)

list(APPEND TARGETS_LIST ${TARGET_PROJECT})
# ====================================================================

# ====================================================================
# Install
# ====================================================================
if (OPIO_INSTALL)
    include(${OPIO_CMAKE_SCRIPTS_DIR}/lib_install.cmake)

    lib_install(
        TARGET_PROJECT ${TARGET_PROJECT}
        VERSION "${OPIO_VERSION}"
        LIB_CONFIG cmake/lib-config.cmake.in
        TARGETS_LIST ${TARGETS_LIST}
    )

    lib_install_headers(
        REMOVE_INCLUDE_DIR_PREFIX include
        HEADERS ${TARGET_PUBLIC_HEADERS}
    )

    INSTALL(FILES
                ${CMAKE_CURRENT_LIST_DIR}/run_cheetah.py
                ${CMAKE_CURRENT_LIST_DIR}/cheetah_wrapper.cmake
            DESTINATION
                ${CMAKE_INSTALL_LIBDIR}/cmake/${TARGET_PROJECT})

    INSTALL(DIRECTORY
            ${CMAKE_CURRENT_LIST_DIR}/templates
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${TARGET_PROJECT})
endif ()
