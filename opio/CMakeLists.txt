cmake_minimum_required(VERSION 3.25)

if (NOT OPIO_LIBRARY_NAME)
    set(OPIO_LIBRARY_NAME opio)
endif ()

# Define where cmake-scripts dir is located.
# in case of using this lib as a submodule it might be located somewhere else
# and not next to a root `CMakeLists.txt` file of the project.
if (NOT OPIO_CMAKE_SCRIPTS_DIR)
    # If not explicitly set use a default one:
    set(OPIO_CMAKE_SCRIPTS_DIR "${CMAKE_SOURCE_DIR}/cmake-scripts")
endif ()

# ====================================================================
# Version
# ====================================================================
include(${OPIO_CMAKE_SCRIPTS_DIR}/extract_version.cmake)
extract_version(
    "include/${OPIO_LIBRARY_NAME}/version.hpp"
    # Define extracted version as following:
    PRJ_VERSION_MAJOR
    PRJ_VERSION_MINOR
    PRJ_VERSION_PATCH
)

set(PRJ_VERSION ${PRJ_VERSION_MAJOR}.${PRJ_VERSION_MINOR}.${PRJ_VERSION_PATCH})
# ABI compatibility version.
# A safer approach is to declare compatibility on the level of minor version.
# If a major version compatibility is chosen then consider changing
# a COMPATIBILITY type in install section (SameMinorVersion => SameMajorVersion).
set(PRJ_SOVERSION ${PRJ_VERSION_MAJOR}.${PRJ_VERSION_MINOR})

message(STATUS "${OPIO_LIBRARY_NAME} VERSION  : " ${PRJ_VERSION})
message(STATUS "${OPIO_LIBRARY_NAME} SOVERSION: " ${PRJ_SOVERSION})

# ====================================================================
# Declare project
# ====================================================================

set(TARGET_PROJECT ${OPIO_LIBRARY_NAME})

project(${TARGET_PROJECT} LANGUAGES CXX VERSION ${PRJ_VERSION})

# ====================================================================
# Set sources
# ====================================================================

list(APPEND TARGET_PUBLIC_HEADERS
    include/opio/binary_view_fmt.hpp
    include/opio/exception.hpp
    include/opio/expected_include.hpp
    include/opio/log.hpp
    include/opio/version.hpp
)

# ====================================================================
# TARGET
# ====================================================================
add_library(${TARGET_PROJECT} INTERFACE ${TARGET_PUBLIC_HEADERS})
add_library(${TARGET_PROJECT}::${TARGET_PROJECT} ALIAS ${TARGET_PROJECT})

set(PROJECT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
message(STATUS "${TARGET_PROJECT} include_dir: ${PROJECT_INCLUDE_DIR}")

target_include_directories(${TARGET_PROJECT}
                           INTERFACE
                           $<BUILD_INTERFACE:${PROJECT_INCLUDE_DIR}>
                           $<INSTALL_INTERFACE:include>
)

# Set dependencies here:
target_link_libraries(${TARGET_PROJECT}
                      INTERFACE
                      nonstd::expected-lite
                      fmt::fmt
                      logr::logr
)

target_compile_definitions(${TARGET_PROJECT}
                           INTERFACE
                           LIB_OPIO
)


if (OPIO_PRJ_COLLAPSE_SRC_LOCATION_ENABLED)
    target_compile_definitions(${TARGET_PROJECT}
                               INTERFACE
                               OPIO_PRJ_COLLAPSE_SRC_LOCATION
    )
else ()
    target_compile_definitions(${TARGET_PROJECT}
                               INTERFACE
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
        VERSION "${PRJ_VERSION}"
        LIB_CONFIG cmake/lib-config.cmake.in
        TARGETS_LIST ${TARGETS_LIST}
    )

    lib_install_headers(
        REMOVE_INCLUDE_DIR_PREFIX include
        HEADERS ${TARGET_PUBLIC_HEADERS}
    )
endif ()
