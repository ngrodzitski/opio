cmake_minimum_required(VERSION 3.14)

if (NOT OPIO_LIBRARY_NAME)
    set(OPIO_LIBRARY_NAME test_utils)
endif ()

set(TARGET_PROJECT opio${OPIO_LIBRARY_NAME})

project(${TARGET_PROJECT} LANGUAGES CXX)

# ====================================================================
# Set sources
# ====================================================================

list(APPEND TARGET_PUBLIC_HEADERS
    include/opio/test_utils/test_logger.hpp
    include/opio/test_utils/test_read_config.hpp
)

list(APPEND target_src
    src/opio/test_utils/test_logger.cpp
)

# ====================================================================
# TARGET
# ====================================================================
add_library(${TARGET_PROJECT} STATIC ${TARGET_PUBLIC_HEADERS}
                                     ${target_src}
)
add_library(opio::test_utils ALIAS ${TARGET_PROJECT})

set(PROJECT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
message(STATUS "${TARGET_PROJECT} include_dir: ${PROJECT_INCLUDE_DIR}")

target_include_directories(${TARGET_PROJECT}
                           PUBLIC
                           ${PROJECT_INCLUDE_DIR}
)

# Set dependencies here:
target_link_libraries(${TARGET_PROJECT}
                      PUBLIC
                      opio::opio
                      opio::logger
)
