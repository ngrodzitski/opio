#include <opio/test_utils/test_logger.hpp>

#include <opio/logger/cfg.hpp>
#include <opio/logger/sink_factory.hpp>
#include <opio/logger/logger_factory.hpp>

namespace opio::test_utils
{

//
// make_test_logger()
//

opio::logger::logger_t make_test_logger( std::string_view logger_name,
                                         opio::logger::log_level log_lvl )
{
    // Global logger factory:
    static opio::logger::logger_factory_uptr_t g_logger =
        opio::logger::make_logger_factory(
            log_lvl,
            spdlog::sinks_init_list{ opio::logger::make_color_sink(
                opio::logger::global_logger_cfg_t::
                    default_log_message_pattern ) } );

    return g_logger->make_logger( logger_name );
}

}  // namespace opio::test_utils
