/*
 * Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.
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

#include "mysqlshdk/libs/utils/utils_lexing.h"
#include <cctype>

namespace mysqlshdk {
namespace utils {

SQL_string_iterator::SQL_string_iterator(const std::string &str,
                                         std::string::size_type offset)
    : m_s(str), m_offset(offset - 1) {
  // Let's make sure we start from valid SQL
  ++(*this);
}

SQL_string_iterator &SQL_string_iterator::operator++() {
  if (++m_offset > m_s.length())
    throw std::out_of_range("SQL_string_iterator offset out of range");

  bool incremented = false;
  do {
    switch (m_s[m_offset]) {
      case '\'':
        m_offset = span_quoted_string_sq(m_s, m_offset);
        break;
      case '"':
        m_offset = span_quoted_string_dq(m_s, m_offset);
        break;
      case '`':
        m_offset = span_quoted_sql_identifier_bt(m_s, m_offset);
        break;
      case '/':
        if (m_s[m_offset + 1] == '*')
          m_offset = span_cstyle_comment(m_s, m_offset);
        else
          incremented = true;
        break;
      case '#':
        m_offset = span_to_eol(m_s, m_offset + 1);
        break;
      case '-':
        if (m_offset + 2 < m_s.length() && m_s[m_offset + 1] == '-' &&
            std::isspace(m_s[m_offset + 2]))
          m_offset = span_to_eol(m_s, m_offset + 2);
        else
          incremented = true;
        break;
      default:
        incremented = true;
    }
    if (m_offset == std::string::npos) m_offset = m_s.length();
  } while (!incremented && m_offset < m_s.length());

  return *this;
}

std::string SQL_string_iterator::get_next_sql_token() {
  while (valid() && std::isspace(get_char())) ++(*this);
  if (!valid()) return std::string();

  auto previous = m_offset;
  const auto start = previous;

  while ((++(*this)).valid()) {
    if (std::isspace(get_char()) || get_char() == ';' ||
        m_offset - previous > 1)
      break;
    previous = m_offset;
  }
  return m_s.substr(start, previous + 1 - start);
}

}  // namespace utils
}  // namespace mysqlshdk
