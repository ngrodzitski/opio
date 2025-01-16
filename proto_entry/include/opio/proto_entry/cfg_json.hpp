/**
 * @file Contains integration of proto_entry cfg with json (throuth json_dto).
 */

#pragma once

#include <string>

#include <fmt/format.h>

#include <json_dto/pub.hpp>

#include <opio/net/tcp/cfg_json.hpp>
#include <opio/proto_entry/cfg.hpp>

namespace json_dto
{

template < typename Json_Io >
void json_io( Json_Io & io, opio::proto_entry::entry_full_cfg_t & cfg )
{
    io & json_dto::mandatory( "endpoint", cfg.endpoint )
        & json_dto::optional(
            "reconnect_timeout_msec",
            cfg.reconnect_timeout_msec,
            opio::proto_entry::details::default_reconnect_timeout_msec )
        & json_dto::optional(
            "initiate_heartbeat_timeout_msec",
            cfg.initiate_heartbeat_timeout_msec,
            opio::proto_entry::details::default_initiate_heartbeat_timeout_msec )
        & json_dto::optional( "await_heartbeat_reply_timeout_msec",
                              cfg.await_heartbeat_reply_timeout_msec,
                              opio::proto_entry::details::
                                  default_await_heartbeat_reply_timeout_msec )
        & json_dto::optional(
            "max_valid_package_size",
            cfg.max_valid_package_size,
            opio::proto_entry::details::default_max_valid_package_size )
        & json_dto::optional(
            "input_buffer_size",
            cfg.input_buffer_size,
            opio::proto_entry::details::default_input_buffer_size )
        & json_dto::optional(
            "write_timeout_per_1mb_msec",
            cfg.write_timeout_per_1mb_msec,
            opio::proto_entry::details::default_write_timeout_per_1mb_msec );
}

}  // namespace json_dto
