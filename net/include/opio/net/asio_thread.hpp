/**
 * @brief A routine to run ASIO thread.
 */

#pragma once

#include <thread>

#include <opio/log.hpp>

#include <opio/net/asio_include.hpp>

namespace opio::net
{

//
// asio_thread_t
//

/**
 * @brief a wrapper class to run ASIO io_context object on a separate thread.
 *
 * @todo: Add thread pinning.
 *
 * @pre This class is intended to be used from a single (or externally
 * synchronized) context. Which effectively means calling it's member function is
 * not therad safe.
 */
template < typename Logger >
class asio_thread_t
{
public:
    inline static constexpr int concurrency_hint_1 = 1;

    asio_thread_t( bool busy_wait, Logger logger )
        : m_ioctx{ concurrency_hint_1 }
        , m_busy_wait{ busy_wait }
        , m_logger{ std::move( logger ) }
    {
    }

    ~asio_thread_t()
    {
        if( m_thread )
        {
            m_ioctx.stop();
            m_thread->join();
        }
    }

    [[nodiscard]] net::asio_ns::io_context & ioctx() noexcept { return m_ioctx; }

    /**
     * @brief Start running asio io_context in a separate thread.
     *
     * @todo: handle duplicate start as error.
     */
    void start()
    {
        if( !m_thread )
        {
            m_thread = std::make_unique< std::thread >( [ this ] {
                try
                {
                    net::asio_ns::executor_work_guard<
                        net::asio_ns::any_io_executor >
                        ioctx_guard{ m_ioctx.get_executor() };

                    if( m_busy_wait )
                    {
                        m_logger.info( OPIO_SRC_LOCATION,
                                       "start running io context (busy wait)" );
                        while( !m_ioctx.stopped() )
                        {
                            m_ioctx.poll();
                            m_ioctx.poll();
                            m_ioctx.poll();
                            m_ioctx.poll();

                            m_ioctx.poll();
                            m_ioctx.poll();
                            m_ioctx.poll();
                            m_ioctx.poll();
                        }
                    }
                    else
                    {
                        m_logger.info( OPIO_SRC_LOCATION,
                                       "start running io context" );
                        m_ioctx.run();
                    }

                    m_logger.info( OPIO_SRC_LOCATION,
                                   "finish running io context" );
                    ioctx_guard.reset();

                    constexpr int gracefull_finish_timeout = 10;
                    m_logger.debug( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                        format_to( out,
                                   "io context's main event loop was stopped, "
                                   "will apply gracefull_finish_timeout of {} sec",
                                   gracefull_finish_timeout );
                    } );

                    // Let the iocontext finish gracefully:
                    m_ioctx.restart();
                    m_ioctx.run_for(
                        std::chrono::seconds( gracefull_finish_timeout ) );

                    m_logger.trace( OPIO_SRC_LOCATION,
                                    "io context stopped completely" );
                }
                catch( const std::exception & ex )
                {
                    m_logger.critical( OPIO_SRC_LOCATION, [ & ]( auto out ) {
                        format_to(
                            out, "Error running asio io context: {}", ex.what() );
                    } );
                }
            } );
        }
        else
        {
            m_logger.error( OPIO_SRC_LOCATION,
                            "Duplicate call to feed::asio_thread_t::start()" );
        }
    }

    /**
     * @brief Stop running asio io_context in a separate thread.
     *
     * @todo: handle duplicate stop as error.
     */
    void stop()
    {
        if( m_thread )
        {
            m_ioctx.stop();
        }
        else
        {
            m_logger.error( OPIO_SRC_LOCATION,
                            "Duplicate call to feed::asio_thread_t::stop()" );
        }
    }

    /**
     * @brief Stop running asio io_context in a separate thread.
     *
     * @todo: handle duplicate stop as error.
     */
    void join()
    {
        if( m_thread )
        {
            m_logger.trace( OPIO_SRC_LOCATION,
                            "[begin] waiting for feed::asio_thread_t to finish" );
            m_thread->join();
            m_logger.trace( OPIO_SRC_LOCATION,
                            "[end] waiting for feed::asio_thread_t to finish" );

            m_thread.reset();
        }
    }

private:
    net::asio_ns::io_context m_ioctx;
    const bool m_busy_wait;
    [[no_unique_address]] Logger m_logger;

    std::unique_ptr< std::thread > m_thread;
};

}  // namespace opio::net
