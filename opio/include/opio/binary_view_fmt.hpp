#pragma once

#include <cctype>
#include <span>

#include <fmt/format.h>

namespace opio
{

//
// binary_view_fmt_t
//

/**
 * @brief An integrator type for introducing handy fmt formatter.
 *
 * Acts as a proxy class integrated with fmt to write a binary to log.
 */
struct binary_view_fmt_t
{
    using cbinary_span_t = std::span< const std::byte >;
    cbinary_span_t binary;

    explicit binary_view_fmt_t( const void * data, std::size_t size ) noexcept
        : binary{ static_cast< const std::byte * >( data ),
                  static_cast< cbinary_span_t::size_type >( size ) }
    {
    }
};

static_assert( std::is_trivially_copyable_v< binary_view_fmt_t > );

//
// make_binary_view_fmt()
//

/**
 * @brief Creates fmt integrator for binary.
 *
 * Helps to create a parameter for fmt formatting routines
 * that enables nice formatting of the binary.
 */
[[nodiscard]] inline binary_view_fmt_t make_binary_view_fmt(
    const void * data,
    std::size_t size ) noexcept
{
    return binary_view_fmt_t{ data, size };
}

/**
 * @brief Creates fmt integrator for binary.
 *
 * Helps to create a parameter for fmt formatting routines
 * that enables nice formatting of the binary.
 *
 * @pre Buffer must represent elements of size of 1 byte.
 *
 * @code
 * const auto buf = receive_data();
 * print( fmt::format( "Receive buffer:\n{}", make_binary_view_fmt( buf ) ) );
 * @endcode
 *
 * @tparam Buffer  The type of an underlying buffer.
 *
 * @param buf  Source buffer.
 *
 * @return  And instance of a type that has fmt integration.
 */
template < typename Buffer >
[[nodiscard]] inline binary_view_fmt_t make_binary_view_fmt(
    const Buffer & buf ) noexcept
{
    static_assert( sizeof( *buf.data() ) == sizeof( std::byte ) );
    return make_binary_view_fmt( buf.data(), buf.size() );
}

//
// to_string()
//

[[nodiscard]] std::string to_string( binary_view_fmt_t binary_view );

}  // namespace opio

namespace fmt
{

template <>
struct formatter< ::opio::binary_view_fmt_t >
{
    template < class Parse_Context >
    constexpr auto parse( Parse_Context & ctx )
    {
        auto it  = std::begin( ctx );
        auto end = std::end( ctx );
        if( it != end && *it != '}' ) throw fmt::format_error( "invalid format" );
        return it;
    }

    template < class Format_Context >
    auto format( ::opio::binary_view_fmt_t bv, Format_Context & ctx )
    {
        // clang-format off
        // In essense the following code produces the output of
        // the following form:
        // 1. For short binaries:
        //  ptr=0x55befb525320, size=10:
        //  0000:  33 31 32 33 34 31 32 33  34 35  | 3123412345
        //
        // 2. For Long binaries:
        // ptr=0x55befb565330, size=112:
        // 0000:  31 32 31 32 33 31 32 33  34 31 32 33 34 35 31 32  31 32 33 31 32 33 34 31  32 33 34 35 31 32 31 32 | 12123123412345121231234123451212
        // 0020:  33 31 32 33 34 31 32 33  34 35 31 32 31 32 33 31  32 33 34 31 32 33 34 35  31 32 31 32 33 31 32 33 | 31234123451212312341234512123123
        // 0040:  34 31 32 33 34 35 31 32  31 32 33 31 32 33 34 31  32 33 34 35 31 32 31 32  33 31 32 33 34 31 32 33 | 41234512123123412345121231234123
        // 0060:  34 35 31 32 31 32 33 31  32 33 34 31 32 33 34 35                                                   | 4512123123412345
        // clang-format on

        // IMPROVE ME: that can be subjected to format options.
        constexpr std::size_t max_bytes_to_print        = 512;
        constexpr std::size_t max_bytes_in_dump_line    = 32;
        constexpr std::size_t extra_space_after_n_bytes = 8;

        auto data_ptr =
            reinterpret_cast< const unsigned char * >( bv.binary.data() );
        auto print_size =
            std::min< std::size_t >( bv.binary.size(), max_bytes_to_print );

        auto out = fmt::format_to( ctx.out(),
                                   "ptr={}, size={}:",
                                   static_cast< const void * >( data_ptr ),
                                   bv.binary.size() );

        for( std::size_t pos = 0; pos < print_size; pos += max_bytes_in_dump_line )
        {
            out = fmt::format_to( ctx.out(), "\n{:04X}:", pos );
            const auto bytes_in_this_line = std::min< std::size_t >(
                print_size - pos, max_bytes_in_dump_line );
            const auto fantom_bytes =
                max_bytes_in_dump_line < print_size ?
                    max_bytes_in_dump_line - bytes_in_this_line :
                    0;

            for( std::size_t i = 0; i < bytes_in_this_line; ++i )
            {
                if( i % extra_space_after_n_bytes == 0 )
                {
                    *out++ = ' ';
                }

                out = fmt::format_to( out, " {:02X}", data_ptr[ pos + i ] );
            }

            out = fmt::format_to(
                ctx.out(),
                " {:>{}} ",
                "|",
                1 + fantom_bytes * 3 + fantom_bytes / extra_space_after_n_bytes );

            for( std::size_t i = 0; i < bytes_in_this_line; ++i )
            {
                if( std::isprint( static_cast< char >( data_ptr[ pos + i ] ) ) )
                {
                    *out++ = data_ptr[ pos + i ];
                }
                else
                {
                    *out++ = '.';
                }
            }
        }
        return out;
    }
};

}  // namespace fmt
