/**
 * @file
 *
 * This header file contains a necessary unification routines for asio package.
 * So that opio library can be developed without explicit biass to
 * standalone-asio or to Boost::asio.
 * Check original approach here:
 *     https://github.com/Stiffstream/restinio/blob/v.0.7.0/dev/restinio/asio_include.hpp
 *
 * The content of this file should be extended if anything else should be unified
 * between standalone-asio or to Boost::asio.
 */

#pragma once

#include <memory>

#if !defined( OPIO_USE_BOOST_ASIO )
#    include <asio.hpp>
#else
#    include <boost/asio.hpp>
#endif

#include <fmt/format.h>

namespace opio::net
{

// =============================================================================
#if !defined( OPIO_USE_BOOST_ASIO )
// =============================================================================

namespace asio_ns = ::asio;

//! @name Adoptation functions to cover differences between standalone and boost
//! asio.
///@{
[[nodiscard]] inline bool error_is_operation_aborted(
    const asio_ns::error_code & ec ) noexcept
{
    return ec == asio_ns::error::operation_aborted;
}

[[nodiscard]] inline bool error_is_eof( const asio_ns::error_code & ec ) noexcept
{
    return ec == asio_ns::error::eof;
}

[[nodiscard]] inline bool error_is_would_block(
    const asio_ns::error_code & ec ) noexcept
{
    return ec == asio_ns::error::would_block;
}
///@}

namespace asio_ec
{
constexpr auto eof = asio_ns::error::eof;
inline const auto & system_category()
{
    return asio_ns::system_category();
}
}  // namespace asio_ec

//! An alias for base class of error category entity.
using error_category_base_t = asio_ns::error_category;

#    if defined( ASIO_WINDOWS )
#        define OPIO_NET_ASIO_WINDOWS ASIO_WINDOWS
#    endif  // defined(ASIO_WINDOWS)

#    if defined( ASIO_MSVC )
#        define OPIO_NET_ASIO_MSVC ASIO_MSVC
#    endif  // defined(ASIO_MSVC)

#    if defined( ASIO_VERSION )
#        define OPIO_ASIO_VERSION ASIO_VERSION
#    else
#        error "ASIO version macro not defined"
#    endif

// =============================================================================
#else  // => #if !defined( OPIO_USE_BOOST_ASIO )
// =============================================================================

namespace asio_ns
{
using namespace ::boost::asio;
using error_code = ::boost::system::error_code;
}  // namespace asio_ns

//! @name Adoptation functions to cover differences between snad-alone and beast
//! asio.
///@{
[[nodiscard]] inline bool error_is_operation_aborted(
    const asio_ns::error_code & ec ) noexcept
{
    return ec == asio_ns::error::basic_errors::operation_aborted;
}

[[nodiscard]] inline bool error_is_eof( const asio_ns::error_code & ec ) noexcept
{
    return ec == asio_ns::error::misc_errors::eof;
}

[[nodiscard]] inline bool error_is_would_block(
    const asio_ns::error_code & ec ) noexcept
{
    return ec == asio_ns::error::would_block;
}
///@}

namespace asio_ec
{
constexpr auto eof = asio_ns::error::misc_errors::eof;

inline const auto & system_category()
{
    return ::boost::system::system_category();
}

}  // namespace asio_ec

//! An alias for base class of error category entity.
using error_category_base_t = ::boost::system::error_category;

#    if defined( BOOST_ASIO_WINDOWS )
#        define OPIO_NET_ASIO_WINDOWS BOOST_ASIO_WINDOWS
#    endif  // defined(BOOST_ASIO_WINDOWS)

#    if defined( BOOST_ASIO_MSVC )
#        define OPIO_NET_ASIO_MSVC BOOST_ASIO_MSVC
#    endif  // defined(BOOST_ASIO_MSVC)

#    if defined( BOOST_ASIO_VERSION )
#        define OPIO_ASIO_VERSION BOOST_ASIO_VERSION
#    else
#        error "ASIO version macro not defined"
#    endif

// =============================================================================
#endif
// =============================================================================

//
// ec_fmt_integrator_t
//

/**
 * @brief An integrator type for introducing handy fmt formatter.
 *
 * Acts as a parameter for fmt formatting routines.
 */
struct ec_fmt_integrator_t
{
    ec_fmt_integrator_t( const asio_ns::error_code & ec ) noexcept
        : error_code{ ec }
    {
    }

    const asio_ns::error_code & error_code;
};

/**
 * @brief Creates fmt integrator for error code.
 *
 * Allows an inplace creation of an intagrator when calling fmt formatting
 * functions.
 *
 * @code
 * asio_ns::error_code ec;
 * socket.shutdown( asio_ns::ip::tcp::socket::shutdown_both, ec );
 * if( ec ) {
 *   handle_bad_shutdown(
 *       fmt::format( "Shutdown failed: {}", fmt_integrator( ec ) ) );
 * }
 * @endcode
 *
 * @param ec  Erro code instance.
 *
 * @return  And instance of type that has fmt integration.
 */
[[nodiscard]] inline ec_fmt_integrator_t fmt_integrator(
    const asio_ns::error_code & ec ) noexcept
{
    return { ec };
}

}  // namespace opio::net

namespace fmt
{

template <>
struct formatter< opio::net::ec_fmt_integrator_t >
{
    template < class Parse_Context >
    constexpr auto parse( Parse_Context & ctx )
    {
        auto it  = std::begin( ctx );
        auto end = std::end( ctx );
        if( it != end && *it != '}' )
        {
            throw fmt::format_error( "invalid format" );
        }
        return it;
    }

    template < class Format_Context >
    auto format( opio::net::ec_fmt_integrator_t ec, Format_Context & ctx ) const
    {
        return fmt::format_to( ctx.out(),
                               "{{{0}(0x{0:X}) \"{1}\"}}",
                               ec.error_code.value(),
                               ec.error_code.message() );
    }
};

template <>
struct formatter< opio::net::asio_ns::ip::tcp::endpoint >
{
    template < class Parse_Context >
    constexpr auto parse( Parse_Context & ctx )
    {
        auto it  = std::begin( ctx );
        auto end = std::end( ctx );
        if( it != end && *it != '}' )
        {
            throw fmt::format_error( "invalid format" );
        }
        return it;
    }

    template < class Format_Context >
    auto format( const opio::net::asio_ns::ip::tcp::endpoint & ep,
                 Format_Context & ctx ) const
    {
        return fmt::format_to(
            ctx.out(), "{}:{}", ep.address().to_string(), ep.port() );
    }
};

}  // namespace fmt
