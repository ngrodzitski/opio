cmake_minimum_required(VERSION 3.14)

if (NOT OPIO_LIBRARY_NAME)
    set(OPIO_LIBRARY_NAME logger)
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
    include/opio/logger/cfg.hpp
    include/opio/logger/cfg_json.hpp
    include/opio/logger/log.hpp
    include/opio/logger/logger_factory.hpp
    include/opio/logger/sink_factory.hpp
)

list(APPEND target_src
    src/opio/logger/cfg_json.cpp
    src/opio/logger/log.cpp
    src/opio/logger/logger_factory.cpp
    src/opio/logger/sink_factory.cpp
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

# Set dependencies here:
target_link_libraries(${TARGET_PROJECT}
                      PUBLIC
                      json-dto::json-dto
                      logr::logr_spdlog
)


if (OPIO_PRJ_COLLAPSE_SRC_LOCATION_ENABLED)
    target_compile_definitions(${TARGET_PROJECT}
                               PUBLIC
                               OPIO_PRJ_COLLAPSE_SRC_LOCATION
    )
else ()
    target_compile_definitions(${TARGET_PROJECT}
                               PUBLIC
                               OPIO_PRJ_ROOT_LENGTH_HINT=${OPIO_PRJ_ROOT_LENGTH_HINT}
    )
endif()


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
