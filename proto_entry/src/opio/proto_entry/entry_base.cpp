#include <opio/proto_entry/entry_base.hpp>

#include <atomic>

namespace opio::proto_entry::details
{

namespace /* anonymous */
{

/**
 * @brief A safe counter for generating uniqye connection ids.
 *
 * @note NOLINT tag is added here for clang-tidy
 *       https://github.com/isocpp/CppCoreGuidelines/blob/master/CppCoreGuidelines.md#Ri-global
 */
std::atomic< opio::net::tcp::connection_id_t > connection_id_counter =
    0;  // NOLINT

}  // anonymous namespace

//
// make_unique_connection_id()
//

opio::net::tcp::connection_id_t make_global_unique_connection_id() noexcept
{
    return connection_id_counter++;
}

}  // namespace opio::proto_entry::details
