/**
 * @file
 *
 * This header file contains core library routine which is connection_t
 * and various auxiliary routines assisting it.
 */

#pragma once

#include <vector>
#include <array>
#include <queue>
#include <string>
#include <type_traits>
#include <optional>
#include <span>

#if defined( OPIO_USE_BOOST_ASIO )
#    include <boost/function.hpp>
#    include <boost/container/static_vector.hpp>
#else  // defined( OPIO_USE_BOOST_ASIO )
#    include <functional>
#endif  // defined( OPIO_USE_BOOST_ASIO )

#include <opio/net/asio_include.hpp>

#include <opio/log.hpp>

#include <opio/net/buffer.hpp>
#include <opio/net/stats.hpp>
#include <opio/net/operation_watchdog.hpp>
#include <opio/net/locking.hpp>
#include <opio/net/tcp/connection_id.hpp>
#include <opio/net/tcp/utils.hpp>
#include <opio/net/tcp/error_code.hpp>

#if !defined( OPIO_NET_QUIK_SYNC_WRITE_HEURISTIC_SIZE )
// This defines do we consider a given buffer for write-to-socket
// with async write or sync write.
// Note: that is not a stable approach and user code should not rely on it.
//       current purpose is to alternate this value in unit tests
//       to have a human-manageable buffer sizes.
#    define OPIO_NET_QUIK_SYNC_WRITE_HEURISTIC_SIZE 64 * 1024  // NOLINT
#endif

namespace opio::net::tcp
{

//
// send_buffers_result
//

/**
 * @brief The result of sending buffers.
 *
 * Acts as a parameter for send_complete_cb_t.
 */
enum class send_buffers_result
{
    /**
     * @brief Buffers were transmited successfully.
     *
     * @note It doesn't mean data was received by peer,
     *       it only means that data was reported by OS
     *       as the one that was passed to network without errors.
     */
    success,

    /**
     * @brief While sending data there was an error,
     *        Not all the data might been passed to network.
     */
    io_error,

    /**
     * @brief Buffers were not even tried to be send.
     *
     * It means there was an io error in data sending/reading well before
     * a given group of buffers was considered for sending,
     * or connection was shutdowned before it.
     */
    didnt_send,

    /**
     * @brief Buffers were rejected to be even considered for send.
     *
     * It means buffers came when it was already known that
     * connection is shutdowned.
     */
    rejected_schedule_send
};

/**
 * @brief Send completion notification callback.
 *
 * Used as a part of mechanism that allows user
 * to set a callback associated with buffers passed to `schedule_send()`
 * function. Once all the buffers in a given group are sent,
 * or send attempt failed, or connection io failed for buffers that are
 * earlier in the queue this callback is called if one is provided.
 *
 * This is intended for building a back preassure mechanisms.
 *
 * @pre This Callback should not throw otherwise results are undefined.
 */
using send_complete_cb_t =
#if defined( OPIO_USE_BOOST_ASIO )
    boost::function< void( send_buffers_result ) >;
#else   // defined( OPIO_USE_BOOST_ASIO )
    std::function< void( send_buffers_result ) >;
#endif  // defined( OPIO_USE_BOOST_ASIO )

/**
 * @brief The result of resetting socket options.
 *
 * Acts as a parameter for update_socket_options_cb_t which tells
 * how successful the operation was.
 */
enum class update_socket_options_cb_result
{
    /**
     * @brief Socket options were applied successfully.
     */
    success,

    /**
     * @brief Target socket appears to be closed.
     */
    socket_closed,

    /**
     * @brief Error occured while setting the option.
     */
    error
};

/**
 * @brief Reset socket options completion notification callback.
 *
 * When modifying socket option on a running connection
 * a CB can be provided to trace the result of operation.
 *
 * @pre This Callback should not throw otherwise results are undefined.
 */
using update_socket_options_cb_t =
    std::function< void( update_socket_options_cb_result ) >;

namespace details
{

constexpr std::size_t quik_sync_write_heuristic_size =
    OPIO_NET_QUIK_SYNC_WRITE_HEURISTIC_SIZE;

/**
 * @brief The maximum number of buffers that can be written with
 *        gather write operation.
 */
constexpr auto reasonable_max_iov_len() noexcept
{
    // That is kind of undocumented property in asio
    // ("details" namespace explicitly point this out).
    // But it is reliable for years.
    // If settled value is greater than 32, we prefer to
    // have a reasonable limited value for this (like 32).
    using iov_len_len_t = decltype( asio_ns::detail::max_iov_len );
    return std::min< iov_len_len_t >( asio_ns::detail::max_iov_len, 16 );
}

//
// buf_descriptors_span_t
//

/**
 * @brief a light weight view to continious buffs sequence.
 */
using buf_descriptors_span_t = std::span< asio_ns::const_buffer >;

/**
 * @brief An item ready to be written as a single write operation.
 *
 * Incapsulates a sequence of buffers. Controls the number of buffers
 * to allow storage of number of buffers possible to be sent with
 * gather write.
 *
 * Inside the connection_t class there is a queue of such items.
 *
 * It is intended to be used as a buf-data context for a single write operation.
 *
 * The interface of the class can be devided into following groups:
 *   1. Accumulating
 *   2. Providing (exposing accumulated buffers so that it can be used
 *      as argumentts to asio async write functions).
 *   3. Finalizing (provides access to callbacks to notify send results
 *      to party which scheduled buffers to be sent).
 *
 * @tparam Buffer_Driver                 The definition os a buffer engine.
 * @tparam Concatenated_Buffer_Max_Size  The definition of the small buffer
 *                                       which we can consider for concatenation
 *                                       to compact the existing buf-sequence.
 */
template < typename Buffer_Driver,
           std::size_t Concatenated_Buffer_Max_Size = 16 * 1024 >
class single_writable_sequence_t
{
public:
    static auto constexpr max_seq_length = reasonable_max_iov_len();

    static constexpr std::size_t concatenated_buffer_max_size =
        Concatenated_Buffer_Max_Size;

    using buffer_t = typename Buffer_Driver::output_buffer_t;
    /**
     * @brief Check if it is possible to append to this write operation.
     *
     * This function is used for loop of adding buffers to
     * "stream" of buffers to be send.
     *
     * @param  n The number of buffer considered for append.
     *
     * @return True if a @c n buffer(s) can be appended, False otherwise.
     */
    [[nodiscard]] bool can_append_buffer( std::size_t n = 1 ) const noexcept
    {
        // We actually don't check for overflow,
        // considering n is reasonably small.
        assert( n < std::numeric_limits< decltype( n ) >::max()
                        - m_bufs_storage.size() );

        return m_bufs_storage.size() + n <= max_seq_length;
    }

    /**
     * @brief Append one more buffer.
     *
     * @param buf yet another buffer.
     *
     * @pre `can_append_buffer(1)` should return true.
     */
    void append_buffer( buffer_t buf ) noexcept
    {
        assert( can_append_buffer() );

        m_bufs_storage.emplace_back( std::move( buf ) );
    }

    /**
     * @brief Append notificator to a given sequence of buffers.
     *
     * CB are considered for the whole sequence and not to a specific buffer.
     * So it not possible to assume that this CB is for the first 7 buffers
     * Callbacks and their invocation are attributed to the whole sequence.
     *
     * @param cb  Callback to call on send completion.
     */
    void append_completion_cb( send_complete_cb_t cb )
    {
        assert( m_send_completion_cbs.size() < max_seq_length );
        m_send_completion_cbs.emplace_back( std::move( cb ) );
    }

    /**
     * @brief Asio buffers ready for write.
     */
    struct asio_bufs_seq_t
    {
        buf_descriptors_span_t bufs;
        std::size_t total_size;
    };

    /**
     * @brief Get parameters for write operation.
     *
     * @note Before v0.7.0 descriptors span (`buf_descriptors_span_t`)
     *       was refering to to `const asio_ns::const_buffer`,
     *       but after introducing a fast sync write
     *       for considerably small buffers
     *       we might need to be able to modify them:
     *       to correctly refer a tail part of the data
     *       that is to be written with asyn write operation.
     *       We might think of making a different container to store
     *       buff array for async write, but that would imply
     *       sophistiacation of a logic that starts async write,
     *       because it now will have to bother how to keep lifetime
     *       of such container for the case we faced would_block event
     *       and so have to have a correction on buff references.
     *       So as for now being able to reuse the storage
     *       of the buffs allows us to keep async_write operation logic
     *       exactly the same (no additional logic or messing
     *       with allocation and  lifetime  of an extra container with bufs).
     *
     * @return  Data-pointer and size which can be used as arguments for asio
     *          async write functions.
     */
    asio_bufs_seq_t asio_bufs() noexcept
    {
        asio_bufs_seq_t res{ { m_asio_bufs.data(),
                               static_cast< buf_descriptors_span_t::size_type >(
                                   m_bufs_storage.size() ) },
                             0 };

        for( auto i = 0UL; i < m_bufs_storage.size(); ++i )
        {
            m_asio_bufs[ i ] =
                Buffer_Driver::make_asio_const_buffer( m_bufs_storage[ i ] );
        }

        for( auto i = 0UL; i < m_bufs_storage.size(); ++i )
        {
            res.total_size += m_asio_bufs[ i ].size();
        }

        return res;
    }

    /**
     * @brief Access send complete callbacks.
     */
    std::span< send_complete_cb_t > send_complete_cb_list() noexcept
    {
        return { m_send_completion_cbs.data(), m_send_completion_cbs.size() };
    }

    /**
     * @brief Concatenates adjacent small buffers.
     *
     * If we think of a single-writable-seq as a resource,
     * then we can say that we use it efficiently if
     * all buf-entries in it are considerably big.
     * The case we have two adjecent small buffers
     * (for example of size 100b, 200b) can be considered
     * a waste of resource because on the one hand it is cheap to
     * "merge" them into a single buffer and on the other hand
     * keeping them as two separate buffers we occupy
     * an extra slot which otherwise can be assigned to another
     * buffer. With connection we use gathered output
     * (sending up to `max_iov_len` at once) which means
     * we are interested in having bigger individual buffers.
     *
     *
     * This function finds adjecent sequences of buffers with total size less
     * than `Concatenated_Buffer_Max_Size` and creates a single buffer
     * instead of N. This allows us to utilize the resource more efficiently.
     *
     * If nothing gets concatenated it means all buffers
     * are large on average and we are not willing to pay
     * for copying large chunks of data to make a better
     * utilization of resource, which is already on a decent level.
     */
    void concat_small_buffers( Buffer_Driver & buffer_driver );

private:
#if defined( OPIO_USE_BOOST_ASIO )
    using buf_starage_options_t = boost::container::static_vector_options_t<
        boost::container::throw_on_overflow< false > >;

    using bufs_container_t = boost::container::
        static_vector< buffer_t, max_seq_length, buf_starage_options_t >;
    using send_completion_cbs_container_t = boost::container::
        static_vector< send_complete_cb_t, max_seq_length, buf_starage_options_t >;
#else   // defined( OPIO_USE_BOOST_ASIO )
    using bufs_container_t                = std::vector< buffer_t >;
    using send_completion_cbs_container_t = std::vector< send_complete_cb_t >;
#endif  // defined( OPIO_USE_BOOST_ASIO )

    bufs_container_t m_bufs_storage;
    std::array< asio_ns::const_buffer, max_seq_length > m_asio_bufs;
    send_completion_cbs_container_t m_send_completion_cbs;
};

template < typename Buffer_Driver, std::size_t Concatenated_Buffer_Max_Size >
void single_writable_sequence_t< Buffer_Driver, Concatenated_Buffer_Max_Size >::
    concat_small_buffers( Buffer_Driver & buffer_driver )
{
    // We expect this to be executed for a full buf seq.
    assert( 0 != m_bufs_storage.size() );

    std::size_t results_first   = 0;
    std::size_t first_to_concat = 0;
    std::size_t last_to_concat  = first_to_concat + 1;
    std::size_t new_seq_size    = m_bufs_storage.size();

    auto bufs = asio_bufs().bufs;
    while( first_to_concat < m_bufs_storage.size() )
    {
        std::size_t concatenated_size = bufs[ first_to_concat ].size();

        // Let's find the last_to_concat, so that
        // the total size of buffers starting from buf at `pos=first_to_concat`
        // and finishing with buf at `pos=last_to_concat-1`.
        while( last_to_concat < m_bufs_storage.size()
               && concatenated_size + bufs[ last_to_concat ].size()
                      <= concatenated_buffer_max_size )
        {
            concatenated_size += bufs[ last_to_concat ].size();
            ++last_to_concat;
        }

        if( last_to_concat - first_to_concat > 1 )
        {
            // So we have something to concatenate.

            // Create a new buffer to accumulate existing ones.
            auto new_buf = buffer_driver.allocate_output( concatenated_size );

            // Get the data/size reference for the buffer.
            auto buf_to      = Buffer_Driver::make_asio_mutable_buffer( new_buf );
            std::byte * dest = static_cast< std::byte * >( buf_to.data() );

            // Copy data from all the buffers in the range
            for( auto i = first_to_concat; i < last_to_concat; ++i )
            {
                auto buf_from = bufs[ i ];
                std::memcpy( dest, buf_from.data(), buf_from.size() );
                dest += buf_from.size();
            }

            m_bufs_storage[ results_first ] = std::move( new_buf );
        }
        else if( results_first != first_to_concat )
        {
            // Move a single buffer if necessary.
            // Nothing to concat but we need do move forward
            // a single buff at `pos=first_to_concat`
            m_bufs_storage[ results_first ] =
                std::move( m_bufs_storage[ first_to_concat ] );
        }

        new_seq_size -= last_to_concat - first_to_concat - 1;
        first_to_concat = last_to_concat;
        ++last_to_concat;
        ++results_first;
    }

    assert( m_bufs_storage.size() >= new_seq_size );

    m_bufs_storage.resize( new_seq_size );
}

//
// make_remote_endpoint_str()
//

/**
 * @brief Creates a string representation of endpoint behind socket.
 *
 * Acts as a default factory for obtaining remote-host endpoint description,
 * which for tcp socket creates "ipaddr:port" pair.
 */
inline std::string make_remote_endpoint_str( const asio_ns::ip::tcp::socket & s )
{
    const auto re = s.remote_endpoint();
    return fmt::format( "{}:{}", re.address().to_string(), re.port() );
}

//
// skip_transferred_part()
//

/**
 * @brief Create an adjusted span of original buffers which excludes
 *        the head data of a given size.
 *
 * @param  bufs         A span to buffers' descriptors
 *                      observing a certain data.
 * @param  transferred  The size of data already transferred
 *                      and which must be skipped in results.
 *
 * @return A new buffers' span that observes the tail part of the original data
 *         skipping the first `transferred` bytes.
 *
 * @pre The value of `transferred` must be stricktly less than the size of data
 *      observed by the buffers' descriptors in `bufs`.
 */
inline buf_descriptors_span_t skip_transferred_part( buf_descriptors_span_t bufs,
                                                     std::size_t transferred )
{
    assert( bufs.size() > 0 );

    std::size_t first_buff_to_keep = 0;
    while( bufs[ first_buff_to_keep ].size() <= transferred )
    {
        transferred -= bufs[ first_buff_to_keep ].size();
        ++first_buff_to_keep;
    }

    buf_descriptors_span_t res = bufs.subspan( first_buff_to_keep );
    res[ 0 ] = { static_cast< const std::byte * >( res[ 0 ].data() ) + transferred,
                 res[ 0 ].size() - transferred };

    return res;
}

}  // namespace details

//
// connection_cfg_t
//

/**
 * @brief Connection settings.
 *
 * Incapsulates adjustments parameters for connection_t.
 * and comes with a handy fluent-interface:
 * @code
 * auto make_connection( socket_t socket, ... )
 * {
 *     return std::make_shared< connection_t >(
 *         std::move( socket ),
 *         opio::net::tcp::connection_cfg_t{}
 *          .input_buffer_size( 4096 )
 *          .write_timeout_per_1mb( std::chrono::milliseconds( 100 ) );
 * }
 * @code
 */
class connection_cfg_t
{
public:
    using timeout_type_t = std::chrono::steady_clock::duration;

    [[nodiscard]] auto input_buffer_size() const noexcept
    {
        return m_input_buffer_size;
    }
    connection_cfg_t & input_buffer_size( std::size_t value ) & noexcept
    {
        assert( value > 0 );
        m_input_buffer_size = value;
        return *this;
    };
    connection_cfg_t && input_buffer_size( std::size_t value ) && noexcept
    {
        return std::move( this->input_buffer_size( value ) );
    }

    [[nodiscard]] auto write_timeout_per_1mb() const noexcept
    {
        return m_write_timeout_per_1mb;
    }
    connection_cfg_t & write_timeout_per_1mb( timeout_type_t value ) & noexcept
    {
        if( value > std::chrono::seconds::zero() )
        {
            // It should be a non zero value.
            m_write_timeout_per_1mb = value;
        }
        return *this;
    };
    connection_cfg_t && write_timeout_per_1mb( timeout_type_t value ) && noexcept
    {
        return std::move( this->write_timeout_per_1mb( value ) );
    }

    /**
     * @brief Calculate timeout for a specific amount of data.
     *
     * @param  buffer_size_bytes  Size of data measured in bytes.
     * @return                    Returns `(data in mb) * write_timeout_per_1mb`
     *                            if data size is greater than 1 mb,
     *                            otherwise return 0.
     */
    timeout_type_t make_write_timeout_per_buffer(
        std::size_t buffer_size_bytes ) const noexcept
    {
        constexpr std::size_t size_1mb           = 1024 * 1024;
        constexpr std::size_t adjust_for_ceiling = size_1mb - 1;

        if( size_1mb >= buffer_size_bytes )
        {
            // First quick option:
            return m_write_timeout_per_1mb;
        }
        else
        {
            // It is a conscious div 1mb (which is power of 2),
            // as compilers would optimize it to bit shift,
            // thus giving fast math.
            return ( ( buffer_size_bytes + adjust_for_ceiling ) / size_1mb )
                   * m_write_timeout_per_1mb;
        }
    }

private:
    static constexpr std::size_t default_input_buffer_size = 256 * 1024;
    std::size_t m_input_buffer_size{ default_input_buffer_size };

    static constexpr timeout_type_t default_write_timeout_per_1mb =
        std::chrono::seconds{ 1 };
    timeout_type_t m_write_timeout_per_1mb{ default_write_timeout_per_1mb };
};

// A forward declaration of connection.
template < typename Traits >
class connection_t;

//
// input_ctx_t
//

/**
 * @brief Input context.
 *
 * Acts as a parameter for input handler which is an entry
 * point for user defined input data handling routine.
 *
 * Gives input handler implementation access to a obtained buffer
 * and also provides access to logger and connection by references.
 * So handler can do some logging and is able to manipulate connection.
 *
 * @tparam Traits  Traits class of associated connection.
 */
template < typename Traits >
class input_ctx_t
{
public:
    using source_connection_t = connection_t< Traits >;
    using buffer_t            = typename source_connection_t::input_buffer_t;
    using logger_t            = typename source_connection_t::logger_t;

private:
    // Gives access to connection class:
    friend class connection_t< Traits >;

    input_ctx_t( buffer_t buf,
                 logger_t & logger,
                 source_connection_t & connection )
        : m_buffer{ std::move( buf ) }
        , m_logger{ logger }
        , m_connection{ connection }
    {
    }

    // This would demand input handlers to receive input_ctx_t
    // only by reference.
    input_ctx_t( const input_ctx_t & ) = delete;
    input_ctx_t( input_ctx_t && )      = delete;
    input_ctx_t & operator=( const input_ctx_t & ) = delete;
    input_ctx_t & operator=( input_ctx_t && ) = delete;

public:
    /**
     * @brief Access input buffer by reference.
     *
     * The buffer references raw data read from socket.
     * By default the size of the buffer is at most what is
     * the default `input_buffer_size` (see @c connection_cfg_t)
     * passed into sonstructor of source_connection_t.
     * But that can be customized with input_ctx_t::next_read_buffer.
     *
     * The exact size of data depends on how much data was received from
     * peer and accumulated for the underlying socket at the moment
     * IO event happened.
     *
     * @note Buffer migh be moved,
     *       thus next time an empty buffer will be returned.
     *
     * @code
     * auto make_input_handler() {
     *     return [ & ]( source_connection_t<Traits>::input_ctx_t & ctx ) {
     *         // Echo:
     *         ctx.connection().schedule_send( std::move( ctx.buf() ) );
     *     }
     * }
     * @endcode
     *
     * @return reference to buffer object.
     */
    [[nodiscard]] buffer_t & buf() noexcept { return m_buffer; }

    /**
     * @brief Get a reference to logger.
     *
     * This allows user provided handler to log to the same log that
     * source_connection_t logs to.
     */
    [[nodiscard]] logger_t & log() noexcept { return m_logger; };

    /**
     * @brief Get a reference to connection.
     *
     * This allows user provided handler to log to the same log that
     * source_connection_t logs to.
     */
    [[nodiscard]] source_connection_t & connection() noexcept
    {
        return m_connection;
    };

    /**
     * @brief Explicitly set next buffer.
     *
     * This function allows to override default behaviour regarding
     * input read buffer size and also regarding what is the source of
     * such buffer (by defaul a new buffer "vector<char>" is created).
     * If, based on something (the data in a given buffer), user
     * decides that the next buffer can be of some specific size
     * or of some specific source it can handle it using this function
     *
     * @param buf  Next buffer that would be used for read operation.
     */
    void next_read_buffer( buffer_t buf ) noexcept
    {
        m_next_read_buffer = std::move( buf );
    }
    ///@}

private:
    buffer_t m_buffer;
    logger_t & m_logger;
    source_connection_t & m_connection;
    std::optional< buffer_t > m_next_read_buffer;
};

//
// connection_shutdown_reason
//

/**
 * @brief Flags for connection shutdown reason.
 *
 * Acts as an argument for shutdown handler assigned to connection.
 */
enum class connection_shutdown_reason
{
    user_initiated,
    io_error,
    eof,
    write_timeout,
    read_ts_not_supported_on_this_platform
};

//
// shutdown_handler_t
//

/**
 * @brief An alias for connection shutdown handler.
 *
 * This allows user to specify handling on event of connection shutdown.
 */
using shutdown_handler_t = std::function< void( connection_shutdown_reason ) >;

#define OPIO_NET_CONNECTION_LOCK_GUARD( conn_ptr ) \
    lock_guard_t lock { ( conn_ptr )->m_lock }

//
// connection_t
//

/**
 * @brief Raw connection service class.
 *
 * Builds an IO "service" around a tcp socket exposing some sort of
 * "bytes-in/bytes-out" interface.
 * The key idea is that for handling incoming data user provides
 * an input handler and for sending data it uses `schedule_send()` routine.
 *
 * @note Connection is designed to be stored as a shared pointer.
 *       And that should be considered for input handler implementation:
 *       for instance, if input handler is provided as lambda
 *       it should not capture (directly or inderectly) any obect that
 *       owns (or shares the ownership) the context which in its turn
 *       owns a connection object itself. Otherwise, it would build
 *       something like a mutually referencing shared pointers.
 *
 * @note All the core logic of this class runs on asio event loop
 *       and in case of multiple-thread asio event loop
 *       connection_t should be customized with real_strand_t
 *       in the traits class, which guarantees the assumption that all
 *       callbacs execution associated with this connection are serialized.
 *       That means that on the context of callbacks which constitute
 *       the behaviour ot this class we don't bother about synchronization.
 *       That is why implementation doesn't use any locking approaches
 *       (mutex, atomic, etc.) other then asio excutors (strands).
 *
 * @note Logger assumes no other synchronisation than strand,
 *       so logging happens only if it is safe to call it.
 *
 * @note If connection doesn't run a read operation and doesn't do writes
 *       it means that connection socket is in a state that you
 *       can't tell if the peer "closed" connection (or tcp connection is lost).
 *       So it is not recommended to disable read with @c `stop_reading()`
 *       function without a conscious reason.
 *
 * @tparam Traits  Customization of how connection works and
 *                 what it relies on.
 */
template < typename Traits >
class connection_t final
    : public std::enable_shared_from_this< connection_t< Traits > >
{
public:
    /**
     * @brief Shortcut aliases for types used in implementation.
     */
    using sptr_t = std::shared_ptr< connection_t >;

    /**
     * @brief Underlying socket.
     */
    using socket_t = typename Traits::socket_t;

    /**
     * @brief Strand under which connection events are executed.
     *
     *  connection events include the following:
     *
     *  - IO completion handlers;
     *  - execution of user initiated operations (send, close, etc).
     */
    using strand_t = typename Traits::strand_t;

    /**
     * @brief User defined logger.
     */
    using logger_t = typename Traits::logger_t;

    /**
     * @brief Implementation of timer mechanics.
     */
    using operation_watchdog_t = typename Traits::operation_watchdog_t;

    /**
     * @brief Custom buffer routine.
     */
    using buffer_driver_t = typename Traits::buffer_driver_t;
    static_assert( ::opio::net::Buffer_Driver_Concept< buffer_driver_t > );

    /**
     * @brief The definition of read buffer for this connection.
     */
    using input_buffer_t = typename buffer_driver_t::input_buffer_t;

    /**
     * @brief The definition of write buffer for this connection.
     */
    using output_buffer_t = typename buffer_driver_t::output_buffer_t;

    /**
     * @brief The means to keeptrack of IO stats.
     */
    using stats_driver_t = typename Traits::stats_driver_t;

    /**
     * @brief An alias for input handler.
     *
     * Input handler is an entry point to user provided logic regarding
     * inpu data (which is raw bytes buffer).
     */
    using input_handler_t = typename Traits::input_handler_t;

    /**
     * @name Complementary lock routines.
     *
     * The purpose is to allow write operations
     * from non asio controled execution context
     * without doing post (initialyy dispatch).
     *
     * For standard case it must be a noop-structs.
     */
    ///@{
    using lock_t       = typename Traits::locking_t::lock_t;
    using lock_guard_t = typename Traits::locking_t::lock_guard_t;
    ///@}

    /**
     * @brief Fluent interface param setter for creating connection instance.
     *
     * Acts as a factory for parameters that connection_t requires for its
     * construction. The intention is to reduce the boilerplate necessary
     * to provide all the parameters.
     *
     * It allows to skip setting of parameters that are otherwise unavoidable
     * when using direct constructor or a fully parameterized `make()` function.
     *
     * If the user skips a setting of a given parameter
     * (`params.logger( make_my_logger() );`
     * then two options are possible:
     *
     * - If param type is default constructible then a default
     *   constructed instance would be used.
     *
     * - If the type is not default constructible an exception would be thrown.
     *
     * A sample of using this param factory:
     * @code
     *    using connection_t = opio::net::tcp::connection_t< Traits >;
     *    // ...
     *    auto conn = connection_t::make(
     *        std::move( socket ),
     *        [&]( auto & params ){  // <--- This is a param factory.
     *            params
     *                .connection_id( id )
     *                .connection_cfg( cfg )
     *                .logger( std::move( logger ) )
     *                .input_handler( std::move( input_handler ) );
     *        } );
     * @endcode
     */
    class ctor_params_t
    {
    public:
        ctor_params_t() = default;

        ctor_params_t( const ctor_params_t & ) = delete;
        ctor_params_t & operator=( const ctor_params_t & ) = delete;

        ctor_params_t( ctor_params_t && ) = default;
        ctor_params_t & operator=( ctor_params_t && ) = default;
        //============================================================

        ctor_params_t & connection_id( connection_id_t conn_id ) & noexcept
        {
            m_conn_id = conn_id;
            return *this;
        }

        ctor_params_t && connection_id( connection_id_t conn_id ) && noexcept
        {
            return std::move( this->connection_id( conn_id ) );
        }

        connection_id_t connection_id() const noexcept { return m_conn_id; }
        //============================================================

        ctor_params_t & connection_cfg( connection_cfg_t cfg ) & noexcept
        {
            m_cfg = cfg;
            return *this;
        }

        ctor_params_t && connection_cfg( connection_cfg_t cfg ) && noexcept
        {
            return std::move( this->connection_cfg( cfg ) );
        }

        const connection_cfg_t & connection_cfg() const noexcept { return m_cfg; }
        //============================================================

        ctor_params_t & logger( logger_t log ) &
        {
            m_logger.emplace( std::move( log ) );
            return *this;
        }

        ctor_params_t && logger( logger_t log ) &&
        {
            return std::move( this->logger( std::move( log ) ) );
        }

        logger_t logger_giveaway()
        {
            return giveaway( std::move( m_logger ), "logger" );
        }
        //============================================================

        ctor_params_t & buffer_driver( buffer_driver_t buffer_driver ) &
        {
            m_buffer_driver.emplace( std::move( buffer_driver ) );
            return *this;
        }

        ctor_params_t && buffer_driver( buffer_driver_t buffer_driver ) &&
        {
            return std::move( this->buffer_driver( std::move( buffer_driver ) ) );
        }

        buffer_driver_t buffer_driver_giveaway()
        {
            return giveaway( std::move( m_buffer_driver ), "buffer_driver" );
        }
        //============================================================

        ctor_params_t & input_handler( input_handler_t input_handler ) &
        {
            m_input_handler.emplace( std::move( input_handler ) );
            return *this;
        }

        ctor_params_t && input_handler( input_handler_t input_handler ) &&
        {
            return std::move( this->input_handler( std::move( input_handler ) ) );
        }

        input_handler_t input_handler_giveaway()
        {
            return giveaway( std::move( m_input_handler ), "input_handler" );
        }
        //============================================================

        ctor_params_t & shutdown_handler( shutdown_handler_t shutdown_handler ) &
        {
            m_shutdown_handler = std::move( shutdown_handler );
            return *this;
        }

        ctor_params_t && shutdown_handler( shutdown_handler_t shutdown_handler ) &&
        {
            return std::move(
                this->shutdown_handler( std::move( shutdown_handler ) ) );
        }

        shutdown_handler_t shutdown_handler_giveaway()
        {
            // That is a std::function so, it can be empty
            // that is ok.
            return std::move( m_shutdown_handler );
        }
        //============================================================

        ctor_params_t & operation_watchdog(
            operation_watchdog_t operation_watchdog ) &
        {
            m_operation_watchdog.emplace( std::move( operation_watchdog ) );
            return *this;
        }

        ctor_params_t && operation_watchdog(
            operation_watchdog_t operation_watchdog ) &&
        {
            return std::move(
                this->operation_watchdog( std::move( operation_watchdog ) ) );
        }

        std::optional< operation_watchdog_t > operation_watchdog_giveaway()
        {
            // We don't use `giveaway(p)` here because we can try
            // one more approach:
            //     When we are creating an instance of connection we
            //     have a socket and we can get its executor
            //     (`sock.get_executor()`) so we can also check
            //     if we can construct operation_watchdog of a given type
            //     with executor as parameter.
            //
            // So here we leave a higher level to decide whether it can handle it
            // or not.
            return std::move( m_operation_watchdog );
        }
        //============================================================

        ctor_params_t & stats_driver( stats_driver_t stats_driver ) &
        {
            m_stats_driver.emplace( std::move( stats_driver ) );
            return *this;
        }

        ctor_params_t && stats_driver( stats_driver_t stats_driver ) &&
        {
            return std::move( this->stats_driver( std::move( stats_driver ) ) );
        }

        stats_driver_t stats_driver_giveaway()
        {
            return giveaway( std::move( m_stats_driver ), "stats_driver" );
        }
        //============================================================

    private:
        template < typename Param_Type >
        static Param_Type giveaway( std::optional< Param_Type > && param,
                                    std::string_view param_name )
        {
            if( !param )
            {
                if constexpr( std::is_default_constructible_v< Param_Type > )
                {
                    return Param_Type{};
                }
                else
                {
                    throw std::runtime_error{ fmt::format(
                        "connection parameter {} must be explicitly set "
                        "(the type is not default constructible)",
                        param_name ) };
                }
            }

            Param_Type result{ std::move( *param ) };
            return result;
        }

        connection_id_t m_conn_id{};
        connection_cfg_t m_cfg{};
        std::optional< logger_t > m_logger;
        std::optional< buffer_driver_t > m_buffer_driver;
        std::optional< input_handler_t > m_input_handler;
        shutdown_handler_t m_shutdown_handler;
        std::optional< operation_watchdog_t > m_operation_watchdog;
        std::optional< stats_driver_t > m_stats_driver;
    };

protected:
    /**
     * @brief A proxy function for obtaining remote endpoint string description.
     *
     * Assists constructor when creating a value for const string data-member
     * of connection which must be set upon constructin an instance of connection.
     *
     * @param  socket  Target socket.
     *
     * @return  String representation of a remote endpoint.
     */
    static std::string cusomizable_make_remote_endpoint_str(
        const socket_t & socket )
    {
        // Here we promote our own implementations
        // as the default ones.
        using details::make_remote_endpoint_str;

        // Technically we don't know the eventual type of the socket
        // so if we have another socket implementation
        // (which if done in decent way lives in its own namespace)
        // it can provide its own `make_remote_endpoint_str`
        // in the same namespace. And if so compiler would pick the
        // ADL based implementation.
        // That allows us to integrate the library with another types of sockets
        // without modifying libraries code.
        return make_remote_endpoint_str( socket );
    }

    using ioctx_executor_t = std::decay_t< decltype(
        std::declval< asio_ns::io_context & >().get_executor() ) >;

    static ioctx_executor_t make_executor_from_socket( socket_t & socket )
    {
        auto ex                  = socket.get_executor();
        auto * original_executor = ex.template target< ioctx_executor_t >();
        assert( original_executor );
        return *original_executor;
    }

    /**
     * @brief Main constructo of this class.
     *
     * @param  socket            An instance to socket around which to build
     *                           connection "service".
     * @param  conn_id           A unique identifyer of the connection object.
     * @param  cfg               Parameters used for this connection.
     * @param  logger            Logger assigned to this connection.
     * @param  buffer_driver     Buffer driver.
     * @param  input_handler     User defined logic to handle input data
     *                           (the data received from peer).
     *                           This MUST not be empty.
     * @param  shutdown_handler  User defined shutdown handlers.
     * @param  watchdog          An implementation of hanging timer guards.
     * @param  stats             An implementation of IO stats.
     */
    connection_t( socket_t socket,
                  connection_id_t conn_id,
                  const connection_cfg_t & cfg,
                  logger_t logger,
                  buffer_driver_t buffer_driver,
                  input_handler_t input_handler,
                  shutdown_handler_t shutdown_handler,
                  operation_watchdog_t watchdog,
                  stats_driver_t stats )
        : m_socket{ std::move( socket ) }  // , m_strand{ m_socket.get_executor() }
        , m_strand{ make_executor_from_socket( m_socket ) }
        , m_conn_id{ conn_id }
        , m_cfg{ cfg }
        , m_logger{ std::move( logger ) }
        , m_buffer_driver{ std::move( buffer_driver ) }
        , m_input_handler{ std::move( input_handler ) }
        , m_shutdown_handler{ std::move( shutdown_handler ) }
        , m_write_operation_watchdog{ std::move( watchdog ) }
        , m_stats{ std::move( stats ) }
        , m_remote_endpoint_str{ cusomizable_make_remote_endpoint_str( m_socket ) }
    {
        asio_ns::ip::tcp::no_delay option{};
        m_socket.non_blocking( true );

        m_logger.info( OPIO_SRC_LOCATION, [ & ]( auto out ) {
            format_to( out,
                       "[{};cid:{}] Create new connection instance (@{})",
                       m_remote_endpoint_str,
                       m_conn_id,
                       static_cast< const void * >( this ) );
        } );
        // We should alway have a single element in the queue ()that is an
        // invariant.
        m_write_queue.push( {} );

        m_read_buffer =
            m_buffer_driver.allocate_input( m_cfg.input_buffer_size() );
    }

public:
    //! @name Factory function to create an instance of a connection.
    ///@{

    /**
     * @brief A fully parametrized factory for connection.
     *
     * @see connection_t::connection_t
     * @return A shared instance of connection.
     */
    static sptr_t make( socket_t socket,
                        connection_id_t conn_id,
                        const connection_cfg_t & cfg,
                        logger_t logger,
                        buffer_driver_t buffer_driver,
                        input_handler_t input_handler,
                        shutdown_handler_t shutdown_handler,
                        operation_watchdog_t operation_watchdog,
                        stats_driver_t stats_driver )
    {
        return sptr_t( new connection_t{ std::move( socket ),
                                         conn_id,
                                         cfg,
                                         std::move( logger ),
                                         std::move( buffer_driver ),
                                         std::move( input_handler ),
                                         std::move( shutdown_handler ),
                                         std::move( operation_watchdog ),
                                         std::move( stats_driver ) } );
    }

    /**
     * @brief Creates connection using ctor params aggregate.
     */
    static sptr_t make( socket_t socket, ctor_params_t params )
    {
        auto operation_watchdog = params.operation_watchdog_giveaway();

        if( !operation_watchdog )
        {
            // If operation_watchdog is not provided then we can
            // try to construct it with `socket.get_executor()` as parameter
            // or maybe it is default constructible.
            if constexpr( std::is_constructible_v< operation_watchdog_t,
                                                   decltype(
                                                       socket.get_executor() ) > )
            {
                operation_watchdog = operation_watchdog_t{ socket.get_executor() };
            }
            else if constexpr( std::is_default_constructible_v<
                                   operation_watchdog_t > )
            {
                operation_watchdog = operation_watchdog_t{};
            }
            else
            {
                // Here We can't proceed without actual instance.
                throw std::runtime_error{
                    "connection parameter operation_watchdog must be explicitly "
                    "set "
                    "(the type is not default constructible or constructible "
                    "from socket.get_executor())"
                };
            }
        }

        return make( std::move( socket ),
                     params.connection_id(),
                     params.connection_cfg(),
                     params.logger_giveaway(),
                     params.buffer_driver_giveaway(),
                     params.input_handler_giveaway(),
                     params.shutdown_handler_giveaway(),
                     std::move( *operation_watchdog ),
                     params.stats_driver_giveaway() );
    }

    /**
     * @brief A boilerplate-killer version of connection factory
     *        which expects a function-like object (usually lambda)
     *        to set parameters.
     *
     * @code
     *    using connection_t = opio::net::tcp::connection_t< Traits >;
     *    // ...
     *    auto conn = connection_t::make(
     *        std::move( socket ),
     *        [&]( auto & params ){ // <--- no need to specify type explicitly.
     *            params
     *                .connection_id( id )
     *                .connection_cfg( cfg )
     *                .logger( std::move( logger ) )
     *                .input_handler( std::move( input_handler ) );
     *        } );
     * @endcode
     */
    template <
        typename Param_Setter,
        std::enable_if_t< std::is_invocable_v< Param_Setter, ctor_params_t & >,
                          void * > = nullptr >
    static sptr_t make( socket_t socket, Param_Setter param_setter )
    {
        ctor_params_t params;
        param_setter( params );
        return make( std::move( socket ), std::move( params ) );
    }
    ///@}

    ~connection_t() { handle_remainig_write_queue(); }

    /**
     * @brief Start reading data from connection.
     *
     * Initiates a async read-from-socket loop.
     */
    void start_reading()
    {
        asio_ns::dispatch( m_strand, [ self = this->shared_from_this() ] {
            OPIO_NET_CONNECTION_LOCK_GUARD( self );
            // We should have at most a single read loop, otherwise results are
            // unexpected. To ensure we have only one read loop running
            // we use flag connection_t::m_read_is_enabled.
            if( !self->m_read_is_enabled )
            {
                // Ok, loop is not yet running, so we start it:
                self->m_read_is_enabled = true;
                self->initiate_read();
            }
        } );
    }

    /**
     * @brief Stop reading data from connection.
     *
     * Breaks read-from-socket loop. Works best when executed
     * in the context of input-handler call - in this case
     * the next read will not happen. When calling this function from another
     * context (e.g. from some worker thread other than which runs ASIO)
     * you should accept that it won't cancel already running read operation.
     *
     * @note Please, don't use it without a reason.
     *
     * @todo The idea for this routine was to allow back presure mechanics
     *       which is not quite fully developed right now.
     *       The possible scenario is when we notice that we handle data
     *       from a specific connection with rates lower then new data comes from
     *       peer, we can stop reading data from socket and system provided
     *       buffers for it would become exhausted and TCP would try it best
     *       to make peer stop sending data (if it also cares about back pressure).
     */
    void stop_reading()
    {
        asio_ns::dispatch( m_strand, [ self = this->shared_from_this() ] {
            OPIO_NET_CONNECTION_LOCK_GUARD( self );
            if( self->m_read_is_enabled )
            {
                // We disable next read:
                // If any read is running now, stop reading
                // will take effect once current read completes.
                self->m_read_is_enabled = false;
            }
        } );
    }

    /**
     * @brief Reset input handler.
     *
     * Intalls another input handler.
     *
     * @note As things happens asynchronously handler might not be
     *       replaced with a new one immediately.
     */
    void reset_input_handler( input_handler_t input_handler )
    {
        if( !input_handler )
        {
            throw std::runtime_error{
                "invalid input handler: empty function object"
            };
        }

        // We use dispatch as this function might be called
        // from input handler (which runs on the same strand as this whole
        // connection and so the operation can be considered safe
        // to be performed as usual function call).
        asio_ns::dispatch(
            m_strand,
            [ self = this->shared_from_this(), h = std::move( input_handler ) ] {
                OPIO_NET_CONNECTION_LOCK_GUARD( self );
                self->reset_input_handler_impl( std::move( h ) );
            } );
    }

    /**
     * @brief Reset shutdown handler.
     *
     * Installs another shutdown handler.
     *
     * @note As things happens asynchronously handler might not be
     *       replaced with a new one immediately.
     */
    void reset_shutdown_handler( shutdown_handler_t shutdown_handler )
    {
        asio_ns::dispatch( m_strand,
                           [ self = this->shared_from_this(),
                             h    = std::move( shutdown_handler ) ] {
                               OPIO_NET_CONNECTION_LOCK_GUARD( self );
                               self->reset_shutdown_handler_impl( std::move( h ) );
                           } );
    }

    /**
     * @name Send buffer routines.
     */
    /// @{
    /**
     * @brief Schedule sending a given sequence of buffers.
     *
     * @tparam Buffer_Vec  Types of container with buffers.
     */
    template < typename Buffer_Vec >
    void schedule_send_vec( Buffer_Vec bufs )
    {
        schedule_send_vec_impl< send_buffer_strategy::dispatch >(
            std::move( bufs ) );
    }

    /**
     * @brief Schedule sending a given sequence of buffers.
     *
     * @tparam Buffers  Types of objects passed as buf-parameters.
     */
    template < typename... Buffers >
    void schedule_send( Buffers &&... bufs )
    {
        schedule_send_impl< send_buffer_strategy::dispatch, Buffers... >(
            std::forward< Buffers >( bufs )... );
    }

    /**
     * @brief Schedule sending a given sequence of buffers.
     *
     * @tparam Buffer_Vec  Types of container with buffers.
     */
    template < typename Buffer_Vec >
    void post_send_vec( Buffer_Vec bufs )
    {
        schedule_send_vec_impl< send_buffer_strategy::post >( std::move( bufs ) );
    }

    /**
     * @brief Schedule sending a given sequence of buffers.
     *
     * @tparam Buffers  Types of objects passed as buf-parameters.
     */
    template < typename... Buffers >
    void post_send( Buffers &&... bufs )
    {
        schedule_send_impl< send_buffer_strategy::post, Buffers... >(
            std::forward< Buffers >( bufs )... );
    }

    /**
     * @brief Schedule sending a given sequence of buffers.
     *
     * @tparam Buffer_Vec  Types of container with buffers.
     */
    template < typename Buffer_Vec >
    void dispatch_send_vec( Buffer_Vec bufs )
    {
        schedule_send_vec_impl< send_buffer_strategy::dispatch >(
            std::move( bufs ) );
    }

    /**
     * @brief Schedule sending a given sequence of buffers.
     *
     * @tparam Buffers  Types of objects passed as buf-parameters.
     */
    template < typename... Buffers >
    void dispatch_send( Buffers &&... bufs )
    {
        schedule_send_impl< send_buffer_strategy::dispatch, Buffers... >(
            std::forward< Buffers >( bufs )... );
    }

    /**
     * @brief Schedule sending a given sequence of buffers.
     *
     * @tparam Buffer_Vec  Types of container with buffers.
     */
    template < typename Buffer_Vec >
    void aggressive_dispatch_send_vec( Buffer_Vec bufs )
    {
        schedule_send_vec_impl< send_buffer_strategy::aggressive_dispatch >(
            std::move( bufs ) );
    }

    /**
     * @brief Schedule sending a given sequence of buffers.
     *
     * @tparam Buffers  Types of objects passed as buf-parameters.
     */
    template < typename... Buffers >
    void aggressive_dispatch_send( Buffers &&... bufs )
    {
        schedule_send_impl< send_buffer_strategy::aggressive_dispatch,
                            Buffers... >( std::forward< Buffers >( bufs )... );
    }

    /**
     * @brief Schedule sending a given sequence of buffers.
     *
     * @tparam Buffer_Vec  Types of container with buffers.
     */
    template < typename Buffer_Vec >
    void schedule_send_vec_with_cb( send_complete_cb_t cb, Buffer_Vec bufs )
    {
        schedule_send_vec_impl_with_cb< send_buffer_strategy::dispatch >(
            std::move( cb ), std::move( bufs ) );
    }

    /**
     * @brief Schedule sending a given sequence of buffers.
     *
     * @tparam Buffers  Types of objects passed as buf-parameters.
     *
     * @param cb    Send completion callback.
     */
    template < typename... Buffers >
    void schedule_send_with_cb( send_complete_cb_t cb, Buffers &&... bufs )
    {
        schedule_send_impl_with_cb< send_buffer_strategy::dispatch, Buffers... >(
            std::move( cb ), std::forward< Buffers >( bufs )... );
    }

    /**
     * @brief Schedule sending a given sequence of buffers.
     *
     * @tparam Buffer_Vec  Types of container with buffers.
     */
    template < typename Buffer_Vec >
    void post_send_vec_with_cb( send_complete_cb_t cb, Buffer_Vec bufs )
    {
        schedule_send_vec_impl_with_cb< send_buffer_strategy::post >(
            std::move( cb ), std::move( bufs ) );
    }

    /**
     * @brief Schedule sending a given sequence of buffers.
     *
     * @tparam Buffers  Types of objects passed as buf-parameters.
     *
     * @param cb    Send completion callback.
     */
    template < typename... Buffers >
    void post_send_with_cb( send_complete_cb_t cb, Buffers &&... bufs )
    {
        schedule_send_impl_with_cb< send_buffer_strategy::post, Buffers... >(
            std::move( cb ), std::forward< Buffers >( bufs )... );
    }

    /**
     * @brief Schedule sending a given sequence of buffers.
     *
     * @tparam Buffer_Vec  Types of container with buffers.
     */
    template < typename Buffer_Vec >
    void dispatch_send_vec_with_cb( send_complete_cb_t cb, Buffer_Vec bufs )
    {
        schedule_send_vec_impl_with_cb< send_buffer_strategy::dispatch >(
            std::move( cb ), std::move( bufs ) );
    }

    /**
     * @brief Schedule sending a given sequence of buffers.
     *
     * @tparam Buffers  Types of objects passed as buf-parameters.
     *
     * @param cb    Send completion callback.
     */
    template < typename... Buffers >
    void dispatch_send_with_cb( send_complete_cb_t cb, Buffers &&... bufs )
    {
        schedule_send_impl_with_cb< send_buffer_strategy::dispatch, Buffers... >(
            std::move( cb ), std::forward< Buffers >( bufs )... );
    }

    /**
     * @brief Schedule sending a given sequence of buffers.
     *
     * @tparam Buffer_Vec  Types of container with buffers.
     */
    template < typename Buffer_Vec >
    void aggressive_dispatch_send_vec_with_cb( send_complete_cb_t cb,
                                               Buffer_Vec bufs )
    {
        schedule_send_vec_impl_with_cb<
            send_buffer_strategy::aggressive_dispatch >( std::move( cb ),
                                                         std::move( bufs ) );
    }

    /**
     * @brief Schedule sending a given sequence of buffers.
     *
     * @tparam Buffers  Types of objects passed as buf-parameters.
     *
     * @param cb    Send completion callback.
     */
    template < typename... Buffers >
    void aggressive_dispatch_send_with_cb( send_complete_cb_t cb,
                                           Buffers &&... bufs )
    {
        schedule_send_impl_with_cb< send_buffer_strategy::aggressive_dispatch,
                                    Buffers... >(
            std::move( cb ), std::forward< Buffers >( bufs )... );
    }
    /// @}

    /**
     * @brief Shutdown connection.
     */
    void shutdown()
    {
        asio_ns::dispatch( m_strand, [ self = this->shared_from_this() ] {
            OPIO_NET_CONNECTION_LOCK_GUARD( self );
            self->shutdown_impl( connection_shutdown_reason::user_initiated );
        } );
    }

    /**
     * @brief Executor associated with connection.
     */
    strand_t & executor() noexcept { return m_strand; }

    /**
     * @brief Get curent connection id.
     */
    connection_id_t connection_id() const noexcept { return m_conn_id; }

    /**
     * @brief Get curent connection configuration parameters.
     */
    const connection_cfg_t & cfg() const noexcept { return m_cfg; }

    /**
     * @brief Access stats driver.
     */
    stats_driver_t & stats_driver() noexcept { return m_stats; }

    /**
     * @brief Access stats driver.
     */
    buffer_driver_t & buffer_driver() noexcept { return m_buffer_driver; }

    /**
     * @brief Get a remote endpoint string (like `ip:port`).
     */
    const std::string & remote_endpoint_str() const noexcept
    {
        return m_remote_endpoint_str;
    }

    /**
     * @brief Initiate socket options update.
     *
     * Creates a CB that performs socket options' resetting and
     * schedules to run it on connection's strand.
     *
     * @param cfg                       Socket option to apply.
     * @param update_socket_options_cb  CB to notify the resuld of update.
     */
    void update_socket_options(
        socket_options_cfg_t cfg,
        update_socket_options_cb_t update_socket_options_cb = {} )
    {
        if( cfg.is_empty() )
        {
            // If no options supplied then
            // consider we already applied them.
            if( update_socket_options_cb )
            {
                update_socket_options_cb(
                    update_socket_options_cb_result::success );
            }

            return;
        }

        // Use `dispatch()` so we can take advantage
        // when the routine was called within conection's strand.
        asio_ns::dispatch( executor(),
                           [ self = this->shared_from_this(),
                             cfg  = std::move( cfg ),
                             cb   = std::move( update_socket_options_cb ) ] {
                               OPIO_NET_CONNECTION_LOCK_GUARD( self );
                               self->update_socket_options_impl( std::move( cfg ),
                                                                 std::move( cb ) );
                           } );
    }

    /**
     * @brief Access logger.
     */
    logger_t & logger() noexcept { return m_logger; }

private:
    /**
     * @brief A reduction proxy which maps any type to output_buffer_t.
     *
     * Acts as fixed type for parameters which are passed as
     * variadic arguments for schedule_send_impl().
     */
    template < typename T >
    using ensure_buffer_type_t = output_buffer_t;

    /**
     * @name Schedule sending a given sequence of buffers.
     *
     * @note: The trickery behind implementation is caused with the
     *        following requirements:
     *            - We want a comfortable API which allows to pass a n buffers:
     *              ```C++
     *              conn.schedule_send( buffer_t{ ... },
     *                                  std::move(buf2),
     *                                  std::move(buf2) );
     *              ```
     *            - We wand no unintentional copies of buffers throughout all the
     *              process starting from call of this function.
     *
     *        To meet the first requirement we need variadics. To ensure
     *        this function would be instantiated with only @c buffer_t type
     *        as the type of its parameters we use @c ensure_buffer_type_t
     *        which substitutes any type with @c nocopy_buffer_wrapper_t type,
     *        thus setting each parameter to be of a type convertible to
     *        @c buffer_t.
     *
     *        To exclude multiple allocations we also put all the buffers as a part
     *        of lambda capture (extra allocation comes from using
     *        `std::vector<buffer_t>` as a carrier for the sequence of buffers,
     *        which can be assumed a straight approach).
     *        The reason for such approach (and not using something like
     *        `std::vector<buffer_t>`) is that we know that asio
     *        will aready allocate for this `post an action` call,
     *        so we can attach our "data" to it and do not do a separate
     *        allocation. But as in the context of variadic templates we
     *        are unaware of how much items it would be we should express
     *        our intention as a single solid thing. And the way to go is
     *        a tuple which takes the load of dealing with multiple items while
     *        telling lambda capture it is a single thing. Having this
     *        when running lambda we should go back to variadics which is
     *        done with `std::apply`.
     *
     *        See the initial concept here: https://godbolt.org/z/TndzEPTvd
     *
     * @note We can do much simpler if we only accept single buffer,
     *       but it would mean that if we want to
     *       send 2 buffers (header and body) we would have 2 times
     *       synchronization and allocation on asio side, and depending on
     *       current state of the connection outgoing traffic
     *       we would use networking inefficient as we might
     *       trigger a sending of the first small packet (consider Nagle's algo
     *       disabled) and and the second packet would have to rest
     *       in the queue while ongoing send operation completes.
     *       Considering this it is a reasonable thing to do
     *       to put that much effort to handle a generic case of N buffers.
     */
    ///@{

    /**
     * @brief Enum to specify send scheduling strategy.
     */
    enum class send_buffer_strategy
    {
        // Use asio::dispatch routine to initiate senging
        dispatch,
        // Use direct function calls to initiate a send operation
        // (requires `locking_t::noop_lock==false` ) if possible
        // or fallbacks to asio::dispatch
        aggressive_dispatch,
        // Use asio::post routine to initiate senging
        post,
    };

    template < send_buffer_strategy Send_Buffer_Strategy, typename... Buffers >
    void schedule_send_impl( ensure_buffer_type_t< Buffers >... bufs )
    {
        if constexpr( send_buffer_strategy::aggressive_dispatch
                          == Send_Buffer_Strategy
                      && !Traits::locking_t::noop_lock )
        {
            // Here:
            // We can do aggressive write.
            OPIO_NET_CONNECTION_LOCK_GUARD( this );

            if( !m_schedule_for_write_is_enabled ) [[unlikely]]
            {
                // Connection object is in shutdown state, and no longer
                // accepts buffers for write.
                return;
            }

            // https://godbolt.org/z/3adhe8qa4
            // MSVC c++17 emmits C2039 error saying:
            // > 'weak_from_this': is not a member of "lambda"...
            // which is a quite confusing behaviour, so we do a more verbose way.
            // instead of just using this.
            auto * msvc_this_workaround = this;

            const auto all_aggressive = [ & ] {
                auto be_aggressive = true;
                ( msvc_this_workaround
                      ->append_outgoing_buffer_try_aggressive_write(
                          bufs, be_aggressive ),
                  ... );
                return be_aggressive;
            }();

            if( !all_aggressive )
            {
                // Not all the data went through sync write
                // completely. In that case we expect the data was added
                // to queue and we wshould initiate write as usual.
                initiate_write_if_necessary();
            }
        }
        else
        {
            // Seems like in C++20 implementation here can be cleaner.
            // https://newbedev.com/c-lambdas-how-to-capture-variadic-parameter-pack-from-the-upper-scope
            auto send_work = [ self       = this->shared_from_this(),
                               tuple_bufs = std::make_tuple(
                                   std::move( bufs )... ) ]() mutable {
                OPIO_NET_CONNECTION_LOCK_GUARD( self );
                if( !self->m_schedule_for_write_is_enabled ) [[unlikely]]
                {
                    // Connection object is in shutdown state, and no longer
                    // accepts buffers for write.
                    return;
                }

                const auto all_aggressive = std::apply(
                    [ self_ptr = self.get() ]( auto &&... bufs ) {
                        auto be_aggressive = true;

                        ( self_ptr->append_outgoing_buffer_try_aggressive_write(
                              bufs, be_aggressive ),
                          ... );
                        return be_aggressive;
                    },
                    std::move( tuple_bufs ) );

                if( !all_aggressive )
                {
                    self->initiate_write_if_necessary();
                }
            };

            if constexpr( send_buffer_strategy::post == Send_Buffer_Strategy )
            {
                asio_ns::post( m_strand, std::move( send_work ) );
            }
            else
            {
                asio_ns::dispatch( m_strand, std::move( send_work ) );
            }
        }
    }

    template < send_buffer_strategy Send_Buffer_Strategy, typename Buffer_Vec >
    void schedule_send_vec_impl( Buffer_Vec bufs )
    {
        if constexpr( send_buffer_strategy::aggressive_dispatch
                          == Send_Buffer_Strategy
                      && !Traits::locking_t::noop_lock )
        {
            // Here:
            // We can do aggressive write.
            OPIO_NET_CONNECTION_LOCK_GUARD( this );

            if( !m_schedule_for_write_is_enabled ) [[unlikely]]
            {
                // Connection object is in shutdown state, and no longer
                // accepts buffers for write.
                return;
            }

            // https://godbolt.org/z/3adhe8qa4
            // MSVC c++17 emmits C2039 error saying:
            // > 'weak_from_this': is not a member of "lambda"...
            // which is a quite confusing behaviour, so we do a more verbose way.
            // instead of just using this.
            auto * msvc_this_workaround = this;

            const auto all_aggressive = [ & ] {
                auto be_aggressive = true;
                for( auto & b : bufs )
                {
                    msvc_this_workaround
                        ->append_outgoing_buffer_try_aggressive_write(
                            b, be_aggressive );
                }

                return be_aggressive;
            }();

            if( !all_aggressive )
            {
                // Not all the data went through sync write
                // completely. In that case we expect the data was added
                // to queue and we wshould initiate write as usual.
                initiate_write_if_necessary();
            }
        }
        else
        {
            // Seems like in C++20 implementation here can be cleaner.
            // https://newbedev.com/c-lambdas-how-to-capture-variadic-parameter-pack-from-the-upper-scope
            auto send_work = [ self = this->shared_from_this(),
                               bufs = std::move( bufs ) ]() mutable {
                OPIO_NET_CONNECTION_LOCK_GUARD( self );
                if( !self->m_schedule_for_write_is_enabled ) [[unlikely]]
                {
                    // Connection object is in shutdown state, and no longer
                    // accepts buffers for write.
                    return;
                }

                auto * msvc_this_workaround = self.get();
                const auto all_aggressive   = [ & ] {
                    auto be_aggressive = true;
                    for( auto & b : bufs )
                    {
                        msvc_this_workaround
                            ->append_outgoing_buffer_try_aggressive_write(
                                b, be_aggressive );
                    }

                    return be_aggressive;
                }();

                if( !all_aggressive )
                {
                    self->initiate_write_if_necessary();
                }
            };

            if constexpr( send_buffer_strategy::post == Send_Buffer_Strategy )
            {
                asio_ns::post( m_strand, std::move( send_work ) );
            }
            else
            {
                asio_ns::dispatch( m_strand, std::move( send_work ) );
            }
        }
    }

    template < send_buffer_strategy Send_Buffer_Strategy, typename... Buffers >
    void schedule_send_impl_with_cb( send_complete_cb_t cb,
                                     ensure_buffer_type_t< Buffers >... bufs )
    {
        if constexpr( send_buffer_strategy::aggressive_dispatch
                          == Send_Buffer_Strategy
                      && !Traits::locking_t::noop_lock )
        {
            // Here:
            // We can do aggressive write.
            OPIO_NET_CONNECTION_LOCK_GUARD( this );

            if( !m_schedule_for_write_is_enabled ) [[unlikely]]
            {
                // Connection object is in shutdown state, and no longer
                // accepts buffers for write.
                return;
            }

            // https://godbolt.org/z/3adhe8qa4
            // MSVC c++17 emmits C2039 error saying:
            // > 'weak_from_this': is not a member of "lambda"...
            // which is a quite confusing behaviour, so we do a more verbose way.
            // instead of just using this.
            auto * msvc_this_workaround = this;

            const auto all_aggressive = [ & ] {
                auto be_aggressive = true;
                ( msvc_this_workaround
                      ->append_outgoing_buffer_try_aggressive_write(
                          bufs, be_aggressive ),
                  ... );
                return be_aggressive;
            }();

            if( !all_aggressive )
            {
                if( cb )
                {
                    // If send-completion-CB is not empty
                    // then attach it to the last seq-item in queue.
                    m_write_queue.back().append_completion_cb( std::move( cb ) );
                }

                // Not all the data went through sync write
                // completely. In that case we expect the data was added
                // to queue and we wshould initiate write as usual.
                initiate_write_if_necessary();
            }
            else
            {
                // Execute a callback right away:
                if( cb )
                {
                    cb( send_buffers_result::success );
                }
            }
        }
        else
        {
            // Seems like in C++20 implementation here can be cleaner.
            // https://newbedev.com/c-lambdas-how-to-capture-variadic-parameter-pack-from-the-upper-scope
            auto send_work = [ self       = this->shared_from_this(),
                               cb         = std::move( cb ),
                               tuple_bufs = std::make_tuple( output_buffer_t{
                                   std::move( bufs ) }... ) ]() mutable {
                OPIO_NET_CONNECTION_LOCK_GUARD( self );
                if( !self->m_schedule_for_write_is_enabled ) [[unlikely]]
                {
                    // Connection object is in shutdown state, and no longer
                    // accepts buffers for write.
                    self->run_send_completion_callback(
                        send_buffers_result::rejected_schedule_send, cb );

                    return;
                }

                const auto all_aggressive = std::apply(
                    [ self_ptr = self.get() ]( auto &&... bufs ) {
                        auto be_aggressive = true;

                        ( self_ptr->append_outgoing_buffer_try_aggressive_write(
                              bufs, be_aggressive ),
                          ... );
                        return be_aggressive;
                    },
                    std::move( tuple_bufs ) );

                if( !all_aggressive )
                {
                    if( cb )
                    {
                        // If send-completion-CB is not empty
                        // then attach it to the last seq-item in queue.
                        self->m_write_queue.back().append_completion_cb(
                            std::move( cb ) );
                    }
                    self->initiate_write_if_necessary();
                }
                else
                {
                    // Execute a callback right away:
                    if( cb )
                    {
                        cb( send_buffers_result::success );
                    }
                }
            };

            if constexpr( send_buffer_strategy::post == Send_Buffer_Strategy )
            {
                asio_ns::post( m_strand, std::move( send_work ) );
            }
            else
            {
                asio_ns::dispatch( m_strand, std::move( send_work ) );
            }
        }
    }

    template < send_buffer_strategy Send_Buffer_Strategy, typename Buffer_Vec >
    void schedule_send_vec_impl_with_cb( send_complete_cb_t cb, Buffer_Vec bufs )
    {
        if constexpr( send_buffer_strategy::aggressive_dispatch
                          == Send_Buffer_Strategy
                      && !Traits::locking_t::noop_lock )
        {
            // Here:
            // We can do aggressive write.
            OPIO_NET_CONNECTION_LOCK_GUARD( this );

            if( !m_schedule_for_write_is_enabled ) [[unlikely]]
            {
                // Connection object is in shutdown state, and no longer
                // accepts buffers for write.
                return;
            }

            // https://godbolt.org/z/3adhe8qa4
            // MSVC c++17 emmits C2039 error saying:
            // > 'weak_from_this': is not a member of "lambda"...
            // which is a quite confusing behaviour, so we do a more verbose way.
            // instead of just using this.
            auto * msvc_this_workaround = this;

            const auto all_aggressive = [ & ] {
                auto be_aggressive = true;
                for( auto & b : bufs )
                {
                    msvc_this_workaround
                        ->append_outgoing_buffer_try_aggressive_write(
                            b, be_aggressive );
                }

                return be_aggressive;
            }();

            if( !all_aggressive )
            {
                if( cb )
                {
                    // If send-completion-CB is not empty
                    // then attach it to the last seq-item in queue.
                    m_write_queue.back().append_completion_cb( std::move( cb ) );
                }

                // Not all the data went through sync write
                // completely. In that case we expect the data was added
                // to queue and we wshould initiate write as usual.
                initiate_write_if_necessary();
            }
            else
            {
                // Execute a callback right away:
                if( cb )
                {
                    cb( send_buffers_result::success );
                }
            }
        }
        else
        {
            // Seems like in C++20 implementation here can be cleaner.
            // https://newbedev.com/c-lambdas-how-to-capture-variadic-parameter-pack-from-the-upper-scope
            auto send_work = [ self = this->shared_from_this(),
                               cb   = std::move( cb ),
                               bufs = std::move( bufs ) ]() mutable {
                OPIO_NET_CONNECTION_LOCK_GUARD( self );
                if( !self->m_schedule_for_write_is_enabled ) [[unlikely]]
                {
                    // Connection object is in shutdown state, and no longer
                    // accepts buffers for write.
                    self->run_send_completion_callback(
                        send_buffers_result::rejected_schedule_send, cb );

                    return;
                }

                auto * msvc_this_workaround = self.get();
                const auto all_aggressive   = [ & ] {
                    auto be_aggressive = true;
                    for( auto & b : bufs )
                    {
                        msvc_this_workaround
                            ->append_outgoing_buffer_try_aggressive_write(
                                b, be_aggressive );
                    }

                    return be_aggressive;
                }();

                if( !all_aggressive )
                {
                    if( cb )
                    {
                        // If send-completion-CB is not empty
                        // then attach it to the last seq-item in queue.
                        self->m_write_queue.back().append_completion_cb(
                            std::move( cb ) );
                    }
                    self->initiate_write_if_necessary();
                }
                else
                {
                    // Execute a callback right away:
                    if( cb )
                    {
                        cb( send_buffers_result::success );
                    }
                }
            };

            if constexpr( send_buffer_strategy::post == Send_Buffer_Strategy )
            {
                asio_ns::post( m_strand, std::move( send_work ) );
            }
            else
            {
                asio_ns::dispatch( m_strand, std::move( send_work ) );
            }
        }
    }
    ///@}

    /**
     * @brief Append a new buffer to next to be send sequence of buffers.
     */
    void append_outgoing_buffer( output_buffer_t buf )
    {
        m_logger.debug( OPIO_SRC_LOCATION, [ & ]( auto out ) {
            format_to( out,
                       "[{};cid:{}] Appending buffer of size {}",
                       remote_endpoint_str(),
                       connection_id(),
                       m_buffer_driver.buffer_size( buf ) );

            if( logr::log_message_level::trace == m_logger.log_level() )
            {
                format_to( out, "; {}", buf_fmt_integrator( buf ) );
            }
        } );
        assert( !m_write_queue.empty() );  // Check invariant.

        auto * seq = &m_write_queue.back();

        if( !seq->can_append_buffer() ) [[unlikely]]
        {
            // The number of buf-sequences in write queue
            // until exceeding which we go with a simple logic
            // when the current "back" buf-seq in a queue
            // becomes full.
            constexpr std::size_t
                items_in_write_queue_count_not_to_really_bother_about = 1;

            auto simple_strategy_write_queue_extension = [ & ] {
                // Simple case we just add new buf-sequence to write queue.
                m_write_queue.push( {} );
                return &m_write_queue.back();
            };

            if( m_write_queue.size()
                <= items_in_write_queue_count_not_to_really_bother_about )
            {
                seq = simple_strategy_write_queue_extension();
                m_logger.debug( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                    format_to(
                        out,
                        "[{};cid:{}] Previous item in buf-seq-queue is full "
                        "(write to socket), add another one, items in "
                        "queue: {}",
                        remote_endpoint_str(),
                        connection_id(),
                        m_write_queue.size() );
                } );
            }
            else
            {
                seq->concat_small_buffers( m_buffer_driver );
                if( !seq->can_append_buffer() )
                {
                    seq = simple_strategy_write_queue_extension();
                    m_logger.debug( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                        format_to(
                            out,
                            "[{};cid:{}] Previous item in buf-queue is full "
                            "(write to socket) and connot be compacted "
                            "using small bufs concatenation, "
                            "queue: {}",
                            remote_endpoint_str(),
                            connection_id(),
                            m_write_queue.size() );
                    } );
                }
                else
                {
                    m_logger.debug( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                        format_to(
                            out,
                            "[{};cid:{}] Previous item in buf-queue is full "
                            "(write to socket), successfully apply "
                            "small bufs concatenation, the same buf-seq-queue "
                            "item would be used to append to buffer",
                            remote_endpoint_str(),
                            connection_id() );
                    } );
                }
            }
        }

        seq->append_buffer( std::move( buf ) );
    }

public:
    void append_outgoing_buffer_try_aggressive_write( output_buffer_t & buf,
                                                      bool & be_aggressive )
    {
        be_aggressive = be_aggressive && !m_is_write_operation_running;

        if( be_aggressive ) [[likely]]
        {
            const auto asio_buf = buffer_driver_t::make_asio_const_buffer( buf );

            if( details::quik_sync_write_heuristic_size <= asio_buf.size() )
                [[unlikely]]
            {
                be_aggressive = false;
                append_outgoing_buffer( std::move( buf ) );
                return;
            }

            m_logger.debug( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                format_to( out,
                           "[{};cid:{}] Aggressive write buffer of size {}",
                           remote_endpoint_str(),
                           connection_id(),
                           asio_buf.size() );
            } );

            asio_ns::error_code ec;
            m_stats.sync_write_started( asio_buf.size(), *this );

            const auto transferred = asio_ns::write( m_socket, asio_buf, ec );

            m_stats.sync_write_finished( transferred, *this );
            m_stats.inc_bytes_tx_sync( transferred, *this );

            m_logger.debug( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                format_to( out,
                           "[{};cid:{}] sync-write operation, "
                           "transferred: {}; ec: {}",
                           remote_endpoint_str(),
                           connection_id(),
                           transferred,
                           fmt_integrator( ec ) );
            } );

            if( transferred == asio_buf.size() ) [[likely]]
            {
                assert( !static_cast< bool >( ec ) );
                // Job is done, sync write succeed.
                return;
            }

            // Notify to stats that we faced would_block_error.
            m_stats.hit_would_block_event( asio_buf.size(), *this );

            be_aggressive = false;
            // We can't do aggressive writes anymore...

            if( transferred != 0 )
            {
                // Oh dear, the worst case:
                // we can't do writes anymore an current buffer is half sent...
                // so we need to create a new buffer that contains a tail
                // data (that is yet to be sent) from the current one:
                simple_buffer_t tail_buf{ static_cast< const std::byte * >(
                                              asio_buf.data() )
                                              + transferred,
                                          asio_buf.size() - transferred };

                append_outgoing_buffer( std::move( tail_buf ) );

                // Job is done. Nothing more to do for this buffer.
                return;
            }
        }

        append_outgoing_buffer( std::move( buf ) );
    }

private:
    /**
     * @brief Real implementation of shutdown.
     *
     * Tries to gracefully shutdown socket if it is open.
     *
     * @param reason  The reason for shutdown.
     */
    void shutdown_impl( connection_shutdown_reason reason )
    {
        if( !m_shutdown_was_called )
        {
            if( m_socket.is_open() )
            {
                auto report_err_if_needed = [ this ]( auto src_location,
                                                      const auto & ec,
                                                      auto func ) {
                    if( ec )
                    {
                        m_logger.warn( src_location, [ & ]( auto out ) {
                            format_to( out,
                                       "[{};cid:{}] {} finished with error: {}",
                                       remote_endpoint_str(),
                                       connection_id(),
                                       func,
                                       fmt_integrator( ec ) );
                        } );
                    }
                };

                // No more further reads:
                m_read_is_enabled = false;

                // Do not accept data for write anymore:
                m_schedule_for_write_is_enabled = false;

                asio_ns::error_code ec;

#if !defined( OPIO_NET_ASIO_WINDOWS )
                m_socket.shutdown( asio_ns::ip::tcp::socket::shutdown_both, ec );
#else   // !defined( OPIO_NET_ASIO_WINDOWS )

                // For windows `m_socket.shutdown(...)` doesn't work
                // reliable on windows and might give the error:
                // ```
                // Failed read operation: {121(0x79)
                // "The semaphore timeout period has expired."}
                // ```
                // Which happens after 120 seconds
                // which is unacceptable, so for windows we
                // call close which is a stronger action.
                m_socket.close( ec );
#endif  // !defined( OPIO_NET_ASIO_WINDOWS )

                report_err_if_needed( OPIO_SRC_LOCATION, ec, "socket.shutdown" );

                m_logger.trace( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                    format_to( out,
                               "[{};cid:{}] socket is shutdowned",
                               remote_endpoint_str(),
                               connection_id() );
                } );

                // We don't want watchdog to happen after we shutdown connection:
                m_write_operation_watchdog.cancel_watch_operation();
            }

            if( m_shutdown_handler )
            {
                // Call only if function object is not empty.
                m_shutdown_handler( reason );
            }
            // Regardless of whether any actual call happened
            // consider it happened.
            m_shutdown_was_called = true;
        }
    }

    /**
     * @brief Check if we can start write operation and runs it
     *        in event we can start.
     *
     * Conditions to start write operatio are:
     *
     *   - No write operation runs at the moment.
     *   - There is actually something in the output queue to send to peer.
     */
    void initiate_write_if_necessary()
    {
        assert( !m_write_queue.empty() );

        if( m_is_write_operation_running ) [[unlikely]]
        {
            m_logger.trace( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                format_to( out,
                           "[{};cid:{}] Skip starting write opertion: "
                           "already running",
                           remote_endpoint_str(),
                           connection_id() );
            } );
            return;
        }

        auto bufs_seq = m_write_queue.front().asio_bufs();

        if( bufs_seq.bufs.size() == 0 ) [[unlikely]]
        {
            m_logger.trace( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                format_to( out,
                           "[{};cid:{}] Skip starting write opertion: "
                           "no buffers accumulated",
                           remote_endpoint_str(),
                           connection_id() );
            } );
            return;
        }

        // When starting async write or successfully completing sync write
        // we "freeze" a first item in the queue.
        // In async-write case:
        // Given that we have only one item in queue
        // we cannot use the same item for adding buffers.
        // That's why to be capable of receiving more outgoing buffers
        // we whould add a new item so `m_write_queue.back()`
        // would refer to the item that is not freezed.
        //
        // Successfull sync-write case:
        // Given that we have only one item in queue
        // and we've just have written it to socket
        // we should add another item to queue.
        // Note: strictly speaking in this particular case
        // we might reuse this single item in queue because there is no way
        // that access to the queue (adding new buffers)
        // happens as we do write synchronously but
        // to "complete" sync-scenario of this routine the same way
        // as async one we bring the queue to the same state as in
        // the asunc-scenario and can call after_write() as a completion
        // right away.
        auto freeze_first_buf_sequece_in_queue = [ this ] {
            if( 1 == m_write_queue.size() )
            {
                m_write_queue.push( {} );

                m_logger.trace( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                    format_to( out,
                               "[{};cid:{}] First item in queue of size 1 is "
                               "freezed for running write operation, adding one "
                               "more item to receive output buffers",
                               remote_endpoint_str(),
                               connection_id() );
                } );
            }
        };

        auto start_async_write = [ & ] {
            m_logger.debug( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                format_to( out,
                           "[{};cid:{}] Starting async_write operation, "
                           "number of buffers: {}; "
                           "size in bytes: {}",
                           remote_endpoint_str(),
                           connection_id(),
                           bufs_seq.bufs.size(),
                           bufs_seq.total_size );

                if( bufs_seq.bufs.size() > 1 )
                {
                    format_to( out, "; buf-sizes: [" );
                    for( const auto & b : bufs_seq.bufs )
                    {
                        format_to( out, "{}, ", b.size() );
                    }
                    format_to( out, "]" );
                }
            } );

            // Please note, we will use WEAK POINTER to raw connection.
            // And the reason is that `this` owns m_write_operation_watchdog
            // and passing a true shared pointer to it might cause
            // circular references.

            // https://godbolt.org/z/3adhe8qa4
            // MSVC c++17 emmits C2039 error saying:
            // > 'weak_from_this': is not a member of "lambda"...
            // which is a quite confusing behaviour, so we do a more verbose way.
            // instead of just using this.
            auto * msvc_this_workaround = this;

            m_write_operation_watchdog.start_watch_operation(
                m_cfg.make_write_timeout_per_buffer( bufs_seq.total_size ),
                [ wp = msvc_this_workaround->weak_from_this() ](
                    auto timeout_key ) {
                    // This callback runs on watchdog context.
                    // So, please, note this code is not under strand,
                    // thus not safe to manipulate with connection internals.
                    if( auto conn = wp.lock(); conn )
                    {
                        // Ok, Phoenix worked, so connection object
                        // still exists.
                        //
                        // So what we do here is calling
                        // a function that would dispath
                        // handling properly.
                        conn->hadle_write_operation_timeout( timeout_key );
                    }
                } );

            m_stats.async_write_started( bufs_seq.total_size, *this );
            asio_ns::async_write(
                m_socket,
                bufs_seq.bufs,
                asio_ns::bind_executor(
                    m_strand,
                    [ self = msvc_this_workaround->shared_from_this() ](
                        const auto & ec, auto length ) {
                        self->m_stats.async_write_finished( length, *self );
                        self->after_write( ec, length );
                        self->m_stats.inc_bytes_tx_async( length, *self );
                    } ) );
            // Set the flag that we run write operation.
            m_is_write_operation_running = true;
            freeze_first_buf_sequece_in_queue();
        };

        if( details::quik_sync_write_heuristic_size >= bufs_seq.total_size )
        {
            m_logger.debug( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                format_to( out,
                           "[{};cid:{}] Starting sync_write operation, "
                           "number of buffers: {}; "
                           "size in bytes: {}",
                           remote_endpoint_str(),
                           connection_id(),
                           bufs_seq.bufs.size(),
                           bufs_seq.total_size );

                if( bufs_seq.bufs.size() > 1 )
                {
                    format_to( out, "; buf-sizes: [" );
                    for( const auto & b : bufs_seq.bufs )
                    {
                        format_to( out, "{}, ", b.size() );
                    }
                    format_to( out, "]" );
                }
            } );

            asio_ns::error_code ec;
            m_stats.sync_write_started( bufs_seq.total_size, *this );

            const auto transferred = asio_ns::write( m_socket, bufs_seq.bufs, ec );

            m_stats.sync_write_finished( transferred, *this );

            m_logger.debug( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                format_to( out,
                           "[{};cid:{}] sync-write operation, "
                           "transferred: {}; "
                           "ec: {}",
                           remote_endpoint_str(),
                           connection_id(),
                           transferred,
                           fmt_integrator( ec ) );
            } );

            auto complete_operation_now = [ & ]( const asio_ns::error_code & ec,
                                                 std::size_t length ) {
                freeze_first_buf_sequece_in_queue();
                after_write( ec, length );
                m_stats.inc_bytes_tx_sync( transferred, *this );
            };

            if( !error_is_would_block( ec ) ) [[likely]]
            {
                complete_operation_now( ec, transferred );
                return;
            }

            // Notify to stats that we faced would_block_error.
            m_stats.hit_would_block_event( bufs_seq.total_size, *this );

            if( transferred >= bufs_seq.total_size )
            {
                // Note: that should not happen if operations behave as documented.
                // So we treat this condition as one we can't survive.
                m_logger.error( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                    format_to( out,
                               "[{};cid:{}] Unexpected sync-write "
                               "operation result, transferred: {}; "
                               "ec: {}; considering 'ec' trasferred "
                               "size is expected to be less than {}, "
                               "will treat this as an error breaking "
                               "the connection",
                               remote_endpoint_str(),
                               connection_id(),
                               transferred,
                               fmt_integrator( ec ),
                               bufs_seq.total_size );
                } );

                complete_operation_now(
                    make_std_compaible_error(
                        error_codes::sync_write_unexpected_results ),
                    transferred );
                return;
            }

            // ELSE: Ok, sync write didn't succeed completly well,
            //       but we are good go with blocking approach.

            bufs_seq.total_size -= transferred;

            // Skip part of a data that was already
            // written with a sync operation.
            // Note: next line might modify `m_write_queue.front()`
            // as the field that stores buffers might experience
            // an adjustment of one of the buffers (offset the biginning
            // to no longer include bytes that were in the tail of
            // the portion of data written successfully with sync write).
            bufs_seq.bufs =
                details::skip_transferred_part( bufs_seq.bufs, transferred );
        }

        start_async_write();
    }

    /**
     * @brief Common routine to handle IO errors.
     *
     * When IO error happens there is a common sequence of steps to do
     * which is are gathered in this function.
     */
    template < typename Src_Location >
    void handle_io_error( const asio_ns::error_code & ec,
                          std::string_view operation,
                          Src_Location src_loc )
    {
        // asio_ns::error::operation_aborted means that we were initiator of
        // closing the channel (shutdown). For instance when we
        // close connection an ongoing read operation is canceled
        // with operation_aborted error code.
        if( ec == asio_ns::error::operation_aborted )
        {
            return;
        }

        auto reason = connection_shutdown_reason::io_error;

        if( ec == asio_ns::error::eof )
        {
            reason = connection_shutdown_reason::eof;
            m_logger.debug( src_loc, [ & ]( auto out ) {
                format_to( out,
                           "[{};cid:{}] peer closed connection",
                           remote_endpoint_str(),
                           connection_id() );
            } );
        }
        else
        {
            m_logger.error( src_loc, [ & ]( auto out ) {
                format_to( out,
                           "[{};cid:{}] Failed {} operation: {}",
                           remote_endpoint_str(),
                           connection_id(),
                           operation,
                           fmt_integrator( ec ) );
            } );
        }

        // Make sure to shutdown and close.
        shutdown_impl( reason );
    }

    /**
     * @brief Handle write operation result.
     */

    void after_write( const asio_ns::error_code & ec,
                      [[maybe_unused]] std::size_t length )
    {
        m_write_operation_watchdog.cancel_watch_operation();
        auto cbs = m_write_queue.front().send_complete_cb_list();

        if( ec ) [[unlikely]]
        {
            handle_io_error( ec, "write", OPIO_SRC_LOCATION );
            run_send_completion_callbacks( send_buffers_result::io_error, cbs );
            return;
        }

        m_logger.trace( OPIO_SRC_LOCATION, [ & ]( auto out ) {
            format_to( out,
                       "[{};cid:{}] Succeed write operation, written: {} bytes",
                       remote_endpoint_str(),
                       connection_id(),
                       length );
        } );

        run_send_completion_callbacks( send_buffers_result::success, cbs );

        // The first item in queue (aka seq of n buffs) is handled, so we can
        // "unfreeze" it and remove from queue.
        m_write_queue.pop();

        // When we were freeezing (which happens on write operation initiated)
        // we should have added an additional item to m_write_queue
        // and so here after pop we expect `m_write_queue`
        // to be NOT empty.
        assert( !m_write_queue.empty() );

        m_is_write_operation_running = false;
        initiate_write_if_necessary();
    }

    /**
     * @brief Handle possible write-operation timeout event implementation.
     *
     * @param timeout_key The key associated with the event.
     */
    void handle_write_operation_timeout_impl(
        typename operation_watchdog_t::timeout_event_key_t timeout_key )
    {
        // To make sure that it is really a timeout for operation we should
        // be sure that write operation is running,
        // and current timeout key of the watchdog (it is a key assigned to current
        // write operation) is equal to one that watchdog timeout check was
        // scheduled before and the check was triggered. This multiple level check
        // is required for cases when write operation completes in roughly the same
        // time that timeout is triggered. For example:
        // 1. start write operation at timepoint 0.
        // 2. Schedule watchdot of write operation at timepoint T.
        // ...
        // ... almost T
        // 3. Write operation completes and asio schedules completion
        //    token of the operation to events queue.
        // ... T
        // 4. Watchdog posts check callback to asio queue.
        //    That is because write completion token was not executed yet
        //    and the check was not cancelled.
        // ... ASIO EVENTS QUEUE now have: (write CT)(watchdog check)
        // 5. Completion token for write executes and then a new write operation
        //    is started (new watchdog would be started and a new key
        //    whould be assighed: `m_write_operation_watchdog.timeout_key()`)
        //    or there is nothing to send and so
        //    `m_is_write_operation_running == false`.
        // 6. watchdog check callback would be started and would do
        //    no harm as it would have an obsolete key or
        //    write operation is no longer running.
        if( m_is_write_operation_running
            && timeout_key == m_write_operation_watchdog.timeout_key() )
        {
            m_logger.error( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                format_to( out,
                           "[{};cid:{}] Write operation timed out",
                           remote_endpoint_str(),
                           connection_id() );
            } );
            // Make sure to shutdown and close.
            shutdown_impl( connection_shutdown_reason::write_timeout );
        }
    }

    /**
     * @brief Handle possible write-operation timeout event.
     *
     * @param timeout_key The key associated with the event.
     */
    void hadle_write_operation_timeout(
        typename operation_watchdog_t::timeout_event_key_t timeout_key )
    {
        // Timer might calls us not from our strand.
        // So we must ensure running handling on the strand of the connection.
        asio_ns::dispatch(
            m_strand, [ self = this->shared_from_this(), timeout_key ] {
                OPIO_NET_CONNECTION_LOCK_GUARD( self );
                self->handle_write_operation_timeout_impl( timeout_key );
            } );
    }

    /**
     * @brief Initiate next read operation.
     */
    void initiate_read()
    {
        m_logger.trace( OPIO_SRC_LOCATION, [ & ]( auto out ) {
            format_to( out,
                       "[{};cid:{}] Starting read operation, buffer "
                       "size in bytes: {}",
                       remote_endpoint_str(),
                       connection_id(),
                       m_read_buffer.size() );
        } );

        m_socket.async_read_some(
            m_buffer_driver.make_asio_mutable_buffer( m_read_buffer ),
            asio_ns::bind_executor(
                m_strand,
                [ self = this->shared_from_this() ]( auto ec, auto length ) {
                    // We do not lock right here,
                    // because we want to call a user callback
                    // without holding a lock
                    // So a precise control here is passed to
                    // implementation routine
                    self->after_read( ec, length );
                } ) );
    }

    /**
     * @brief Handle read operation result.
     *
     * In essence feeds input data to user defined callback,
     * prepares for next read operation and initiates it.
     */
    void after_read( const asio_ns::error_code & ec, std::size_t length )
    {
        // Ok, with locks in mind this part becomes somewhat complicated.
        // The root is that when we call a callback we must
        // free lock so that handler implementation is able to initiate
        // a write (think os req-resp interaction and imagine
        // the response is created and sent right away).

        // So the approach regarding locks here is
        // to do it precisely when it is required
        // We will also use the fact that a "competition"
        // might come only from a write routine,
        // thus not messing with read operation data
        // which we handle here.
        // And from other kinds of mutations
        // (like timer events, aply socket options, stop reading)
        // we are protected by the means of strand.

        // Checking ec doesn;t require a lock.
        if( ec ) [[unlikely]]
        {
            // Error, need to lock in that scope...
            OPIO_NET_CONNECTION_LOCK_GUARD( this );
            handle_io_error( ec, "read", OPIO_SRC_LOCATION );
            return;
        }

        // To Lock or Not To Lock?
        // Call a logging routine:
        // data subjected to logging here is either not shared with write operation
        // (from which we might expect a competition) or is immutable (const).
        m_logger.debug( OPIO_SRC_LOCATION, [ & ]( auto out ) {
            format_to( out,
                       "[{};cid:{}] Succeed read operation, received {} bytes",
                       remote_endpoint_str(),  // Const.
                       connection_id(),        // Const.
                       length );               // Not shared.

            if( logr::log_message_level::trace == m_logger.log_level() )
            // Const or safe to access ~~~~~~~~~~~~~~~~~~~~^
            {
                format_to( out,
                           "; {}",
                           buf_fmt_integrator( m_read_buffer.data(), length ) );
                // Not shared with write operations ^~~~~~~~~~~~~~~~~^
            }
        } );

        // Not shared with write operations:
        m_stats.inc_bytes_rx_async( length, *this );

        assert( m_read_buffer.size() >= length );

        // Not shared with write operations:
        input_ctx_t input_ctx{ m_buffer_driver.reduce_size_input(
                                   std::move( m_read_buffer ), length ),
                               m_logger,
                               *this };

        // Well, looks good enaugh.
        // We are here and didn't lock yet.
        m_input_handler( input_ctx );

        // Preaparing m_read_buffer for next read does not require
        // any locking.
        if( input_ctx.m_next_read_buffer
            && input_ctx.m_next_read_buffer->size() > 0 )
        {
            // If input handler callback return a buffer
            // then we rely on it.
            m_read_buffer = std::move( *input_ctx.m_next_read_buffer );
            m_logger.debug( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                format_to( out,
                           "[{};cid:{}] Next read buffer provided "
                           "by consumer, size in bytes: {}",
                           remote_endpoint_str(),
                           connection_id(),
                           m_read_buffer.size() );
            } );
        }
        else
        {
            // If the user didn't point us a buffer,
            // then the best we can try is to reuse original buffer
            // which is in `input_ctx` (if wasn't moved out by user)
            // if we are lucky then we can hopefully reuse it again
            // (if user didn't smth weired with it - like explicitly
            // release it for some reason). Anyway the buffer-driver
            // should guarantee that eventualy we will get the correct
            // buffer, and luckely we won't mess with memory allocations.
            m_read_buffer = m_buffer_driver.reallocate_input(
                std::move( input_ctx.m_buffer ), m_cfg.input_buffer_size() );
        }

        OPIO_NET_CONNECTION_LOCK_GUARD( this );
        // ^~~~~ Here we must take a lock.
        //       Because `m_read_is_enabled` is shared with read operation
        //   +------------/
        //   |   throuth error handling.
        //   |   Think of a competing write faced an error and
        //   |   starts shutdown operation.
        //   V   that would mean stop of the service.
        if( m_read_is_enabled )
        {
            initiate_read();
        }
        else
        {
            m_logger.debug( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                format_to( out,
                           "[{};cid:{}] read is disabled, skip next read",
                           remote_endpoint_str(),
                           connection_id() );
            } );
        }
    }

    /**
     * @brief Reset input handler implementation.
     */
    void reset_input_handler_impl( input_handler_t input_handler )
    {
        m_logger.debug( OPIO_SRC_LOCATION, [ & ]( auto out ) {
            format_to( out,
                       "[{};cid:{}] Reset input handler",
                       remote_endpoint_str(),
                       connection_id(),
                       m_read_buffer.size() );
        } );
        m_input_handler = std::move( input_handler );
    }

    /**
     * @brief Reset shutdown handler implementation.
     */
    void reset_shutdown_handler_impl( shutdown_handler_t shutdown_handler )
    {
        m_logger.debug( OPIO_SRC_LOCATION, [ & ]( auto out ) {
            format_to( out,
                       "[{};cid:{}] Reset shutdown handler",
                       remote_endpoint_str(),
                       connection_id(),
                       m_read_buffer.size() );
        } );
        m_shutdown_handler = std::move( shutdown_handler );
    }

    /**
     * @brief A routine to call send-completion callback.
     *
     * @pre Callback must not throw.
     */
    void run_send_completion_callback( send_buffers_result res,
                                       send_complete_cb_t & cb ) noexcept
    {
        if( cb ) [[likely]]
        {
            cb( res );
        }
    }

    /**
     * @brief A routine to call all send-completion callbacks in a given array.
     *
     * @param res  Send operation result.
     *
     * @pre Callbacks must not throw.
     */
    template < typename Container_View >
    void run_send_completion_callbacks( send_buffers_result res,
                                        Container_View cbs ) noexcept
    {
        for( auto & cb : cbs )
        {
            run_send_completion_callback( res, cb );
        }
    }

    /**
     * @brief Handle remainig write queue.
     *
     * In essence does some logging for unsent buffers and
     * calls send-completion-callback reportin the status accordingly.
     */
    void handle_remainig_write_queue()
    {
        try
        {
            m_logger.debug( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                format_to( out,
                           "[{};cid:{}] ~Destroy connection instance (@{})",
                           remote_endpoint_str(),
                           connection_id(),
                           static_cast< const void * >( this ) );
            } );

            const auto first_seq_size =
                m_write_queue.front().asio_bufs().total_size;

            if( 1 < m_write_queue.size() || 0 < first_seq_size )
            {
                m_logger.warn( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                    format_to(
                        out,
                        "[{};cid:{}] Connection's write queue is not empty, "
                        "size: {} (bytes in first seq: {})",
                        remote_endpoint_str(),
                        connection_id(),
                        m_write_queue.size(),
                        first_seq_size );
                } );

                if( m_is_write_operation_running )
                {
                    // We might experience this if io_context is stopped.
                    // Which means cancelled handlers might not be called
                    // and so the "state" might stuck with "running" write.
                    m_logger.error( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                        format_to( out,
                                   "[{};cid:{}] write operation is running, "
                                   "which is unexpected in destructor",
                                   remote_endpoint_str(),
                                   connection_id() );
                    } );

                    run_send_completion_callbacks(
                        send_buffers_result::io_error,
                        m_write_queue.front().send_complete_cb_list() );
                    m_write_queue.pop();
                }

                while( !m_write_queue.empty() )
                {
                    run_send_completion_callbacks(
                        send_buffers_result::didnt_send,
                        m_write_queue.front().send_complete_cb_list() );
                    m_write_queue.pop();
                }
            }
        }
        catch( ... )
        {
        }
    }

    /**
     * @brief Strand agnostic reset socket options logic.
     *
     * @see update_socket_options().
     */
    void update_socket_options_impl(
        socket_options_cfg_t cfg,
        update_socket_options_cb_t update_socket_options_cb )
    {
        m_logger.trace( OPIO_SRC_LOCATION, [ & ]( auto out ) {
            format_to( out,
                       "[{};cid:{}] Reset socket options with {}",
                       remote_endpoint_str(),
                       connection_id(),
                       cfg );
        } );

        auto call_cb_if_necessary = [ & ]( auto result ) {
            if( update_socket_options_cb )
            {
                update_socket_options_cb( result );
            }
        };

        if( m_socket.is_open() )
        {
            try
            {
                set_socket_options( cfg, m_socket );
                m_logger.debug( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                    format_to( out,
                               "[{};cid:{}] Reset socket options with {} succeed",
                               remote_endpoint_str(),
                               connection_id(),
                               cfg );
                } );
                call_cb_if_necessary( update_socket_options_cb_result::success );
            }
            catch( const std::exception & ex )
            {
                m_logger.error( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                    format_to( out,
                               "[{};cid:{}] Reset socket options, "
                               "with {} failed: {}",
                               remote_endpoint_str(),
                               connection_id(),
                               cfg,
                               ex.what() );
                } );
                call_cb_if_necessary( update_socket_options_cb_result::error );
            }
        }
        else
        {
            m_logger.warn( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                format_to( out,
                           "[{};cid:{}] Unable to reset socket "
                           "options because socket is closed",
                           remote_endpoint_str(),
                           connection_id() );
            } );
            call_cb_if_necessary( update_socket_options_cb_result::socket_closed );
        }
    }

    /**
     * @brief An instance of socket on which the connection operates.
     */
    socket_t m_socket;

    /**
     * @brief A strand associated with connection.
     *
     * This one can be a real strand (for cases running ASIO on multiple threads)
     * or it can be an executor itself (for cases running ASIO on a single thread).
     */
    strand_t m_strand;

    /**
     * @brief This connection id assigned in constructor.
     */
    const connection_id_t m_conn_id;

    /**
     * @brief Connection confuguration.
     */
    const connection_cfg_t m_cfg;

    /**
     * Logger for events of this connection.
     */
    [[no_unique_address]] logger_t m_logger;

    using single_writable_sequence_t =
        details::single_writable_sequence_t< buffer_driver_t >;
    using write_queue_t = std::queue< single_writable_sequence_t >;

    /**
     * @brief A queue of write operations.
     *
     * @note We should always have at least one element in the queue.
     *       Also the last item in queue (`m_write_queue.back()`)
     *       should always be possible to use. It means it shouldn't be
     *       the one that is "freezed" by currently running write operation.
     *       So write operation initiator should check if the new item should be
     *       pushed to queue.
     */
    write_queue_t m_write_queue;

    /**
     * @brief Buffer driver.
     */
    [[no_unique_address]] buffer_driver_t m_buffer_driver;

    /**
     * @brief A buffer to which read operation stores input bytes.
     */
    input_buffer_t m_read_buffer;

    /**
     * @brief A callback to handle incoming data.
     */
    [[no_unique_address]] input_handler_t m_input_handler;

    /**
     * @brief A callback to to handle shutdown event.
     */
    shutdown_handler_t m_shutdown_handler;

    /// Flag to track write operation state.
    bool m_is_write_operation_running{ false };

    /// Flag which tells if the reading from socket is running.
    bool m_read_is_enabled{ false };

    /**
     * @bief Flag which tells if connection accepts buffers for write.
     */
    bool m_schedule_for_write_is_enabled{ true };

    /// Marker whether we have called shutdown on socket.
    bool m_shutdown_was_called{ false };

    /**
     * @brief An additional lock.
     *
     * The purpose is to allow write operations
     * from non asio controled execution context
     * without doing post (initialyy dispatch).
     */
    [[no_unique_address]] lock_t m_lock;
    /**
     * @brief A watch dog to guard write operation
     */
    [[no_unique_address]] operation_watchdog_t m_write_operation_watchdog;

    [[no_unique_address]] stats_driver_t m_stats;

    /**
     * @brief `Ip:port` of the remote party.
     */
    const std::string m_remote_endpoint_str;
};

//
// Strands
//

/**
 * @brief An alias for a fake strand which does no synchronization.
 *
 * This one is an option to go with for cases when asio event loop
 * is sunning on a single thread, so all the jobs running on asio
 * are serialized by definition and it no need to pay extra costs
 * for real strand.
 */
using noop_strand_t = asio_ns::any_io_executor;

/**
 * @brief An alias for an real strand (the one that actually does serialize jobs
 *        running on asio which are assiciated with this strand).
 */
using real_strand_t = asio_ns::strand< noop_strand_t >;

//
// default_traits_st_t
//

/**
 * @brief Default tratis class for tcp connection class (connection_t)
 *        for a single thread asio event loop.
 *
 * As the whole library is customizable for specific conditions:
 *
 * - `socket_t`. By defaul it is `asio::ip::tcp::socket`,
 *               but it can also be `asio::ssl::stream<asio:ip::tcp::socket>`.
 * - `strand_t`. An executor used to run callback (jobs) associated with
 *               this connection.
 * - `logger_t`. A logger type to use for this connection.
 * - `operation_watchdog_t`. IO operation hang detection and reaction mechanism.
 * - `buffer_driver_t`. The customization driver for buffers concept.
 * - `input_handler_t`. The type of input data handler.
 * - `locking_t`. Locking mechanics details.
 */
struct default_traits_st_t
{
    using socket_t             = asio_ns::ip::tcp::socket;
    using strand_t             = noop_strand_t;
    using logger_t             = ::opio::noop_logger_t;
    using operation_watchdog_t = noop_operation_watchdog_t;
    using buffer_driver_t      = simple_buffer_driver_t;
    static_assert( ::opio::net::Buffer_Driver_Concept< buffer_driver_t > );
    using stats_driver_t = noop_stats_driver_t;
    using input_handler_t =
        std::function< void( input_ctx_t< default_traits_st_t > & ) >;

#if defined( OPIO_NET_FORCE_DEFAULT_LOCKING_WITH_MUTEX )
    // This makes it possible:
    using locking_t = mutex_locking_t;
#else   // defined( OPIO_NET_FORCE_DEFAULT_LOCKING_WITH_MUTEX )
    using locking_t = noop_locking_t;
#endif  // defined( OPIO_NET_FORCE_DEFAULT_LOCKING_WITH_MUTEX )
};

//
// default_traits_mt_t
//

/**
 * @brief Default tratis class for tcp connection class (connection_t)
 *        for a multiple threads asio event loop.
 *
 * Derives from single-thread traits and overrides strand and
 * input handler.
 */
struct default_traits_mt_t : public default_traits_st_t
{

    using strand_t = real_strand_t;
    using input_handler_t =
        std::function< void( input_ctx_t< default_traits_mt_t > & ) >;
};

}  // namespace opio::net::tcp

namespace fmt
{

// Make it possible to use opio::net::tcp::connection_shutdown_reason enum
// as an argument for fmt format functions.
template <>
struct formatter< opio::net::tcp::connection_shutdown_reason >
{
    using shutdown_reason = opio::net::tcp::connection_shutdown_reason;
    template < class Parse_Context >
    constexpr auto parse( Parse_Context & ctx ) const
    {
        auto it  = std::begin( ctx );
        auto end = std::end( ctx );
        if( it != end && *it != '}' ) throw fmt::format_error( "invalid format" );
        return it;
    }

    template < class Format_Context >
    auto format( shutdown_reason reason, Format_Context & ctx ) const
    {
        switch( reason )
        {
            case shutdown_reason::user_initiated:
                return fmt::format_to( ctx.out(), "user_initiated" );
            case shutdown_reason::io_error:
                return fmt::format_to( ctx.out(), "io_error" );
            case shutdown_reason::eof:
                return fmt::format_to( ctx.out(), "eof" );
            case shutdown_reason::write_timeout:
                return fmt::format_to( ctx.out(), "write_timeout" );
            case shutdown_reason::read_ts_not_supported_on_this_platform:
                return fmt::format_to( ctx.out(),
                                       "read_ts_not_supported_on_this_platform" );
            default:
                return fmt::format_to( ctx.out(),
                                       "unknown_shutdown_reason({})",
                                       static_cast< int >( reason ) );
        }
    }
};

}  // namespace fmt
