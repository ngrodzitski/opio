#pragma once

#include <memory>
#include <string>

#include <logr/logr.hpp>
#include <logr/spdlog_backend.hpp>
#include <logr/null_backend.hpp>

#include <opio/log.hpp>
#include <opio/logger/log.hpp>

namespace opio::test_utils
{
//
// make_test_logger()
//

/**
 * @brief Creates an instance of logger that ould be used for test.
 */
::opio::logger::logger_t make_test_logger(
    std::string_view logger_name,
    ::opio::logger::log_level log_lvl = ::opio::logger::log_level::trace );

}  // namespace opio::test_utils
