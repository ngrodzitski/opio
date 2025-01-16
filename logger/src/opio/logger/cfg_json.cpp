#include <opio/logger/cfg_json.hpp>

#include <array>
#include <string_view>

namespace json_dto
{

namespace /* anonymous */
{

const std::array< std::string_view, 7 > all_levels = {
    "trace", "debug", "info", "warn", "error", "critical", "nolog"
};

}  // anonymous namespace

//
// read_json_value()
//

template <>
void read_json_value( ::opio::logger::log_level & v,
                      const rapidjson::Value & object )
{
    if( !object.IsString() )
    {
        throw std::runtime_error{ "log_level must be a string" };
    }

    v = ::opio::logger::log_level_from_string( object.GetString() );
}

//
// write_json_value()
//

template <>
void write_json_value( const ::opio::logger::log_level & v,
                       rapidjson::Value & object,
                       rapidjson::MemoryPoolAllocator<> & allocator )
{

    const auto numeric_value = static_cast< int >( v );
    if( numeric_value >= static_cast< int >( all_levels.size() )
        || numeric_value < 0 )
    {
        throw std::runtime_error{ fmt::format(
            "invalid log_level value, the numeric representation "
            "must be in a range 0..{}, while {} is provided",
            all_levels.size(),
            numeric_value ) };
    }

    json_dto::write_json_value(
        ::opio::logger::log_level_to_string( v ), object, allocator );
}

}  // namespace json_dto
