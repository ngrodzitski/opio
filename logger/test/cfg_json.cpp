#include <opio/logger/cfg_json.hpp>

#include <opio/test_utils/test_read_config.hpp>

#include <gtest/gtest.h>

namespace /* anonymous */
{

// NOLINTNEXTLINE
using namespace ::opio::logger;

// NOLINTNEXTLINE
TEST( OpioCfg, ReadGlobalLoggerCfg )
{
    {
        const auto cfg =
            opio::test_utils::test_read_config< global_logger_cfg_t >( R"-({
            "log_message_pattern" : "[%H:%M:%S.%e] %v\n",
            "path" : "./logs",
            "global_log_level" : "trace",
            "log_to_stdout" : false,
        })-" );

        EXPECT_EQ( cfg.log_message_pattern, "[%H:%M:%S.%e] %v\n" );
        EXPECT_EQ( cfg.path, "./logs" );
        EXPECT_EQ( cfg.global_log_level, log_level::trace );
        EXPECT_EQ( cfg.log_to_stdout, false );
    }

    {
        const auto cfg =
            opio::test_utils::test_read_config< global_logger_cfg_t >( "{}" );

        EXPECT_EQ( cfg.log_message_pattern,
                   global_logger_cfg_t::default_log_message_pattern );
        EXPECT_EQ( cfg.path, global_logger_cfg_t::default_path );
        EXPECT_EQ( cfg.global_log_level,
                   global_logger_cfg_t::default_global_log_level );
        EXPECT_EQ( cfg.log_to_stdout, global_logger_cfg_t::default_log_to_stdout );
    }
}

}  // anonymous namespace
