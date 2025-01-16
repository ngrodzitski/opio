#include <array>
#include <string_view>

#include <opio/logger/log.hpp>

namespace opio::logger
{

namespace /* anonymous */
{

const std::array< std::string_view, 7 > all_levels = {
    "trace", "debug", "info", "warn", "error", "critical", "nolog"
};

}  // anonymous namespace

//
// log_level_from_string()
//

log_level log_level_from_string( std::string_view str )
{
    auto throw_invalid_log_level = [] {
        throw std::runtime_error{ fmt::format(
            "invalid log_level value, must be one of: \"{}\"",
            fmt::join( begin( all_levels ), end( all_levels ), "\", \"" ) ) };
    };

    const auto level_it = std::find( begin( all_levels ), end( all_levels ), str );

    if( level_it == end( all_levels ) )
    {
        throw_invalid_log_level();
    }

    return static_cast< ::opio::logger::log_level >(
        std::distance( begin( all_levels ), level_it ) );
}

//
// log_level_to_string()
//

std::string log_level_to_string( log_level lvl )
{
    const auto numeric_value = static_cast< int >( lvl );

    if( numeric_value >= static_cast< int >( all_levels.size() )
        || numeric_value < 0 )
    {
        throw std::runtime_error{ fmt::format(
            "invalid log_level value, the numeric representation "
            "must be in a range 0..{}, while {} is provided",
            all_levels.size(),
            numeric_value ) };
    }

    return std::string{ all_levels.at( numeric_value ) };
}

}  // namespace opio::logger
