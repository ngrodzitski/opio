project(opio.net.tcp.examples})

add_executable(_example.opio.net.tcp.echo_server echo_server.cpp)
target_link_libraries(_example.opio.net.tcp.echo_server
                       PRIVATE CLI11::CLI11
                       opio::net
                       logr::logr_spdlog
)

add_executable(_example.opio.net.tcp.slow_proxy slow_proxy.cpp)
target_link_libraries( _example.opio.net.tcp.slow_proxy
                       PRIVATE
                       opio::net
                       logr::logr_spdlog
)
