/**
 * @file
 *
 * This header file contains helper routines for working with protocol entries.
 */

#pragma once

#include <optional>
#include <cstring>

#include <opio/net/buffer.hpp>
#include <opio/net/heterogeneous_buffer.hpp>

#include <opio/proto_entry/pkg_header.hpp>

namespace opio::proto_entry
{

//
//  make_package_image()
//

/**
 * @name Create a given type of package with a serialized protobuf
 *       message as its content.
 */

///@{

/**
 * @brief Create an image of a given package.
 *
 * An image is buffer containing package header and then a protobuf serialized
 * message (package).
 *
 * Acts as an independent routine to create images for a given message,
 * which can be used for entry "bypass", so that a buffer can be sent to
 * remote peer directly through underlying connection.
 *
 * @param  pkg_content_type      The type of a package.
 * @param  message_type_id       Identification for the message type.
 * @param  msg                   An instance of a message that must be a content
 *                               of a package.
 * @param  buffer_driver         Buffer driver to create a buffer that would
 *                               contain an image.
 * @param  attached_binary_size  Acount for the size of attached binary.
 *
 * @return  An instance of optional, which in case of success is not empty
 *          and contains an image.
 */
template < typename Message, ::opio::net::Buffer_Driver_Concept Buffer_Driver >
[[nodiscard]] auto make_package_image( std::uint16_t message_type_id,
                                       const Message & msg,
                                       Buffer_Driver & buffer_driver,
                                       std::uint32_t attached_binary_size = 0UL )
{
    // Package is structured the following way:
    // | header | serialized Message |

    const auto header =
        pkg_header_t::make( pkg_content_message,
                            message_type_id,
                            static_cast< std::uint32_t >( msg.ByteSizeLong() ),
                            attached_binary_size );

    auto buf =
        buffer_driver.allocate_output( sizeof( header ) + header.content_size );

    // "Serialize header":
    std::memcpy( buf.data(), &header, sizeof( header ) );

    [[maybe_unused]] const auto serialization_res = msg.SerializeToArray(
        reinterpret_cast< char * >( buf.offset_data( sizeof( header ) ) ),
        header.content_size );

    assert( serialization_res );

    return buf;
}

/**
 * @brief Create images of a header and the message.
 *
 * Acts as an independent routine to create images for a given message
 * when it makes sence to have a separate images for header and body of the
 * message, which can be used for entry "bypass", so that a buffer can be sent to
 * remote peer directly through underlying connection.
 *
 * @param  pkg_content_type      The type of a package.
 * @param  message_type_id       Identification for the message type.
 * @param  msg                   An instance of a message that must be a content
 *                               of a package.
 * @param  buffer_driver         Buffer driver to create a buffer that would
 *                               contain an image.
 * @param  attached_binary_size  Acount for the size of attached binary.
 *
 * @return  An instance of optional, which in case of success is not empty
 *          and contains an image.
 */
template < typename Message, ::opio::net::Buffer_Driver_Concept Buffer_Driver >
[[nodiscard]] auto make_separate_package_image(
    std::uint16_t message_type_id,
    const Message & msg,
    Buffer_Driver & buffer_driver,
    std::uint32_t attached_binary_size          = 0UL,
    std::size_t additional_space_for_header_buf = 0UL )
{
    assert( additional_space_for_header_buf % 4 == 0 );
    // Package is structured the following way:
    // | header | serialized Message |

    auto header =
        pkg_header_t::make( pkg_content_message,
                            message_type_id,
                            static_cast< std::uint32_t >( msg.ByteSizeLong() ),
                            attached_binary_size );

    header.header_size_dwords += additional_space_for_header_buf / 4;

    // make
    auto header_buf = buffer_driver.allocate_output(
        sizeof( header ) + additional_space_for_header_buf );

    // "Serialize header":
    std::memcpy( header_buf.data(), &header, sizeof( header ) );

    auto msg_buf = buffer_driver.allocate_input( header.content_size );

    [[maybe_unused]] const auto serialization_res = msg.SerializeToArray(
        reinterpret_cast< char * >( msg_buf.data() ), header.content_size );

    assert( serialization_res );

    return std::make_pair( std::move( header_buf ), std::move( msg_buf ) );
}

/**
 * @brief Vanilla version of creating an image of the package.
 */
template < typename Message >
[[nodiscard]] auto make_package_image( std::uint16_t message_type_id,
                                       const Message & msg )
{
    net::simple_buffer_driver_t buffer_driver{};
    return make_package_image( message_type_id, msg, buffer_driver );
}

///@}

namespace details
{

template < typename Client, typename = void >
struct supports_shared_buffer : std::false_type
{
};

template < typename Client >
struct supports_shared_buffer<
    Client,
    std::void_t< decltype(
        ( *std::declval< Client >() )
            .schedule_send_raw_bufs( std::declval< std::shared_ptr<
                                         opio::net::simple_buffer_t > >() ) ) > >
    : std::true_type
{
};

template < typename Client >
inline constexpr bool supports_shared_buffer_v =
    supports_shared_buffer< Client >::value;

template < typename Clients_Range >
void multi_send_image( Clients_Range & clients, opio::net::simple_buffer_t buf )
{
    using client_t = std::decay_t< decltype( clients.front() ) >;

    if constexpr( supports_shared_buffer_v< client_t > )
    {
        auto shared_buf =
            std::make_shared< opio::net::simple_buffer_t >( std::move( buf ) );

        for( auto & c : clients )
        {
            c->schedule_send_raw_bufs( shared_buf );
        }
    }
    else
    {
        for( auto & c : clients )
        {
            c->schedule_send_raw_bufs( buf.make_copy() );
        }
    }
}

}  // namespace details

//
// make_package_with_binary_content()
//

template < typename Datasizeable,
           ::opio::net::Buffer_Driver_Concept Buffer_Driver >
[[deprecated]] net::simple_buffer_t make_package_with_binary_content(
    pkg_content_type_t pkg_content_type,
    const Datasizeable & ds,
    Buffer_Driver & buffer_driver )
{
    const auto header = pkg_header_t::make( ds.size(), pkg_content_type );
    auto buf = buffer_driver.allocate_output( sizeof( header ) + header.size );

    auto buf_write_ref = buffer_driver.make_asio_mutable_buffer( buf );

    std::memcpy( buf_write_ref.data(), &header, sizeof( header ) );
    std::memcpy(
        static_cast< std::byte * >( buf_write_ref.data() ) + sizeof( header ),
        ds.data(),
        ds.size() );

    return buf;
}

//
// get_entry_ptr()
//

/**
 * @name Get a reference to an entry object.
 *
 * Adjusts for cases of `Entry*`, `shared_ptr<Entry>` and `std::pair<K, Entry>`
 * and gets a reference accordingly.
 */
///@{
template < typename Entry >
auto * get_entry_ptr( Entry * entry )
{
    return entry;
}

template < typename Entry >
auto * get_entry_ptr( std::shared_ptr< Entry > entry )
{
    return entry.get();
}

template < typename Entry >
auto * get_entry_ptr( std::unique_ptr< Entry > entry )
{
    return entry.get();
}

template < typename Key, typename Entry >
auto * get_entry_ptr( std::pair< Key, Entry > & entry_source )
{
    return get_entry_ptr( entry_source.second );
}

///@}

}  // namespace opio::proto_entry
