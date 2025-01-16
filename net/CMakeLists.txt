cmake_minimum_required(VERSION 3.14)

if (NOT OPIO_LIBRARY_NAME)
    set(OPIO_LIBRARY_NAME net)
endif ()

# ====================================================================
# Declare project
# ====================================================================

set(TARGET_PROJECT opio${OPIO_LIBRARY_NAME})

project(${TARGET_PROJECT} LANGUAGES CXX VERSION ${OPIO_VERSION})

# ====================================================================
# Set sources
# ====================================================================

list(APPEND TARGET_PUBLIC_HEADERS
    include/opio/net/asio_include.hpp
    include/opio/net/asio_thread.hpp
    include/opio/net/buffer.hpp
    include/opio/net/heterogeneous_buffer.hpp
    include/opio/net/locking.hpp

    include/opio/net/network_iface_to_addr.hpp
    include/opio/net/operation_watchdog.hpp
    include/opio/net/stats.hpp
    include/opio/net/try_make_addr.hpp

    include/opio/net/udp/udp_message_receiver.hpp
    include/opio/net/udp/udp_message_receiver_fwd.hpp
    include/opio/net/udp/cfg.hpp
    include/opio/net/udp/cfg_json.hpp

    include/opio/net/tcp/acceptor.hpp
    include/opio/net/tcp/cfg.hpp
    include/opio/net/tcp/cfg_json.hpp
    include/opio/net/tcp/connection.hpp
    include/opio/net/tcp/connection_id.hpp
    include/opio/net/tcp/connector.hpp
    include/opio/net/tcp/error_code.hpp
    include/opio/net/tcp/utils.hpp
)

list(APPEND target_src
    src/opio/net/network_iface_to_addr.cpp
    src/opio/net/try_make_addr.cpp

)

# ====================================================================
# TARGET
# ====================================================================
add_library(${TARGET_PROJECT} STATIC ${TARGET_PUBLIC_HEADERS}
                                     ${target_src}
)
add_library(opio::${OPIO_LIBRARY_NAME} ALIAS ${TARGET_PROJECT})
set_target_properties(
    ${TARGET_PROJECT} PROPERTIES
    VERSION ${OPIO_VERSION}
    SOVERSION ${OPIO_VERSION_MAJOR}.${OPIO_VERSION_MINOR}
)

set(PROJECT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
message(STATUS "${TARGET_PROJECT} include_dir: ${PROJECT_INCLUDE_DIR}")

target_include_directories(${TARGET_PROJECT}
                           PUBLIC
                           $<BUILD_INTERFACE:${PROJECT_INCLUDE_DIR}>
                           $<INSTALL_INTERFACE:include>
)

if (OPIO_ASIO_SOURCE MATCHES "standalone")
    target_link_libraries(${TARGET_PROJECT} PUBLIC asio::asio)
else ()
    target_link_libraries(${TARGET_PROJECT} PUBLIC Boost::headers)
    target_compile_definitions(${TARGET_PROJECT} INTERFACE OPIO_USE_BOOST_ASIO)
endif ()

# Set dependencies here:
target_link_libraries(${TARGET_PROJECT}
                      PUBLIC
                      json-dto::json-dto

                      opio::opio
)

# Targets for install
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
endif ()
