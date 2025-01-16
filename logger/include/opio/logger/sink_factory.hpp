/**
 * @file Contains a helper factory-function to crete sinks for logger.
 */
#pragma once

#include <optional>

#include <logr/spdlog_backend.hpp>

#include <opio/logger/log.hpp>

namespace opio::logger
{

//! An alias for spd loggers.
using logger_sink_sptr_t = spdlog::sink_ptr;

//
// make_color_sink()
//

/**
 * @brief Create console color log-sink.
 */
[[nodiscard]] logger_sink_sptr_t make_color_sink(
    std::optional< std::string > pattern = std::nullopt );

//
// make_daily_sink()
//

/**
 * @brief Make Daily sink.
 *
 * @param  path             Log files directory.
 * @param  filename_prefix  Log-file name prefix.
 */
[[nodiscard]] logger_sink_sptr_t make_daily_sink(
    std::string_view path,
    std::string_view filename_prefix,
    std::optional< std::string > pattern = std::nullopt );

}  // namespace opio::logger
