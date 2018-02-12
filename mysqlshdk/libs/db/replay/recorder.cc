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

#include "mysqlshdk/libs/db/replay/recorder.h"

#include <fstream>
#include <map>
#include <memory>
#include <string>

#include "mysqlshdk/libs/db/mysql/session.h"
#include "mysqlshdk/libs/db/mysqlx/session.h"
#include "mysqlshdk/libs/db/replay/setup.h"
#include "mysqlshdk/libs/db/session.h"
#include "mysqlshdk/libs/utils/utils_string.h"
#include "mysqlshdk/libs/utils/utils_stacktrace.h"

namespace mysqlshdk {
namespace db {
namespace replay {

std::function<void(std::shared_ptr<mysqlshdk::db::ISession>)>
    on_recorder_connect_hook;

std::function<void(std::shared_ptr<mysqlshdk::db::ISession>)>
    on_recorder_close_hook;

Recorder_mysql::Recorder_mysql(int print_traces) : _print_traces(print_traces) {
}

void Recorder_mysql::connect(const mysqlshdk::db::Connection_options& data) {
  _trace.reset(
      Trace_writer::create(new_recording_path("mysql_trace"), _print_traces));

  try {
    if (data.has_port())
      _port = data.get_port();
    _trace->serialize_connect(data, "classic");
    super::connect(data);
    std::map<std::string, std::string> info;
    if (const char *s = get_ssl_cipher())
      info["ssl_cipher"] = s;
    if (const char *s = get_connection_info())
      info["connection_info"] = s;
    if (const char *s = get_server_info())
      info["server_info"] = s;
    info["server_version"] = mysql::Session::get_server_version().get_full();
    info["connection_id"] = std::to_string(get_connection_id());
    _trace->serialize_connect_ok(info);

    if (on_recorder_connect_hook)
      on_recorder_connect_hook(shared_from_this());
  } catch (db::Error& e) {
    _trace->serialize_error(e);
    throw;
  }
}

std::shared_ptr<IResult> Recorder_mysql::query(const std::string& sql, bool) {
  try {
    // todo - add synchronization points for error.log on every query
    // assuming that error log contents change when a query is executed
    // if (set_log_save_point) set_log_save_point(_port);

    _trace->serialize_query(sql);
    // Always buffer to make row serialization easier
    std::shared_ptr<IResult> result(super::query(sql, true));
    _trace->serialize_result(result);
    std::dynamic_pointer_cast<mysql::Result>(result)->rewind();
    return result;
  } catch (db::Error& e) {
    _trace->serialize_error(e);
    throw;
  }
}

void Recorder_mysql::execute(const std::string& sql) {
  query(sql, true);
}

void Recorder_mysql::close() {
  try {
    if (_trace && !_closed) {
      _closed = true;
      _trace->serialize_close();
      super::close();
      _trace->serialize_ok();
      _trace.reset();

      if (on_recorder_close_hook)
        on_recorder_close_hook(shared_from_this());
    } else {
      super::close();
    }
  } catch (db::Error& e) {
    if (_trace) {
      _trace->serialize_error(e);
      _trace.reset();
      if (on_recorder_close_hook)
        on_recorder_close_hook(shared_from_this());
    }
    throw;
  }
}

// ---

Recorder_mysqlx::Recorder_mysqlx(int print_traces)
    : _print_traces(print_traces) {
}

void Recorder_mysqlx::connect(const mysqlshdk::db::Connection_options& data) {
  _trace.reset(
      Trace_writer::create(new_recording_path("mysqlx_trace"), _print_traces));
  try {
    if (data.has_port())
      _port = data.get_port();
    _trace->serialize_connect(data, "x");
    super::connect(data);
    std::map<std::string, std::string> info;
    if (const char *s = get_ssl_cipher())
      info["ssl_cipher"] = s;
    info["connection_info"] = get_connection_info();
    info["connection_id"] = std::to_string(get_connection_id());
    info["server_version"] = get_server_version().get_full();
    _trace->serialize_connect_ok(info);

    if (on_recorder_connect_hook)
      on_recorder_connect_hook(shared_from_this());
  } catch (db::Error& e) {
    _trace->serialize_error(e);
    throw;
  }
}

std::shared_ptr<IResult> Recorder_mysqlx::query(const std::string& sql, bool) {
  try {
    // todo - add synchronization points for error.log on every query
    // assuming that error log contents change when a query is executed
    // if (set_log_save_point) set_log_save_point(_port);

    _trace->serialize_query(sql);
    // Always buffer to make row serialization easier
    std::shared_ptr<IResult> result(super::query(sql, true));
    _trace->serialize_result(result);
    std::dynamic_pointer_cast<mysqlx::Result>(result)->rewind();
    return result;
  } catch (db::Error& e) {
    _trace->serialize_error(e);
    throw;
  }
}

std::shared_ptr<IResult> Recorder_mysqlx::execute_stmt(
    const std::string &ns, const std::string &stmt,
    const ::xcl::Arguments &args) {
  if (ns != "sql" || !args.empty())
    throw std::logic_error("not implemented");
  return query(stmt, true);
}

void Recorder_mysqlx::execute(const std::string& sql) {
  query(sql, true);
}

void Recorder_mysqlx::close() {
  try {
    if (_trace && !_closed) {
      _closed = true;
      _trace->serialize_close();
      super::close();
      _trace->serialize_ok();
      _trace.reset();

      if (on_recorder_close_hook)
        on_recorder_close_hook(shared_from_this());
    } else {
      super::close();
    }
  } catch (db::Error& e) {
    if (_trace) {
      _trace->serialize_error(e);
      _trace.reset();
      if (on_recorder_close_hook)
        on_recorder_close_hook(shared_from_this());
    }
    throw;
  }
}

}  // namespace replay
}  // namespace db
}  // namespace mysqlshdk
