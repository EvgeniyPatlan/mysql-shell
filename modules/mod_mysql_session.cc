/*
 * Copyright (c) 2014, 2019, Oracle and/or its affiliates. All rights reserved.
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

#include "modules/mod_mysql_session.h"

#include <set>
#include <string>
#include <thread>
#include <vector>

#include "scripting/lang_base.h"
#include "scripting/object_factory.h"
#include "shellcore/shell_core.h"
#include "shellcore/shell_notifications.h"

#include "modules/mysqlxtest_utils.h"
#include "scripting/proxy_object.h"

#include "modules/mod_mysql_resultset.h"
#include "modules/mod_utils.h"
#include "shellcore/utils_help.h"
#include "utils/utils_general.h"
#include "utils/utils_path.h"
#include "utils/utils_sqlstring.h"

using namespace std::placeholders;
using namespace mysqlsh;
using namespace mysqlsh::mysql;
using namespace shcore;

// Documentation for ClassicSession class
REGISTER_HELP_CLASS(ClassicSession, mysql);
REGISTER_HELP(CLASSICSESSION_GLOBAL_BRIEF,
              "Represents the currently "
              "open MySQL session.");
REGISTER_HELP(CLASSICSESSION_BRIEF,
              "Enables interaction with a MySQL Server "
              "using the MySQL Protocol.");
REGISTER_HELP(CLASSICSESSION_DETAIL, "Provides facilities to execute queries.");
ClassicSession::ClassicSession() { init(); }

ClassicSession::ClassicSession(const ClassicSession &session)
    : ShellBaseSession(session),
      std::enable_shared_from_this<mysqlsh::mysql::ClassicSession>(session),
      _session(session._session) {
  init();
}

ClassicSession::ClassicSession(
    std::shared_ptr<mysqlshdk::db::mysql::Session> session)
    : ShellBaseSession(), _session(session) {
  init();

  // TODO(alfredo) maybe can remove _connection_options ivar
  _connection_options = session->get_connection_options();
}

ClassicSession::~ClassicSession() {
  if (is_open()) close();
}

void ClassicSession::init() {
  //_schema_proxy.reset(new Proxy_object(std::bind(&ClassicSession::get_db,
  // this, _1)));

  add_property("uri", "getUri");

  add_method("close", std::bind(&ClassicSession::_close, this, _1), "data");
  add_method("runSql", std::bind(&ClassicSession::run_sql, this, _1), "stmt",
             shcore::String);
  add_method("query", std::bind(&ClassicSession::query, this, _1), "stmt",
             shcore::String);

  add_method("isOpen", std::bind(&ClassicSession::_is_open, this, _1));
  add_method("startTransaction",
             std::bind(&ClassicSession::_start_transaction, this, _1), "data");
  add_method("commit", std::bind(&ClassicSession::_commit, this, _1), "data");
  add_method("rollback", std::bind(&ClassicSession::_rollback, this, _1),
             "data");
}

void ClassicSession::connect(
    const mysqlshdk::db::Connection_options &connection_options) {
  _connection_options = connection_options;

  try {
    _session = mysqlshdk::db::mysql::Session::create();

    _session->connect(_connection_options);
  }
  CATCH_AND_TRANSLATE();
}

std::string ClassicSession::get_ssl_cipher() const {
  const char *cipher = _session ? _session->get_ssl_cipher() : nullptr;
  return cipher ? cipher : "";
}

// Documentation of close function
REGISTER_HELP_FUNCTION(close, ClassicSession);
REGISTER_HELP(CLASSICSESSION_CLOSE_BRIEF,
              "Closes the internal connection to the MySQL Server held on this "
              "session object.");

/**
 * $(CLASSICSESSION_CLOSE_BRIEF)
 */
#if DOXYGEN_JS
Undefined ClassicSession::close() {}
#elif DOXYGEN_PY
None ClassicSession::close() {}
#endif
void ClassicSession::close() {
  // Connection must be explicitly closed, we can't rely on the
  // automatic destruction because if shared across different objects
  // it may remain open
  if (_session) _session->close();

  _session.reset();
}

Value ClassicSession::_close(const shcore::Argument_list &args) {
  args.ensure_count(0, get_function_name("close").c_str());

  close();

  return shcore::Value();
}

REGISTER_HELP_FUNCTION(isOpen, ClassicSession);
REGISTER_HELP(CLASSICSESSION_ISOPEN_BRIEF,
              "Returns true if session is "
              "known to be open.");
REGISTER_HELP(CLASSICSESSION_ISOPEN_RETURNS,
              "@returns A boolean value "
              "indicating if the session is still open.");
REGISTER_HELP(
    CLASSICSESSION_ISOPEN_DETAIL,
    "Returns true if the session is "
    "still open and false otherwise. Note: may return true if connection "
    "is lost.");

shcore::Value ClassicSession::_is_open(const shcore::Argument_list &args) {
  args.ensure_count(0, get_function_name("isOpen").c_str());

  return shcore::Value(is_open());
}

shcore::Value ClassicSession::_run_sql(const std::string &function,
                                       const shcore::Argument_list &args) {
  args.ensure_count(1, 2, get_function_name(function).c_str());
  Value ret_val;

  try {
    auto query = args.string_at(0);
    shcore::Array_t values;
    if (args.size() > 1) values = args.array_at(1);

    // Will return the result of the SQL execution
    // In case of error will be Undefined
    if (!_session || !_session->is_open()) {
      throw Exception::logic_error("Not connected.");
    } else {
      Interruptible intr(this);
      ret_val = execute_sql(query, values);
    }
  }
  CATCH_AND_TRANSLATE_FUNCTION_EXCEPTION(get_function_name(function));

  return ret_val;
}

REGISTER_HELP_FUNCTION(runSql, ClassicSession);
REGISTER_HELP_FUNCTION_TEXT(CLASSICSESSION_RUNSQL, R"*(
Executes a query and returns the corresponding ClassicResult object.

@param query the SQL query to execute against the database.
@param args Optional list of literals to use when replacing ? placeholders in
the query string.

@returns A ClassicResult object.

@throw LogicError if there's no open session.
@throw ArgumentError if the parameters are invalid.

)*");
/**
 * $(CLASSICSESSION_RUNSQL_BRIEF)
 *
 * $(CLASSICSESSION_RUNSQL)
 */
#if DOXYGEN_JS
ClassicResult ClassicSession::runSql(String query, Array args) {}
#elif DOXYGEN_PY
ClassicResult ClassicSession::run_sql(str query, list args) {}
#endif
Value ClassicSession::run_sql(const shcore::Argument_list &args) {
  return _run_sql("runSql", args);
}

REGISTER_HELP_FUNCTION(query, ClassicSession);
REGISTER_HELP(CLASSICSESSION_QUERY_BRIEF,
              "Executes a query and returns the "
              "corresponding ClassicResult object.");

REGISTER_HELP(CLASSICSESSION_QUERY_PARAM,
              "@param query the SQL query string "
              "to execute, with optional ? placeholders");
REGISTER_HELP(
    CLASSICSESSION_QUERY_PARAM1,
    "@param args Optional list of "
    "literals to use when replacing ? placeholders in the query string.");
REGISTER_HELP(CLASSICSESSION_QUERY_RETURNS,
              "@returns A ClassicResult "
              "object.");
REGISTER_HELP(CLASSICSESSION_QUERY_EXCEPTION,
              "@exception An exception is "
              "thrown if an error occurs on the SQL execution.");

REGISTER_HELP(CLASSICSESSION_QUERY_DETAIL,
              "${CLASSICSESSION_QUERY_DEPRECATED}");

REGISTER_HELP(CLASSICSESSION_QUERY_DEPRECATED,
              "@attention This function will be removed in a future release, "
              "use the <b><<<runSql>>></b> function instead.");

//! $(CLASSICSESSION_QUERY_BRIEF)
#if DOXYGEN_CPP
//! \param args should contain the SQL query to execute against the database.
#else
//! $(CLASSICSESSION_QUERY_PARAM)
//! $(CLASSICSESSION_QUERY_PARAM1)
#endif
/**
 * $(CLASSICSESSION_QUERY_RETURNS)
 *
 * $(CLASSICSESSION_QUERY_EXCEPTION)
 *
 * $(CLASSICSESSION_QUERY_DEPRECATED)
 */
#if DOXYGEN_JS
ClassicResult ClassicSession::query(String query, Array args = []) {}
#elif DOXYGEN_PY
ClassicResult ClassicSession::query(str query, list args = []) {}
#endif
Value ClassicSession::query(const shcore::Argument_list &args) {
  auto console = mysqlsh::current_console();
  console->print_warning("'query' is deprecated, use " +
                         get_function_name("runSql") + " instead.");

  return _run_sql("query", args);
}

shcore::Object_bridge_ref ClassicSession::raw_execute_sql(
    const std::string &query) {
  return execute_sql(query, {}).as_object();
}

shcore::Value ClassicSession::execute_sql(const std::string &query,
                                          const shcore::Array_t &args) {
  Value ret_val;
  if (!_session || !_session->is_open()) {
    throw Exception::logic_error("Not connected.");
  } else {
    if (query.empty()) {
      throw Exception::argument_error("No query specified.");
    } else {
      Interruptible intr(this);
      try {
        ClassicResult *result;
        ret_val = Value::wrap(
            result = new ClassicResult(
                std::dynamic_pointer_cast<mysqlshdk::db::mysql::Result>(
                    _session->query(sub_query_placeholders(query, args)))));
      } catch (const mysqlshdk::db::Error &error) {
        throw shcore::Exception::mysql_error_with_code_and_state(
            error.what(), error.code(), error.sqlstate());
      }
    }
  }
  return ret_val;
}

// We need to hide this from doxygen to avoif warnings
#if !defined DOXYGEN_JS && !defined DOXYGEN_PY
std::shared_ptr<ClassicResult> ClassicSession::execute_sql(
    const std::string &query) {
  if (!_session || !_session->is_open()) {
    throw Exception::logic_error("Not connected.");
  } else {
    if (query.empty()) {
      throw Exception::argument_error("No query specified.");
    } else {
      auto result = std::shared_ptr<ClassicResult>(new ClassicResult(
          std::dynamic_pointer_cast<mysqlshdk::db::mysql::Result>(
              _session->query(query))));
      return result;
    }
  }
}
#endif

void ClassicSession::create_schema(const std::string &name) {
  if (!_session || !_session->is_open()) {
    throw Exception::logic_error("Not connected.");
  } else {
    // Options are the statement and optionally options to modify
    // How the resultset is created.

    if (name.empty()) {
      throw Exception::argument_error("The schema name can not be empty.");
    } else {
      std::string statement = sqlstring("create schema !", 0) << name;
      execute_sql(statement, shcore::Array_t());
    }
  }
}

// Documentation of getCurrentSchema function
REGISTER_HELP_PROPERTY(uri, ClassicSession);
REGISTER_HELP(CLASSICSESSION_URI_BRIEF,
              "Retrieves the URI for the current "
              "session.");
REGISTER_HELP_FUNCTION(getUri, ClassicSession);
REGISTER_HELP(CLASSICSESSION_GETURI_BRIEF,
              "Retrieves the URI for the current "
              "session.");
REGISTER_HELP(CLASSICSESSION_GETURI_RETURNS,
              "@return A string representing "
              "the connection data.");

/**
 * $(SHELLBASESESSION_GETURI_BRIEF)
 *
 * $(CLASSICSESSION_GETURI_RETURNS)
 */
#if DOXYGEN_JS
String ClassicSession::getUri() {}
#elif DOXYGEN_PY
str ClassicSession::get_uri() {}
#endif

Value ClassicSession::get_member(const std::string &prop) const {
  // Retrieves the member first from the parent
  Value ret_val;

  if (prop == "__connection_info") {
    // FIXME: temporary code until ISession refactoring
    const char *i = _session ? _session->get_connection_info() : nullptr;
    return Value(i ? i : "");
  }

  if (prop == "uri") {
    ret_val = shcore::Value(uri());
  } else {
    ret_val = ShellBaseSession::get_member(prop);
  }
  return ret_val;
}

std::string ClassicSession::get_current_schema() {
  std::string name;

  if (_session && _session->is_open()) {
    shcore::Argument_list query;
    query.push_back(Value("select schema()"));

    Value res = run_sql(query);

    std::shared_ptr<ClassicResult> rset = res.as_object<ClassicResult>();
    Value next_row = rset->fetch_one(shcore::Argument_list());

    if (next_row) {
      std::shared_ptr<mysqlsh::Row> row = next_row.as_object<mysqlsh::Row>();
      shcore::Value schema = row->get_member(0);

      if (schema) name = schema.get_string();
    }
  }

  return name;
}

void ClassicSession::set_current_schema(const std::string &name) {
  execute_sql(sqlstring("use !", 0) << name);
}

std::shared_ptr<shcore::Object_bridge> ClassicSession::create(
    const shcore::Argument_list &args) {
  std::shared_ptr<ClassicSession> session(new ClassicSession());

  auto connection_options =
      mysqlsh::get_connection_options(args, mysqlsh::PasswordFormat::STRING);

  // DevAPI getClassicSession uses ssl-mode = REQUIRED by default if no
  // ssl-ca or ssl-capath are specified
  if (!connection_options.get_ssl_options().has_mode() &&
      !connection_options.has_value(mysqlshdk::db::kSslCa) &&
      !connection_options.has_value(mysqlshdk::db::kSslCaPath)) {
    connection_options.get_ssl_options().set_mode(
        mysqlshdk::db::Ssl_mode::Required);
  }

  session->connect(connection_options);

  shcore::ShellNotifications::get()->notify("SN_SESSION_CONNECTED", session);

  return std::dynamic_pointer_cast<shcore::Object_bridge>(session);
}

void ClassicSession::drop_schema(const std::string &name) {
  execute_sql(sqlstring("drop schema !", 0) << name, shcore::Array_t());
}

/*
 * This function verifies if the given object exist in the database, works for
 * schemas, tables and views. The check for tables and views is done is done
 * based on the type. If type is not specified and an object with the name is
 * found, the type will be returned.
 */

std::string ClassicSession::db_object_exists(std::string &type,
                                             const std::string &name,
                                             const std::string &owner) {
  std::string statement;
  std::string ret_val;
  // match must be exact, since both branches below use LIKE and both escape
  // their arguments it's enough to just escape the wildcards
  std::string escaped_name = escape_wildcards(name);

  if (type == "Schema") {
    statement = sqlstring("show databases like ?", 0) << escaped_name;
    auto val_result = execute_sql(statement, shcore::Array_t());
    auto result = val_result.as_object<ClassicResult>();
    auto val_row = result->fetch_one(shcore::Argument_list());

    if (val_row) {
      auto row = val_row.as_object<mysqlsh::Row>();
      if (row) ret_val = row->get_member(0).get_string();
    }
  } else {
    statement = sqlstring("show full tables from ! like ?", 0)
                << owner << escaped_name;
    auto val_result = execute_sql(statement, shcore::Array_t());
    auto result = val_result.as_object<ClassicResult>();
    auto val_row = result->fetch_one(shcore::Argument_list());

    if (val_row) {
      auto row = val_row.as_object<mysqlsh::Row>();

      if (row) {
        std::string db_type = row->get_member(1).get_string();

        if (type == "Table" &&
            (db_type == "BASE TABLE" || db_type == "LOCAL TEMPORARY"))
          ret_val = row->get_member(0).get_string();
        else if (type == "View" &&
                 (db_type == "VIEW" || db_type == "SYSTEM VIEW"))
          ret_val = row->get_member(0).get_string();
        else if (type.empty()) {
          ret_val = row->get_member(0).get_string();
          type = db_type;
        }
      }
    }
  }

  return ret_val;
}

// Documentation of startTransaction function
REGISTER_HELP_FUNCTION(startTransaction, ClassicSession);
REGISTER_HELP(CLASSICSESSION_STARTTRANSACTION_BRIEF,
              "Starts a transaction context on the server.");
REGISTER_HELP(CLASSICSESSION_STARTTRANSACTION_RETURNS,
              "@returns A ClassicResult object.");
REGISTER_HELP(
    CLASSICSESSION_STARTTRANSACTION_DETAIL,
    "Calling this function will turn off the autocommit mode on the server.");
REGISTER_HELP(CLASSICSESSION_STARTTRANSACTION_DETAIL1,
              "All the operations executed after calling this function will "
              "take place only when commit() is called.");
REGISTER_HELP(CLASSICSESSION_STARTTRANSACTION_DETAIL2,
              "All the operations executed after calling this function, will "
              "be discarded is rollback() is called.");
REGISTER_HELP(CLASSICSESSION_STARTTRANSACTION_DETAIL3,
              "When commit() or rollback() are called, the server autocommit "
              "mode will return back to it's state before calling "
              "startTransaction().");

/**
 * $(CLASSICSESSION_STARTTRANSACTION_BRIEF)
 *
 * $(CLASSICSESSION_STARTTRANSACTION_RETURNS)
 *
 * $(CLASSICSESSION_STARTTRANSACTION_DETAIL)
 *
 * $(CLASSICSESSION_STARTTRANSACTION_DETAIL1)
 *
 * $(CLASSICSESSION_STARTTRANSACTION_DETAIL2)
 *
 * $(CLASSICSESSION_STARTTRANSACTION_DETAIL3)
 */
#if DOXYGEN_JS
ClassicResult ClassicSession::startTransaction() {}
#elif DOXYGEN_PY
ClassicResult ClassicSession::start_transaction() {}
#endif
shcore::Value ClassicSession::_start_transaction(
    const shcore::Argument_list &args) {
  args.ensure_count(0, get_function_name("startTransaction").c_str());

  return execute_sql("start transaction", shcore::Array_t());
}

// Documentation of commit function
REGISTER_HELP_FUNCTION(commit, ClassicSession);
REGISTER_HELP(
    CLASSICSESSION_COMMIT_BRIEF,
    "Commits all the operations executed after a call to startTransaction().");
REGISTER_HELP(CLASSICSESSION_COMMIT_RETURNS,
              "@returns A ClassicResult object.");
REGISTER_HELP(CLASSICSESSION_COMMIT_DETAIL,
              "All the operations executed after calling startTransaction() "
              "will take place when this function is called.");
REGISTER_HELP(CLASSICSESSION_COMMIT_DETAIL1,
              "The server autocommit mode will return back to it's state "
              "before calling startTransaction().");

/**
 * $(CLASSICSESSION_COMMIT_BRIEF)
 *
 * $(CLASSICSESSION_COMMIT_RETURNS)
 *
 * $(CLASSICSESSION_COMMIT_DETAIL)
 *
 * $(CLASSICSESSION_COMMIT_DETAIL1)
 */
#if DOXYGEN_JS
ClassicResult ClassicSession::commit() {}
#elif DOXYGEN_PY
ClassicResult ClassicSession::commit() {}
#endif
shcore::Value ClassicSession::_commit(const shcore::Argument_list &args) {
  args.ensure_count(0, get_function_name("commit").c_str());

  return execute_sql("commit", shcore::Array_t());
}

// Documentation of rollback function
REGISTER_HELP_FUNCTION(rollback, ClassicSession);
REGISTER_HELP(
    CLASSICSESSION_ROLLBACK_BRIEF,
    "Discards all the operations executed after a call to startTransaction().");
REGISTER_HELP(CLASSICSESSION_ROLLBACK_RETURNS,
              "@returns A ClassicResult object.");
REGISTER_HELP(CLASSICSESSION_ROLLBACK_DETAIL,
              "All the operations executed after calling startTransaction() "
              "will be discarded when this function is called.");
REGISTER_HELP(CLASSICSESSION_ROLLBACK_DETAIL1,
              "The server autocommit mode will return back to it's state "
              "before calling startTransaction().");

/**
 * $(CLASSICSESSION_ROLLBACK_BRIEF)
 *
 * $(CLASSICSESSION_ROLLBACK_RETURNS)
 *
 * $(CLASSICSESSION_ROLLBACK_DETAIL)
 *
 * $(CLASSICSESSION_ROLLBACK_DETAIL1)
 */
#if DOXYGEN_JS
ClassicResult ClassicSession::rollback() {}
#elif DOXYGEN_PY
ClassicResult ClassicSession::rollback() {}
#endif
shcore::Value ClassicSession::_rollback(const shcore::Argument_list &args) {
  args.ensure_count(0, get_function_name("rollback").c_str());

  return execute_sql("rollback", shcore::Array_t());
}

shcore::Value::Map_type_ref ClassicSession::get_status() {
  shcore::Value::Map_type_ref status(new shcore::Value::Map_type);

  try {
    auto result = _session->query("select DATABASE(), USER() limit 1");
    auto row = result->fetch_one();
    if (row) {
      (*status)["SESSION_TYPE"] = shcore::Value("Classic");
      (*status)["NODE_TYPE"] = shcore::Value(get_node_type());
      //        (*status)["DEFAULT_SCHEMA"] =
      //          shcore::Value(_connection_options.has_schema() ?
      //                        _connection_options.get_schema() : "");

      std::string current_schema;
      if (!row->is_null(0)) current_schema = row->get_string(0);

      (*status)["CURRENT_SCHEMA"] = shcore::Value(current_schema);
      (*status)["CURRENT_USER"] = shcore::Value(row->get_string(1));
      (*status)["CONNECTION_ID"] =
          shcore::Value(uint64_t(_session->get_connection_id()));

      //(*status)["SKIP_UPDATES"] = shcore::Value(???);

      (*status)["SERVER_INFO"] = shcore::Value(_session->get_server_info());

      (*status)["PROTOCOL_VERSION"] =
          shcore::Value(std::string("classic ") +
                        std::to_string(_session->get_protocol_info()));

      if (_session->is_compression_enabled())
        (*status)["COMPRESSION"] = shcore::Value(std::string("Enabled"));
      else
        (*status)["COMPRESSION"] = shcore::Value(std::string("Disabled"));

      (*status)["CONNECTION"] = shcore::Value(_session->get_connection_info());
      //(*status)["INSERT_ID"] = shcore::Value(???);
    }

    const char *cipher = _session->get_ssl_cipher();
    if (cipher != NULL) {
      result = _session->query("show session status like 'ssl_version';");
      row = result->fetch_one();
      std::string version;
      if (row) {
        version = " " + row->get_string(1);
      }
      (*status)["SSL_CIPHER"] = shcore::Value(cipher + version);
    }

    result = _session->query(
        "select @@character_set_client, @@character_set_connection, "
        "@@character_set_server, @@character_set_database, "
        "concat(@@version, \" \", @@version_comment) as version, "
        "@@socket, @@port, @@datadir "
        "limit 1");

    row = result->fetch_one();

    if (row) {
      (*status)["CLIENT_CHARSET"] = shcore::Value(row->get_string(0));
      (*status)["CONNECTION_CHARSET"] = shcore::Value(row->get_string(1));
      (*status)["SERVER_CHARSET"] = shcore::Value(row->get_string(2));
      (*status)["SCHEMA_CHARSET"] = shcore::Value(row->get_string(3));
      (*status)["SERVER_VERSION"] = shcore::Value(row->get_string(4));

      if (!_connection_options.has_transport_type() ||
          _connection_options.get_transport_type() ==
              mysqlshdk::db::Transport_type::Tcp) {
        (*status)["TCP_PORT"] = shcore::Value(row->get_int(6));
      } else if (_connection_options.get_transport_type() ==
                 mysqlshdk::db::Transport_type::Socket) {
        const std::string datadir = row->get_string(7);
        const std::string socket = row->get_string(5);
        const std::string socket_abs_path = shcore::path::normalize(
            shcore::path::join_path(std::vector<std::string>{datadir, socket}));
        (*status)["UNIX_SOCKET"] = shcore::Value(socket_abs_path);
      }

      unsigned long ver = mysql_get_client_version();
      std::stringstream sv;
      sv << ver / 10000 << "." << (ver % 10000) / 100 << "." << ver % 100;
      (*status)["CLIENT_LIBRARY"] = shcore::Value(sv.str());

      if (_session->get_stats())
        (*status)["SERVER_STATS"] = shcore::Value(_session->get_stats());

      try {
        if (_connection_options.get_transport_type() ==
            mysqlshdk::db::Transport_type::Tcp)
          (*status)["TCP_PORT"] = shcore::Value(_connection_options.get_port());
      } catch (...) {
      }
      //(*status)["PROTOCOL_COMPRESSED"] = row->get_value(3);

      // STATUS

      // SAFE UPDATES
    }
  } catch (const shcore::Exception &e) {
    (*status)["STATUS_ERROR"] = shcore::Value(e.format());
  }

  return status;
}

void ClassicSession::start_transaction() {
  if (_tx_deep == 0) execute_sql("start transaction", shcore::Array_t());

  _tx_deep++;
}

void ClassicSession::commit() {
  _tx_deep--;

  assert(_tx_deep >= 0);

  if (_tx_deep == 0) execute_sql("commit", shcore::Array_t());
}

void ClassicSession::rollback() {
  _tx_deep--;

  assert(_tx_deep >= 0);

  if (_tx_deep == 0) execute_sql("rollback", shcore::Array_t());
}

uint64_t ClassicSession::get_connection_id() const {
  return _session->get_connection_id();
}

bool ClassicSession::is_open() const { return _session && _session->is_open(); }

std::string ClassicSession::query_one_string(const std::string &query,
                                             int field) {
  shcore::Value val_result = execute_sql(query, shcore::Array_t());
  auto result = val_result.as_object<mysql::ClassicResult>();
  shcore::Value val_row = result->fetch_one(shcore::Argument_list());
  if (val_row) {
    auto row = val_row.as_object<mysqlsh::Row>();
    if (row) {
      return row->get_member(field).get_string();
    }
  }
  return "";
}

void ClassicSession::kill_query() {
  uint64_t cid = get_connection_id();
  try {
    auto kill_session = mysqlshdk::db::mysql::Session::create();

    kill_session->connect(_session->get_connection_options());

    kill_session->execute("kill query " + std::to_string(cid));

    kill_session->close();
  } catch (const std::exception &e) {
    log_warning("Error cancelling SQL query: %s", e.what());
  }
}
