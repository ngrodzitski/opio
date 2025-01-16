#pragma once

#include <array>

#include <google/protobuf/io/zero_copy_stream.h>

#include <opio/net/buffer.hpp>
#include <opio/proto_entry/pkg_header.hpp>

namespace opio::proto_entry
{

//
// pkg_input_base_t
//

/**
 * @brief A base class for pkg_input buffer.
 *
 * Acts as a single type to be passed to generated entry implementation,
 * thus making it possibble alternate the type of `m_pkg_input`
 * (which eventually an instantiation of a template with given params) in
 * base class of the entry.
 */
class pkg_input_base_t : public google::protobuf::io::ZeroCopyInputStream
{
public:
    /**
     * @brief Reads a binary buffer of a given size.
     *
     * Reads the data and consumes it from stream.
     *
     * @pre Stream must have a requested number of bytes.
     * @pre A ZCBuf interface function `Next()` should not be
     *      called imeidatly before this.
     *
     * @post The data is consumed from stream.
     */
    virtual void read_buffer( void * buffer, std::size_t size ) = 0;
};

//
// pkg_input_t
//

/**
 * @brief A class helping with reading packages (header+message)
 *        from a sequence of buffer.
 *
 * @tparam Buffer_Queue_Capacity  Capacity of the queue of buffers.
 *                                It is better to use the power of 2 values.
 *
 * Incapsulates the mechanics of handling stream of data
 * splitted into multiple buffers.
 *
 * Always keeps an instance of protobuf zero-copy stream
 * that represents entire buffer.
 */
template < typename Buffer                   = opio::net::simple_buffer_t,
           std::size_t Buffer_Queue_Capacity = 8 >
class pkg_input_t final : public pkg_input_base_t
{
public:
    using buffer_t = Buffer;
    // Obtains a chunk of data from the stream.
    //
    // Preconditions:
    // * "size" and "data" are not NULL.
    //
    // Postconditions:
    // * If the returned value is false, there is no more data to return or
    //   an error occurred.  All errors are permanent.
    // * Otherwise, "size" points to the actual number of bytes read and "data"
    //   points to a pointer to a buffer containing these bytes.
    // * Ownership of this buffer remains with the stream, and the buffer
    //   remains valid only until some other method of the stream is called
    //   or the stream is destroyed.
    // * It is legal for the returned buffer to have zero size, as long
    //   as repeatedly calling Next() eventually yields a buffer with non-zero
    //   size.
    bool Next( const void ** data, int * size ) override
    {
        reset_first_buffer_served();

        if( m_total_size == 0 ) return false;

        *data = m_bufs[ m_first_buffer_pos ].offset_data( m_first_buffer_offset );
        *size = remained_in_the_first_buffer();

        m_byte_size_counter += *size;
        m_first_buffer_served = true;

        return true;
    }

    // Backs up a number of bytes, so that the next call to Next() returns
    // data again that was already returned by the last call to Next().  This
    // is useful when writing procedures that are only supposed to read up
    // to a certain point in the input, then return.  If Next() returns a
    // buffer that goes beyond what you wanted to read, you can use BackUp()
    // to return to the point where you intended to finish.
    //
    // Preconditions:
    // * The last method called must have been Next().
    // * count must be less than or equal to the size of the last buffer
    //   returned by Next().
    //
    // Postconditions:
    // * The last "count" bytes of the last buffer returned by Next() will be
    //   pushed back into the stream.  Subsequent calls to Next() will return
    //   the same data again before producing new data.
    void BackUp( int count ) override
    {
        assert( m_first_buffer_served );

        // We are no longer serving any block.
        m_first_buffer_served = false;

        const auto consumed_size = remained_in_the_first_buffer() - count;
        m_total_size -= consumed_size;
        m_byte_size_counter -= count;

        if( 0 < count )
        {
            // Simple case we actually return something.
            // In next we always serve first block so backup goes to it.
            // As precondition says, we are safe to do the following:
            m_first_buffer_offset += consumed_size;
        }
        else
        {
            // We don't really backup anything, so
            // we confirm the consumption of first block,
            // which is what we return with `Next()` method.
            pop_buffer();
        }
    }

    // Skips a number of bytes.  Returns false if the end of the stream is
    // reached or some input error occurred.  In the end-of-stream case, the
    // stream is advanced to the end of the stream (so ByteCount() will return
    // the total size of the stream).
    bool Skip( int count ) override
    {
        reset_first_buffer_served();
        m_byte_size_counter += count;

        if( m_total_size < static_cast< std::size_t >( count ) )
        {
            return false;
        }

        skip_bytes( count );
        return true;
    }

    // Returns the total number of bytes read since this object was created.
    int64_t ByteCount() const override { return m_byte_size_counter; }

    /**
     * @brief Size of the accumulated unconsumed input.
     */
    std::size_t size() const noexcept { return m_total_size; }

    /**
     * @brief Appends a buffer to input queue.
     *
     * @param buf  New buffer.
     */
    void append( buffer_t buf )
    {
        assert( !m_first_buffer_served );

        m_total_size += buf.size();
        if( Buffer_Queue_Capacity > m_buffers_count )
        {
            const auto next_pos =
                ( m_first_buffer_pos + m_buffers_count ) % Buffer_Queue_Capacity;
            m_bufs[ next_pos ] = std::move( buf );
            ++m_buffers_count;
        }
        else
        {
            // The worst case:
            // If we have a full queue, so we cannot simply append
            // so wee just add to last item in queue.
            const auto last_buf_pos = ( m_first_buffer_pos + m_buffers_count - 1 )
                                      % Buffer_Queue_Capacity;

            auto & last_buf = m_bufs[ last_buf_pos ];

            const std::size_t append_offset = last_buf.size();
            last_buf.resize( append_offset + buf.size() );
            std::memcpy( last_buf.data() + append_offset, buf.data(), buf.size() );
            // TODO: The above line is potentially throwing,
            // and if it does then we have invalid m_total_size
            // (we increase it already).
            // That is not a big issue (for now), though we should better
            // keep things valid, but no quick idea of how
            // to make it look pretty.
        }
    }

    /**
     * @brief Reads a package header from the beggining of accumulated buffer
     *        without actually consuming it.
     *
     * @pre Stream must have at least `sizeof(pkg_header_t)` bytes
     *      available. Use `.size()` to get current input size.
     * @pre A ZCBuf interface function `Next()` should not be
     *      called imeidatly before this.
     *
     * @post The input stream itself remains untouched,
     *       the operation doesn't consume buffer.
     *
     * @return Package deserialized from the beginning of the stream.
     */
    [[nodiscard]] pkg_header_t view_pkg_header() const noexcept
    {
        assert( !m_first_buffer_served );
        assert( m_total_size >= sizeof( pkg_header_t ) );

        pkg_header_t header;

        copy_n_bytes_to(
            &header, sizeof( header ), m_first_buffer_pos, m_first_buffer_offset );

        return header;
    }

    /**
     * @brief Reads a binary buffer of a given size.
     *
     * Reads the data and consumes it from stream.
     *
     * @pre Stream must have a requested number of bytes.
     * @pre A ZCBuf interface function `Next()` should not be
     *      called imeidatly before this.
     *
     * @post The data is consumed from stream.
     */
    void read_buffer( void * buffer, std::size_t size ) final
    {
        assert( !m_first_buffer_served );
        assert( m_total_size >= size );

        copy_n_bytes_to( buffer, size, m_first_buffer_pos, m_first_buffer_offset );
        skip_bytes( size );
    }

    /**
     * @brief Skip a number of bytes in the buffer.
     *
     * @pre Buffer must have at least n bytes.
     *
     * @param n  Number of bytes to skip.
     */
    void skip_bytes( std::size_t n )
    {
        assert( !m_first_buffer_served );
        assert( m_total_size >= n );

        if( 0 == n ) return;

        const auto bytes_in_the_first_buffer = remained_in_the_first_buffer();
        if( n < bytes_in_the_first_buffer )
        {
            // Simple case:
            // First buffer is larger then skip request,
            // so we only adjust first buffer offset and total size.
            m_first_buffer_offset += n;
            m_total_size -= n;
            return;
        }

        // We totaly skip the remaining part in the first buffer.
        pop_buffer();
        // after poping it we update size:
        m_total_size -= bytes_in_the_first_buffer;

        skip_bytes( n - bytes_in_the_first_buffer );
        // TODO: check tail recursion optimization works here.
    }

private:
    /**
     * @brief A helper routine to handle `Next()` buffer provisioning
     *        confirmation.
     */
    void reset_first_buffer_served()
    {
        if( m_first_buffer_served )
        {
            m_total_size -= remained_in_the_first_buffer();
            // Next was called before so the data returned previously
            // was "consumed".
            pop_buffer();
            m_first_buffer_served = false;
        }
    }

    /**
     * @brief Gets the next logical buffer-bucket in the ring storage.
     *
     * @param  pos  A pos in the queue to increment.
     * @return      The next position in the ring storage.
     */
    static constexpr auto next_pos( std::size_t pos ) noexcept
    {
        return ( pos + 1 ) % Buffer_Queue_Capacity;
    }

    /**
     * @brief Copy a given number of bytes to buffer.
     */
    void copy_n_bytes_to( void * dest,
                          std::size_t n,
                          std::size_t buf_pos,
                          std::size_t buf_offset = 0 ) const
    {
        assert( m_total_size >= n );

        const auto remained_in_this_buffer = m_bufs[ buf_pos ].size() - buf_offset;

        if( n < remained_in_this_buffer )
        {
            std::memcpy( dest, m_bufs[ buf_pos ].offset_data( buf_offset ), n );
            return;
        }

        std::memcpy( dest,
                     m_bufs[ buf_pos ].offset_data( buf_offset ),
                     remained_in_this_buffer );

        copy_n_bytes_to( static_cast< char * >( dest ) + remained_in_this_buffer,
                         n - remained_in_this_buffer,
                         next_pos( buf_pos ) );
        // TODO: check tail recursion optimization works here.
    }

    /**
     * @brief Pops the first buffer from queue.
     */
    void pop_buffer()
    {
        assert( m_buffers_count > 0 );
        --m_buffers_count;
        m_first_buffer_pos    = ( m_first_buffer_pos + 1 ) % Buffer_Queue_Capacity;
        m_first_buffer_offset = 0;
    }

    /**
     * @brief Gets a number of bytes available in the first buffer.
     *
     */
    [[nodiscard]] auto remained_in_the_first_buffer() const noexcept
    {
        return m_bufs[ m_first_buffer_pos ].size() - m_first_buffer_offset;
    }
    //! The total size of accumulated buffer.
    std::size_t m_total_size{};

    //! Number of buffers in the queue
    std::size_t m_buffers_count{};

    //! The position (index) of the first buffer in the queue.
    std::size_t m_first_buffer_pos{};

    /**
     * @brief First buffer offset.
     *
     * Tells where unconsumed data starts within a first buffer.
     *
     * In cases original data in buffer layed out so that
     * beggining of the buffer is dedicated to one package and
     * the end of the buffer contains a start of another buffer.
     * So once the beggining is consumed then we should able to tell
     * where the next package starts.
     */
    std::size_t m_first_buffer_offset{};

    /**
     * @brief Queue storage.
     *
     * This is where a queue of buffers is stored,
     * the order of buffers in the storage is not match the order
     * in the logical queue. The first item in the queue
     * located at @c m_first_buffer_pos in the storage
     * and continues further following the ring principle.
     */
    std::array< buffer_t, Buffer_Queue_Capacity > m_bufs;

    /**
     * @brief Flag telling if the Next function was called
     *        so that the data in the first buffer was served
     */
    bool m_first_buffer_served = false;

    /**
     * @brief A value for ByteCount function.
     *
     * This counts only for protobuf consumed bytes.
     * Bytes dedicated to package headers are not counted for this value.
     */
    std::size_t m_byte_size_counter{};
};

}  // namespace opio::proto_entry
