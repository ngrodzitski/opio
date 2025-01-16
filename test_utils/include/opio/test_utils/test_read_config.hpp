#pragma once

#include <string>

#include <json_dto/pub.hpp>

namespace opio::test_utils
{

template < typename Dto >
[[nodiscard]] auto test_read_config( std::string json_str )
{
    std::istringstream input{ json_str };

    return json_dto::from_stream< Dto,
                                  rapidjson::kParseCommentsFlag
                                      | rapidjson::kParseTrailingCommasFlag >(
        input );
}

}  // namespace opio::test_utils
