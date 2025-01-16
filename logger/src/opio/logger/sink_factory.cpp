#include <opio/logger/sink_factory.hpp>

#include <mutex>
#include <filesystem>

#if defined( _WIN32 )
#    include <spdlog/sinks/wincolor_sink.h>
#    define OPIO_LOGGER_COLOR_SING_TYPE spdlog::sinks::wincolor_stdout_sink_st
#else
#    include <spdlog/sinks/ansicolor_sink.h>
#    define OPIO_LOGGER_COLOR_SING_TYPE spdlog::sinks::ansicolor_stdout_sink_mt
#endif  // defined(_WIN32)

#include <spdlog/sinks/daily_file_sink.h>

namespace opio::logger
{

namespace fs = std::filesystem;

namespace /* anonymous */
{

//
// prepare_path_and_basename()
//

[[nodiscard]] inline std::string prepare_path_and_basename(
    std::string_view path,
    std::string_view filename_prefix )
{
    const fs::path log_path{ path };

    // Make sure target dir exists.
    fs::create_directories( log_path );
    return ( log_path / fs::path{ filename_prefix } ).string();
}

//
// daily_filename_calculator_t
//

/**
 * @brief Generator for logger name based on timestamp.
 */
// NOLINTNEXTLINE(altera-struct-pack-align)
struct daily_filename_calculator_t
{
    static spdlog::filename_t calc_filename( const spdlog::filename_t & filename,
                                             const tm & now_tm )
    {
        return fmt::format( "{}_{:%Y-%m-%d_%H-%M-%S}.log", filename, now_tm );
    }
};

}  // anonymous namespace

//
//  make_color_sink()
//

[[nodiscard]] logger_sink_sptr_t make_color_sink(
    std::optional< std::string > pattern )
{
    auto sink = std::make_shared< OPIO_LOGGER_COLOR_SING_TYPE >();
    if( pattern )
    {
        sink->set_pattern( *pattern );
    }

    return sink;
}

//
// make_daily_sink()
//

[[nodiscard]] logger_sink_sptr_t make_daily_sink(
    std::string_view path,
    std::string_view filename_prefix,
    std::optional< std::string > pattern )
{
    const auto basename = prepare_path_and_basename( path, filename_prefix );

    using filename_calculator_t = daily_filename_calculator_t;

    auto daily_sink = std::make_shared<
        spdlog::sinks::daily_file_sink< std::mutex, filename_calculator_t > >(
        basename, 0, 0 );

    if( pattern )
    {
        daily_sink->set_pattern( *pattern );
    }

    return daily_sink;
}

}  // namespace opio::logger
