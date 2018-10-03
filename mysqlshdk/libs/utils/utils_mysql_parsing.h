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
#ifndef _UTILS_MYSQL_PARSING_H_
#define _UTILS_MYSQL_PARSING_H_

#include <functional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace mysqlshdk {
namespace utils {

class Sql_splitter {
 public:
  // callback(cmdstr, length, single_line, line_num) ->
  //                    (real_length, is_delimiter)
  using Command_callback = std::function<std::pair<size_t, bool>(
      const char *, size_t, bool, size_t)>;

  using Error_callback = std::function<void(const std::string &)>;

  Sql_splitter(const Command_callback &cmd_callback,
               const Error_callback &err_callback)
      : m_cmd_callback(cmd_callback), m_err_callback(err_callback) {
    reset();
  }

  void reset();

  void feed_chunk(char *buffer, size_t size);
  void feed(char *buffer, size_t size);

  void set_ansi_quotes(bool flag);

  bool set_delimiter(const std::string &delim);
  const std::string &delimiter() const { return m_delimiter; }

  struct Range {
    size_t offset;
    size_t length;
    size_t line_num;
  };

  bool next_range(Range *out_range, std::string *out_delim);

  void pack_buffer(std::string *buffer, Range last_range);

  size_t chunk_size() const { return m_end - m_begin; }

  bool eof() const { return m_last_chunk && (m_ptr >= m_end || m_eof); }

  bool is_last_chunk() const { return m_last_chunk; }

  enum Context {
    NONE,
    STATEMENT,          // any non-comments before delimiter
    COMMENT,            // /* ... */
    COMMENT_HINT,       // /*! ... */ ...; or /*+ ... */ ...;
    SQUOTE_STRING,      // '...'
    DQUOTE_STRING,      // "..." (only if not ansi_quotes)
    BQUOTE_IDENTIFIER,  // `...`
    DQUOTE_IDENTIFIER   // "..." (ansi_quotes)
  };

  Context context() const { return m_context; }

 private:
  char *m_begin;
  char *m_end;
  char *m_ptr;

  std::string m_delimiter = ";";
  Context m_context = NONE;

  Command_callback m_cmd_callback;
  Error_callback m_err_callback;

  size_t m_shrinked_bytes;
  size_t m_current_line;
  size_t m_total_offset;
  bool m_ansi_quotes;
  bool m_last_chunk;
  bool m_eof;
};

std::string to_string(Sql_splitter::Context context);

std::vector<std::tuple<std::string, std::string, size_t>> split_sql_stream(
    std::istream *stream, size_t chunk_size,
    const Sql_splitter::Error_callback &err_callback, bool ansi_quotes = false,
    std::string *delimiter = nullptr);

bool iterate_sql_stream(
    std::istream *stream, size_t chunk_size,
    const std::function<bool(const char *, size_t, const std::string &, size_t)>
        &stmt_callback,
    const Sql_splitter::Error_callback &err_callback, bool ansi_quotes = false,
    std::string *delimiter = nullptr);

}  // namespace utils
}  // namespace mysqlshdk

#endif
