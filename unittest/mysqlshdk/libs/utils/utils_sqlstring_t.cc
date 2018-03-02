/* Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <string>

#include "unittest/gtest_clean.h"

#include "mysqlshdk/libs/utils/utils_sqlstring.h"

namespace shcore {

TEST(utils_sqlstring, escape_wildcards) {
  EXPECT_EQ("", escape_wildcards(""));
  EXPECT_EQ("alphabet", escape_wildcards("alphabet"));
  EXPECT_EQ(R"(\_)", escape_wildcards("_"));
  EXPECT_EQ(R"(\%)", escape_wildcards("%"));
  EXPECT_EQ(R"(\%\_)", escape_wildcards("%_"));
  EXPECT_EQ(R"(\_\%)", escape_wildcards("_%"));
  EXPECT_EQ(R"(\%\%\%\%)", escape_wildcards("%%%%"));
  EXPECT_EQ(R"(\_\_\_\_)", escape_wildcards("____"));
  EXPECT_EQ(R"(\_\%\_\%)", escape_wildcards("_%_%"));
  EXPECT_EQ(R"(\%\_\%\_)", escape_wildcards("%_%_"));
  EXPECT_EQ(R"(\_al\%phab\_et\%)", escape_wildcards("_al%phab_et%"));
  EXPECT_EQ(R"(\%al\%phab\_et\_)", escape_wildcards("%al%phab_et_"));
}

}  // namespace shcore
