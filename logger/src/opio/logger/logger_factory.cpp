#include <opio/logger/logger_factory.hpp>
#include <opio/logger/sink_factory.hpp>

namespace opio::logger
{

namespace /* anonymous */
{

//
// logger_t_factory_impl_t
//

class logger_t_factory_impl_t final : public logger_factory_t
{
public:
    logger_t_factory_impl_t( log_level default_level,
                             spdlog::sinks_init_list spd_sinks )
        : m_default_level{ default_level }
        , m_spd_sinks{ begin( spd_sinks ), end( spd_sinks ) }
    {
    }

    [[nodiscard]] logger_t make_logger( std::string_view logger_name ) final
    {
        return make_logger( m_default_level, logger_name );
    }

    [[nodiscard]] logger_t make_logger( log_level level,
                                        std::string_view logger_name ) final
    {
        return logger_t{ std::string{ logger_name },
                         begin( m_spd_sinks ),
                         end( m_spd_sinks ),
                         level };
    }

private:
    const log_level m_default_level;
    const std::vector< spdlog::sink_ptr > m_spd_sinks;
};

}  // anonymous namespace

//
// make_logger_factory
//

[[nodiscard]] logger_factory_uptr_t make_logger_factory(
    log_level default_level,
    spdlog::sinks_init_list spd_sinks )
{
    return std::make_unique< logger_t_factory_impl_t >( default_level, spd_sinks );
}

[[nodiscard]] logger_factory_uptr_t make_logger_factory(
    std::string_view app_name,
    const global_logger_cfg_t & cfg )
{
    if( cfg.path.empty() )
    {
        // No path means we want no logger at all.
        return make_logger_factory( log_level::nolog, {} );
    }

    if( cfg.log_to_stdout )
    {
        return make_logger_factory(
            cfg.global_log_level,
            { make_color_sink( cfg.log_message_pattern ),
              make_daily_sink( cfg.path, app_name, cfg.log_message_pattern ) } );
    }

    return make_logger_factory(
        cfg.global_log_level,
        { make_daily_sink( cfg.path, app_name, cfg.log_message_pattern ) } );
}

}  // namespace opio::logger
