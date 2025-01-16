#pragma once

#include <stdexcept>

#include <fmt/format.h>
#include <fmt/compile.h>

namespace opio
{

//
// exception_t
//

/**
 * @brief A base class for opio own exceptions.
 */
class exception_t : public std::runtime_error
{
    using base_type_t = std::runtime_error;

    using fmt_compile_string = ::fmt::detail::compile_string;

public:
    exception_t( std::string err )
        : base_type_t{ std::move( err ) }
    {
    }

    template < typename... Args >
    exception_t( fmt::format_string< Args... > format_str, Args &&... args )
        : exception_t{ ::fmt::format( format_str,
                                      std::forward< Args >( args )... ) }
    {
    }

    template <
        typename Fmt_String,
        typename... Args,
        typename = std::enable_if_t<
            std::is_base_of_v< ::fmt::detail::compile_string, Fmt_String > > >
    exception_t( Fmt_String format_str, Args &&... args )
        : base_type_t{ ::fmt::format( format_str,
                                      std::forward< Args >( args )... ) }
    {
    }

    template < typename... Args >
    exception_t( ::fmt::runtime_format_string<> format_str, Args &&... args )
        : base_type_t{ ::fmt::format( format_str,
                                      std::forward< Args >( args )... ) }
    {
    }
};

template < typename... Args >
[[noreturn]] void throw_exception( fmt::format_string< Args... > format_str,
                                   Args &&... args )
{
    throw exception_t{ format_str, std::forward< Args >( args )... };
}

template < typename Fmt_String,
           typename... Args,
           typename = std::enable_if_t<
               std::is_base_of_v< ::fmt::detail::compile_string, Fmt_String > > >
[[noreturn]] void throw_exception( Fmt_String format_str, Args &&... args )
{
    throw exception_t{ format_str, std::forward< Args >( args )... };
}

template < typename... Args >
[[noreturn]] void throw_exception( ::fmt::runtime_format_string<> format_str,
                                   Args &&... args )
{
    throw exception_t{ format_str, std::forward< Args >( args )... };
}

}  // namespace opio
