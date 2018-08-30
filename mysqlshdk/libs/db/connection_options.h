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

#ifndef MYSQLSHDK_LIBS_DB_CONNECTION_OPTIONS_H_
#define MYSQLSHDK_LIBS_DB_CONNECTION_OPTIONS_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "mysqlshdk/include/mysqlshdk_export.h"
#include "mysqlshdk/include/shellcore/ishell_core.h"
#include "mysqlshdk/libs/db/ssl_options.h"
#include "mysqlshdk/libs/db/uri_common.h"
#include "mysqlshdk/libs/utils/nullable_options.h"

namespace mysqlsh {
enum class SessionType { Auto, X, Classic };
};

namespace mysqlshdk {
namespace db {
using utils::Nullable_options;
using utils::nullable;
using utils::nullable_options::Comparison_mode;
enum Transport_type { Tcp, Socket, Pipe };
std::string to_string(Transport_type type);

constexpr int k_default_mysql_port = 3306;
constexpr int k_default_mysql_x_port = 33060;
constexpr int k_default_connect_timeout = 10000;

class SHCORE_PUBLIC Connection_options
    : public mysqlshdk::utils::Nullable_options {
 public:
  explicit Connection_options(
      Comparison_mode mode = Comparison_mode::CASE_INSENSITIVE);
  Connection_options(const std::string &uri,
                     Comparison_mode mode = Comparison_mode::CASE_INSENSITIVE);

  void set_login_options_from(const Connection_options &options);
  void set_ssl_connection_options_from(const Ssl_options &options);
  void set_ssl_options(const Ssl_options &options);

  const std::string &get_scheme() const { return get_value(kScheme); }
  const std::string &get_user() const { return get_value(kUser); }
  const std::string &get_password() const { return get_value(kPassword); }
  const std::string &get_host() const { return get_value(kHost); }
  const std::string &get_schema() const { return get_value(kSchema); }
  const std::string &get_socket() const { return get_value(kSocket); }
  const std::string &get_pipe() const { return get_value(kSocket); }
  int get_port() const;
  Transport_type get_transport_type() const;

  const std::string &get(const std::string &name) const;

  const Ssl_options &get_ssl_options() const { return _ssl_options; }
  Ssl_options &get_ssl_options() { return _ssl_options; }

  bool has_data() const;
  bool has_scheme() const { return has_value(kScheme); }
  bool has_user() const { return has_value(kUser); }
  bool has_password() const { return has_value(kPassword); }
  bool has_host() const { return has_value(kHost); }
  bool has_port() const { return !_port.is_null(); }
  bool has_schema() const { return has_value(kSchema); }
  bool has_socket() const { return has_value(kSocket); }
  bool has_pipe() const { return has_value(kSocket); }
  bool has_transport_type() const { return !_transport_type.is_null(); }

  bool has(const std::string &name) const;
  bool has_value(const std::string &name) const;

  void set_scheme(const std::string &scheme) { _set_fixed(kScheme, scheme); }
  void set_user(const std::string &user) { _set_fixed(kUser, user); }
  void set_password(const std::string &pwd) { _set_fixed(kPassword, pwd); }
  void set_host(const std::string &host);
  void set_port(int port);
  void set_schema(const std::string &schema) { _set_fixed(kSchema, schema); }
  void set_socket(const std::string &socket);
  void set_pipe(const std::string &pipe);

  void set(const std::string &attribute,
           const std::vector<std::string> &values);

  void clear_scheme() { clear_value(kScheme); }
  void clear_user() { clear_value(kUser); }
  void clear_password() { clear_value(kPassword); }
  void clear_host();
  void clear_port();
  void clear_schema() { clear_value(kSchema); }
  void clear_socket();
  void clear_pipe();

  void remove(const std::string &name);

  bool operator==(const Connection_options &other) const;
  bool operator!=(const Connection_options &other) const;

  std::string as_uri(
      uri::Tokens_mask format = uri::formats::full_no_password()) const;

  std::string uri_endpoint() const {
    return as_uri(uri::formats::only_transport());
  }

  const Nullable_options &get_extra_options() const { return _extra_options; }

  mysqlsh::SessionType get_session_type() const;

  /**
   * Sets the default connection data, if they're missing:
   * - uses system user if no user is specified,
   * - uses localhost if host is not specified and transport type is not
   *   specified or it's TCP.
   */
  void set_default_connection_data();

  static void throw_invalid_connect_timeout(const std::string &value);

 private:
  void _set_fixed(const std::string &key, const std::string &val);
  void _clear_fixed(const std::string &key);
  std::string get_iname(const std::string &name) const;

  void raise_connection_type_error(const std::string &source);

  nullable<int> _port;
  nullable<Transport_type> _transport_type;

  Ssl_options _ssl_options;
  Nullable_options _extra_options;
};

}  // namespace db
}  // namespace mysqlshdk

#endif  // MYSQLSHDK_LIBS_DB_CONNECTION_OPTIONS_H_
