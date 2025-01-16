#pragma once

// Define default SRC_LOCATION macro if necessary:
#if !defined(OPIO_SRC_LOCATION)

#include <logr/logr.hpp>

#if defined( OPIOPRJ_COLLAPSE_SRC_LOCATION )
    // OPIO_PRJ_COLLAPSE_SRC_LOCATION is defined that means
    // all src location pieces must be collapsed to nothing.
#   define OPIO_SRC_LOCATION ::logr::no_src_location_t {}
#else
#    if !defined( OPIO_PRJ_ROOT_LENGTH_HINT )
#        define OPIO_PRJ_ROOT_LENGTH_HINT 0
#    endif

#    define OPIO_SRC_LOCATION                                             \
        ::logr::src_location_t                                            \
        {                                                                 \
            LOGR_STRIP_FILE_NAME_N( OPIO_PRJ_ROOT_LENGTH_HINT ), __LINE__ \
        }
#endif

#endif  // !defined(OPIO_SRC_LOCATION)

namespace opio
{

//
// noop_logger_t
//

/**
 * @brief Type implementing noop logger.
 *
 * Serves as a default template parameter for traits classes.
 */
class noop_logger_t
{
public:
    explicit noop_logger_t() {}

    template < typename... Args >
    constexpr void trace( Args &&... ) const noexcept
    {}

    template < typename... Args >
    constexpr void debug( Args &&... ) const noexcept
    {}

    template < typename... Args >
    constexpr void info( Args &&... ) const noexcept
    {}

    template < typename... Args >
    constexpr void warn( Args &&... ) const noexcept
    {}

    template < typename... Args >
    constexpr void error( Args &&... ) const noexcept
    {}

    template < typename... Args >
    constexpr void critical( Args &&... ) const noexcept
    {}
};

}  // namespace opio
