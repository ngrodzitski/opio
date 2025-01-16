/**
 * @file
 *
 * This header file contains routines that define fixed binary layout
 * for a family of protocols.
 */

#pragma once

#include <cstdint>

namespace opio::proto_entry
{

//
// pkg_content_type_t
//

using pkg_content_type_t = std::uint8_t;

constexpr pkg_content_type_t pkg_content_message           = 0;
constexpr pkg_content_type_t pkg_content_heartbeat_request = 1;
constexpr pkg_content_type_t pkg_content_heartbeat_reply   = 2;

//
//  pkg_header_t
//

/**
 * A header prepending any binary package send between endpoints.
 *
 * @todo: think of a better constructor so it would operate over a safer types.
 *       Now you can do `pkg_header_t h{ size, content_tag}` as well as
 *       `pkg_header_t h{ content_tag, size}`.
 */
struct pkg_header_t
{
    /**
     * @brief A constant defining the size of the header's image in dwords.
     */
    static inline constexpr std::uint16_t image_size_dwords = 12 / 4;

    /**
     * @brief A hint telling what is the content of the following binary.
     */
    pkg_content_type_t pkg_content_type;

    /**
     * @brief Size of this header in dwords (32bits).
     */
    std::uint8_t header_size_dwords{ image_size_dwords };

    /**
     * @brief A word containing content specific data.
     *
     * Currently there exists only a single usecase:
     *
     *   - id of the message (recognized on the protocol level).
     */
    std::uint16_t content_specific_value{};

    /**
     * @brief Size of this header in bytes.
     */
    std::uint32_t content_size{};

    /**
     * @brief Size of atached binary.
     */
    std::uint32_t attached_binary_size{};

    /**
     * @brief get the size of the header in bytes based on header_size_dwords.
     */
    [[nodiscard]] constexpr std::size_t advertized_header_size() const noexcept
    {
        return header_size_dwords * 4ULL;
    }

    [[nodiscard]] static constexpr pkg_header_t make(
        pkg_content_type_t pkg_content_type,
        std::uint16_t content_specific_value = 0,
        std::uint32_t content_size           = 0,
        std::uint32_t attached_binary_size   = 0 ) noexcept
    {
        pkg_header_t res{};

        res.pkg_content_type       = pkg_content_type;
        res.header_size_dwords     = image_size_dwords;
        res.content_specific_value = content_specific_value;
        res.content_size           = content_size;
        res.attached_binary_size   = attached_binary_size;

        return res;
    }
};

static_assert(
    sizeof( pkg_header_t )
        == sizeof( std::int32_t ) * pkg_header_t::image_size_dwords,
    "pkg_header_t::image_size_dwords*4 must be equal to sizeof(pkg_header_t)" );

}  // namespace opio::proto_entry
