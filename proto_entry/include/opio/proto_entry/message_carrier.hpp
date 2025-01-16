#pragma once

#include <google/protobuf/arena.h>

#include <opio/net/buffer.hpp>

#include <opio/proto_entry/pkg_header.hpp>

namespace opio::proto_entry
{

//
// trivial_proxy_message_carrier_t
//

/**
 * @brief A class that carries a casual protobuf message (
 *        with heap allocated internals).
 *
 * Provides a `unique_ptr<M>` like interface to a protobuf message.
 * Which acts as unified interface for arena allocated protobuf messages
 * and heap-allocated messages.
 */
template < typename Message >
class trivial_proxy_message_carrier_t
{
public:
    using message_t = Message;

    /**
     * @brief Create an instance of trivial message with
     *        getting a shared ownership of an arena.
     *
     * @param  msg              A rvalue ref to a message to carry.
     */
    explicit trivial_proxy_message_carrier_t( message_t && msg )
        : m_message( std::move( msg ) )
    {
    }

    /**
     * @brief Create an instance of trivial message with
     *        getting a shared ownership of an arena.
     *
     * @param  msg              A rvalue ref to a message to carry.
     * @param  attached_buffer  A bufer complementing the message.
     */
    explicit trivial_proxy_message_carrier_t(
        message_t && msg,
        net::simple_buffer_t && attached_buffer )
        : m_message( std::move( msg ) )
        , m_attached_buffer{ std::move( attached_buffer ) }
    {
    }

    trivial_proxy_message_carrier_t( const trivial_proxy_message_carrier_t & ) =
        delete;
    trivial_proxy_message_carrier_t & operator    =(
        const trivial_proxy_message_carrier_t & ) = delete;

    trivial_proxy_message_carrier_t( trivial_proxy_message_carrier_t && ) =
        default;
    trivial_proxy_message_carrier_t & operator=(
        trivial_proxy_message_carrier_t && ) = default;

    [[nodiscard]] message_t * get() noexcept { return &m_message; }
    [[nodiscard]] const message_t * get() const noexcept
    {
        return const_cast< trivial_proxy_message_carrier_t * >( this )->get();
    }

    [[nodiscard]] message_t & operator*() noexcept { return *get(); }
    [[nodiscard]] const message_t & operator*() const noexcept { return *get(); }

    [[nodiscard]] message_t * operator->() noexcept { return get(); }
    [[nodiscard]] const message_t * operator->() const noexcept { return get(); }

    [[nodiscard]] net::simple_buffer_t & attached_buffer() noexcept
    {
        return m_attached_buffer;
    }
    [[nodiscard]] const net::simple_buffer_t & attached_buffer() const noexcept
    {
        return m_attached_buffer;
    }

private:
    Message m_message;
    net::simple_buffer_t m_attached_buffer;
};

//
// with_arena_message_carrier_t
//

/**
 * @brief A class that carries an arena allocated protobuf message.
 *
 * Provides a `unique_ptr<M>` like interface to a protobuf message.
 * Which acts as unified interface for arena allocated protobuf messages
 * and heap-allocated messages.
 *
 * @since v0.11.0
 */
template < typename Message >
class with_arena_message_carrier_t
{
public:
    using message_t = Message;

    /**
     * @brief Create an instance of arena-backed message with
     *        getting a shared ownership of an arena.
     *
     * @param  msg           A pointer to protobuf message that is hosted by
     *                       the arena.
     * @param  arena_anchor  The hosting arena.
     */
    explicit with_arena_message_carrier_t(
        message_t * msg,
        std::unique_ptr< google::protobuf::Arena > arena_anchor )
        : m_message( msg )
        , m_arena_anchor{ std::move( arena_anchor ) }
    {
    }

    /**
     * @brief Create an instance of arena-backed message with
     *        getting a shared ownership of an arena.
     *
     * @param  msg              A pointer to protobuf message that is hosted by
     *                          the arena.
     * @param  arena_anchor     The hosting arena.
     * @param  attached_buffer  A bufer complementing the message.
     */
    explicit with_arena_message_carrier_t(
        message_t * msg,
        std::unique_ptr< google::protobuf::Arena > arena_anchor,
        net::simple_buffer_t && attached_buffer )
        : m_message( msg )
        , m_arena_anchor{ std::move( arena_anchor ) }
        , m_attached_buffer{ std::move( attached_buffer ) }
    {
    }

    with_arena_message_carrier_t( const with_arena_message_carrier_t & ) = delete;
    with_arena_message_carrier_t & operator                              =(
        const with_arena_message_carrier_t & ) = delete;

    with_arena_message_carrier_t( with_arena_message_carrier_t && carrier )
    {
        *this = std::move( carrier );
    }

    with_arena_message_carrier_t & operator=(
        with_arena_message_carrier_t && carrier )
    {
        m_message         = carrier.m_message;
        carrier.m_message = nullptr;
        m_arena_anchor    = std::move( carrier.m_arena_anchor );
        m_attached_buffer = std::move( carrier.m_attached_buffer );
        return *this;
    }

    [[nodiscard]] message_t * get() noexcept { return m_message; }
    [[nodiscard]] const message_t * get() const noexcept
    {
        return const_cast< with_arena_message_carrier_t * >( this )->get();
    }

    [[nodiscard]] message_t & operator*() noexcept { return *get(); }
    [[nodiscard]] const message_t & operator*() const noexcept { return *get(); }

    [[nodiscard]] message_t * operator->() noexcept { return get(); }
    [[nodiscard]] const message_t * operator->() const noexcept { return get(); }

    [[nodiscard]] net::simple_buffer_t & attached_buffer() noexcept
    {
        return m_attached_buffer;
    }
    [[nodiscard]] const net::simple_buffer_t & attached_buffer() const noexcept
    {
        return m_attached_buffer;
    }

private:
    Message * m_message;
    std::unique_ptr< google::protobuf::Arena > m_arena_anchor;
    net::simple_buffer_t m_attached_buffer;
};

//
// protobuf_parsing_strategy
//

/**
 * @brief Parsing protobuf messages strategy.
 */
enum class protobuf_parsing_strategy
{
    /**
     * Standard trivial parsing to a message with allocations on heap.
     */
    trivial,
    /**
     * Parsing to a message with allocations on arena.
     */
    with_arena
};

}  // namespace opio::proto_entry
