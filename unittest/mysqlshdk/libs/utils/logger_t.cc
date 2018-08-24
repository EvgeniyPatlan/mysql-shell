/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms, as
   designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.
   This program is distributed in the hope that it will be useful,  but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
   the GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA */

#include <algorithm>
#include <deque>
#include <fstream>
#include <memory>
#include <string>

#include "unittest/gtest_clean.h"
#include "unittest/test_utils/mocks/gmock_clean.h"

#include "mysqlshdk/libs/utils/logger.h"
#include "mysqlshdk/libs/utils/utils_file.h"
#include "mysqlshdk/libs/utils/utils_path.h"

#ifndef _WIN32
#include <fcntl.h>
#endif  // !_WIN32

namespace ngcommon {

namespace {

bool is_timestamp(const char *text) {
  // example timestamp: "2015-12-23 09:26:49"
  using std::isdigit;
  return isdigit(text[0]) && isdigit(text[1]) && isdigit(text[2]) &&
         isdigit(text[3]) && '-' == text[4] &&                      // 2015-
         isdigit(text[5]) && isdigit(text[6]) && '-' == text[7] &&  // 12-
         isdigit(text[8]) && isdigit(text[9]) &&
         ' ' == text[10] &&  // 23<space>

         isdigit(text[11]) && isdigit(text[12]) && ':' == text[13] &&  // 09:
         isdigit(text[14]) && isdigit(text[15]) && ':' == text[16] &&  // 26:
         isdigit(text[17]) && isdigit(text[18]);                       // 49
}

}  // namespace

class Logger_test : public ::testing::Test {
 protected:
  static void log_hook(const Logger::Log_entry &) { ++s_hook_executed; }

  static int hook_executed() { return s_hook_executed; }

  static std::string get_log_file(const char *filename) {
    static const auto k_test_dir = getenv("TMPDIR");
    return shcore::path::join_path(k_test_dir, filename);
  }

  static bool get_log_file_contents(const char *filename,
                                    std::string *contents) {
    return shcore::load_text_file(get_log_file(filename), *contents);
  }

  void SetUp() override {
    m_previous_log_file = Logger::singleton()->logfile_name();
    m_previous_use_stderr = Logger::singleton()->use_stderr();
    m_previous_log_level = Logger::singleton()->get_log_level();
    m_previous_stderr_format = Logger::stderr_output_format();
  }

  void TearDown() override {
    const auto current_log_file = Logger::singleton()->logfile_name();

    if (current_log_file != m_previous_log_file) {
      Logger::setup_instance(m_previous_log_file.c_str(), m_previous_use_stderr,
                             m_previous_log_level);

      if (!shcore::is_folder(current_log_file)) {
        shcore::delete_file(current_log_file);
      }
    }

    Logger::set_stderr_output_format(m_previous_stderr_format);

    s_hook_executed = 0;
  }

 private:
  static int s_hook_executed;

  std::string m_previous_log_file;
  bool m_previous_use_stderr;
  Logger::LOG_LEVEL m_previous_log_level;
  std::string m_previous_stderr_format;
};

int Logger_test::s_hook_executed = 0;

TEST_F(Logger_test, intialization) {
  const auto name = get_log_file("mylog.txt");

  Logger::setup_instance(name.c_str(), false, Logger::LOG_DEBUG);
  const auto l = Logger::singleton();

  EXPECT_EQ(name, l->logfile_name());
  EXPECT_EQ(Logger::LOG_DEBUG, l->get_log_level());

  l->set_log_level(Logger::LOG_WARNING);
  EXPECT_EQ(Logger::LOG_WARNING, l->get_log_level());
}

TEST_F(Logger_test, log_levels_and_hooks) {
  Logger::setup_instance(get_log_file("mylog.txt").c_str(), false,
                         Logger::LOG_WARNING);

  const auto l = Logger::singleton();

  l->attach_log_hook(log_hook);
  l->log(Logger::LOG_ERROR, "Unit Test Domain", "Error due to %s", "critical");
  l->detach_log_hook(log_hook);
  // Debug message will not appear in the log
  l->log(Logger::LOG_DEBUG, "Unit Test Domain", "Memory deallocated");
  l->log(Logger::LOG_WARNING, "Unit Test Domain",
         "Warning the file already exists");

  EXPECT_EQ(1, hook_executed());

  std::string contents;
  EXPECT_TRUE(get_log_file_contents("mylog.txt", &contents));

  EXPECT_THAT(
      contents,
      ::testing::HasSubstr("Error: Unit Test Domain: Error due to critical\n"));
  EXPECT_THAT(
      contents,
      ::testing::HasSubstr(
          "Warning: Unit Test Domain: Warning the file already exists"));
  EXPECT_THAT(contents,
              ::testing::Not(::testing::HasSubstr("Memory deallocated")));
}

TEST_F(Logger_test, get_level_by_name) {
  EXPECT_EQ(Logger::LOG_NONE, Logger::get_level_by_name("unknown"));
  EXPECT_EQ(Logger::LOG_NONE, Logger::get_level_by_name("NONE"));
  EXPECT_EQ(Logger::LOG_INTERNAL_ERROR, Logger::get_level_by_name("internal"));
  EXPECT_EQ(Logger::LOG_ERROR, Logger::get_level_by_name("error"));
  EXPECT_EQ(Logger::LOG_WARNING, Logger::get_level_by_name("WArning"));
  EXPECT_EQ(Logger::LOG_INFO, Logger::get_level_by_name("infO"));
  EXPECT_EQ(Logger::LOG_DEBUG, Logger::get_level_by_name("DEBUG"));
  EXPECT_EQ(Logger::LOG_DEBUG2, Logger::get_level_by_name("DEBUG2"));
  EXPECT_EQ(Logger::LOG_DEBUG3, Logger::get_level_by_name("DEBUG3"));
  EXPECT_EQ(Logger::LOG_NONE, Logger::get_level_by_name("0"));
  EXPECT_EQ(Logger::LOG_NONE, Logger::get_level_by_name("1"));
  EXPECT_EQ(Logger::LOG_NONE, Logger::get_level_by_name("2"));
  EXPECT_EQ(Logger::LOG_NONE, Logger::get_level_by_name("3"));
  EXPECT_EQ(Logger::LOG_NONE, Logger::get_level_by_name("4"));
  EXPECT_EQ(Logger::LOG_NONE, Logger::get_level_by_name("5"));
  EXPECT_EQ(Logger::LOG_NONE, Logger::get_level_by_name("6"));
  EXPECT_EQ(Logger::LOG_NONE, Logger::get_level_by_name("7"));
  EXPECT_EQ(Logger::LOG_NONE, Logger::get_level_by_name("8"));
  EXPECT_EQ(Logger::LOG_NONE, Logger::get_level_by_name("9"));
}

TEST_F(Logger_test, get_log_level) {
  EXPECT_EQ(Logger::LOG_NONE, Logger::get_log_level("unknown"));
  EXPECT_EQ(Logger::LOG_NONE, Logger::get_log_level("NONE"));
  EXPECT_EQ(Logger::LOG_INTERNAL_ERROR, Logger::get_log_level("internal"));
  EXPECT_EQ(Logger::LOG_ERROR, Logger::get_log_level("error"));
  EXPECT_EQ(Logger::LOG_WARNING, Logger::get_log_level("WArning"));
  EXPECT_EQ(Logger::LOG_INFO, Logger::get_log_level("infO"));
  EXPECT_EQ(Logger::LOG_DEBUG, Logger::get_log_level("DEBUG"));
  EXPECT_EQ(Logger::LOG_DEBUG2, Logger::get_log_level("DEBUG2"));
  EXPECT_EQ(Logger::LOG_DEBUG3, Logger::get_log_level("DEBUG3"));
  EXPECT_EQ(Logger::LOG_NONE, Logger::get_log_level("0"));
  EXPECT_EQ(Logger::LOG_NONE, Logger::get_log_level("1"));
  EXPECT_EQ(Logger::LOG_INTERNAL_ERROR, Logger::get_log_level("2"));
  EXPECT_EQ(Logger::LOG_ERROR, Logger::get_log_level("3"));
  EXPECT_EQ(Logger::LOG_WARNING, Logger::get_log_level("4"));
  EXPECT_EQ(Logger::LOG_INFO, Logger::get_log_level("5"));
  EXPECT_EQ(Logger::LOG_DEBUG, Logger::get_log_level("6"));
  EXPECT_EQ(Logger::LOG_DEBUG2, Logger::get_log_level("7"));
  EXPECT_EQ(Logger::LOG_DEBUG3, Logger::get_log_level("8"));
  EXPECT_EQ(Logger::LOG_NONE, Logger::get_log_level("9"));
}

TEST_F(Logger_test, is_level_none) {
  EXPECT_TRUE(Logger::is_level_none("NONE"));
  EXPECT_TRUE(Logger::is_level_none("none"));
  EXPECT_TRUE(Logger::is_level_none("1"));
  EXPECT_FALSE(Logger::is_level_none("0"));
  EXPECT_FALSE(Logger::is_level_none("unknown"));
}

TEST_F(Logger_test, log_open_failure) {
  EXPECT_THROW(
      {
        const std::string filename = get_log_file("");
        try {
          Logger::setup_instance(filename.c_str(), false, Logger::LOG_WARNING);
        } catch (const std::logic_error &e) {
          EXPECT_EQ("Error in Logger::Logger when opening file '" + filename +
                        "' for writing",
                    e.what());
          throw;
        }
      },
      std::logic_error);
}

TEST_F(Logger_test, log_format) {
  Logger::setup_instance(get_log_file("mylog.txt").c_str(), false,
                         Logger::LOG_DEBUG);

  const auto l = Logger::singleton();

  std::deque<std::pair<Logger::LOG_LEVEL, std::string>> tests = {
      {Logger::LOG_DEBUG, "Debug"},
      {Logger::LOG_INFO, "Info"},
      {Logger::LOG_WARNING, "Warning"},
      {Logger::LOG_ERROR, "Error"}};

  for (const auto &p : tests) {
    l->log(p.first, "Unit Test Domain", "Some message");
  }

  std::string contents;
  EXPECT_TRUE(get_log_file_contents("mylog.txt", &contents));

  for (const auto &line : shcore::str_split(contents, "\n")) {
    if (line.empty()) {
      continue;
    }

    ASSERT_FALSE(tests.empty());

    EXPECT_EQ(": " + tests.front().second + ": Unit Test Domain: Some message",
              line.substr(19));
    EXPECT_TRUE(is_timestamp(line.c_str()));

    tests.pop_front();
  }

  EXPECT_TRUE(tests.empty());
}

TEST_F(Logger_test, log_exception_format) {
  // exception should always be logged, even if log is disabled
  Logger::setup_instance(get_log_file("mylog.txt").c_str(), false,
                         Logger::LOG_NONE);

  const auto l = Logger::singleton();
  const std::runtime_error e{"Some exception"};

  l->log(e, "Unit Test Domain", "First message");
  l->log(e, "Unit Test Domain", "Second message with parameter: %s", "invalid");

  std::string contents;
  EXPECT_TRUE(get_log_file_contents("mylog.txt", &contents));
  const auto lines = shcore::str_split(contents, "\n");

  EXPECT_EQ(": Error: Unit Test Domain: First message: Some exception",
            lines[0].substr(19));
  EXPECT_TRUE(is_timestamp(lines[0].c_str()));

  EXPECT_EQ(
      ": Error: Unit Test Domain: Second message with parameter: invalid: Some "
      "exception",
      lines[1].substr(19));
  EXPECT_TRUE(is_timestamp(lines[1].c_str()));
}

#ifndef _WIN32
// on Windows Logger is using OutputDebugString() instead of stderr

namespace {

class Stderr_reader final {
 public:
  Stderr_reader() {
    m_pipe[0] = 0;
    m_pipe[1] = 0;

    if (pipe(m_pipe) == -1) {
      throw std::runtime_error{"Failed to create pipe"};
    }

    // copy stderr
    m_stderr = dup(fileno(stderr));
    // set read end to non-blocking
    fcntl(m_pipe[0], F_SETFL, O_NONBLOCK | fcntl(m_pipe[0], F_GETFL, 0));
    // replace stderr with write end
    dup2(m_pipe[1], fileno(stderr));
  }

  ~Stderr_reader() {
    // restore stderr
    dup2(m_stderr, fileno(stderr));

    // close file descriptors
    if (m_stderr > 0) {
      close(m_stderr);
    }

    if (m_pipe[0] > 0) {
      close(m_pipe[0]);
    }

    if (m_pipe[1] > 0) {
      close(m_pipe[1]);
    }
  }

  Stderr_reader(const Stderr_reader &) = delete;
  Stderr_reader(Stderr_reader &&) = delete;

  Stderr_reader &operator=(const Stderr_reader &) = delete;
  Stderr_reader &operator=(Stderr_reader &&) = delete;

  std::string output() const {
    static constexpr size_t k_size = 512;
    char buffer[k_size];
    std::string result;
    ssize_t bytes = 0;

    while ((bytes = ::read(m_pipe[0], buffer, k_size - 1)) > 0) {
      buffer[bytes] = '\0';
      result += buffer;
    }

    return result;
  }

 private:
  int m_pipe[2];
  int m_stderr;
};

}  // namespace

TEST_F(Logger_test, stderr_output) {
  Stderr_reader reader;

  Logger::set_stderr_output_format("table");
  Logger::setup_instance(get_log_file("mylog.txt").c_str(), true,
                         Logger::LOG_WARNING);

  const auto l = Logger::singleton();

  l->log(Logger::LOG_DEBUG, "Unit Test Domain", "First");
  l->log(Logger::LOG_INFO, "Unit Test Domain", "Second");
  l->log(Logger::LOG_WARNING, "Unit Test Domain", "Third");
  l->log(Logger::LOG_ERROR, "Unit Test Domain", "Fourth");

  std::string contents;
  EXPECT_TRUE(get_log_file_contents("mylog.txt", &contents));

  std::string error = reader.output();
  EXPECT_EQ(contents, error);

  EXPECT_THAT(error, ::testing::Not(::testing::HasSubstr("First")));
  EXPECT_THAT(error, ::testing::Not(::testing::HasSubstr("Second")));
  EXPECT_THAT(error, ::testing::HasSubstr("Third"));
  EXPECT_THAT(error, ::testing::HasSubstr("Fourth"));
}

TEST_F(Logger_test, stderr_json_output) {
  Stderr_reader reader;

  Logger::set_stderr_output_format("json");
  Logger::setup_instance(get_log_file("mylog.txt").c_str(), true,
                         Logger::LOG_WARNING);

  const auto l = Logger::singleton();

  l->log(Logger::LOG_DEBUG, "Unit Test Domain", "First");
  l->log(Logger::LOG_INFO, "Unit Test Domain", "Second");
  l->log(Logger::LOG_WARNING, "Unit Test Domain", "Third");
  l->log(Logger::LOG_ERROR, "Unit Test Domain", "Fourth");

  std::string contents;
  EXPECT_TRUE(get_log_file_contents("mylog.txt", &contents));
  const auto log_lines = shcore::str_split(contents, "\n");

  EXPECT_EQ(": Warning: Unit Test Domain: Third", log_lines[0].substr(19));
  EXPECT_TRUE(is_timestamp(log_lines[0].c_str()));

  EXPECT_EQ(": Error: Unit Test Domain: Fourth", log_lines[1].substr(19));
  EXPECT_TRUE(is_timestamp(log_lines[1].c_str()));

  const auto lines = shcore::str_split(reader.output(), "\n");

  EXPECT_EQ("{", lines[0]);
  EXPECT_EQ("    \"timestamp\": \"", lines[1].substr(0, 18));
  auto timestamp = lines[1].substr(18, 10);
  EXPECT_TRUE(std::all_of(timestamp.cbegin(), timestamp.cend(), ::isdigit));
  EXPECT_EQ("\",", lines[1].substr(28));
  EXPECT_EQ("    \"level\": \"Warning\",", lines[2]);
  EXPECT_EQ("    \"domain\": \"Unit Test Domain\",", lines[3]);
  EXPECT_EQ("    \"message\": \"Third\"", lines[4]);
  EXPECT_EQ("}", lines[5]);
  EXPECT_EQ("{", lines[6]);
  EXPECT_EQ("    \"timestamp\": \"", lines[7].substr(0, 18));
  timestamp = lines[7].substr(18, 10);
  EXPECT_TRUE(std::all_of(timestamp.cbegin(), timestamp.cend(), ::isdigit));
  EXPECT_EQ("\",", lines[7].substr(28));
  EXPECT_EQ("    \"level\": \"Error\",", lines[8]);
  EXPECT_EQ("    \"domain\": \"Unit Test Domain\",", lines[9]);
  EXPECT_EQ("    \"message\": \"Fourth\"", lines[10]);
  EXPECT_EQ("}", lines[11]);
}

TEST_F(Logger_test, stderr_json_raw_output) {
  Stderr_reader reader;

  Logger::set_stderr_output_format("json/raw");
  Logger::setup_instance(get_log_file("mylog.txt").c_str(), true,
                         Logger::LOG_WARNING);

  const auto l = Logger::singleton();

  l->log(Logger::LOG_DEBUG, "Unit Test Domain", "First");
  l->log(Logger::LOG_INFO, "Unit Test Domain", "Second");
  l->log(Logger::LOG_WARNING, "Unit Test Domain", "Third");
  l->log(Logger::LOG_ERROR, "Unit Test Domain", "Fourth");

  std::string contents;
  EXPECT_TRUE(get_log_file_contents("mylog.txt", &contents));
  const auto log_lines = shcore::str_split(contents, "\n");

  EXPECT_EQ(": Warning: Unit Test Domain: Third", log_lines[0].substr(19));
  EXPECT_TRUE(is_timestamp(log_lines[0].c_str()));

  EXPECT_EQ(": Error: Unit Test Domain: Fourth", log_lines[1].substr(19));
  EXPECT_TRUE(is_timestamp(log_lines[1].c_str()));

  const auto lines = shcore::str_split(reader.output(), "\n");

  EXPECT_EQ("{\"timestamp\":\"", lines[0].substr(0, 14));
  auto timestamp = lines[0].substr(14, 10);
  EXPECT_TRUE(std::all_of(timestamp.cbegin(), timestamp.cend(), ::isdigit));
  EXPECT_EQ(
      "\",\"level\":\"Warning\",\"domain\":\"Unit Test "
      "Domain\",\"message\":\"Third\"}",
      lines[0].substr(24));

  EXPECT_EQ("{\"timestamp\":\"", lines[1].substr(0, 14));
  timestamp = lines[1].substr(14, 10);
  EXPECT_TRUE(std::all_of(timestamp.cbegin(), timestamp.cend(), ::isdigit));
  EXPECT_EQ(
      "\",\"level\":\"Error\",\"domain\":\"Unit Test "
      "Domain\",\"message\":\"Fourth\"}",
      lines[1].substr(24));
}

#endif  // !_WIN32

}  // namespace ngcommon
