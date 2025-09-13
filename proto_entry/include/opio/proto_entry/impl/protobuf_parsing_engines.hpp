/**
 * @file
 *
 * Implementations of protobuf parsing strategies.
 */

#pragma once

#include <optional>

#include <google/protobuf/io/zero_copy_stream.h>

#include <opio/proto_entry/message_carrier.hpp>

namespace opio::proto_entry::impl
{

//
// common_protobuf_parsing_engine_t
//

/**
 * @brief common parsing engine.
 */
template < typename Parse_Result >
class common_protobuf_parsing_engine_t
{
public:
    using parse_results_t = Parse_Result;

    using message_carrier_t = typename parse_results_t::message_carrier_t;

    [[nodiscard]] static std::optional< parse_results_t > parse_package(
        google::protobuf::io::ZeroCopyInputStream & input )
    {
        std::optional< parse_results_t > res{ parse_results_t{} };

        if( !res->message().ParseFromZeroCopyStream( &input ) ) [[unlikely]]
        {
            res = std::nullopt;
        }

        return res;
    }
};

//
// protobuf_trivial_parse_results_t
//

/**
 * @brief Parse result for case of trivial parse strategy.
 */
template < typename Message >
struct protobuf_trivial_parse_results_t
{
    using message_carrier_t = trivial_proxy_message_carrier_t< Message >;

    [[nodiscard]] message_carrier_t carry_message()
    {
        return message_carrier_t{ std::move( m_message ) };
    }

    [[nodiscard]] message_carrier_t carry_message(
        net::simple_buffer_t && attached_buf )
    {
        return message_carrier_t{ std::move( m_message ),
                                  std::move( attached_buf ) };
    }

    [[nodiscard]] Message & message() noexcept { return m_message; }

private:
    Message m_message;
};

//
// protobuf_trivial_parsing_engine_t
//

template < typename Message >
using protobuf_trivial_parsing_engine_t = common_protobuf_parsing_engine_t<
    protobuf_trivial_parse_results_t< Message > >;

//
// protobuf_with_arena_parse_results_t
//

/**
 * @brief Parse result for case of trivial parse strategy.
 */
template < typename Message, std::size_t Block_Size = 4 * 1024 >
struct protobuf_with_arena_parse_results_t
{
    using message_carrier_t = with_arena_message_carrier_t< Message >;

    [[nodiscard]] message_carrier_t carry_message()
    {
        assert( m_arena );
        return message_carrier_t{ m_message, std::move( m_arena ) };
    }

    [[nodiscard]] message_carrier_t carry_message(
        net::simple_buffer_t && attached_buf )
    {
        assert( m_arena );
        return message_carrier_t{ m_message,
                                  std::move( m_arena ),
                                  std::move( attached_buf ) };
    }

    protobuf_with_arena_parse_results_t() = default;

    [[nodiscard]] Message & message() noexcept
    {
        assert( m_arena );
        return *m_message;
    }

private:
    [[nodiscard]] static google::protobuf::ArenaOptions make_arena_options()
    {
        google::protobuf::ArenaOptions options;
        options.start_block_size = Block_Size;
        return options;
    }

    std::unique_ptr< google::protobuf::Arena > m_arena =
        std::make_unique< google::protobuf::Arena >( make_arena_options() );

    Message * m_message =
        google::protobuf::Arena::Create< Message >( m_arena.get() );
};

//
// protobuf_with_arena_parsing_engine_t
//

template < typename Message >
using protobuf_with_arena_parsing_engine_t = common_protobuf_parsing_engine_t<
    protobuf_with_arena_parse_results_t< Message > >;

//
// protobuf_parsing_engine_lut
//

template < protobuf_parsing_strategy Protobuf_Parsing_Strategy >
struct protobuf_parsing_engine_lut
{
};

template <>
struct protobuf_parsing_engine_lut< protobuf_parsing_strategy::trivial >
{
    template < typename Message >
    using type = protobuf_trivial_parsing_engine_t< Message >;
};

template <>
struct protobuf_parsing_engine_lut< protobuf_parsing_strategy::with_arena >
{
    template < typename Message >
    using type = protobuf_with_arena_parsing_engine_t< Message >;
};

template < protobuf_parsing_strategy Protobuf_Parsing_Strategy, typename Message >
using protobuf_parsing_engine_t = typename protobuf_parsing_engine_lut<
    Protobuf_Parsing_Strategy >::template type< Message >;

}  // namespace opio::proto_entry::impl
