/**
 * @file
 *
 * Configuration for logger routines.
 */
#pragma once

#include <string>
#include <optional>

#include <opio/logger/log.hpp>
#include <opio/logger/cfg.hpp>

namespace opio::logger
{

//
// global_logger_cfg_t
//

/**
 * @brief Global logger configurateion.
 *
 * Some compomnents of the application might override logging level.
 */
struct global_logger_cfg_t
{
    static constexpr const char default_log_message_pattern[] =
        "[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v [%g:%#]\n";

    //! Pattern used for logging.
    std::string log_message_pattern = default_log_message_pattern;

    static constexpr const char default_path[] = "./";
    //! Path to log dir.
    std::string path = default_path;

    static constexpr log_level default_global_log_level = log_level::info;
    //! Default log level.
    log_level global_log_level = default_global_log_level;

    static constexpr bool default_log_to_stdout = false;
    bool log_to_stdout                          = default_log_to_stdout;
};

}  // namespace opio::logger
