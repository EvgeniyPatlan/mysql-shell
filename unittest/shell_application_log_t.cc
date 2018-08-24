/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms, as
 * designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include "gtest_clean.h"
#include "scripting/common.h"
#include "shellcore/base_session.h"
#include "test_utils.h"
#include "utils/utils_file.h"

namespace shcore {
class Shell_application_log_tests : public Shell_core_test_wrapper {
 protected:
  static int i;
  ngcommon::Logger *_logger;

  static std::string error;

  static void my_hook(const ngcommon::Logger::Log_entry &entry) {
    std::string message_s(entry.message);
    EXPECT_TRUE(message_s.find(error) != std::string::npos);
    if (getenv("TEST_DEBUG")) std::cout << "LOG:" << entry.message << "\n";
    i++;
  }

  // You can define per-test set-up and tear-down logic as usual.
  virtual void SetUp() {
    Shell_core_test_wrapper::SetUp();
    Shell_application_log_tests::i = 0;

    std::string log_path = shcore::get_user_config_path();
    log_path += "mysqlsh.log";
    ngcommon::Logger::setup_instance(log_path.c_str(), false,
                                     ngcommon::Logger::LOG_ERROR);
    _logger = ngcommon::Logger::singleton();
    _logger->attach_log_hook(my_hook);

    _interactive_shell->process_line("\\js");

    exec_and_out_equals("var mysql = require('mysql');");
    exec_and_out_equals("var session = mysql.getClassicSession('" + _mysql_uri +
                        "');");
  }

  virtual void TearDown() { _logger->detach_log_hook(my_hook); }
};

std::string Shell_application_log_tests::error = "";

TEST_F(Shell_application_log_tests, test) {
  // issue an stmt with syntax error, then check the log.
  error = "SyntaxError: Unexpected token ; at :1:9\nin print('x';";
  execute("print('x';");

  error =
      "You have an error in your SQL syntax; check the manual that corresponds "
      "to your MySQL server version for the right syntax to use near '' at "
      "line 1";
  execute("session.runSql('select * from sakila.actor1 limit');");
  // The hook was invoked
  EXPECT_EQ(3, Shell_application_log_tests::i);

  execute("session.close();");
}

int Shell_application_log_tests::i;
}  // namespace shcore
