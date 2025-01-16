//#attr $generated_at = time.strftime("%a, %d %b %Y %T GMT", time.gmtime())

// GENERATED CODE, DO NOT MODIFY
// Date:              ${generated_at}
// Template sha256:   ${template_hash}

// clang-format off

#pragma once

#include <numeric>

#include <opio/proto_entry/impl/protobuf_parsing_engines.hpp>
#include <opio/proto_entry/impl/self_contained_protobuf_arena.hpp>
#include <opio/proto_entry/entry_base.hpp>
#include <opio/proto_entry/std_entry_shortcuts_factory.hpp>

//#for $h in $headers
#include <${h}>
//#end for

namespace ${namespace}
{

//
// noop_stats_driver_t
//

struct noop_stats_driver_t
{
//#for $msg in $protocol.incoming
    constexpr void inc_incoming_${msg.message_str_tag}() const noexcept {}
//#end for

//#for $msg in $protocol.outgoing
    constexpr void inc_outgoing_${msg.message_str_tag}() const noexcept {}
//#end for
};

//
// core_entry_t
//

template < typename Traits, typename Message_Consumer >
class core_entry_t : public ::opio::proto_entry::entry_base_t< Traits >
{
public:
    using sptr_t = std::shared_ptr< core_entry_t >;
    using wptr_t = std::weak_ptr< core_entry_t >;
    using base_type_t = ::opio::proto_entry::entry_base_t< Traits >;

    using socket_t = typename Traits::socket_t;
    using buffer_driver_t = typename Traits::buffer_driver_t;
    using socket_io_operation_watchdog_t = typename Traits::socket_io_operation_watchdog_t;
    using underlying_stats_driver_t = typename Traits::underlying_stats_driver_t;

    using strand_t = typename Traits::strand_t;
    using logger_t = typename Traits::logger_t;

    using message_consumer_t = Message_Consumer;
    using stats_driver_t = typename Traits::stats_driver_t;

protected:
    core_entry_t( strand_t strand,
             logger_t logger,
             const ::opio::proto_entry::entry_cfg_t & cfg,
             buffer_driver_t buffer_driver,
             ::opio::proto_entry::shutdown_handler_variant_t shutdown_handler,
             message_consumer_t message_consumer,
             stats_driver_t stats )
        : base_type_t{ std::move( strand ),
                       std::move( logger ),
                       cfg,
                       std::move( buffer_driver ),
                       std::move( shutdown_handler ) }
        , m_consumer{ std::move( message_consumer ) }
        , m_stats{ std::move( stats ) }
    {
    }

    /**
     * @brief An extension factory allowing reuse in descendants classes.
     *
     * Acts as a standard routine to implement a factory (like `core_entry_t::make()`)
     * for extensions which do not require additional params to pass in constructor.
     *
     * @tparam Eventual_Entry_Type  The type of the entry to create.
     */
    template < typename Eventual_Entry_Type >
    [[nodiscard]] static std::shared_ptr< Eventual_Entry_Type > make_custom(
        socket_t socket,
        ::opio::net::tcp::connection_id_t conn_id,
        const ::opio::net::tcp::connection_cfg_t & underlying_cfg,
        logger_t logger,
        buffer_driver_t buffer_driver,
        socket_io_operation_watchdog_t operation_watchdog,
        underlying_stats_driver_t underlying_stats,
        strand_t strand,
        const ::opio::proto_entry::entry_cfg_t & cfg,
        ::opio::proto_entry::shutdown_handler_variant_t shutdown_handler,
        message_consumer_t message_consumer,
        stats_driver_t stats )
    {
        std::shared_ptr< Eventual_Entry_Type > entry{
            new Eventual_Entry_Type{ std::move( strand ),
                                     std::move( logger ),
                                     cfg,
                                     std::move( buffer_driver ),
                                     std::move( shutdown_handler ),
                                     std::move( message_consumer ),
                                     std::move( stats ) } };

        entry->init_underlying_connection( std::move( socket ),
                                           conn_id,
                                           underlying_cfg,
                                           std::move( operation_watchdog ),
                                           std::move( underlying_stats ) );

        return entry;
    }

    using ctor_params_t =
        opio::proto_entry::entry_ctor_params_t< Traits, Message_Consumer >;

    /**
     * @brief An extension factory allowing reuse in descendants classes.
     *
     * @tparam Eventual_Entry_Type  The type of the entry to create.
     */
    template < typename Eventual_Entry_Type >
    [[nodiscard]] static std::shared_ptr< Eventual_Entry_Type > make_custom(
        socket_t socket, ctor_params_t params )
    {
        auto operation_watchdog = params.operation_watchdog_giveaway();

        if( !operation_watchdog )
        {
            // TODO: this is duplicate from opio::net...
            //       And a similar pattern is used for strand later.

            static_assert(
                std::is_constructible_v< socket_io_operation_watchdog_t,
                                                   decltype(
                                                       socket.get_executor() ) >);

            // If operation_watchdog is not provided then we can
            // try to construct it with `socket.get_executor()` as parameter
            // or maybe it is default constructible.
            if constexpr( std::is_constructible_v< socket_io_operation_watchdog_t,
                                                   decltype(
                                                       socket.get_executor() ) > )
            {
                operation_watchdog = socket_io_operation_watchdog_t{ socket.get_executor() };
            }
            else if constexpr( std::is_default_constructible_v<
                                   socket_io_operation_watchdog_t > )
            {
                operation_watchdog = socket_io_operation_watchdog_t{};
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

        auto strand = params.strand_giveaway();

        if( !strand )
        {
            // If operation_watchdog is not provided then we can
            // try to construct it with `socket.get_executor()` as parameter
            // or maybe it is default constructible.
            if constexpr( std::is_constructible_v< strand_t,
                                                   decltype(
                                                       socket.get_executor() ) > )
            {
                strand = strand_t{ socket.get_executor() };
            }
            else if constexpr( std::is_default_constructible_v< strand_t > )
            {
                strand = strand_t{};
            }
            else
            {
                // Here We can't proceed without actual instance.
                throw std::runtime_error{
                    "entry parameter strand must be explicitly "
                    "set "
                    "(the type is not default constructible or constructible "
                    "from socket.get_executor())"
                };
            }
        }

        return make_custom< Eventual_Entry_Type >(
            std::move( socket ),
            params.connection_id(),
            params.underlying_connection_cfg(),
            params.logger_giveaway(),
            params.buffer_driver_giveaway(),
            std::move( *operation_watchdog ),
            params.underlying_stats_driver_giveaway(),
            std::move( *strand ),
            params.entry_config(),
            params.shutdown_handler_giveaway(),
            params.message_consumer_giveaway(),
            params.stats_driver_giveaway() );
    }

    template <
        typename Eventual_Entry_Type,
        typename Param_Setter,
        std::enable_if_t< std::is_invocable_v< Param_Setter, ctor_params_t & > > *
            = nullptr >
    [[nodiscard]] static std::shared_ptr< Eventual_Entry_Type > make_custom(
        socket_t socket, Param_Setter param_setter )
    {
        ctor_params_t params;
        param_setter( params );
        return make_custom< Eventual_Entry_Type >(
            std::move( socket ), std::move( params ) );
    }


public:
//#for $msg in $protocol.outgoing
    void send( const ${msg.type} & msg )
    {
        auto buf = ::opio::proto_entry::make_package_image(
            static_cast< std::uint16_t >( ${msg.enum_id} ),
            msg,
            this->buffer_driver() );

        this->schedule_send_raw_bufs( std::move( buf ) );
    }

    void send( const ${msg.type} & msg,
               typename buffer_driver_t::output_buffer_t attached_binary )
    {
        auto buf = ::opio::proto_entry::make_package_image(
            static_cast< std::uint16_t >( ${msg.enum_id} ),
            msg,
            this->buffer_driver(),
            buffer_driver_t::buffer_size( attached_binary ) );

        this->schedule_send_raw_bufs( std::move( buf ), std::move( attached_binary ) );
    }


    template < typename Attached_Bufs_Container >
    void send_vec( const ${msg.type} & msg, Attached_Bufs_Container attached_binaries )
    {
        using std::begin;
        using std::end;

        const auto attached_binary_size =
            std::accumulate( begin( attached_binaries ),
                             end( attached_binaries ),
                             std::size_t{},
                             []( auto memo, const auto & buf ) {
                                 return memo + buffer_driver_t::buffer_size( buf );
                             } );

        auto buf = ::opio::proto_entry::make_package_image(
            static_cast< std::uint16_t >( ${msg.enum_id} ),
            msg,
            this->buffer_driver(),
            attached_binary_size );

        this->schedule_send_raw_bufs( std::move( buf ) );
        this->schedule_send_vec_raw_bufs( std::move( attached_binaries ) );
    }

    void send_with_cb( ::opio::net::tcp::send_complete_cb_t cb,
                       const ${msg.type} & msg )
    {

        auto buf = ::opio::proto_entry::make_package_image(
            static_cast< std::uint16_t >( ${msg.enum_id} ),
            msg,
            this->buffer_driver() );

        this->schedule_send_raw_bufs_with_cb( std::move( cb ), std::move( buf ) );
    }

    void send_with_cb( ::opio::net::tcp::send_complete_cb_t cb,
                       const ${msg.type} & msg,
                       typename buffer_driver_t::output_buffer_t attached_binary )
    {

        auto buf = ::opio::proto_entry::make_package_image(
            static_cast< std::uint16_t >( ${msg.enum_id} ),
            msg,
            this->buffer_driver(),
            buffer_driver_t::buffer_size( attached_binary ) );

        this->schedule_send_raw_bufs_with_cb( std::move( cb ),
                                              std::move( buf ),
                                              std::move( attached_binary ) );
    }

    template < typename Attached_Bufs_Container >
    void send_vec_with_cb( ::opio::net::tcp::send_complete_cb_t cb,
                           const ${msg.type} & msg,
                           Attached_Bufs_Container attached_binaries )
    {
        using std::begin;
        using std::end;

        const auto attached_binary_size =
            std::accumulate( begin( attached_binaries ),
                             end( attached_binaries ),
                             std::size_t{},
                             []( auto memo, const auto & buf ){
                                 return memo + buffer_driver_t::buffer_size( buf );
                             } );

        auto buf = ::opio::proto_entry::make_package_image(
            static_cast< std::uint16_t >( ${msg.enum_id} ),
            msg,
            this->buffer_driver(),
            attached_binary_size );

        this->schedule_send_raw_bufs( std::move( buf ) );
        this->schedule_send_vec_raw_bufs_with_cb( std::move( cb ),
                                                  std::move( attached_binaries ) );
    }

    void post_send( ${msg.type} msg )
    {
        ::opio::net::asio_ns::post(
            this->strand(),
            [ m = std::move( msg ),
                e = this->shared_from_this() ] () mutable {
            e->send( m );
        } );
    }

    void post_send_with_cb( ::opio::net::tcp::send_complete_cb_t cb, ${msg.type} msg )
    {
        ::opio::net::asio_ns::post(
            this->strand(),
            [ m = std::move( msg ),
                cb = std::move( cb ),
                e = this->shared_from_this() ] () mutable {
            e->send_with_cb( m, std::move( cb ) );
        } );
    }

    void dispatch_send( ${msg.type} msg )
    {
        ::opio::net::asio_ns::dispatch(
            this->strand(),
            [ m = std::move( msg ),
                e = this->shared_from_this() ] () mutable {
            e->send( m );
        } );
    }

    void dispatch_send( ::opio::net::tcp::send_complete_cb_t cb, ${msg.type} msg )
    {
        ::opio::net::asio_ns::dispatch(
            this->strand(),
            [ m = std::move( msg ),
                cb = std::move( cb ),
                e = this->shared_from_this() ] () mutable {
            e->send_with_cb( m, std::move( cb ) );
        } );
    }

//#end for
    stats_driver_t & stats() noexcept{ return m_stats; }

protected:
    template< typename Entry_Type >
    base_type_t::package_handling_result handle_incoming_message_custom(
        const ::opio::proto_entry::pkg_header_t & header,
        ::opio::proto_entry::pkg_input_base_t & stream,
        Entry_Type & actual_entry )
    {
        const auto message_id =
            static_cast< ${proto_namespace}::MessageType >( header.content_specific_value );

        using google::protobuf::io::LimitingInputStream;

        switch( message_id )
        {
//#for $msg in $protocol.incoming
            case ${msg.enum_id}:
            {
                this->logger().trace( [ & ]( auto out ) {
                    format_to( out,
                               "[{};cid:{}] incoming message: ${msg.type}",
                               this->remote_endpoint_str(),
                               this->underlying_connection_id() );
                } );

                auto parse_results = [&]{
                    LimitingInputStream message_stream{ &stream, header.content_size };

                    using protobuf_engine_t =
                        typename base_type_t::template protobuf_engine_t< ${msg.type} >;
                    auto res = protobuf_engine_t::parse_package( message_stream );

                    if( message_stream.ByteCount() != header.content_size ) [[unlikely]]
                    {
                        this->logger().error( [ & ]( auto out ) {
                            format_to(
                                out,
                                "[{};cid:{}] parsing package expected to consume "
                                "{} bytes, but actual count is: {} the further"
                                "stream considered unreliable",
                                this->remote_endpoint_str(),
                                this->underlying_connection_id(),
                                header.content_size,
                                message_stream.ByteCount() );
                        } );

                        res.reset();
                    }

                    return res;
                }();

                if( !parse_results ) [[unlikely]]
                {
                    // Parsing fails.
                    this->logger().error([ & ]( auto out ) {
                        format_to( out,
                                   "[{};cid:{}] unable to parse ${msg.type} package",
                                   this->remote_endpoint_str(),
                                   this->underlying_connection_id() );
                    } );

                    this->shutdown_and_terminate(
                        ::opio::proto_entry::connection_shutdown_context_t{
                            ::opio::proto_entry::entry_shutdown_reason::invalid_input_package } );

                    return base_type_t::package_handling_result::invalid_package;
                }

                // Avoid any hanging bytes in terms of protobuf ZeroCopy stream:
                stream.Skip( 0 );

                auto make_message_carier = [&]{
                    if( 0 == header.attached_binary_size )
                    {
                        return parse_results->carry_message();
                    }

                    auto buf =
                        this->buffer_driver()
                            .allocate_input( header.attached_binary_size );

                    stream.read_buffer( buf.data(), buf.size() );

                    return parse_results->carry_message( std::move( buf ) );
                };

                opio::proto_entry::details::consume_message(
                    m_consumer,
                    make_message_carier(),
                    actual_entry );

                m_stats.inc_incoming_${msg.message_str_tag}();
            }
            break;
//#end for
            default:
                this->logger().error( [ & ]( auto out ) {
                    format_to( out,
                               "[{};cid:{}] unknown incoming message, message_id={}",
                               this->remote_endpoint_str(),
                               this->underlying_connection_id(),
                               static_cast< std::uint32_t >( message_id ) );
                } );

            this->shutdown_and_terminate(
                ::opio::proto_entry::connection_shutdown_context_t{
                    ::opio::proto_entry::entry_shutdown_reason::unexpected_input_package_size } );
            return base_type_t::package_handling_result::invalid_package;
        }
        return base_type_t::package_handling_result::fully_consumed;
    }

    message_consumer_t m_consumer;
    [[no_unique_address]] stats_driver_t m_stats;
};

//
// entry_t
//

template < typename Traits, typename Message_Consumer >
class entry_t final : public core_entry_t< Traits, Message_Consumer >
{
public:
    using core_base_type_t = core_entry_t< Traits, Message_Consumer >;
    using sptr_t = std::shared_ptr< entry_t >;
    using wptr_t = std::weak_ptr< entry_t >;

    [[nodiscard]] sptr_t shared_from_this() {
        return std::dynamic_pointer_cast<entry_t>(
            this->core_base_type_t::shared_from_this());
    }

    [[nodiscard]] wptr_t weak_from_this()
    {
        return wptr_t(this->shared_from_this());
    }

public:
    /**
     * @brief Factory to create an entry object.
     *
     * Delegates creation to base class.
     */
    template <typename... Args>
    [[nodiscard]] static sptr_t make(Args&&... args) {
        return core_base_type_t::template make_custom< entry_t >(
            std::forward<Args>(args)...);
    }

protected:
    using core_base_type_t::core_base_type_t;

    core_base_type_t::package_handling_result handle_incoming_message(
        const ::opio::proto_entry::pkg_header_t & header,
        ::opio::proto_entry::pkg_input_base_t & stream ) override
    {
        return core_base_type_t::handle_incoming_message_custom(
            header, stream, *this );
    }
};

// Shortcut definitions for standard incornations.

using entry_shortcuts = ::opio::proto_entry::std_entry_shortcuts_factory<
    entry_t, noop_stats_driver_t >;

template< typename Message_Consumer,
          typename Logger,
          ::opio::proto_entry::protobuf_parsing_strategy Protobuf_Parsing_Strategy =
              ::opio::proto_entry::protobuf_parsing_strategy::trivial >
using entry_singlethread_t =
    entry_shortcuts::singlethread_t< Message_Consumer, Logger, Protobuf_Parsing_Strategy >;

template< typename Message_Consumer,
          typename Logger,
          ::opio::proto_entry::protobuf_parsing_strategy Protobuf_Parsing_Strategy =
              ::opio::proto_entry::protobuf_parsing_strategy::trivial >
using entry_multithread_t =
    entry_shortcuts::multithread_t< Message_Consumer, Logger, Protobuf_Parsing_Strategy >;

// Shortcut definitions for standard incornations for core_entry type.

using core_entry_shortcuts = ::opio::proto_entry::std_entry_shortcuts_factory<
    core_entry_t, noop_stats_driver_t >;

template< typename Message_Consumer,
          typename Logger,
          ::opio::proto_entry::protobuf_parsing_strategy Protobuf_Parsing_Strategy =
              ::opio::proto_entry::protobuf_parsing_strategy::trivial >
using core_entry_singlethread_t =
    core_entry_shortcuts::singlethread_t< Message_Consumer, Logger, Protobuf_Parsing_Strategy >;

template< typename Message_Consumer,
          typename Logger,
          ::opio::proto_entry::protobuf_parsing_strategy Protobuf_Parsing_Strategy =
              ::opio::proto_entry::protobuf_parsing_strategy::trivial >
using core_entry_multithread_t =
    core_entry_shortcuts::multithread_t< Message_Consumer, Logger, Protobuf_Parsing_Strategy >;

//
// Protocol message types meta-programming helper routines
//

template < typename Message_Type >
struct msg_type_lut_t
{
    /// Fake struct to prevent real usage.
    struct fake_type_t{};

    static_assert( !std::is_same_v< fake_type_t, fake_type_t >,  // Always false.
                   "msg_type_lut_t<Message_Type> was not specialized "
                   "for a given type. Maybe this type is not one of the messages "
                   "constituting a protocol ${proto_namespace}" );
};

constexpr std::size_t msg_types_count_incoming = ${len($protocol.incoming)};
constexpr std::size_t msg_types_count_outgoing = ${len($protocol.outgoing)};
constexpr std::size_t msg_types_count =
    msg_types_count_incoming + msg_types_count_outgoing;

//#import re
//#set $idx_all = 0
//#set $idx_incoming = 0
//#set $idx_outgoing = 0
//#set $incoming_fields = [$m.enum_id for $m in $protocol.incoming]
//#set $outgoing_fields = [$m.enum_id for $m in $protocol.outgoing]
//#for $msg in $protocol.incoming
template<>
struct msg_type_lut_t< ${msg.type} >
{
    static constexpr ${proto_namespace}::MessageType enum_value = ${msg.enum_id};
    static constexpr int numeric_tag_value = static_cast< int >( enum_value );
    static constexpr int protocol_index = ${idx_all};
    static constexpr int protocol_index_in = ${idx_incoming};
//#if $msg.enum_id in $outgoing_fields
    static constexpr int protocol_index_out = ${idx_outgoing};
//#set $idx_outgoing = $idx_outgoing + 1
//#else
    static constexpr int protocol_index_out = -1;
//#end if

//#set $short_class = re.findall(r'([^:]*)$', $msg.type)[0]
//#set $short_enum = re.findall(r'([^:]*)$', $msg.enum_id)[0]
    // Various strings:
    static constexpr std::string_view class_name       = "${msg.type}";
    static constexpr std::string_view class_name_short = "${short_class}";

    static constexpr std::string_view enum_name       = "${msg.enum_id}";
    static constexpr std::string_view enum_name_short = "${short_enum}";
//#set $idx_all = $idx_all + 1
//#set $idx_incoming = $idx_incoming + 1
};
//#end for

//#for $msg in $protocol.outgoing
//#if not $msg.enum_id in $incoming_fields
template<>
struct msg_type_lut_t< ${msg.type} >
{
    static constexpr ${proto_namespace}::MessageType enum_value = ${msg.enum_id};
    static constexpr int numeric_tag_value = static_cast< int >( enum_value );
    static constexpr int protocol_index = ${idx_all};
    static constexpr int protocol_index_in = -1;
    static constexpr int protocol_index_out = ${idx_outgoing};
//#set $idx_all = $idx_all + 1
//#set $idx_outgoing = $idx_outgoing + 1

//#set $short_class = re.findall(r'([^:]*)$', $msg.type)[0]
//#set $short_enum = re.findall(r'([^:]*)$', $msg.enum_id)[0]
    // Various strings:
    static constexpr std::string_view class_name       = "${msg.type}";
    static constexpr std::string_view class_name_short = "${short_class}";

    static constexpr std::string_view enum_name       = "${msg.enum_id}";
    static constexpr std::string_view enum_name_short = "${short_enum}";
//#set $idx_all = $idx_all + 1
//#set $idx_incoming = $idx_incoming + 1
};
//#end if
//#end for

template < ${proto_namespace}::MessageType Enum_Value >
struct enum_value_lut_t
{
    /// Fake struct to prevent real usage.
    struct fake_type_t{};

    static_assert( !std::is_same_v< fake_type_t, fake_type_t >,  // Always false.
                   "enum_value_lut_t<Message_Type> was not specialized "
                   "for a given enum value. Maybe this enum is not used to "
                   "identify any of ${proto_namespace} protocol types" );
};

constexpr std::array< ${proto_namespace}::MessageType,
                      msg_types_count_incoming > incoming_enums_list{
//#for $msg in $protocol.incoming
    ${msg.enum_id},
//#end for
};

constexpr std::array< ${proto_namespace}::MessageType,
                      msg_types_count_outgoing > outgoing_enums_list{
//#for $msg in $protocol.outgoing
    ${msg.enum_id},
//#end for
};

//#for $msg in $protocol.incoming
//#if not $msg.enum_id in $outgoing_fields
template<>
struct enum_value_lut_t< ${msg.enum_id} >
{
    using msg_type_t = ${msg.type};
};
//#end if
//#end for

//#for $msg in $protocol.outgoing
template<>
struct enum_value_lut_t< ${msg.enum_id} >
{
    using msg_type_t = ${msg.type};
};
//#end for

// A summary of protocol meta-helpers
// Acts as a traits class gathering helper routines
// otherwise located in namespace only, which is not
// convinient for using in template-code.
struct proto_traits_t
{
    //
    // Entry definitions.
    //

    template < typename Traits, typename Message_Consumer >
    using core_entry_t = ::${namespace}::core_entry_t< Traits, Message_Consumer >;

    template < typename Traits, typename Message_Consumer >
    using entry_t = ::${namespace}::entry_t< Traits, Message_Consumer >;

    template< typename Message_Consumer,
              typename Logger,
              ::opio::proto_entry::protobuf_parsing_strategy Protobuf_Parsing_Strategy =
                  ::opio::proto_entry::protobuf_parsing_strategy::trivial >
    using entry_singlethread_t =
        ::${namespace}::entry_shortcuts::singlethread_t< Message_Consumer, Logger, Protobuf_Parsing_Strategy >;

    template< typename Message_Consumer,
              typename Logger,
              ::opio::proto_entry::protobuf_parsing_strategy Protobuf_Parsing_Strategy =
                  ::opio::proto_entry::protobuf_parsing_strategy::trivial >
    using core_entry_singlethread_t =
        ::${namespace}::core_entry_shortcuts::singlethread_t< Message_Consumer, Logger, Protobuf_Parsing_Strategy >;

    template< typename Message_Consumer,
              typename Logger,
              ::opio::proto_entry::protobuf_parsing_strategy Protobuf_Parsing_Strategy =
                  ::opio::proto_entry::protobuf_parsing_strategy::trivial >
    using entry_multithread_t =
        ::${namespace}::entry_shortcuts::multithread_t< Message_Consumer, Logger, Protobuf_Parsing_Strategy >;

    template< typename Message_Consumer,
              typename Logger,
              ::opio::proto_entry::protobuf_parsing_strategy Protobuf_Parsing_Strategy =
                  ::opio::proto_entry::protobuf_parsing_strategy::trivial >
    using core_entry_multithread_t =
        ::${namespace}::core_entry_shortcuts::multithread_t< Message_Consumer, Logger, Protobuf_Parsing_Strategy >;

    //
    // IO Messages traits.
    //

    template < typename X >
    using msg_type_lut_t = ::${namespace}::msg_type_lut_t< X >;

    template < ${proto_namespace}::MessageType X >
    using enum_value_lut_t = ::${namespace}::enum_value_lut_t< X >;

    static constexpr auto msg_types_count_outgoing = ::${namespace}::msg_types_count_outgoing;
    static constexpr auto msg_types_count_incoming = ::${namespace}::msg_types_count_incoming;
    static constexpr auto msg_types_count = ::${namespace}::msg_types_count;

    static constexpr auto outgoing_enums_list = ::${namespace}::outgoing_enums_list;
    static constexpr auto incoming_enums_list = ::${namespace}::incoming_enums_list;
};

//
// make_package_image()
//

template < typename Message, ::opio::net::Buffer_Driver_Concept Buffer_Driver >
[[nodiscard]] auto make_package_image(
    const Message & msg,
    Buffer_Driver & buffer_driver,
    std::uint32_t attached_binary_size = 0UL )
{
    using this_msg_type_lut_t = msg_type_lut_t< Message >;

    auto buf = ::opio::proto_entry::make_package_image(
        static_cast< std::uint16_t >( this_msg_type_lut_t::enum_value ),
        msg,
        buffer_driver,
        attached_binary_size );

    return buf;
}

template < typename Message>
[[nodiscard]] auto make_package_image(
    const Message & msg,
    std::uint32_t attached_binary_size = 0UL )
{
    opio::net::simple_buffer_driver_t buffer_driver{};

    return make_package_image( msg, buffer_driver, attached_binary_size );
}

} // namespace ${namespace}

// GENERATED CODE, DO NOT MODIFY
// -----------------------------------------------------------------------------
