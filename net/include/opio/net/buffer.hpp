#pragma once

#include <vector>
#include <type_traits>
#include <string_view>
#include <cstring>
#include <span>
#include <concepts>

#include <opio/net/asio_include.hpp>

namespace opio::net
{

//
// buffer_fmt_integrator_t
//

/**
 * @brief An integrator type for introducing handy fmt formatter.
 *
 * Acts as a parameter for fmt formatting routines.
 */
struct buffer_fmt_integrator_t
{
    buffer_fmt_integrator_t( const void * data, std::size_t size ) noexcept
        : buf{ static_cast< const unsigned char * >( data ),
               static_cast< buff_span_t::size_type >( size ) }
    {
    }

    using buff_span_t = std::span< const unsigned char >;
    buff_span_t buf;
};

//
// buf_fmt_integrator()
//

/**
 * @brief Creates fmt integrator for buffer.
 *
 * Acts as a parameter for fmt formatting routines.
 */
[[nodiscard]] inline buffer_fmt_integrator_t buf_fmt_integrator(
    const void * data,
    std::size_t size ) noexcept
{
    return { data, size };
}

/**
 * @brief Creates fmt integrator for buffer.
 *
 * Allows an inplace creation of an intagrator when calling fmt formatting
 * functions.
 *
 * @code
 * const auto buf = receive_data();
 * print( fmt::format( "Receive buffer:\n{}", buf_fmt_integrator( buf ) ) );
 * @endcode
 *
 * @tparam Buffer  The type of an underlying buffer.
 *
 * @param buf  Source buffer.
 *
 * @return  And instance of a type that has fmt integration.
 */
template < typename Buffer >
[[nodiscard]] inline buffer_fmt_integrator_t buf_fmt_integrator(
    const Buffer & buf ) noexcept
{
    return buf_fmt_integrator( buf.data(), buf.size() );
}

//
// simple_buffer_t
//

/**
 * @brief A standard simple byte buffer.
 *
 * This type is a default buffer.
 */
class simple_buffer_t
{
public:
    using size_type  = std::size_t;
    using value_type = std::byte;

    constexpr simple_buffer_t() = default;

    /**
     * @brief Creates a buffer that has a given size
     *
     * @note the data is not initialized.
     */
    explicit simple_buffer_t( size_type n )
        : m_size{ n }
        , m_capacity{ n }
        , m_buf{ m_size ? std::make_unique< value_type[] >( m_size ) : nullptr }
    {
    }

    /**
     * @brief Creates a buffer that has a given size and fill it with a given
     * value.
     */
    explicit simple_buffer_t( size_type n, value_type v )
        : simple_buffer_t( n )
    {
        std::fill( data(), data() + size(), v );
    }

    /**
     * @brief Creates a buffer and initializes it to a given data.
     */
    explicit simple_buffer_t( const void * src, size_type n )
        : simple_buffer_t( n )
    {
        std::memcpy( data(), src, size() );
    }

    // No unintended copies allowed,
    // use make_copy.
    simple_buffer_t( const simple_buffer_t & sb ) = delete;
    simple_buffer_t & operator=( const simple_buffer_t & sb ) = delete;

    simple_buffer_t( simple_buffer_t && sb ) noexcept
        : m_size{ sb.size() }
        , m_capacity{ sb.capacity() }
        , m_buf{ std::move( sb.m_buf ) }
    {
        // Do a non default implementation to leave the source instance in
        // a valid state (`size==0 && capacity==0`).
        sb.m_size     = 0;
        sb.m_capacity = 0;
    }

    simple_buffer_t & operator=( simple_buffer_t && sb )
    {
        const auto n   = sb.size();
        const auto cap = sb.capacity();
        buffer_ptr_t buf{ std::move( sb.m_buf ) };
        sb.m_size     = 0;
        sb.m_capacity = 0;

        m_size     = n;
        m_capacity = cap;
        m_buf      = std::move( buf );
        return *this;
    }

    /**
     * @brief Creates a buffer and initializes with a given sequence.
     *
     * Useful for unit tests to produce buffers with comprehensible content.
     */
    static simple_buffer_t make_from( std::initializer_list< char > init );

    [[nodiscard]] size_type size() const noexcept { return m_size; }
    [[nodiscard]] bool empty() const noexcept { return 0 != m_size; }
    [[nodiscard]] size_type capacity() const noexcept { return m_capacity; }
    [[nodiscard]] const value_type * data() const noexcept { return m_buf.get(); }
    [[nodiscard]] value_type * data() noexcept { return m_buf.get(); }

    [[nodiscard]] const value_type * offset_data( size_type n ) const noexcept
    {
        return m_buf.get() + n;
    }
    [[nodiscard]] value_type * offset_data( size_type n ) noexcept
    {
        return m_buf.get() + n;
    }

    [[nodiscard]] simple_buffer_t make_copy() const
    {
        return simple_buffer_t{ data(), size() };
    }

    [[nodiscard]] std::string_view make_string_view() const noexcept
    {
        return { reinterpret_cast< const char * >( data() ), size() };
    }

    /**
     * @brief Get underlying buffer as span.
     */
    template < typename Char_Type = std::byte >
    [[nodiscard]] std::span< const Char_Type > make_const_span() const noexcept
    {
        static_assert( sizeof( Char_Type ) == sizeof( std::byte ) );
        static_assert( std::is_trivial_v< Char_Type > );

        return std::span< const Char_Type >{
            reinterpret_cast< const Char_Type * >( data() ), size()
        };
    }

    /**
     * @brief Get underlying buffer as span.
     */
    template < typename Char_Type = std::byte >
    [[nodiscard]] std::span< Char_Type > make_mutable_span() noexcept
    {
        static_assert( sizeof( Char_Type ) == sizeof( std::byte ) );
        static_assert( std::is_trivial_v< Char_Type > );

        return std::span< Char_Type >{ reinterpret_cast< Char_Type * >( data() ),
                                       size() };
    }

    /**
     * @brief Create a buffer reference that is understood by ASIO.
     *
     * @param buf  A reference to buffer for which to create asio's one.
     *
     * @return An instance of ASIO buffer.
     */
    [[nodiscard]] asio_ns::const_buffer make_asio_const_buffer() const noexcept
    {
        return { data(), size() };
    }

    /**
     * @brief Create a buffer reference that is understood by ASIO.
     *
     * @param buf  A reference to buffer for which to create asio's one.
     *
     * @return An instance of ASIO buffer.
     */
    [[nodiscard]] asio_ns::mutable_buffer make_asio_mutable_buffer() noexcept
    {
        return { static_cast< void * >( data() ), size() };
    }

    /**
     * @brief Make a buffer represent less data.
     *
     * @note Capacity remains the same.
     *
     * @param n  New size of data carried by the buffer.
     */
    void shrink_size( size_type n ) noexcept
    {
        assert( size() >= n );
        m_size = n;
    }

    /**
     * @brief Resize the buffer preserving the data curently stored in.
     *
     * If capacity allows it the buffer will be extended without
     * new allocation.
     *
     * @param n  New size of data carried by the buffer.
     */
    void resize( size_type n )
    {
        if( n <= capacity() )
        {
            m_size = n;
            return;
        }

        buffer_ptr_t new_buf = std::make_unique< value_type[] >( n );
        std::memcpy( new_buf.get(), data(), size() );
        m_buf      = std::move( new_buf );
        m_size     = n;
        m_capacity = n;
    }

    /**
     * @brief Resize the buffer and double the capacity
     *        if it needs to be grown in case of small increment.
     * @param n  New size of data carried by the buffer.
     */
    void resize_with_double_capacity_growth( size_type n )
    {
        if( n <= capacity() )
        {
            m_size = n;
            return;
        }

        const auto new_capacity = std::max( m_capacity * 2, n );
        buffer_ptr_t new_buf    = std::make_unique< value_type[] >( new_capacity );

        std::memcpy( new_buf.get(), data(), size() );
        m_buf      = std::move( new_buf );
        m_size     = n;
        m_capacity = new_capacity;
    }

    void increment_size_with_double_capacity_growth( size_type k )
    {
        resize_with_double_capacity_growth( size() + k );
    }

    /**
     * @brief Resize the buffer not preserving the data curently stored in.
     *
     * If capacity allows it the buffer will be extended without
     * new allocation and the previous data will be preserved.
     *
     * @param n  New size of data carried by the buffer.
     */
    void resize_drop_data( size_type n )
    {
        assert( size() <= n );

        if( n <= capacity() )
        {
            m_size = n;
            return;
        }

        m_buf      = std::make_unique< value_type[] >( n );
        m_size     = n;
        m_capacity = n;
    }

private:
    size_type m_size{};
    size_type m_capacity{};

    using buffer_ptr_t = std::unique_ptr< value_type[] >;
    buffer_ptr_t m_buf{};
};

//
// begin() / end() / size ()
//

/**
 * @name Begin/End/Size function for simple_buffer_t.
 */
///@{
[[nodiscard]] inline std::size_t size( const simple_buffer_t & sb ) noexcept
{
    return sb.size();
}

[[nodiscard]] inline simple_buffer_t::value_type * begin(
    simple_buffer_t & sb ) noexcept
{
    return sb.data();
}

[[nodiscard]] inline simple_buffer_t::value_type * end(
    simple_buffer_t & sb ) noexcept
{
    return sb.data() + sb.size();
}

[[nodiscard]] inline const simple_buffer_t::value_type * begin(
    const simple_buffer_t & sb ) noexcept
{
    return begin( const_cast< simple_buffer_t & >( sb ) );
}

[[nodiscard]] inline const simple_buffer_t::value_type * end(
    const simple_buffer_t & sb ) noexcept
{
    return end( const_cast< simple_buffer_t & >( sb ) );
}
///@}

inline bool operator==( const simple_buffer_t & left,
                        const simple_buffer_t & right ) noexcept
{
    return size( left ) == size( right )
           && 0 == std::memcmp( left.data(), right.data(), size( left ) );
}

/**
 * @brief Creates a buffer and initializes with a given sequence.
 *
 * Useful for unit tests to produce buffers with comprehensible content.
 */
inline simple_buffer_t simple_buffer_t::make_from(
    std::initializer_list< char > init )
{
    simple_buffer_t buf{ std::size( init ) };

    std::transform( begin( init ), end( init ), begin( buf ), []( auto c ) {
        return static_cast< value_type >( c );
    } );

    return buf;
}

//
// simple_buffer_driver_t
//

/**
 * @brief Default tratis class for buffer concept.
 *
 * Acts as a default definition of a buffer-customization for connections.
 */
struct simple_buffer_driver_t
{
    using input_buffer_t  = simple_buffer_t;
    using output_buffer_t = simple_buffer_t;

    /**
     * @brief Create an instance of an inputs buffer of a given size.
     *
     * @param size  The size of a requested buffer.
     *
     * @return An instance of a buffer of a given size.
     */
    [[nodiscard]] input_buffer_t allocate_input( std::size_t n ) const
    {
        return simple_buffer_t{ n };
    }

    /**
     * @brief Resize a given input buffer.
     *
     * @param old_buf  The old buffer we might reuse (with ownership).
     * @param size     The size of a requested buffer.
     *
     * @return An instance of a buffer of a given size.
     */
    [[nodiscard]] input_buffer_t reallocate_input( input_buffer_t old_buf,
                                                   std::size_t n ) const
    {
        old_buf.resize( n );
        return old_buf;
    }

    /**
     * @brief Resize a given input buffer.
     *
     * @param old_buf  The old buffer we might reuse (with ownership).
     * @param size     The ruduced size for a buffer to represent.
     *
     * @pre `old_buf.size() >= n`
     *
     * @return An instance of a buffer of a given size.
     */
    [[nodiscard]] input_buffer_t reduce_size_input( input_buffer_t old_buf,
                                                    std::size_t n ) const noexcept
    {
        old_buf.shrink_size( n );
        return old_buf;
    }

    /**
     * @brief Create an instance of an outputs buffer of a given size.
     *
     * @param size  The size of a requested buffer.
     *
     * @return An instance of a buffer of a given size.
     */
    [[nodiscard]] auto allocate_output( std::size_t n ) const
    {
        return simple_buffer_t{ n };
    }

    /**
     * @brief Resize a given output buffer.
     *
     * @param old_buf  The old buffer we might reuse.
     * @param size     The size of a requested buffer.
     *
     * @return An instance of a buffer of a given size.
     */
    [[nodiscard]] output_buffer_t reallocate_output( output_buffer_t old_buf,
                                                     std::size_t n ) const
    {
        old_buf.resize( n );
        return old_buf;
    }

    /**
     * @brief Create a buffer reference that is understood by ASIO.
     *
     * @param buf  A reference to buffer for which to create asio's one.
     *
     * @return An instance of ASIO buffer.
     */
    [[nodiscard]] static asio_ns::const_buffer make_asio_const_buffer(
        const output_buffer_t & buf ) noexcept
    {
        return buf.make_asio_const_buffer();
    }

    /**
     * @brief Obtain the size of the buffer.
     *
     * @param buf  A reference to a buffer to ask for size.
     */
    [[nodiscard]] static std::size_t buffer_size(
        const output_buffer_t & buf ) noexcept
    {
        return buf.size();
    }

    /**
     * @brief Create a buffer reference that is understood by ASIO.
     *
     * @param buf  A reference to buffer for which to create asio's one.
     *
     * @return An instance of ASIO buffer.
     */
    [[nodiscard]] static asio_ns::mutable_buffer make_asio_mutable_buffer(
        input_buffer_t & buf ) noexcept
    {
        return buf.make_asio_mutable_buffer();
    }
};

//
// Buffer_Driver_Concept
//

template < typename T >
concept Buffer_Driver_Concept = requires( T driver, std::size_t n )
{
    {
        driver.allocate_input( n )
    }
    ->std::same_as< typename T::input_buffer_t >;
    {
        driver.reallocate_input( std::declval< typename T::input_buffer_t >(), n )
    }
    ->std::same_as< typename T::input_buffer_t >;
    {
        driver.reduce_size_input( std::declval< typename T::input_buffer_t >(), n )
    }
    ->std::same_as< typename T::input_buffer_t >;
    { driver.allocate_output( n ) };
    {
        driver.reallocate_output( std::declval< typename T::output_buffer_t >(),
                                  n )
    }
    ->std::same_as< typename T::output_buffer_t >;
    {
        T::make_asio_const_buffer(
            std::declval< const typename T::output_buffer_t & >() )
    }
    ->std::same_as< asio_ns::const_buffer >;
    {
        T::buffer_size( std::declval< const typename T::output_buffer_t & >() )
    }
    ->std::same_as< std::size_t >;
    {
        T::make_asio_mutable_buffer(
            std::declval< typename T::input_buffer_t & >() )
    }
    ->std::same_as< asio_ns::mutable_buffer >;
};

static_assert( Buffer_Driver_Concept< simple_buffer_driver_t > );

}  // namespace opio::net

namespace fmt
{

template <>
struct formatter< opio::net::buffer_fmt_integrator_t >
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
    auto format( opio::net::buffer_fmt_integrator_t buf,
                 Format_Context & ctx ) const
    {
        // clang-format off
        // In essense the following code produces the output of
        // the following form:
        // 1. For short buffers:
        //  ptr=0x55befb525320, size=10:
        //  0000:  33 31 32 33 34 31 32 33  34 35  | 3123412345
        //
        // 2. For Long buffers:
        // ptr=0x55befb565330, size=112:
        // 0000:  31 32 31 32 33 31 32 33  34 31 32 33 34 35 31 32  31 32 33 31 32 33 34 31  32 33 34 35 31 32 31 32 | 12123123412345121231234123451212
        // 0020:  33 31 32 33 34 31 32 33  34 35 31 32 31 32 33 31  32 33 34 31 32 33 34 35  31 32 31 32 33 31 32 33 | 31234123451212312341234512123123
        // 0040:  34 31 32 33 34 35 31 32  31 32 33 31 32 33 34 31  32 33 34 35 31 32 31 32  33 31 32 33 34 31 32 33 | 41234512123123412345121231234123
        // 0060:  34 35 31 32 31 32 33 31  32 33 34 31 32 33 34 35                                                   | 4512123123412345
        // clang-format on

        constexpr std::size_t max_bytes_to_print        = 512;
        constexpr std::size_t max_bytes_in_dump_line    = 32;
        constexpr std::size_t extra_space_after_n_bytes = 8;
        auto data_ptr                                   = buf.buf.data();
        auto print_size =
            std::min< std::size_t >( buf.buf.size(), max_bytes_to_print );

        auto out = fmt::format_to( ctx.out(),
                                   "ptr={}, size={}:",
                                   static_cast< const void * >( data_ptr ),
                                   buf.buf.size() );

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
                if( std::isprint( data_ptr[ pos + i ] ) )
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
