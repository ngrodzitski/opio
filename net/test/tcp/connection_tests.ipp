namespace /* anonymous */
{

using namespace opio::net;           // NOLINT
using namespace opio::net::tcp;      // NOLINT
using namespace ::opio::test_utils;  // NOLINT

struct connection_traits_st_t : public default_traits_st_t
{
    using logger_t = opio::logger::logger_t;
    using input_handler_t =
        std::function< void( input_ctx_t< connection_traits_st_t > & ) >;
};

using connection_cfg_t = opio::net::tcp::connection_cfg_t;
using connection_t     = opio::net::tcp::connection_t< connection_traits_st_t >;
using buffer_t         = opio::net::simple_buffer_t;

template < typename Input_Handler >
connection_t::sptr_t make_connection(
    connection_traits_st_t::socket_t socket,
    opio::net::tcp::connection_id_t id,
    const opio::net::tcp::connection_cfg_t & cfg,
    opio::logger::logger_t logger,
    Input_Handler input_handler,
    opio::net::tcp::shutdown_handler_t shutdown_handler = {} )
{
    return connection_t::make( std::move( socket ), [ & ]( auto & params ) {
        params.connection_id( id )
            .connection_cfg( cfg )
            .logger( std::move( logger ) )
            .input_handler( std::move( input_handler ) )
            .shutdown_handler( std::move( shutdown_handler ) );
    } );
}

TEST( OPIO_NET_CONNECTION_TEST_NAME( OpioIpcTcp ),
      ConnectionMinimalPingPong )  // NOLINT
{
    connection_cfg_t cfg{};

    buffer_t etalon_buf1 = simple_buffer_t::make_from( { 'a', 'b', 'c', 'd' } );
    buffer_t etalon_buf2 = simple_buffer_t::make_from( { '0', '1', '2', '3' } );
    buffer_t etalon_data_all = etalon_buf1.make_copy();
    etalon_data_all.resize( etalon_data_all.size() + etalon_buf2.size() );
    std::copy( begin( etalon_buf2 ),
               end( etalon_buf2 ),
               etalon_data_all.offset_data( etalon_buf1.size() ) );

    asio_ns::io_context ioctx( 1 );

    opio::net::asio_ns::ip::tcp::socket s1{ ioctx };
    opio::net::asio_ns::ip::tcp::socket s2{ ioctx };

    connect_pair( ioctx, s1, s2 );

    std::string srv_input{};
    std::string cli_input{};

    auto server_conn = make_connection(
        std::move( s1 ),
        0,
        cfg,
        make_test_logger( "SERVER_CONN" ),
        [ & ]( auto & ctx ) {
            ctx.log().info( [ & ]( auto out ) {
                format_to( out, "received: [{}]", ctx.buf().make_string_view() );
            } );
            srv_input += ctx.buf().make_string_view();
            ctx.connection().schedule_send( std::move( ctx.buf() ) );
        } );
    EXPECT_EQ( server_conn->connection_id(), 0 );
    server_conn->start_reading();

    auto client_conn = make_connection(
        std::move( s2 ),
        1,
        cfg,
        make_test_logger( "client_conn" ),
        [ & ]( auto & ctx ) {
            ctx.log().info( [ & ]( auto out ) {
                format_to( out, "received: [{}]", ctx.buf().make_string_view() );
            } );
            cli_input += ctx.buf().make_string_view();

            if( cli_input.size() >= etalon_data_all.size() )
            {
                ctx.connection().shutdown();
            }
        } );
    EXPECT_EQ( client_conn->connection_id(), 1 );
    client_conn->start_reading();
    client_conn->schedule_send( std::move( etalon_buf1 ),
                                std::move( etalon_buf2 ) );

    ioctx.run();

    ASSERT_EQ( etalon_data_all.make_string_view(), srv_input );
    ASSERT_EQ( etalon_data_all.make_string_view(), cli_input );
}

TEST( OPIO_NET_CONNECTION_TEST_NAME( OpioIpcTcp ),
      ConnectionShutdownNotificator )  // NOLINT
{
    connection_cfg_t cfg{};

    buffer_t etalon_buf1 = simple_buffer_t::make_from( { 'a', 'b', 'c', 'd' } );
    buffer_t etalon_buf2 = simple_buffer_t::make_from( { '0', '1', '2', '3' } );
    buffer_t etalon_data_all = etalon_buf1.make_copy();
    etalon_data_all.resize( etalon_data_all.size() + etalon_buf2.size() );
    std::copy( begin( etalon_buf2 ),
               end( etalon_buf2 ),
               etalon_data_all.offset_data( etalon_buf1.size() ) );

    asio_ns::io_context ioctx( 1 );

    bool server_shutdown_notificator_called = false;
    bool client_shutdown_notificator_called = false;

    opio::net::asio_ns::ip::tcp::socket s1{ ioctx };
    opio::net::asio_ns::ip::tcp::socket s2{ ioctx };

    connect_pair( ioctx, s1, s2 );

    auto server_conn = make_connection(
        std::move( s1 ),
        0,
        cfg,
        make_test_logger( "SERVER_CONN" ),
        []( auto & ctx ) {
            // Reset the handler provided in constructor.
            ctx.connection().reset_shutdown_handler( {} );
            ctx.connection().shutdown();
        },
        [ & ]( [[maybe_unused]] auto shutdown_reason ) {
            server_shutdown_notificator_called = true;
        } );
    server_conn->start_reading();
    auto client_conn = make_connection(
        std::move( s2 ),
        1,
        cfg,
        make_test_logger( "client_conn" ),
        []( auto & /*input_ctx*/ ) {},
        [ & ]( [[maybe_unused]] auto shutdown_reason ) {
            client_shutdown_notificator_called = true;
        } );
    client_conn->start_reading();
    client_conn->schedule_send( buffer_t::make_from( { 'x', 'y', 'z' } ) );

    ioctx.run();
    EXPECT_FALSE( server_shutdown_notificator_called );
    EXPECT_TRUE( client_shutdown_notificator_called );
}

TEST( OPIO_NET_CONNECTION_TEST_NAME( OpioIpcTcp ),
      ConnectionStopReading )  // NOLINT
{
    connection_cfg_t cfg{};

    buffer_t etalon_buf1 = simple_buffer_t::make_from( { 'a', 'b', 'c', 'd' } );
    buffer_t etalon_buf2 = simple_buffer_t::make_from( { '0', '1', '2', '3' } );

    asio_ns::io_context ioctx( 1 );

    auto server_input_handler_happened = 0;
    auto client_input_handler_happened = 0;

    opio::net::asio_ns::ip::tcp::socket s1{ ioctx };
    opio::net::asio_ns::ip::tcp::socket s2{ ioctx };

    connect_pair( ioctx, s1, s2 );

    auto server_conn = make_connection(
        std::move( s1 ),
        0,
        cfg,
        make_test_logger( "SERVER_CONN" ),
        [ & ]( auto & ctx ) {
            ctx.log().info( [ & ]( auto out ) {
                format_to( out, "received: [{}]", ctx.buf().make_string_view() );
            } );

            ASSERT_EQ( etalon_buf1, ctx.buf() )
                << "server_input_handler_happened = "
                << ++server_input_handler_happened;

            ctx.connection().schedule_send( std::move( ctx.buf() ) );
            ++server_input_handler_happened;
            ctx.connection().stop_reading();
        } );
    server_conn->start_reading();

    auto client_conn = make_connection(
        std::move( s2 ),
        1,
        cfg,
        make_test_logger( "client_conn" ),
        [ & ]( auto & ctx ) {
            ctx.log().info( [ & ]( auto out ) {
                format_to( out, "received: [{}]", ctx.buf().make_string_view() );
            } );
            ASSERT_EQ( etalon_buf1, ctx.buf() );
            ctx.connection().schedule_send( std::move( etalon_buf2 ) );
            ;
            client_input_handler_happened = 1;
        } );
    client_conn->start_reading();
    client_conn->schedule_send( etalon_buf1.make_copy() );

    while( 0 == client_input_handler_happened )  // NOLINT
    {
        ioctx.poll();
    }

    asio_ns::post( [ & ] { client_conn->shutdown(); } );
    ioctx.run();
    EXPECT_EQ( server_input_handler_happened, 1 );
    EXPECT_EQ( client_input_handler_happened, 1 );
}

TEST( OPIO_NET_CONNECTION_TEST_NAME( OpioIpcTcp ),
      ConnectionResetInputHandler )  // NOLINT
{
    connection_cfg_t cfg{};

    const buffer_t etalon_buf1 =
        simple_buffer_t::make_from( { 'a', 'b', 'c', 'd' } );
    const buffer_t etalon_buf2 =
        simple_buffer_t::make_from( { '0', '1', '2', '3' } );

    asio_ns::io_context ioctx( 1 );

    opio::net::asio_ns::ip::tcp::socket s1{ ioctx };
    opio::net::asio_ns::ip::tcp::socket s2{ ioctx };

    connect_pair( ioctx, s1, s2 );

    auto server_input_handler_happened  = 0;
    auto client_input_handler_happened1 = 0;
    auto client_input_handler_happened2 = 0;

    // NOLINTNEXTLINE(readability-function-cognitive-complexity)
    auto server_conn = make_connection(
        std::move( s1 ),
        0,
        cfg,
        make_test_logger( "SERVER_CONN" ),
        [ & ]( auto & ctx ) {
            ctx.log().info( [ & ]( auto out ) {
                format_to( out, "received: [{}]", ctx.buf().make_string_view() );
            } );
            EXPECT_GE( server_input_handler_happened, 0 );
            EXPECT_LE( server_input_handler_happened, 1 );

            if( server_input_handler_happened == 0 )
            {
                ASSERT_EQ( etalon_buf1, ctx.buf() );
            }
            else
            {
                ASSERT_EQ( etalon_buf2, ctx.buf() );
            }
            ctx.connection().schedule_send( std::move( ctx.buf() ) );
            ++server_input_handler_happened;
        } );
    server_conn->start_reading();

    auto second_client_inout_handler = [ & ]( auto & ctx ) {
        ctx.log().info( [ & ]( auto out ) {
            format_to( out, "received: [{}]", ctx.buf().make_string_view() );
        } );
        ASSERT_EQ( etalon_buf2, ctx.buf() );
        ctx.connection().shutdown();
        ++client_input_handler_happened2;
    };

    auto first_client_inout_handler = [ & ]( auto & ctx ) {
        ctx.log().info( [ & ]( auto out ) {
            format_to( out, "received: [{}]", ctx.buf().make_string_view() );
        } );
        ASSERT_EQ( etalon_buf1, ctx.buf() );

        ctx.connection().schedule_send( etalon_buf2.make_copy() );
        ++client_input_handler_happened1;

        ctx.connection().reset_input_handler( second_client_inout_handler );
    };

    auto client_conn = make_connection( std::move( s2 ),
                                        1,
                                        cfg,
                                        make_test_logger( "client_conn" ),
                                        first_client_inout_handler );
    client_conn->start_reading();
    client_conn->schedule_send( etalon_buf1.make_copy() );

    ioctx.run();
    EXPECT_EQ( server_input_handler_happened, 2 );
    EXPECT_EQ( client_input_handler_happened1, 1 );
    EXPECT_EQ( client_input_handler_happened2, 1 );
}

TEST( OPIO_NET_CONNECTION_TEST_NAME( OpioIpcTcp ),
      ConnectionWriteNSequences )  // NOLINT
{
    connection_cfg_t cfg{};

    asio_ns::io_context ioctx( 1 );

    bool server_input_handler_happened             = false;
    bool client_input_handler_happened             = false;
    std::size_t server_received_data_size          = 0;
    std::size_t server_expected_received_data_size = 0;

    opio::net::asio_ns::ip::tcp::socket s1{ ioctx };
    opio::net::asio_ns::ip::tcp::socket s2{ ioctx };

    connect_pair( ioctx, s1, s2 );

    auto server_conn = make_connection(
        std::move( s1 ),
        0,
        cfg,
        make_test_logger( "SERVER_CONN" ),
        [ & ]( auto & ctx ) {
            server_received_data_size += ctx.buf().size();

            if( server_expected_received_data_size == server_received_data_size )
            {
                ctx.connection().shutdown();
            }

            server_input_handler_happened = true;
        } );
    server_conn->start_reading();

    auto client_conn =
        make_connection( std::move( s2 ),
                         1,
                         cfg,
                         make_test_logger( "client_conn" ),
                         [ & ]( [[maybe_unused]] auto & input_ctx ) {
                             client_input_handler_happened = true;
                         } );
    client_conn->start_reading();

    ioctx.poll();

    for( auto i = 0; i < details::reasonable_max_iov_len(); ++i )
    {
        std::vector< buffer_t > v;
        v.push_back( buffer_t::make_from( { '1', '2' } ) );
        v.push_back( buffer_t::make_from( { '1', '2', '3' } ) );
        v.push_back( buffer_t::make_from( { '1', '2', '3', '4' } ) );
        v.push_back( buffer_t::make_from( { '1', '2', '3', '4', '5' } ) );
        client_conn->schedule_send_vec( std::move( v ) );

        server_expected_received_data_size += 14;
    }

    ioctx.run();
    EXPECT_TRUE( server_input_handler_happened );
    EXPECT_FALSE( client_input_handler_happened );
}

TEST( OPIO_NET_CONNECTION_TEST_NAME( OpioIpcTcp ),
      ConnectionSendCompletionCallback )  // NOLINT
{
    connection_cfg_t cfg{};

    buffer_t etalon_buf1 = simple_buffer_t::make_from( { 'a', 'b', 'c', 'd' } );
    buffer_t etalon_buf2 = simple_buffer_t::make_from( { '0', '1', '2', '3' } );
    buffer_t etalon_data_all = etalon_buf1.make_copy();
    etalon_data_all.resize( etalon_data_all.size() + etalon_buf2.size() );
    std::copy( begin( etalon_buf2 ),
               end( etalon_buf2 ),
               etalon_data_all.offset_data( etalon_buf1.size() ) );

    asio_ns::io_context ioctx( 1 );

    bool send_completion_cb1_called = false;
    bool send_completion_cb2_called = false;

    opio::net::asio_ns::ip::tcp::socket s1{ ioctx };
    opio::net::asio_ns::ip::tcp::socket s2{ ioctx };

    connect_pair( ioctx, s1, s2 );

    auto server_conn = make_connection(
        std::move( s1 ),
        0,
        cfg,
        make_test_logger( "SERVER_CONN" ),
        [ & ]( auto & ctx ) {
            ctx.log().info( [ & ]( auto out ) {
                format_to( out, "received: [{}]", ctx.buf().make_string_view() );
            } );
        } );
    EXPECT_EQ( server_conn->connection_id(), 0 );

    server_conn->start_reading();
    auto client_conn =
        make_connection( std::move( s2 ),
                         1,
                         cfg,
                         make_test_logger( "client_conn" ),
                         [ & ]( [[maybe_unused]] auto & input_ctx ) {} );
    EXPECT_EQ( client_conn->connection_id(), 1 );
    client_conn->start_reading();

    client_conn->schedule_send_with_cb(
        [ & ]( auto result ) {
            EXPECT_EQ( result, send_buffers_result::success );
            send_completion_cb1_called = true;
        },
        std::move( etalon_buf1 ) );

    client_conn->schedule_send_with_cb(
        [ & ]( auto result ) {
            EXPECT_EQ( result, send_buffers_result::success );
            send_completion_cb2_called = true;
        },
        std::move( etalon_buf2 ) );

    while( !send_completion_cb1_called || !send_completion_cb2_called )  // NOLINT
    {
        ioctx.poll();
    }

    client_conn->shutdown();
    ioctx.run();
    EXPECT_TRUE( send_completion_cb1_called );
    EXPECT_TRUE( send_completion_cb2_called );
}

TEST( OPIO_NET_CONNECTION_TEST_NAME( OpioIpcTcp ),
      ConnectionShutdownReasonFmtIntegration )  // NOLINT
{
    using shutdown_reason = opio::net::tcp::connection_shutdown_reason;
    ASSERT_EQ( "user_initiated",
               fmt::format( "{}", shutdown_reason::user_initiated ) );
    ASSERT_EQ( "io_error", fmt::format( "{}", shutdown_reason::io_error ) );
    ASSERT_EQ( "eof", fmt::format( "{}", shutdown_reason::eof ) );
    ASSERT_EQ( "write_timeout",
               fmt::format( "{}", shutdown_reason::write_timeout ) );
    ASSERT_EQ( "unknown_shutdown_reason(999)",
               fmt::format( "{}", static_cast< shutdown_reason >( 999 ) ) );
}

class OPIO_NET_CONNECTION_TEST_NAME( OpioIpcTcpPairOfConnections )
    : public ::testing::Test
{
protected:
    void SetUp() override
    {
        opio::net::asio_ns::ip::tcp::socket s1{ ioctx };
        opio::net::asio_ns::ip::tcp::socket s2{ ioctx };

        port = connect_pair( ioctx, s1, s2 );

        server_conn = make_connection(
            std::move( s1 ),
            0,
            cfg,
            make_test_logger( "SERVER_CONN" ),
            []( auto & ctx ) { ctx.connection().shutdown(); },
            [ & ]( [[maybe_unused]] auto shutdown_reason ) {} );

        client_conn = make_connection(
            std::move( s2 ),
            1,
            cfg,
            make_test_logger( "client_conn" ),
            []( auto & /*input_ctx*/ ) {},
            [ & ]( [[maybe_unused]] auto shutdown_reason ) {} );

        ioctx.run_for( std::chrono::seconds( 1 ) );
    }

    connection_cfg_t cfg{};
    asio_ns::io_context ioctx{ 1 };
    asio_ns::executor_work_guard< asio_ns::executor > work{ ioctx.get_executor() };

    std::uint16_t port{};

    asio_ns::ip::tcp::endpoint ep{ asio_ns::ip::address::from_string(
                                       "127.0.0.1" ),
                                   port };

    asio_ns::ip::tcp::acceptor acceptor{ ioctx, ep };

    connection_t::sptr_t server_conn;
    connection_t::sptr_t client_conn;
};

TEST_F( OPIO_NET_CONNECTION_TEST_NAME( OpioIpcTcpPairOfConnections ),
        ScheduleSendToThisClosedConnection )  // NOLINT
{
    auto log = make_test_logger( "test_body" );

    log.info( "before client_conn->shutdown()" );
    client_conn->shutdown();
    log.info( "after client_conn->shutdown()" );

    ioctx.poll();

    log.info( "before client_conn->schedule_send(...)" );

    std::vector< buffer_t > vec;
    vec.emplace_back( buffer_t::make_from( { '1', '2' } ) );
    vec.emplace_back( buffer_t::make_from( { '1', '2', '3' } ) );
    vec.emplace_back( buffer_t::make_from( { '1', '2', '3', '4' } ) );
    vec.emplace_back( buffer_t::make_from( { '1', '2', '3', '4', '5' } ) );

    client_conn->schedule_send_vec( std::move( vec ) );
    log.info( "after client_conn->schedule_send(...)" );

    ioctx.run_for( std::chrono::milliseconds( 1 ) );

    // Sending with closed connection
    // should trigger no exceptions or crashes.
    ASSERT_TRUE( true );
}

TEST_F( OPIO_NET_CONNECTION_TEST_NAME( OpioIpcTcpPairOfConnections ),
        ScheduleSendToRemoteClosedConnection )  // NOLINT
{
    auto log = make_test_logger( "test_body" );

    log.info( "before server_conn->shutdown()" );
    server_conn->shutdown();
    log.info( "after server_conn->shutdown()" );

    ioctx.poll();

    log.info( "before client_conn->schedule_send(...)" );
    client_conn->schedule_send(
        buffer_t::make_from( { '1', '2' } ),
        buffer_t::make_from( { '1', '2', '3' } ),
        buffer_t::make_from( { '1', '2', '3', '4' } ),
        buffer_t::make_from( { '1', '2', '3', '4', '5' } ) );
    log.info( "after client_conn->schedule_send(...)" );

    ioctx.run_for( std::chrono::milliseconds( 1 ) );

    // Sending with closed connection
    // should trigger no exceptions or crashes.
    ASSERT_TRUE( true );
}

TEST_F( OPIO_NET_CONNECTION_TEST_NAME( OpioIpcTcpPairOfConnections ),
        ConnectionRemoteEndpointStr )  // NOLINT
{
    EXPECT_EQ( "127.0.0.1:" + std::to_string( port ),
               client_conn->remote_endpoint_str() );
}

TEST_F( OPIO_NET_CONNECTION_TEST_NAME( OpioIpcTcpPairOfConnections ),
        ConnectionSocketOptionsUpdate )  // NOLINT
{

    client_conn->start_reading();
    ioctx.poll();

    opio::net::tcp::socket_options_cfg_t socket_options_cfg{};
    socket_options_cfg.linger     = 5;
    socket_options_cfg.no_delay   = true;
    socket_options_cfg.keep_alive = false;

    bool cb_happened = false;

    client_conn->update_socket_options(
        std::move( socket_options_cfg ), [ & ]( auto res ) {
            cb_happened = true;
            ASSERT_EQ( update_socket_options_cb_result::success, res );

            ioctx.post( [ & ] { ioctx.stop(); } );
        } );

    ioctx.run_for( std::chrono::milliseconds( 1 ) );
    ASSERT_TRUE( cb_happened );
}

TEST_F( OPIO_NET_CONNECTION_TEST_NAME( OpioIpcTcpPairOfConnections ),
        ConnectionSocketOptionsUpdateEmpty )  // NOLINT
{
    client_conn->start_reading();
    ioctx.poll();

    opio::net::tcp::socket_options_cfg_t socket_options_cfg{};

    bool cb_happened = false;

    client_conn->update_socket_options(
        std::move( socket_options_cfg ), [ & ]( auto res ) {
            cb_happened = true;
            ASSERT_EQ( update_socket_options_cb_result::success, res );

            ioctx.post( [ & ] { ioctx.stop(); } );
        } );

    ioctx.run_for( std::chrono::milliseconds( 1 ) );
    ASSERT_TRUE( cb_happened );
}

TEST_F( OPIO_NET_CONNECTION_TEST_NAME( OpioIpcTcpPairOfConnections ),
        ConnectionSocketOptionsUpdateOnClosedConnection )  // NOLINT
{
    client_conn->shutdown();
    ioctx.poll();

    opio::net::tcp::socket_options_cfg_t socket_options_cfg{};
    socket_options_cfg.linger = 5;

    bool cb_happened = false;

    client_conn->update_socket_options( std::move( socket_options_cfg ),
                                        [ & ]( auto /*res*/ ) {
                                            cb_happened = true;
                                            ioctx.post( [ & ] { ioctx.stop(); } );
                                        } );

    ioctx.run_for( std::chrono::milliseconds( 1 ) );
    ASSERT_TRUE( cb_happened );
}

TEST( OPIO_NET_CONNECTION_TEST_NAME( OpioIpcTcp ),
      ConnectionIgnoreScheduledBuffsOnShutdownedConnection )  // NOLINT
{
    connection_cfg_t cfg{};

    asio_ns::io_context ioctx( 1 );
    opio::net::asio_ns::ip::tcp::socket s1{ ioctx };
    opio::net::asio_ns::ip::tcp::socket s2{ ioctx };

    connect_pair( ioctx, s1, s2 );

    std::optional< opio::net::tcp::send_buffers_result > send_buf_res;

    auto server_conn =
        make_connection( std::move( s1 ),
                         0,
                         cfg,
                         make_test_logger( "SERVER_CONN" ),
                         [ & ]( auto & ctx ) {
                             // Reset the handler provided in constructor.
                             ctx.connection().reset_shutdown_handler( {} );
                             ctx.connection().shutdown();

                             ctx.connection().schedule_send_with_cb(
                                 [ & ]( auto r ) { send_buf_res = r; },
                                 buffer_t::make_from( { 'x', 'y', 'z' } ) );
                         } );
    server_conn->start_reading();

    auto client_conn = make_connection( std::move( s2 ),
                                        1,
                                        cfg,
                                        make_test_logger( "client_conn" ),
                                        []( auto & /*input_ctx*/ ) {} );
    client_conn->start_reading();
    client_conn->schedule_send( buffer_t::make_from( { 'x', 'y', 'z' } ) );

    ioctx.run();
    ASSERT_TRUE( send_buf_res );
    EXPECT_EQ( opio::net::tcp::send_buffers_result::rejected_schedule_send,
               *send_buf_res );
}

}  // anonymous namespace
