/**
 * @file Contains helpers to set the logging insfrastructure of the application.
 */
#pragma once

#include <optional>

#include <opio/logger/log.hpp>
#include <opio/logger/cfg.hpp>

namespace opio::logger
{

//
// logger_factory_t
//

/**
 * Logger factory.
 */
class logger_factory_t
{
public:
    virtual ~logger_factory_t() = default;

    /**
     * @brief Creates a logger with a given name.
     *
     * Log level is assigned to what factory considers default.
     *
     * @param  logger_name  The name of logger.
     *
     * @return Logger instance.
     */
    [[nodiscard]] virtual logger_t make_logger( std::string_view logger_name ) = 0;

    /**
     * @brief Creates a logger with a given name and explicit level.
     *
     * @param  level        Log level.
     * @param  logger_name  The name of logger.
     *
     * @return              Smart pointer on logger instance.
     */
    [[nodiscard]] virtual logger_t make_logger( log_level level,
                                                std::string_view logger_name ) = 0;

    /**
     * @brief Shortcut factory function which might specifies the
     *        necessary logging level.
     *
     * @param  maybe_specific_level  Log level or null.
     * @param  logger_name           The name of logger.
     *
     * @return Logger instance.
     */
    [[nodiscard]] logger_t make_logger(
        std::optional< log_level > maybe_specific_level,
        std::string_view logger_name )
    {
        if( !maybe_specific_level )
        {
            return make_logger( logger_name );
        }

        return make_logger( *maybe_specific_level, logger_name );
    }

    /**
     * @brief Creates a shared logger with a given name.
     *
     * Log level is assigned to what factory considers default.
     *
     * @param  logger_name  The name of logger.
     *
     * @return Shared pointer to logger instance.
     */
    [[nodiscard]] logger_sptr_t make_logger_shared( std::string_view logger_name )
    {
        return std::make_shared< logger_t >( make_logger( logger_name ) );
    }

    /**
     * @brief Creates a shared logger with a given name.
     *
     * @param  level        Log level.
     * @param  logger_name  The name of logger.
     *
     * @return Shared pointer to logger instance.
     */
    [[nodiscard]] logger_sptr_t make_logger_shared( log_level level,
                                                    std::string_view logger_name )
    {
        return std::make_shared< logger_t >( make_logger( level, logger_name ) );
    }

    /**
     * @brief Shortcut factory function which might specifies the
     *        necessary logging level.
     *
     * @param  maybe_specific_level  Log level or null.
     * @param  logger_name           The name of logger.
     *
     * @return Logger instance.
     */
    [[nodiscard]] logger_sptr_t make_logger_shared(
        std::optional< log_level > maybe_specific_level,
        std::string_view logger_name )
    {
        if( !maybe_specific_level )
        {
            return make_logger_shared( logger_name );
        }

        return make_logger_shared( *maybe_specific_level, logger_name );
    }
};

using logger_factory_uptr_t = std::unique_ptr< logger_factory_t >;

//
// make_logger_factory
//

/**
 * @brief Makes logger factory to create logger to a specific sinks.
 *
 * @param  default_level  Defaul implicit level for loggers.
 * @param  spd_sinks      Log messages sinks for created loggers.
 *
 * @return A unique pointer to factory object.
 */
[[nodiscard]] logger_factory_uptr_t make_logger_factory(
    log_level default_level,
    spdlog::sinks_init_list spd_sinks );

/**
 * @brief Makes logger factory based on config.
 *
 * @param  app_name  Application name, that will be used as log prefix.
 * @param  cfg       Global logger configuration.
 *
 * @return A unique pointer to factory object.
 */
[[nodiscard]] logger_factory_uptr_t make_logger_factory(
    std::string_view app_name,
    const global_logger_cfg_t & cfg );

}  // namespace opio::logger
