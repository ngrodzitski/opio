/**
 * @file
 *
 * This header file contains routines to "install" erro_codes vocabulary
 * to standard EC mechanics (`std::error_code` for standalone-asio
 * or `boost::system::error_code` Boost::asio) that are specific to this library.
 */

#pragma once

#include <opio/net/asio_include.hpp>

namespace opio::net::tcp
{

//
// error_codes
//

/**
 * @brief Errors codes specific to opio::net::tcp
 *        that can be converted to `asio_ns::error_code`.
 */
enum class error_codes : int
{
    open_acceptor_failed_already_started    = 0x20001,
    open_acceptor_failed_exception_happened = 0x20002,

    close_acceptor_failed_not_running        = 0x20003,
    close_acceptor_failed_exception_happened = 0x20004,

    sync_write_unexpected_results = 0x20100,
};

namespace details
{

//
// error_category_t
//

/**
 * Error category for asio compatible error codes
 */
class error_category_t : public error_category_base_t
{
public:
    const char * name() const noexcept override { return "opio::net::tcp"; }

    std::string message( int value ) const override
    {
        std::string result{};
        switch( static_cast< error_codes >( value ) )
        {
            case error_codes::open_acceptor_failed_already_started:
                result.assign( "open acceptor failed already started" );
                break;
            case error_codes::open_acceptor_failed_exception_happened:
                result.assign( "open acceptor failed exception happened" );
                break;
            case error_codes::close_acceptor_failed_not_running:
                result.assign( "close acceptor failed not running" );
                break;
            case error_codes::close_acceptor_failed_exception_happened:
                result.assign( "close acceptor failed exception happened" );
                break;
            case error_codes::sync_write_unexpected_results:
                result.assign( "sync write unexpected results" );
                break;
        }
        return result;
    }
};

//
// error_category
//

/**
 * @brief Get an instance of error category.
 * @return A reference to a "global" instance of error category.
 */
inline const auto & error_category()
{
    static error_category_t instance;
    return instance;
}

}  // namespace details

//
// make_std_compaible_error
//

inline asio_ns::error_code make_std_compaible_error( error_codes err )
{
    return asio_ns::error_code{ static_cast< int >( err ),
                                details::error_category() };
}

}  // namespace opio::net::tcp
