#pragma once

#include <memory>
#include <array>
#include <type_traits>

#if defined( OPIO_USE_BOOST_ASIO )
#    include <boost/function.hpp>
#else  // defined( OPIO_USE_BOOST_ASIO )
#    include <functional>
#endif  // defined( OPIO_USE_BOOST_ASIO )

#include <opio/net/asio_include.hpp>
#include <opio/net/buffer.hpp>
#include <opio/exception.hpp>

namespace opio::net
{

namespace details
{

using etalon_adjuster_t =
#if defined( OPIO_USE_BOOST_ASIO )
    boost::function< void( simple_buffer_t::value_type *,
                           simple_buffer_t::size_type ) >;
#else   // defined( OPIO_USE_BOOST_ASIO )
    std::function< void( simple_buffer_t::value_type *,
                         simple_buffer_t::size_type ) >;
#endif  // defined( OPIO_USE_BOOST_ASIO )
}  // namespace details

//
// buffer_iface_t
//

/**
 * @brief Internal interface for a trivial buffer-like entity.
 */
class buffer_iface_t
{
public:
    buffer_iface_t() = default;

    // Move is OK, Copy is not.
    explicit buffer_iface_t( const buffer_iface_t & ) = delete;
    explicit buffer_iface_t( buffer_iface_t && )      = default;
    buffer_iface_t & operator=( const buffer_iface_t & ) = delete;
    buffer_iface_t & operator=( buffer_iface_t && ) = default;

    virtual ~buffer_iface_t() = default;

    /**
     * @brief Move this buffer enitity to a given location.
     *
     * @note Storage must have a sufficient space and proper alignment.
     */
    virtual void relocate_to( void * storage ) = 0;

    /**
     * @brief Get asio buf entity.
     */
    [[nodiscard]] virtual asio_ns::const_buffer make_asio_const_buffer() const = 0;

    /**
     * @brief Get asio buf entity.
     */
    [[nodiscard]] virtual asio_ns::mutable_buffer make_asio_mutable_buffer() = 0;

    /**
     * @brief Get underlying buffer size.
     *
     */
    [[nodiscard]] virtual std::size_t get_size() const = 0;
    // Regarding the name:
    // The name is not simply `size()` because that simple name
    // is associated with a trivial operation like getting
    // member variable. but ine this case it is a virtual
    // call and we are not aware of what is executed behind the call.
    // It might be the whole process. So to emphasize that smth.
    // might be behind it we use a verb in the name.

    /**
     * @brief Obtain reusable simple_buffer if possible.
     *
     * @param buf  A reference to the object that myst receive buffer
     *             that can be reused.
     *
     */
    virtual bool extract_reusable_simple_buffer(
        [[maybe_unused]] simple_buffer_t & buf )
    {
        // Default implementation provides no reusable buffer.
        return false;
    }
};

//
// const_buffer_t
//

/**
 * @brief Buffer entity for const buffer (span).
 */
class const_buffer_t final : public buffer_iface_t
{
public:
    const_buffer_t() = delete;

    const_buffer_t( const_buffer_t && ) = default;
    const_buffer_t & operator=( const_buffer_t && ) = default;

    constexpr const_buffer_t( const void * data, std::size_t size ) noexcept
        : m_data{ data }
        , m_size{ size }
    {
    }

    [[nodiscard]] asio_ns::const_buffer make_asio_const_buffer() const override
    {
        return { m_data, m_size };
    }

    [[nodiscard]] asio_ns::mutable_buffer make_asio_mutable_buffer() override
    {
        throw_exception( "const_buffer_t cannot act as a write to buffer" );
    }

    [[nodiscard]] std::size_t get_size() const override { return m_size; }

    void relocate_to( void * storage ) override
    {
        new( storage ) const_buffer_t{ std::move( *this ) };
    }

private:
    const void * m_data;
    std::size_t m_size;
};

/**
 * @brief Datasizeable type constraints checker.
 *
 * Check if a given type has `.data()` and `.size()` methods with
 * proper return types. Also check a type to be move constructible.
 */
template < typename Datasizeable >
struct is_datasizeable
{
    using pure_type_t = std::decay_t< Datasizeable >;

    static constexpr auto value =
        std::is_convertible_v<
                    decltype( std::declval< const pure_type_t >().data() ),
                    const void *
                >
        && std::is_convertible_v<
                    decltype( std::declval< const pure_type_t >().size() ),
                    std::size_t
                >
        && std::is_move_constructible_v< pure_type_t >;
};

/**
 * @brief Helper variable template for is_datasizeable.
 */
template < typename Datasizeable >
inline constexpr auto is_datasizeable_v = is_datasizeable< Datasizeable >::value;

/**
 * @brief Mutalbe Datasizeable type constraints checker.
 *
 * The same as is_datasizeable, but checks enures buffer is
 * available for write.
 */
template < typename Datasizeable >
struct is_mutable_datasizeable
{
    using pure_type_t = Datasizeable;

    static constexpr auto value =
        std::is_convertible_v<
                    decltype( std::declval< pure_type_t >().data() ),
                    void *
                >
        && std::is_convertible_v<
                    decltype( std::declval< const pure_type_t >().size() ),
                    std::size_t
                >
        && std::is_move_constructible_v< pure_type_t >;
};

/**
 * @brief Helper variable template for is_mutable_datasizeable.
 */
template < typename Datasizeable >
inline constexpr auto is_mutable_datasizeable_v =
    is_mutable_datasizeable< Datasizeable >::value;

//
// datasizeable_buffer_t
//

/**
 * @brief User defined datasizable object wrapper.
 *
 * Helps to introduce an arbitrary datasizeable type as a buffer.
 */
template < typename Datasizeable >
class datasizeable_buffer_t final : public buffer_iface_t
{
    static_assert(
        is_datasizeable_v< Datasizeable >,
        "T must meet the following requirements:\n"
        "    - Datasizeable requires 'T* data() const' member function,\n"
        "      where 'T*' is convertible to 'const void*';\n"
        "    - Datasizeable requires 'N size() const' member function,\n"
        "      where 'N' is convertible to 'std::size_t';\n"
        "    - Datasizeable must be move constructible." );

public:
    datasizeable_buffer_t( Datasizeable buf )
        : m_custom_buffer{ std::move( buf ) }
    {
    }

    // datasizeable_buffer_t( datasizeable_buffer_t && ) noexcept = default;

    [[nodiscard]] asio_ns::const_buffer make_asio_const_buffer() const override
    {
        return asio_ns::const_buffer{ m_custom_buffer.data(),
                                      m_custom_buffer.size() };
    }

    [[nodiscard]] asio_ns::mutable_buffer make_asio_mutable_buffer() override
    {
        return asio_ns::mutable_buffer{
            static_cast< void * >( m_custom_buffer.data() ), m_custom_buffer.size()
        };
    }

    [[nodiscard]] std::size_t get_size() const override
    {
        return m_custom_buffer.size();
    }

    void relocate_to( void * storage ) override
    {
        new( storage ) datasizeable_buffer_t{ std::move( *this ) };
    }

    bool extract_reusable_simple_buffer( simple_buffer_t & buf ) override
    {
        if constexpr( std::is_same_v< simple_buffer_t, Datasizeable > )
        {
            buf = std::move( m_custom_buffer );
            return true;
        }
        else
        {
            return buffer_iface_t::extract_reusable_simple_buffer( buf );
        }
    }

private:
    //! A datasizeable item that represents buffer.
    Datasizeable m_custom_buffer;
};

//
// shared_datasizeable_buffer_t
//

/**
 * @brief Buffer based on shared_ptr of data-sizeable entity.
 */
template < typename Datasizeable >
class shared_datasizeable_buffer_t final : public buffer_iface_t
{
    static_assert(
        is_datasizeable_v< Datasizeable >,
        "T must meet the following requirements:\n"
        "    - Datasizeable requires 'T* data() const' member function,\n"
        "      where 'T*' is convertible to 'const void*';\n"
        "    - Datasizeable requires 'N size() const' member function,\n"
        "      where 'N' is convertible to 'std::size_t';\n"
        "    - Datasizeable must be move constructible." );

public:
    using shared_ptr_t = std::shared_ptr< Datasizeable >;

    shared_datasizeable_buffer_t() = delete;

    shared_datasizeable_buffer_t( shared_ptr_t buf_ptr ) noexcept
        : m_custom_buffer{ std::move( buf_ptr ) }
    {
    }

    [[nodiscard]] asio_ns::const_buffer make_asio_const_buffer() const override
    {
        return asio_ns::const_buffer{ m_custom_buffer->data(),
                                      m_custom_buffer->size() };
    }

    [[nodiscard]] asio_ns::mutable_buffer make_asio_mutable_buffer() override
    {
        if constexpr( is_mutable_datasizeable_v< Datasizeable > )
        {
            return asio_ns::mutable_buffer{ static_cast< void * >(
                                                m_custom_buffer->data() ),
                                            m_custom_buffer->size() };
        }
        else
        {
            throw_exception( "constant buffer cannot act as a write to buffer" );
        }
    }

    [[nodiscard]] std::size_t get_size() const override
    {
        return m_custom_buffer->size();
    }

    void relocate_to( void * storage ) override
    {
        new( storage ) shared_datasizeable_buffer_t{ std::move( *this ) };
    }

    bool extract_reusable_simple_buffer( simple_buffer_t & buf ) override
    {
        if constexpr( std::is_same_v< simple_buffer_t, Datasizeable > )
        {
            if( 1 < m_custom_buffer.use_count() )
            {
                return false;
            }

            buf = std::move( *m_custom_buffer );
            return true;
        }
        else
        {
            return buffer_iface_t::extract_reusable_simple_buffer( buf );
        }
    }

private:
    shared_ptr_t m_custom_buffer;
};

//
// adjustable_content_buffer_t
//

/**
 * @brief Buffer that is capable of adjusting its content
 *        upon requesting (data/size).
 *
 * @pre A call to adjuster should not throw otherwise results are undefined.
 * @pre Buffer type must either own the buffer or
 *      the life-time of it's content must be guaranteed
 *      for the time of using the content of the buffer.
 */
template < typename Adjuster, typename Buffer = simple_buffer_t >
class adjustable_content_buffer_t final : public buffer_iface_t
{
public:
    using buffer_t = Buffer;

    adjustable_content_buffer_t() = delete;

    adjustable_content_buffer_t( buffer_t buf, Adjuster adjuster )
        : m_buffer{ std::move( buf ) }
        , m_adjuster{ std::move( adjuster ) }
    {
    }

    [[nodiscard]] asio_ns::const_buffer make_asio_const_buffer() const override
    {
        m_adjuster( m_buffer.data(), m_buffer.size() );
        return asio_ns::const_buffer{ m_buffer.data(), m_buffer.size() };
    }

    [[nodiscard]] asio_ns::mutable_buffer make_asio_mutable_buffer() override
    {
        return asio_ns::mutable_buffer{ static_cast< void * >( m_buffer.data() ),
                                        m_buffer.size() };
    }

    [[nodiscard]] std::size_t get_size() const override { return m_buffer.size(); }

    void relocate_to( void * storage ) override
    {
        new( storage ) adjustable_content_buffer_t{ std::move( *this ) };
    }

    bool extract_reusable_simple_buffer( simple_buffer_t & buf ) override
    {
        if constexpr( std::is_same_v< buffer_t, simple_buffer_t > )
        {
            buf = std::move( m_buffer );
            return true;
        }
        else
        {
            // TODO: we can also check if buffer_t supports
            //       `extract_reusable_simple_buffer()` itself
            //       and act accordingly.
            return false;
        }
    }

private:
    mutable buffer_t m_buffer;
    [[no_unique_address]] mutable Adjuster m_adjuster;
};

//
// heterogeneous_buffer_t
//

/**
 * @bief Class for storing the buffers used for streaming data to peer.
 *
 * The data that user sends to remote peer using a connection
 * might consist of a searies of buffers and the types of those buffers
 * might vary (for example, user might be choosing different buffers
 * implementation, whatever works best under given conditions.
 *
 *  A couple of issues arises providing such mechanics.
 *  One of them is how to store all these heterogeneous buffers
 *  with fewer allocations and less boilerplate necessary
 *  to handle each corner case.
 *  Storing and moving a bunch of buffers as a vector would be
 *  nice, but vector demands a single type to be used.
 *  And `heterogeneous_buffer_t` represents a custom variant-like abstraction
 *
 *  For storing the data of buffers @c m_storage is used.
 *  It is an aligned buffer sufficient to store
 *  almost "any" buffer: let's look at a datasizeable buffer type T
 *  if `sizeof(T)` is small enough you can use `datasizeable_buffer_t<T>`;
 *  if no then you can always use `shared_datasizeable_buffer_t<T>`.
 *  Also heterogeneous_buffer_t exposes interface to treat it
 *  like a trivial buffer.
 *
 *  Having such heterogeneous_buffer_t we can store a sequence
 *  of arbitrary buffers in array-like classes (vector, dequeue).
 */
class heterogeneous_buffer_t final
{
public:
    /**
     * @brief Constant for suitable alignment for all heterogeneous buffers.
     */
    static constexpr std::size_t buffer_storage_align = std::max< std::size_t >( {
        alignof( const_buffer_t ),
        // ---------------------------------------------------------------
        // As we cannot apply alignof to a template,
        // we apply it to instantiation (using simple buffer as parameter)
        alignof( datasizeable_buffer_t< std::string > ),
        alignof( datasizeable_buffer_t< simple_buffer_t > ),
        alignof( shared_datasizeable_buffer_t< simple_buffer_t > ),
        alignof( adjustable_content_buffer_t< details::etalon_adjuster_t > )
        // ---------------------------------------------------------------
    } );

    /**
     * @brief The size of memory that is enough
     *        to hold any possible buffer entity.
     */
    static constexpr std::size_t needed_storage_max_size =
        std::max< std::size_t >( {
            sizeof( const_buffer_t ),
            // ---------------------------------------------------------------
            // As we cannot apply sizeof to a template,
            // we apply it to instantiation (using simple buffer as parameter)
            sizeof( datasizeable_buffer_t< std::string > ),
            sizeof( datasizeable_buffer_t< simple_buffer_t > ),
            sizeof( shared_datasizeable_buffer_t< simple_buffer_t > ),
            sizeof( adjustable_content_buffer_t< details::etalon_adjuster_t > )
            // ---------------------------------------------------------------
        } );

    heterogeneous_buffer_t( const heterogeneous_buffer_t & ) = delete;
    heterogeneous_buffer_t & operator=( const heterogeneous_buffer_t & ) = delete;

    heterogeneous_buffer_t( heterogeneous_buffer_t && b )
    {
        b.get_base()->relocate_to( m_storage.data() );
    }

    heterogeneous_buffer_t & operator=( heterogeneous_buffer_t && b )
    {
        if( this != &b )
        {
            destroy_stored_buffer();
            b.get_base()->relocate_to( m_storage.data() );
        }

        return *this;
    }

    ~heterogeneous_buffer_t() { destroy_stored_buffer(); }

    heterogeneous_buffer_t() noexcept
    {
        new( m_storage.data() ) const_buffer_t{ nullptr, 0 };
    }

    heterogeneous_buffer_t( const_buffer_t const_buf )
    {
        new( m_storage.data() ) const_buffer_t{ std::move( const_buf ) };
    }

    heterogeneous_buffer_t( const char * str )
        : heterogeneous_buffer_t{ std::string{ str } }
    {
    }

    heterogeneous_buffer_t( std::string_view ) = delete;

    template < typename Datasizeable >
    heterogeneous_buffer_t( Datasizeable ds )
    {
        static_assert( is_datasizeable_v< Datasizeable > );
        static_assert( sizeof( datasizeable_buffer_t< Datasizeable > )
                           <= needed_storage_max_size,
                       "size of a type is too big" );

        new( m_storage.data() )
            datasizeable_buffer_t< Datasizeable >{ std::move( ds ) };
    }

    template < typename Datasizeable >
    heterogeneous_buffer_t( std::shared_ptr< Datasizeable > sp )
    {
        static_assert( is_datasizeable_v< Datasizeable > );
        static_assert( sizeof( shared_datasizeable_buffer_t< Datasizeable > )
                           <= needed_storage_max_size,
                       "size of shared_ptr on a type is too big" );

        if( !sp )
        {
            throw_exception( "empty shared_ptr cannot be used as buffer" );
        }

        new( m_storage.data() )
            shared_datasizeable_buffer_t< Datasizeable >{ std::move( sp ) };
    }

    template < typename Adjuster, typename Buffer >
    heterogeneous_buffer_t( adjustable_content_buffer_t< Adjuster, Buffer > buf )
    {
        new( m_storage.data() )
            adjustable_content_buffer_t< Adjuster, Buffer >{ std::move( buf ) };
    }

    /**
     * @brief Get underlying buffer asio buffer.
     */
    [[nodiscard]] asio_ns::const_buffer make_asio_const_buffer() const
    {
        return get_base()->make_asio_const_buffer();
    }

    /**
     * @brief Get underlying buffer asio buffer.
     */
    [[nodiscard]] asio_ns::mutable_buffer make_asio_mutable_buffer()
    {
        return get_base()->make_asio_mutable_buffer();
    }

    /**
     * @brief Get underlying buffer as span.
     */
    template < typename Char_Type = std::byte >
    [[nodiscard]] std::span< const Char_Type > make_const_span() const
    {
        static_assert( sizeof( Char_Type ) == sizeof( std::byte ) );
        static_assert( std::is_trivial_v< Char_Type > );

        auto asio_buf = make_asio_const_buffer();
        return std::span< const Char_Type >{ reinterpret_cast< const Char_Type * >(
                                                 asio_buf.data() ),
                                             asio_buf.size() };
    }

    /**
     * @brief Get underlying buffer as string_view.
     */
    [[nodiscard]] std::string_view make_string_view() const
    {
        auto s = make_const_span< char >();
        return std::string_view{ s.data(), s.size() };
    }

    /**
     * @brief Get underlying buffer as span.
     */
    template < typename Char_Type = std::byte >
    [[nodiscard]] std::span< Char_Type > make_mutable_span()
    {
        static_assert( sizeof( Char_Type ) == sizeof( std::byte ) );
        static_assert( std::is_trivial_v< Char_Type > );

        auto asio_buf = make_asio_mutable_buffer();
        return std::span< Char_Type >{
            reinterpret_cast< Char_Type * >( asio_buf.data() ), asio_buf.size()
        };
    }

    /**
     * @brief Get underlying buffer size.
     *
     */
    [[nodiscard]] std::size_t get_size() const { return get_base()->get_size(); }

    bool extract_reusable_simple_buffer( simple_buffer_t & buf )
    {
        return get_base()->extract_reusable_simple_buffer( buf );
    }

private:
    void destroy_stored_buffer() { get_base()->~buffer_iface_t(); }

    [[nodiscard]] const buffer_iface_t * get_base() const noexcept
    {
        return std::launder(
            reinterpret_cast< const buffer_iface_t * >( m_storage.data() ) );
    }

    [[nodiscard]] buffer_iface_t * get_base() noexcept
    {
        return std::launder(
            reinterpret_cast< buffer_iface_t * >( m_storage.data() ) );
    }

    /**
     * @brief A storage that is capable of storing an arbitrary
     *        buffer.
     */
    alignas( buffer_storage_align )
        std::array< char, needed_storage_max_size > m_storage;
};

[[nodiscard]] inline buffer_fmt_integrator_t buf_fmt_integrator(
    const heterogeneous_buffer_t & buf ) noexcept
{
    const auto b = buf.make_asio_const_buffer();
    return buf_fmt_integrator( b.data(), b.size() );
}

//
// heterogeneous_buffer_driver_t
//

/**
 * @brief A buffer driver that allows to handle output_buffers
 *        of different origin. It makes it possible to
 *        supply heterogeneous buffers to adjust to user needs
 *        and not to force copies and allocation if the user
 *        already operated with buffer-like objects.
 */
struct heterogeneous_buffer_driver_t
{
    using input_buffer_t  = simple_buffer_t;
    using output_buffer_t = heterogeneous_buffer_t;

    /**
     * @brief Create an instance of a buffer of a given size.
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
     * @brief Create an instance of a buffer of a given size.
     *
     * @param old_buf  The old buffer we might reuse.
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
    [[nodiscard]] output_buffer_t reallocate_output( output_buffer_t && old_buf,
                                                     std::size_t n ) const
    {
        simple_buffer_t buf{};
        old_buf.extract_reusable_simple_buffer( buf );
        buf.resize( n );

        return buf;
    }

    /**
     * @brief Create a buffer reference that is understood by ASIO.
     *
     * @param buf  A reference to buffer for which to create asio's one.
     *
     * @return An instance of ASIO buffer.
     */
    [[nodiscard]] static asio_ns::const_buffer make_asio_const_buffer(
        const input_buffer_t & buf ) noexcept
    {
        return { buf.data(), buf.size() };
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
        return { static_cast< void * >( buf.data() ), buf.size() };
    }

    /**
     * @brief Create a buffer reference that is understood by ASIO.
     *
     * @param buf  A reference to buffer for which to create asio's one.
     *
     * @return An instance of ASIO buffer.
     */
    [[nodiscard]] static asio_ns::const_buffer make_asio_const_buffer(
        const output_buffer_t & buf )
    {
        return buf.make_asio_const_buffer();
    }

    /**
     * @brief Obtain the size of the buffer.
     *
     * @param buf  A reference to a buffer to ask for size.
     *
     */
    [[nodiscard]] static std::size_t buffer_size( const output_buffer_t & buf )
    {
        return buf.get_size();
    }

    /**
     * @brief Create a buffer reference that is understood by ASIO.
     *
     * @param buf  A reference to buffer for which to create asio's one.
     *
     * @return An instance of ASIO buffer.
     */
    [[nodiscard]] static asio_ns::mutable_buffer make_asio_mutable_buffer(
        output_buffer_t & buf )
    {
        return buf.make_asio_mutable_buffer();
    }
};

static_assert( Buffer_Driver_Concept< heterogeneous_buffer_driver_t > );

}  // namespace opio::net
