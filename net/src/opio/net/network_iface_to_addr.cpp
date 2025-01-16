#include <opio/net/network_iface_to_addr.hpp>

#if !defined( _WIN32 )
#    include <arpa/inet.h>
#    include <ifaddrs.h>
#endif  // !defined(_WIN32)

namespace opio::net
{

//
// network_iface_to_addr()
//

[[nodiscard]] ::opio::expected_t< asio_ns::ip::address, ::opio::exception_t >
network_iface_to_addr( [[maybe_unused]] std::string_view iface_name )
{
#if defined( _WIN32 )
    return ::opio::make_unexpected(
        ::opio::exception_t{ "network_iface_to_addr not supported on windows" } );
#else
    try
    {
        ifaddrs * ifap{};

        if( -1 == getifaddrs( &ifap ) )
        {
            return ::opio::make_unexpected(
                ::opio::exception_t{ "getifaddrs() failed" } );
        }

        const std::unique_ptr< ifaddrs, decltype( &freeifaddrs ) > ifap_ptr{
            ifap, &freeifaddrs
        };

        // NOLINTNEXTLINE(altera-id-dependent-backward-branch)
        for( auto * iface = ifap; iface != nullptr; iface = iface->ifa_next )
        {
            // NOLINTNEXTLINE(readability-implicit-bool-conversion)
            if( iface->ifa_addr && iface->ifa_addr->sa_family == AF_INET
                && iface->ifa_name == iface_name )
            {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                auto * sa = reinterpret_cast< sockaddr_in * >( iface->ifa_addr );
                return asio_ns::ip::address_v4{ ntohl( sa->sin_addr.s_addr ) };
            }
        }
    }
    catch( const std::exception & ex )
    {
        return ::opio::make_unexpected( ::opio::exception_t{ ex.what() } );
    }

    return ::opio::make_unexpected(
        ::opio::exception_t{ "cannot find iface match for '{}'", iface_name } );
#endif  // defined(_WIN32)
}

}  // namespace opio::net
