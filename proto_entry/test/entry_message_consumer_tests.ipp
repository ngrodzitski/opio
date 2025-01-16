template < typename Message_Consumer >
using OPIO_PROTO_ENTRY_TEST_CONSUMER_TEST_TYPE( consumer_test_entry_t ) =
    opio::proto_entry::utest::entry_singlethread_t<
        Message_Consumer,
        ::opio::logger::logger_t,
        OPIO_PROTO_ENTRY_TEST_PROTOBUF_PARSING_STRATEGY >;

template < typename Target_Consumer >
struct OPIO_PROTO_ENTRY_TEST_CONSUMER_TEST_TYPE( proxy_message_consumer_t )
{
    OPIO_PROTO_ENTRY_TEST_CONSUMER_TEST_TYPE( proxy_message_consumer_t )
    ( Target_Consumer * c )
        : consumer{ c }
    {
    }

    template < typename Message_Carrier, typename Entry >
    void on_message( Message_Carrier m, Entry & e )
    {
        consumer->on_message( std::move( m ), e );
    }

    Target_Consumer * consumer;
};

class OPIO_PROTO_ENTRY_TEST_CONSUMER_TEST_NAME( OpioProtoEntryMessageConsumer )
    : public testing::Test
{
protected:
    asio_ns::io_context ioctx{};
    asio_ns::executor_work_guard< asio_ns::any_io_executor > work{
        ioctx.get_executor()
    };

    asio_ns::ip::tcp::socket server_socket{ ioctx };
    asio_ns::ip::tcp::socket client_socket{ ioctx };

    using real_consumer_t = StrictMock< message_consumer_mock_t >;
    real_consumer_t message_consumer;

    using proxy_consumer_t = OPIO_PROTO_ENTRY_TEST_CONSUMER_TEST_TYPE(
        proxy_message_consumer_t )< real_consumer_t >;

    utest::YyyRequest msg;
    Sequence utest_calls_seq;

    void send_test_data()
    {
        const auto attached_bin = ::opio::net::simple_buffer_t::make_from(
            { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' } );

        auto pkg_buf = utest::make_package_image( msg );
        client_socket.send(
            asio_ns::const_buffer{ pkg_buf.data(), pkg_buf.size() } );

        pkg_buf = utest::make_package_image( msg, attached_bin.size() );
        client_socket.send(
            asio_ns::const_buffer{ pkg_buf.data(), pkg_buf.size() } );
        client_socket.send(
            asio_ns::const_buffer{ attached_bin.data(), attached_bin.size() } );
    }

    void SetUp() override
    {
        connect_pair( ioctx, server_socket, client_socket );

        msg.set_req_id( 2024 );

        EXPECT_CALL( message_consumer, on_message( An< utest::YyyRequest >() ) )
            .InSequence( utest_calls_seq )
            .WillOnce( Invoke( [ &, m = msg ]( auto received_msg ) {
                EXPECT_EQ( m.req_id(), received_msg.req_id() );
            } ) );

        EXPECT_CALL(
            message_consumer,
            on_message_with_attached_bin( An< utest::YyyRequest >(),
                                          An< ::opio::net::simple_buffer_t >() ) )
            .InSequence( utest_calls_seq )
            .WillOnce( Invoke( [ &, m = msg ]( auto received_msg, auto sb ) {
                EXPECT_EQ( m.req_id(), received_msg.req_id() );

                EXPECT_EQ( sb.size(), 10 );
                EXPECT_EQ( sb.make_string_view(), "0123456789" );
            } ) );
    }
};

TEST_F( OPIO_PROTO_ENTRY_TEST_CONSUMER_TEST_NAME(  // NOLINT
            OpioProtoEntryMessageConsumer ),       // NOLINT
        AsInstance )                               // NOLINT
{
    // Created an entry and immediatelly shuts it down.
    const auto started_at = std::chrono::steady_clock::now();

    using entry_t = OPIO_PROTO_ENTRY_TEST_CONSUMER_TEST_TYPE(
        consumer_test_entry_t )< proxy_consumer_t >;

    auto entry =
        entry_t::make( std::move( server_socket ), [ & ]( auto & params ) {
            params.logger( make_test_logger( "ENTRY" ) )
                .message_consumer( &message_consumer )
                .shutdown_handler(
                    [ & ]( [[maybe_unused]] auto id ) { ioctx.stop(); } );
        } );

    send_test_data();

    ioctx.run_for( std::chrono::milliseconds( 10 ) );

    entry->close();
    ioctx.run_for( std::chrono::milliseconds( 10 ) );

    EXPECT_GT( adjust_for_msvc_if_necessary( 25 ),
               msec_from_x_to_now( started_at ) );
}

TEST_F( OPIO_PROTO_ENTRY_TEST_CONSUMER_TEST_NAME(  // NOLINT
            OpioProtoEntryMessageConsumer ),       // NOLINT
        AsUniquePtr )                              // NOLINT
{
    // Created an entry and immediatelly shuts it down.
    const auto started_at = std::chrono::steady_clock::now();

    using entry_t = OPIO_PROTO_ENTRY_TEST_CONSUMER_TEST_TYPE(
        consumer_test_entry_t )< std::unique_ptr< proxy_consumer_t > >;

    auto entry =
        entry_t::make( std::move( server_socket ), [ & ]( auto & params ) {
            params.logger( make_test_logger( "ENTRY" ) )
                .message_consumer(
                    std::make_unique< proxy_consumer_t >( &message_consumer ) )
                .shutdown_handler(
                    [ & ]( [[maybe_unused]] auto id ) { ioctx.stop(); } );
        } );

    send_test_data();

    ioctx.run_for( std::chrono::milliseconds( 10 ) );

    entry->close();
    ioctx.run_for( std::chrono::milliseconds( 10 ) );

    EXPECT_GT( adjust_for_msvc_if_necessary( 25 ),
               msec_from_x_to_now( started_at ) );
}

TEST_F( OPIO_PROTO_ENTRY_TEST_CONSUMER_TEST_NAME(  // NOLINT
            OpioProtoEntryMessageConsumer ),       // NOLINT
        AsSharedPtr )                              // NOLINT
{
    // Created an entry and immediatelly shuts it down.
    const auto started_at = std::chrono::steady_clock::now();

    using entry_t = OPIO_PROTO_ENTRY_TEST_CONSUMER_TEST_TYPE(
        consumer_test_entry_t )< std::shared_ptr< proxy_consumer_t > >;

    auto entry =
        entry_t::make( std::move( server_socket ), [ & ]( auto & params ) {
            params.logger( make_test_logger( "ENTRY" ) )
                .message_consumer(
                    std::make_shared< proxy_consumer_t >( &message_consumer ) )
                .shutdown_handler(
                    [ & ]( [[maybe_unused]] auto id ) { ioctx.stop(); } );
        } );

    send_test_data();

    ioctx.run_for( std::chrono::milliseconds( 10 ) );

    entry->close();
    ioctx.run_for( std::chrono::milliseconds( 10 ) );

    EXPECT_GT( adjust_for_msvc_if_necessary( 25 ),
               msec_from_x_to_now( started_at ) );
}

TEST_F( OPIO_PROTO_ENTRY_TEST_CONSUMER_TEST_NAME(  // NOLINT
            OpioProtoEntryMessageConsumer ),       // NOLINT
        AsWeakPtr )                                // NOLINT
{
    // Created an entry and immediatelly shuts it down.
    const auto started_at = std::chrono::steady_clock::now();

    using entry_t = OPIO_PROTO_ENTRY_TEST_CONSUMER_TEST_TYPE(
        consumer_test_entry_t )< std::weak_ptr< proxy_consumer_t > >;
    auto consumer_anchor =
        std::make_shared< proxy_consumer_t >( &message_consumer );

    auto entry =
        entry_t::make( std::move( server_socket ), [ & ]( auto & params ) {
            params.logger( make_test_logger( "ENTRY" ) )
                .message_consumer(
                    std::weak_ptr< proxy_consumer_t >{ consumer_anchor } )
                .shutdown_handler(
                    [ & ]( [[maybe_unused]] auto id ) { ioctx.stop(); } );
        } );

    send_test_data();

    ioctx.run_for( std::chrono::milliseconds( 10 ) );

    entry->close();
    ioctx.run_for( std::chrono::milliseconds( 10 ) );

    EXPECT_GT( adjust_for_msvc_if_necessary( 25 ),
               msec_from_x_to_now( started_at ) );
}
