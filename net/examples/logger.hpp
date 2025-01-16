#pragma once

#include <logr/logr.hpp>
#include <logr/spdlog_backend.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>

constexpr std::size_t logger_static_buffer_size = 320;

using console_logger_t = logr::spdlog_logger_t< logger_static_buffer_size >;

//
// make_utest_logger()
//

[[nodiscard]] inline console_logger_t make_logger(
    const std::string & name,
    logr::log_message_level log_level = logr::log_message_level::trace )
{
    using namespace spdlog::sinks;

    auto sink = std::make_shared< spdlog::sinks::stdout_color_sink_mt >();
    sink->set_pattern( "[%Y-%m-%d %T.%e] [%n] [%^%l%$] %v [%g:%#]\n" );

    return console_logger_t{ name, std::move( sink ), log_level };
}
