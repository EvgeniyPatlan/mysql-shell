/*
 * Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include <stdlib.h>
#include <iostream>
#include "shellcore/ishell_core.h"
#include "shell_cmdline_options.h"
#include "utils/utils_general.h"
#include "utils/utils_connection.h"
#include "mysqlshdk/libs/db/ssl_info.h"
#include "utils/utils_string.h"

using namespace shcore;

Shell_command_line_options::Shell_command_line_options(int argc, char **argv)
  : Command_line_options(argc, argv) {
  int arg_format = 0;
  for (int i = 1; i < argc && exit_code == 0; i++) {
    char *value;
    if (check_arg_with_value(argv, i, "--file", "-f", value)) {
      _options.run_file = value;
      // the rest of the cmdline options, starting from here are all passed
      // through to the script
      _options.script_argv.push_back(value);
      for (++i; i < argc; i++) {
        _options.script_argv.push_back(argv[i]);
      }
      break;
    } else if ((arg_format = check_arg_with_value(argv, i, "--uri", NULL, value))) {
      if (shcore::validate_uri(value)) {
        _options.uri = value;

        shcore::Value::Map_type_ref data = shcore::get_connection_data(value, false);

        if (data->has_key("dbPassword")) {
          std::string pwd(data->get_string("dbPassword").length(), '*');
          (*data)["dbPassword"] = shcore::Value(pwd);

          // Required replacement when --uri <value>
          auto nopwd_uri = shcore::build_connection_string(data, true);

          // Required replacement when --uri=<value>
          if (arg_format == 3)
            nopwd_uri = "--uri=" + nopwd_uri;
           strcpy(argv[i], nopwd_uri.substr(0, _options.uri.length()).c_str());
        }
      } else {
        std::cerr << "Invalid value specified in --uri parameter.\n";
        exit_code = 1;
        break;
      }
    }
    else if (check_arg_with_value(argv, i, "--host", "-h", value))
      _options.host = value;
    else if (check_arg_with_value(argv, i, "--dbuser", "-u", value))
      _options.user = value;
    else if (check_arg_with_value(argv, i, "--user", NULL, value))
      _options.user = value;
    else if (check_arg_with_value(argv, i, "--port", "-P", value))
      _options.port = atoi(value);
    else if (check_arg_with_value(argv, i, "--socket", "-S", value))
      _options.sock = value;
    else if (check_arg_with_value(argv, i, "--schema", "-D", value))
      _options.schema = value;
    else if (check_arg_with_value(argv, i, "--database", NULL, value))
      _options.schema = value;
    else if (check_arg(argv, i, "--recreate-schema", NULL))
      _options.recreate_database = true;
    else if (check_arg_with_value(argv, i, "--execute", "-e", value))
      _options.execute_statement = value;
    else if (check_arg_with_value(argv, i, "--dba", NULL, value))
      _options.execute_dba_statement = value;
    else if ((arg_format = check_arg_with_value(argv, i, "--dbpassword", NULL, value, true))) {
      // Note that in any connection attempt, password prompt will be done if the password is missing.
      // The behavior of the password cmd line argument is as follows:

      // ARGUMENT           EFFECT
      // --password         forces password prompt no matter it was already provided
      // --password value   forces password prompt no matter it was already provided (value is not taken as password)
      // --password=        sets password to empty (password is available but empty so it will not be prompted)
      // -p<value> sets the password to <value>
      // --password=<value> sets the password to <value>

      if (!value) {
        // --password=
        if (arg_format == 3)
          _options.password = _options.pwd.c_str();

        // --password
        else
          _options.prompt_password = true;
      }
      // --password=value || --pvalue
      else if (arg_format != 1) {
        _options.pwd.assign(value);
        _options.password = _options.pwd.c_str();

        std::string stars(_options.pwd.length(), '*');
        std::string pwd = arg_format == 2 ? "-p" : "--dbpassword=";
        pwd.append(stars);

        strcpy(argv[i], pwd.c_str());
      }

      // --password value (value is ignored)
      else {
        _options.prompt_password = true;
        i--;
      }
    } else if ((arg_format = check_arg_with_value(argv, i, "--password", "-p", value, true))) {
      // Note that in any connection attempt, password prompt will be done if the password is missing.
      // The behavior of the password cmd line argument is as follows:

      // ARGUMENT           EFFECT
      // --password         forces password prompt no matter it was already provided
      // --password value   forces password prompt no matter it was already provided (value is not taken as password)
      // --password=        sets password to empty (password is available but empty so it will not be prompted)
      // -p<value> sets the password to <value>
      // --password=<value> sets the password to <value>

      if (!value) {
        // --password=
        if (arg_format == 3)
          _options.password = _options.pwd.c_str();

        // --password
        else
          _options.prompt_password = true;
      }
      // --password=value || --pvalue
      else if (arg_format != 1) {
        _options.pwd.assign(value);
        _options.password = _options.pwd.c_str();

        std::string stars(_options.pwd.length(), '*');
        std::string pwd = arg_format == 2 ? "-p" : "--password=";
        pwd.append(stars);

        strcpy(argv[i], pwd.c_str());
      }

      // --password value (value is ignored)
      else {
        _options.prompt_password = true;
        i--;
      }
    } else if (check_arg_with_value(argv, i, "--auth-method", NULL, value))
      _options.auth_method = value;
    else if (check_arg_with_value(argv, i, "--ssl-ca", NULL, value)) {
      _options.ssl_info.ca = value;
      _options.ssl_info.skip = false;
    } else if (check_arg_with_value(argv, i, "--ssl-cert", NULL, value)) {
      _options.ssl_info.cert = value;
      _options.ssl_info.skip = false;
    } else if (check_arg_with_value(argv, i, "--ssl-key", NULL, value)) {
      _options.ssl_info.key = value;
      _options.ssl_info.skip = false;
    } else if (check_arg_with_value(argv, i, "--ssl-capath", NULL, value)) {
      _options.ssl_info.capath = value;
      _options.ssl_info.skip = false;
    } else if (check_arg_with_value(argv, i, "--ssl-crl", NULL, value)) {
      _options.ssl_info.crl = value;
      _options.ssl_info.skip = false;
    } else if (check_arg_with_value(argv, i, "--ssl-crlpath", NULL, value)) {
      _options.ssl_info.crlpath = value;
      _options.ssl_info.skip = false;
    } else if (check_arg_with_value(argv, i, "--ssl-cipher", NULL, value)) {
      _options.ssl_info.ciphers = value;
      _options.ssl_info.skip = false;
    } else if (check_arg_with_value(argv, i, "--tls-version", NULL, value)) {
      _options.ssl_info.tls_version = value;
_options.ssl_info.skip = false;
    } else if (check_arg_with_value(argv, i, "--ssl-mode", NULL, value)) {
      int mode = shcore::MapSslModeNameToValue::get_value(value);
      if (mode == 0)
      {
        std::cerr << "must be any any of [DISABLED, PREFERRED, REQUIRED, VERIFY_CA, VERIFY_IDENTITY]";
        exit_code = 1;
        break;
      }
      _options.ssl_info.mode = mode;
      _options.ssl_info.skip = false;
    } else if (check_arg_with_value(argv, i, "--ssl", NULL, value, true)) {
      if (!value)
        _options.ssl_info.skip = false;
      else {
        if (str_caseeq(value, "yes") || str_caseeq(value, "1"))
          _options.ssl_info.skip = false;
        else if (str_caseeq(value, "no") || str_caseeq(value, "0"))
          _options.ssl_info.skip = true;
        else {
          std::cerr << "Value for --ssl must be any of 1|0|yes|no";
          exit_code = 1;
          break;
        }
      }
    }
    else if (check_arg(argv, i, "--node", "--node"))
      override_session_type(mysqlsh::SessionType::Node, "--node");
    else if (check_arg(argv, i, "--classic", "--classic"))
      override_session_type(mysqlsh::SessionType::Classic, "--classic");
    else if (check_arg(argv, i, "--sql", "--sql")) {
      _options.initial_mode = shcore::IShell_core::Mode::SQL;
    } else if (check_arg(argv, i, "--js", "--javascript")) {
#ifdef HAVE_V8
      _options.initial_mode = shcore::IShell_core::Mode::JavaScript;
#else
      std::cerr << "JavaScript is not supported.\n";
      exit_code = 1;
      break;
#endif
    } else if (check_arg(argv, i, "--py", "--python")) {
#ifdef HAVE_PYTHON
      _options.initial_mode = shcore::IShell_core::Mode::Python;
#else
      std::cerr << "Python is not supported.\n";
      exit_code = 1;
      break;
#endif
    } else if (check_arg(argv, i, NULL, "--sqlc")) {
      _options.initial_mode = shcore::IShell_core::Mode::SQL;
      override_session_type(mysqlsh::SessionType::Classic, "--sqlc");
    } else if (check_arg(argv, i, NULL, "--sqln")) {
      _options.initial_mode = shcore::IShell_core::Mode::SQL;
      override_session_type(mysqlsh::SessionType::Node, "--sqln");
    } else if (check_arg_with_value(argv, i, "--json", NULL, value, true)) {
      if (!value || strcmp(value, "pretty") == 0)
        _options.output_format = "json";
      else if (strcmp(value, "raw") == 0)
        _options.output_format = "json/raw";
      else {
        std::cerr << "Value for --json must be either pretty or raw.\n";
        exit_code = 1;
        break;
      }
    } else if (check_arg(argv, i, "--table", "--table"))
      _options.output_format = "table";
    else if (check_arg(argv, i, "--trace-proto", NULL))
      _options.trace_protocol = true;
    else if (check_arg(argv, i, "--help", "-?")) {
      _options.print_cmd_line_helper = true;
      exit_code = 0;
    } else if (check_arg(argv, i, nullptr, "-VV")) {
      _options.print_version = true;
      _options.print_version_extra = true;
      exit_code = 0;
    } else if (check_arg(argv, i, "--version", "-V")) {
      _options.print_version = true;
      exit_code = 0;
    } else if (check_arg(argv, i, "--force", "--force"))
      _options.force = true;
    else if (check_arg(argv, i, "--no-wizard", "--nw"))
      _options.wizards = false;
    else if (check_arg_with_value(argv, i, "--interactive", "-i", value, true)) {
      if (!value) {
        _options.interactive = true;
        _options.full_interactive = false;
      } else if (strcmp(value, "full") == 0) {
        _options.interactive = true;
        _options.full_interactive = true;
      } else {
        std::cerr << "Value for --interactive if any, must be full\n";
        exit_code = 1;
        break;
      }
    } else if (check_arg(argv, i, NULL, "--passwords-from-stdin"))
      _options.passwords_from_stdin = true;
    else if (check_arg_with_value(argv, i, "--log-level", NULL, value)) {
      ngcommon::Logger::LOG_LEVEL nlog_level;
      if (*value == '@') {
        _options.log_to_stderr = true;
        value++;
      }
      nlog_level = ngcommon::Logger::get_log_level(value);
      if (nlog_level == ngcommon::Logger::LOG_NONE && !ngcommon::Logger::is_level_none(value)) {
        std::cerr << ngcommon::Logger::get_level_range_info() << std::endl;
        exit_code = 1;
        break;
      } else
        _options.log_level = nlog_level;
    } else if (check_arg(argv, i, "--vertical", "-E")) {
      _options.output_format = "vertical";
    } else if (exit_code == 0) {
      if (argv[i][0] != '-') {
          value = argv[i];
        if (shcore::validate_uri(value)) {
          _options.uri = value;
          shcore::Value::Map_type_ref data = shcore::get_connection_data(value, false);
          if (data->has_key("dbPassword")) {
            std::string pwd(data->get_string("dbPassword").length(), '*');
            (*data)["dbPassword"] = shcore::Value(pwd);

            // Hide password being used.
            auto nopwd_uri = shcore::build_connection_string(data, true);
            strcpy(argv[i], nopwd_uri.substr(0, _options.uri.length()).c_str());
          }
        } else {
          std::cerr << "Invalid uri parameter.\n";
          exit_code = 1;
          break;
        }
      }
      else {
        std::cerr << argv[0] << ": unknown option " << argv[i] << "\n";
        exit_code = 1;
        break;
      }
    }
  }

  _options.exit_code = exit_code;
}

void Shell_command_line_options::override_session_type(mysqlsh::SessionType new_type, const std::string& option, char* value) {
  auto get_session_type = [](mysqlsh::SessionType type) {
    std::string label;
    switch (type) {
      case mysqlsh::SessionType::X:
        label = "X";
        break;
      case mysqlsh::SessionType::Node:
        label = "Node";
        break;
      case mysqlsh::SessionType::Classic:
        label = "Classic";
        break;
      case mysqlsh::SessionType::Auto:
        break;
    }

    return label;
  };

  if (new_type != _options.session_type) {
    if (!_options.default_session_type) {
      std::string msg = "Session type already configured to ";
      msg.append(get_session_type(_options.session_type));
      msg.append(", unable to change to ");
      msg.append(get_session_type(new_type));
      msg.append(" with option ");
      msg.append(option);

      if (value) {
        msg.append("=");
        msg.append(value);
      }

      std::cerr << msg << std::endl;
      exit_code = 1;
    }

    _options.session_type = new_type;
  }

  _options.default_session_type = false;
}

std::vector<std::string> Shell_command_line_options::get_details() {
  const std::vector<std::string> details = {
  "  --help                   Display this help and exit.",
  "  -f, --file=file          Process file.",
  "  -e, --execute=<cmd>      Execute command and quit.",
  "  --uri                    Connect to Uniform Resource Identifier.",
  "                           Format: [user[:pass]]@host[:port][/db]",
  "                           or user[:pass]@::socket[/db] .",
  "  -h, --host=name          Connect to host.",
  "  -P, --port=#             Port number to use for connection.",
  "  -S, --socket=sock        Socket name to use in UNIX, pipe name to use in"
  "                           Windows (only classic sessions).",
  "  -u, --dbuser=name        User for the connection to the server.",
  "  --user=name              An alias for dbuser.",
  "  --dbpassword=name        Password to use when connecting to server",
  "  -p, --password=name      An alias for dbpassword.",
  "  -p                       Request password prompt to set the password",
  "  -D, --schema=name        Schema to use.",
  "  --recreate-schema        Drop and recreate the specified schema.",
  "                           Schema will be deleted if it exists!",
  "  --database=name          An alias for --schema.",
  "  --node                   Uses connection data to create a Node Session.",
  "  --classic                Uses connection data to create a Classic Session.",
  "  --sql                    Start in SQL mode.",
  "  --sqlc                   Start in SQL mode using a classic session.",
  "  --sqln                   Start in SQL mode using a node session.",
  "  --js, --javascript       Start in JavaScript mode.",
  "  --py, --python           Start in Python mode.",
  "  --json[=format]          Produce output in JSON format, allowed values:",
  "                           raw, pretty. If no format is specified",
  "                           pretty format is produced.",
  "  --table                  Produce output in table format (default for",
  "                           interactive mode). This option can be used to",
  "                           force that format when running in batch mode.",
  "  -E, --vertical           Print the output of a query (rows) vertically.",
  "  -i, --interactive[=full] To use in batch mode, it forces emulation of",
  "                           interactive mode processing. Each line on the ",
  "                           batch is processed as if it were in ",
  "                           interactive mode.",
  "  --force                  To use in SQL batch mode, forces processing to",
  "                           continue if an error is found.",
  "  --log-level=value        The log level." +
  ngcommon::Logger::get_level_range_info(),
  "  -V, --version            Prints the version of MySQL Shell.",
  "  --ssl                    Enable SSL for connection (automatically enabled",
  "                           with other flags).",
  "  --ssl-key=name           X509 key in PEM format.",
  "  --ssl-cert=name          X509 cert in PEM format.",
  "  --ssl-ca=name            CA file in PEM format.",
  "  --ssl-capath=dir         CA directory.",
  "  --ssl-cipher=name        SSL Cipher to use.",
  "  --ssl-crl=name           Certificate revocation list.",
  "  --ssl-crlpath=dir        Certificate revocation list path.",
  "  --ssl-mode=mode          SSL mode to use, allowed values: DISABLED,",
  "                           PREFERRED, REQUIRED, VERIFY_CA, VERIFY_IDENTITY.",
  "  --tls-version=version    TLS version to use, permitted values are :"
  "                           TLSv1, TLSv1.1.",
  "  --passwords-from-stdin   Read passwords from stdin instead of the tty.",
  "  --auth-method=method     Authentication method to use.",
  "  --show-warnings          Automatically display SQL warnings on SQL mode"
  "                           if available.",
  "  --dba enableXProtocol    Enable the X Protocol in the server connected to."
  "                           Must be used with --classic.",
  "  --nw, --no-wizard        Disables wizard mode."
  };
  return details;
}

