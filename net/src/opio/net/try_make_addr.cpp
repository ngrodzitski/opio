#include <opio/net/try_make_addr.hpp>

#include <opio/net/network_iface_to_addr.hpp>

namespace opio::net
{

//
// network_iface_to_addr()
//

[[nodiscard]] ::opio::expected_t< asio_ns::ip::address, ::opio::exception_t >
try_make_addr( std::string_view iface_or_addr_str )
{

    if( "localhost" == iface_or_addr_str )
    {
        return asio_ns::ip::make_address( "127.0.0.1" );
    }

    if( "ip6-localhost" == iface_or_addr_str )
    {
        return asio_ns::ip::make_address( "::1" );
    }

    asio_ns::error_code ec;
    asio_ns::ip::address addr = asio_ns::ip::make_address( iface_or_addr_str, ec );
    if( !ec )
    {
        return addr;
    }

    // Here: then it might be a network iface.
    return network_iface_to_addr( iface_or_addr_str );
}

}  // namespace opio::net
