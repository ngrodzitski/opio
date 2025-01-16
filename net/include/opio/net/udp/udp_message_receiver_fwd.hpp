/**
 * @file Contains common routines for message handlers implementation used with
 *       udp_message_receiver_t.
 */
#pragma once

#include <span>

namespace opio::net::udp
{

//
// byte_t
//

using byte_t = std::byte;

//
// raw_data_span_t
//

using raw_data_span_t = std::span< const byte_t >;

//
// udp_raw_message_t
//

/**
 * @brief UDP raw message view class.
 *
 * Acts as a maintenance aggragate in case
 * extra data (required by feed handlers) will be attached later.
 * E.g. networking timestamps.
 */
struct udp_raw_message_t
{
    [[nodiscard]] raw_data_span_t raw_data() const noexcept { return m_raw_data; };

    raw_data_span_t m_raw_data{};
};

static_assert( std::is_trivially_copyable_v< udp_raw_message_t > );

}  // namespace opio::net::udp
