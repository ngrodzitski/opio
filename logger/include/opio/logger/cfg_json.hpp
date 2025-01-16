#pragma once

#include <string>

#include <json_dto/pub.hpp>

#include <opio/logger/cfg.hpp>

namespace json_dto
{

//
// read_json_value()
//

/**
 * @brief A helper function to read log level from json.
 */
template <>
void read_json_value( ::opio::logger::log_level & lvl,
                      const rapidjson::Value & object );

//
// write_json_value()
//

/**
 * @brief A helper function to write log level from json.
 */
template <>
void write_json_value( const ::opio::logger::log_level & lvl,
                       rapidjson::Value & object,
                       rapidjson::MemoryPoolAllocator<> & allocator );

//
// json_io()
//

/**
 * @brief Reader customization for global logger.
 */
template < typename Json_Io >
void json_io( Json_Io & io, ::opio::logger::global_logger_cfg_t & cfg )
{
    using cfg_t = ::opio::logger::global_logger_cfg_t;
    io & json_dto::optional( "log_message_pattern",
                             cfg.log_message_pattern,
                             cfg_t::default_log_message_pattern )
        & json_dto::optional( "path", cfg.path, cfg_t::default_path )
        & json_dto::optional( "global_log_level",
                              cfg.global_log_level,
                              cfg_t::default_global_log_level )
        & json_dto::optional(
            "log_to_stdout", cfg.log_to_stdout, cfg_t::default_log_to_stdout );
}

}  // namespace json_dto
