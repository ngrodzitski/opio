/**
 * @file
 *
 * Adjustable buffer content adjuster to embed TS into it.
 */

#pragma once

#include <memory>
#include <array>

#include <google/protobuf/arena.h>

#if defined( _WIN32 )
#    pragma warning( push )
#    pragma warning( disable : 4324 )
#endif  // defined(_WIN32)

namespace opio::proto_entry::impl
{

//
// self_contained_protobuf_arena_t
//

/**
 * @brief A self contained movable `google::protobuf::Arena` wrapper.
 *
 * `google::protobuf::Arena` is not movable and so is problematic to
 * use as a part of movable types (like package builder).
 * This class acts as a movable arena.
 *
 * As this class already requires heap allocation then
 * it also allocats almost 4Kb initial buffer which hopefully
 * would be enaugh for considerable number of cases, thus
 *
 */
class self_contained_protobuf_arena_t
    : public std::enable_shared_from_this< self_contained_protobuf_arena_t >
{
    static google::protobuf::ArenaOptions make_arena_options( char * ptr,
                                                              std::size_t size )
    {
        google::protobuf::ArenaOptions options;
        options.start_block_size   = 4 * 1024;
        options.max_block_size     = 1024 * 1024;
        options.initial_block      = ptr;
        options.initial_block_size = size;

        return options;
    }

public:
    explicit self_contained_protobuf_arena_t()
        : m_arena{ make_arena_options( m_preallocated_buf.data(),
                                       m_preallocated_buf.size() ) }
    {
    }

    [[nodiscard]] google::protobuf::Arena * get_arena() noexcept
    {
        return &m_arena;
    }

private:
    std::array< char, 4000 > m_preallocated_buf;

    alignas( 64 ) google::protobuf::Arena m_arena;
};

}  // namespace opio::proto_entry::impl

#if defined( _WIN32 )
#    pragma warning( pop )
#endif  // defined(_WIN32)
