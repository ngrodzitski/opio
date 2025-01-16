#pragma once

#include <string>

#include <json_dto/pub.hpp>

#include <opio/net/udp/cfg.hpp>

namespace json_dto
{

template < typename Json_Io >
void json_io( Json_Io & io, ::opio::net::udp::udp_receiver_cfg_t & cfg )
{
    io & json_dto::mandatory( "listen_on", cfg.listen_on )
        & json_dto::mandatory( "multicast_address", cfg.multicast_address )
        & json_dto::mandatory( "multicast_port", cfg.multicast_port );
}

}  // namespace json_dto
