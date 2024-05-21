#include "doctest/doctest.h"

#include "misc/TestUtilities.h"
#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/LogMacros.h"
#include "quill/sinks/FileSink.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace quill;

/***/
TEST_CASE("backtrace_dynamic_log_level")
{
  static constexpr char const* filename = "backtrace_dynamic_log_level.log";
  static std::string const logger_name = "logger";

  // Start the logging backend thread
  Backend::start();

  Frontend::preallocate();

  // Set writing logging to a file
  auto file_sink = Frontend::create_or_get_sink<FileSink>(
    filename,
    []()
    {
      FileSinkConfig cfg;
      cfg.set_open_mode('w');
      return cfg;
    }(),
    FileEventNotifier{});

  Logger* logger = Frontend::create_or_get_logger(logger_name, std::move(file_sink));

  // Enable backtrace for 2 messages for error
  logger->init_backtrace(2, LogLevel::Error);

  // log using dynamic log level and flush on warning instead
  LOG_DYNAMIC(logger, LogLevel::Info, "Before dynamic backtrace log");
  for (size_t i = 100; i < 120; ++i)
  {
#if defined(__aarch64__) || ((__ARM_ARCH >= 6) || defined(_M_ARM64))
    // On ARM we add a small delay because log messages can get the same timestamp from rdtsc
    // when in this loop and make the test unstable
    std::this_thread::sleep_for(std::chrono::microseconds{200});
#endif
    LOG_DYNAMIC(logger, LogLevel::Backtrace, "Dynamic backtrace log {}", i);
  }

  LOG_DYNAMIC(logger, LogLevel::Error, "After dynamic error");

  logger->flush_log();
  Frontend::remove_logger(logger);

  // Wait until the backend thread stops for test stability
  Backend::stop();

  // Read file and check
  std::vector<std::string> const file_contents = quill::testing::file_contents(filename);

  REQUIRE_EQ(file_contents.size(), 4);

  std::string expected_string_1 =
    "LOG_INFO      " + logger_name + "       Before dynamic backtrace log";
  REQUIRE(quill::testing::file_contains(file_contents, expected_string_1));

  std::string expected_string_2 = "LOG_ERROR     " + logger_name + "       After dynamic error";
  REQUIRE(quill::testing::file_contains(file_contents, expected_string_2));

  std::string expected_string_3 =
    "LOG_BACKTRACE " + logger_name + "       Dynamic backtrace log 118";
  REQUIRE(quill::testing::file_contains(file_contents, expected_string_3));

  std::string expected_string_4 =
    "LOG_BACKTRACE " + logger_name + "       Dynamic backtrace log 119";
  REQUIRE(quill::testing::file_contains(file_contents, expected_string_4));

  testing::remove_file(filename);
}