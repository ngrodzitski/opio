# Root CMake for opio project.
cmake_minimum_required(VERSION 3.21)

set(OPIO_PROJECT_NAME opio)

project(${OPIO_PROJECT_NAME}_root CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(OPIO_CMAKE_SCRIPTS_DIR "${CMAKE_SOURCE_DIR}/cmake-scripts")

# ------------------------------------------------------------------------------
# Various helpers:
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake-scripts/common_options.cmake")

set(COMPILER_ENABLE_UNUSED_X_AS_ERROR OFF)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake-scripts/compiler_flags.cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake-scripts/find_program_required.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake-scripts/static_analysis.cmake" )

# ------------------------------------------------------------------------------
# Build options
SET(OPIO_ASIO_SOURCE "standalone" CACHE STRING "What source of ASIO to use (standalone, boost)")
SET(OPIO_ASIO_SOURCE_VALUES "standalone;boost")
set_property(CACHE OPIO_ASIO_SOURCE PROPERTY STRINGS ${OPIO_ASIO_SOURCE_VALUES})

option(OPIO_INSTALL           "Generate install target"  ON)
option(OPIO_BUILD_TESTS       "Build tests"              ON)
option(OPIO_BUILD_EXAMPLES    "Build examples"           ON)
option(OPIO_GCC_CODE_COVERAGE "Build with code coverage" OFF)
option(OPIO_CLANG_TIDY        "Build with clang-tidy"    OFF)

message(STATUS "OPIO_INSTAL:            ${OPIO_INSTALL}")
message(STATUS "OPIO_BUILD_TEST:        ${OPIO_BUILD_TESTS}")
message(STATUS "OPIO_BUILD_EXAMPLES:    ${OPIO_BUILD_EXAMPLES}")
message(STATUS "OPIO_GCC_CODE_COVERAGE: ${OPIO_GCC_CODE_COVERAGE}")
message(STATUS "OPIO_CLANG_TIDY:        ${OPIO_CLANG_TIDY}")
# ------------------------------------------------------------------------------

# ------------------------------------------------------------------------------
# Dependencies
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON)

find_package(fmt REQUIRED)
find_package(expected-lite REQUIRED)
find_package(Protobuf REQUIRED)
# ------------------------------------------------------------------------------

if (OPIO_ASIO_SOURCE MATCHES "standalone" )
    find_package(asio REQUIRED)
elseif (OPIO_ASIO_SOURCE MATCHES "boost")
    set(Boost_USE_STATIC_LIBS ON)
    find_package(Boost REQUIRED)
else ()
    message(FATAL_ERROR "OPIO_ASIO_SOURCE ('${OPIO_ASIO_SOURCE}') must be 'standalone' or 'boost'")
endif ()

find_package(json-dto REQUIRED)
find_package(logr REQUIRED)

# ------------------------------------------------------------------------------
# Logger sources hint:
option(OPIO_PRJ_ROOT_LENGTH_HINT_ENABLED
       "Enable a hint to cut prj-root from sources path in logging."
       ON)

option(OPIO_PRJ_COLLAPSE_SRC_LOCATION_ENABLED
       "Enable a hint to collapse all src location (make them unused)."
       OFF)

# A piece to define hint for pretty source files paths.
if (NOT OPIO_PRJ_COLLAPSE_SRC_LOCATION_ENABLED)
    if (OPIO_PRJ_ROOT_LENGTH_HINT_ENABLED)
        if( NOT ${CMAKE_GENERATOR} STREQUAL "Ninja" )
            # Ninja uses relative path for sources.
            string(LENGTH "${CMAKE_SOURCE_DIR}/" project_root_path_len)
            set(OPIO_PRJ_ROOT_LENGTH_HINT ${project_root_path_len})
        else ()
            # Ninja uses a relative path, so OPIO_PRJ_ROOT_LENGTH_HINT must be ignored.
            # For a target project a corresponding macro definition
            # can be adjusted with knowing the "head" to cut from sources.
            message(WARNING "OPIO_PRJ_ROOT_LENGTH_HINT is ignored for Ninja build")
        endif()
    endif ()
endif ()
# ------------------------------------------------------------------------------

# ------------------------------------------------------------------------------
if (OPIO_GCC_CODE_COVERAGE)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake-scripts/code_coverage.cmake" )

    if (NOT ${CMAKE_BUILD_TYPE} STREQUAL "Debug"
        AND NOT ${CMAKE_BUILD_TYPE} STREQUAL "RelWithDebInfo" )

        message(FATAL_ERROR "Codecoverage must be applied for debug builds")
    endif ()

    make_code_coverage_targets(
        NAME OpioCodeCoverage
        HTML_TITLE "Code coverage report for opio library"
        FILTERS
            '${CMAKE_SOURCE_DIR}/opio/include/.*'
            '${CMAKE_SOURCE_DIR}/net/src/.*'
            '${CMAKE_SOURCE_DIR}/net/include/.*'
            '${CMAKE_SOURCE_DIR}/proto_entry/src/.*'
            '${CMAKE_SOURCE_DIR}/proto_entry/include/.*'
    )
# Example (Ubuntu with gcc-11):
#     conan install -pr:a ./conan_profiles/ubu-gcc-11 -s:a build_type=Debug --build missing -of _build .
#     ( source ./_build/conanbuild.sh && cmake -B_build . -DCMAKE_TOOLCHAIN_FILE=_build/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Debug -DOPIO_GCC_CODE_COVERAGE=ON -DTOOLING_CODE_COVERAGE_GCOV=gcov-11 )
#     cmake --build _build
#     cmake --build _build --target OpioCodeCoverage_all
endif()
# ------------------------------------------------------------------------------

# ------------------------------------------------------------------------------
if (OPIO_CLANG_TIDY)
    message("========================================")
    message(STATUS "Enabling clang-tidy")

    find_program_required(
        NAME clang-tidy
        SEARCH_NAME ${TOOLING_CLANG_TIDY_PATH} clang-tidy
        PATH clang_tidy_exe )

    if (NOT TOOLING_CLANG_TIDY_PARAMS)
        include(${CMAKE_CURRENT_SOURCE_DIR}/default-clang-tidy-flags.cmake)
    endif ()

    set(OPIO_CLANG_TIDY_ARGS
        "--header-filter='${CMAKE_SOURCE_DIR}/(hashes|utils|book|logger|net|protocols|server|x_cfe|OPIO|client).*';${TOOLING_CLANG_TIDY_PARAMS}")

    # Rely on CMake feature:
    # https://cmake.org/cmake/help/v3.10/variable/CMAKE_LANG_CLANG_TIDY.html#variable:CMAKE_%3CLANG%3E_CLANG_TIDY
    set(CMAKE_CXX_CLANG_TIDY "${clang_tidy_exe};${OPIO_CLANG_TIDY_ARGS}")
    message(STATUS "CMAKE_CXX_CLANG_TIDY: ${CMAKE_CXX_CLANG_TIDY}")
    message("========================================")

# Example (Ubuntu with clang-tidy-12):
#     cmake -B_build -DCMAKE_BUILD_TYPE=Debug -DOPIO_CLANG_TIDY=ON -DTOOLING_CLANG_TIDY_PATH=clang-tidy-12 .
#     cmake --build _build
endif ()
# ------------------------------------------------------------------------------

# ------------------------------------------------------------------------------
# Define version and VCS details of the libe as cmake-variables:
include("${OPIO_CMAKE_SCRIPTS_DIR}/extract_version.cmake")

extract_version(
    "${CMAKE_SOURCE_DIR}/opio/include/opio/version.hpp"
    # Define extracted version as following:
    OPIO_VERSION_MAJOR
    OPIO_VERSION_MINOR
    OPIO_VERSION_PATCH
)
set(OPIO_VERSION ${OPIO_VERSION_MAJOR}.${OPIO_VERSION_MINOR}.${OPIO_VERSION_PATCH})
message(STATUS "${OPIO_PROJECT_NAME} VERSION  : " ${OPIO_VERSION})

execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE OPIO_GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE       # Remove the trailing newline character
)

execute_process(
    COMMAND git diff --quiet
    RESULT_VARIABLE opio_repo_gitdiff_res
)

if ("0" STREQUAL "${opio_repo_gitdiff_res}")
    set(OPIO_REPO_REVISION "${OPIO_GIT_COMMIT_HASH}")
else ()
    set(OPIO_REPO_REVISION "${OPIO_GIT_COMMIT_HASH}-dirty")
endif ()
# ------------------------------------------------------------------------------

# ------------------------------------------------------------------------------
# This project:
add_subdirectory(net)
add_subdirectory(proto_entry)
add_subdirectory(opio)
add_subdirectory(logger)

if (OPIO_BUILD_TESTS)
    include(CTest)
    enable_testing()
    message(STATUS "Tests are enabled")

    find_package(GTest MODULE REQUIRED)

    add_subdirectory(test_utils)
    add_subdirectory(net/test)
    add_subdirectory(proto_entry/test)
    add_subdirectory(logger/test)
endif ()

if (OPIO_BUILD_EXAMPLES)
    find_package(CLI11 REQUIRED)
    add_subdirectory(net/examples)
    add_subdirectory(proto_entry/examples)
endif ()
