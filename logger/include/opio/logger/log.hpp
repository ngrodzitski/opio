#pragma once

#include <memory>
#include <string>

#include <logr/logr.hpp>
#include <logr/spdlog_backend.hpp>
#include <logr/null_backend.hpp>

namespace opio::logger
{

//
// log_level
//

/**
 * @brief Log levels enum.
 *
 * Follows the one that is in logr,
 * which in turn folows the ones in  spdlog.
 */
using log_level = logr::log_message_level;

//
// log_level_from_string()
//

/**
 * @brief Get a log level from a string
 *
 * Converts a string like "debug", "info" etc. to the value of log level.
 */
log_level log_level_from_string( std::string_view str );

//
// log_level_to_string()
//

/**
 * @brief Make a string with log level.
 */
std::string log_level_to_string( log_level lvl );

//
// logger_static_buffer_size
//

/**
 * @brief Standard static-buffer size for formatting log-message.
 */
constexpr std::size_t logger_static_buffer_size = 512;

//
// logger_t
//

/**
 * @brief Default logger type
 */
using logger_t = logr::spdlog_logger_t< logger_static_buffer_size >;

//
// logger_sptr_t
//

/**
 * @brief Default shared logger type.
 */
using logger_sptr_t = std::shared_ptr< logger_t >;

#if defined( OPIOPRJ_COLLAPSE_SRC_LOCATION )
// OPIO_PRJ_COLLAPSE_SRC_LOCATION is defined that means
// all src location pieces must be collapsed to nothing.
#    define OPIO_SRC_LOCATION \
        ::logr::no_src_location_t {}
#else
#    if !defined( OPIO_PRJ_ROOT_LENGTH_HINT )
#        define OPIO_PRJ_ROOT_LENGTH_HINT 0
#    endif

#    define OPIO_SRC_LOCATION                                             \
        ::logr::src_location_t                                            \
        {                                                                 \
            LOGR_STRIP_FILE_NAME_N( OPIO_PRJ_ROOT_LENGTH_HINT ), __LINE__ \
        }
#endif

using noop_logger_t = logr::null_logger_t<>;

}  // namespace opio::logger
