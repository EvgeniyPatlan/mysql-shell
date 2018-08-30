/*
 * Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.
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

#include <cstring>
#include <string>
#include <system_error>
#include <vector>

#include "modules/adminapi/mod_dba_provisioning_interface.h"
#include "modules/mod_utils.h"
#include "mysqlshdk/include/shellcore/base_shell.h"
#include "mysqlshdk/include/shellcore/console.h"
#include "mysqlshdk/libs/db/replay/setup.h"
#include "mysqlshdk/libs/utils/process_launcher.h"
#include "mysqlshdk/libs/utils/utils_file.h"
#include "mysqlshdk/libs/utils/utils_general.h"
#include "mysqlshdk/libs/utils/utils_net.h"
#include "mysqlshdk/libs/utils/utils_string.h"
#include "shellcore/base_session.h"
#include "shellcore/interrupt_handler.h"

extern const char *g_mysqlsh_path;

using namespace mysqlsh;
using namespace mysqlsh::dba;
using namespace shcore;
using mysqlshdk::db::uri::formats::user_transport;

namespace {
void setup_recorder_environment(const std::string &cmd) {
  static char mode[512];
  static char prefix[512];

  snprintf(mode, sizeof(mode), "MYSQLSH_RECORDER_MODE=");
  snprintf(prefix, sizeof(prefix), "MYSQLSH_RECORDER_PREFIX=");

  // If session recording is wanted, we need to append a mysqlprovision specific
  // suffix to the output path, which also has to be different for each call
  if (mysqlshdk::db::replay::g_replay_mode !=
      mysqlshdk::db::replay::Mode::Direct) {
    if (cmd != "sandbox") {  // don't record sandbox ops
      if (mysqlshdk::db::replay::g_replay_mode ==
          mysqlshdk::db::replay::Mode::Record) {
        snprintf(mode, sizeof(mode), "MYSQLSH_RECORDER_MODE=record");
      } else {
        snprintf(mode, sizeof(mode), "MYSQLSH_RECORDER_MODE=replay");
      }
      snprintf(prefix, sizeof(prefix), "MYSQLSH_RECORDER_PREFIX=%s_%s",
               mysqlshdk::db::replay::external_recording_path("mp").c_str(),
               cmd.c_str());
    }
  }
  putenv(mode);
  putenv(prefix);
}

shcore::Value value_from_argmap(const shcore::Argument_map &argmap) {
  shcore::Value map(shcore::Value::new_map());
  *map.as_map() = argmap.as_map();
  return map;
}

}  // namespace

ProvisioningInterface::ProvisioningInterface(const std::string &provision_path)
    : _verbose(0), _local_mysqlprovision_path(provision_path) {}

ProvisioningInterface::~ProvisioningInterface() {}

int ProvisioningInterface::execute_mysqlprovision(
    const std::string &cmd, const shcore::Argument_list &args,
    const shcore::Argument_map &kwargs, shcore::Value::Array_type_ref *errors,
    int verbose) {
  std::vector<const char *> args_script;
  std::string buf;
  char c;
  int exit_code = 0;
  std::string full_output;

  // suppress ^C propagation, mp should handle ^C itself and signal us about it
  shcore::Interrupt_handler intr([]() {
    // don't propagate up the ^C
    return false;
  });

  std::string log_level = "--log-level=";
  if (mysqlsh::current_shell_options()->get().log_to_stderr)
    log_level.append("@");
  log_level.append(std::to_string(
      static_cast<int>(ngcommon::Logger::singleton()->get_log_level())));

  args_script.push_back(g_mysqlsh_path);
  args_script.push_back(log_level.c_str());
  args_script.push_back("--py");
  args_script.push_back("-f");

  args_script.push_back(_local_mysqlprovision_path.c_str());
  args_script.push_back(cmd.c_str());
  args_script.push_back(NULL);

  setup_recorder_environment(cmd);

  // Wrap arguments to be passed to mysqlprovision
  shcore::Value wrapped_args(shcore::Value::new_array());
  Argument_map kwargs_(kwargs);
  kwargs_["verbose"] = shcore::Value(verbose);
  wrapped_args.as_array()->push_back(value_from_argmap(kwargs_));
  // Use a different wrapped args list with hidden passwords to show on the log
  shcore::Value logged_wrapped_args(shcore::Value::new_array());
  Argument_map logged_kwargs(kwargs_);
  if (logged_kwargs.has_key("passwd"))
    logged_kwargs["passwd"] = shcore::Value("****");
  if (logged_kwargs.has_key("rep_user_passwd"))
    logged_kwargs["rep_user_passwd"] = shcore::Value("****");
  if (logged_kwargs.has_key("server")) {
    Argument_map logged_server_opt(*logged_kwargs["server"].as_map());
    if (logged_server_opt.has_key("passwd"))
      logged_server_opt["passwd"] = shcore::Value("****");
    if (logged_server_opt.has_key("password"))
      logged_server_opt["password"] = shcore::Value("****");
    logged_kwargs["server"] = value_from_argmap(logged_server_opt);
  }
  logged_wrapped_args.as_array()->push_back(value_from_argmap(logged_kwargs));
  for (size_t i = 0; i < args.size(); i++) {
    wrapped_args.as_array()->push_back(args[i]);
    if (args[i].type == shcore::Map) {
      Argument_map logged_args(*args[i].as_map());
      if (logged_args.has_key("passwd"))
        logged_args["passwd"] = shcore::Value("****");
      if (logged_args.has_key("password"))
        logged_args["password"] = shcore::Value("****");
      logged_wrapped_args.as_array()->push_back(value_from_argmap(logged_args));
    } else {
      logged_wrapped_args.as_array()->push_back(args[i]);
    }
  }
  // Create JSON string with wrapped arguments
  std::string json = wrapped_args.json();
  json += "\n.\n";

  // Create JSON string with wrapped arguments to show in log (no passwords)
  std::string logged_json = logged_wrapped_args.json();
#ifndef NDEBUG
#ifdef _WIN32
  logged_json = "(echo(" + logged_json + "^&echo(.^&echo.)";
#else
  logged_json = "printf '" + logged_json + "\\n.\\n'";
#endif
#else
  logged_json = "'" + logged_json + "\\n.\\n'";
#endif

  std::string message =
      "DBA: mysqlprovision: Executing " + logged_json + " | " +
      shcore::str_join(&args_script[0], &args_script[args_script.size() - 1],
                       " ");
  log_info("%s", message.c_str());

#ifndef NDEBUG
  if (getenv("TEST_DEBUG") && strcmp(getenv("TEST_DEBUG"), "2") >= 0) {
    std::cerr << message << "\n" << value_from_argmap(kwargs).repr() << "\n";
    for (const auto &arg : args) std::cerr << arg.repr() << "\n";
  }
#endif
  auto console = mysqlsh::current_console();

  if (verbose > 1) {
    console->println(message);
  }

  if (verbose) {
    std::string title = " MySQL Provision Output ";
    std::string half_header((78 - title.size()) / 2, '=');
    std::string header = half_header + title + half_header;
    console->println(header);
  }

  std::string stage_action;

  shcore::Process_launcher p(&args_script[0]);
  try {
    stage_action = "starting";
    p.start();

    {
      stage_action = "executing";
      p.write(json.c_str(), json.size());
    }

    stage_action = "reading from";

    bool last_closed = false;
    bool json_started = false;
    while (p.read(&c, 1) > 0) {
      // Ignores the initial output (most likely prompts)
      // Until the first { is found, indicating the start of JSON data
      if (!json_started) {
        if (c == '{') {
          json_started = true;

          // Prints any initial data
          if (!buf.empty() && verbose) console->print(buf);

          buf.clear();
        } else {
          buf += c;
          continue;
        }
      }

      if (c == '\n') {
        // TODO(paulo): We may need to also filter other messages about
        //              password retrieval

        if (last_closed) {
          shcore::Value raw_data;
          try {
            raw_data = shcore::Value::parse(buf);
          } catch (shcore::Exception &e) {
            std::string error = e.what();
            error += ": ";
            error += buf;

            // Prints the bad formatted buffer, instead of trowing an exception
            // and aborting This is because despite the problam parsing the MP
            // output The work may have been completed there.
            console->print(buf);
            // throw shcore::Exception::parser_error(error);

            log_error("DBA: mysqlprovision: %s", error.c_str());
          }

          if (raw_data && raw_data.type == shcore::Map) {
            auto data = raw_data.as_map();

            std::string type = data->get_string("type");
            std::string info;

            if (type == "WARNING" || type == "ERROR") {
              if (!(*errors)) (*errors).reset(new shcore::Value::Array_type());

              (*errors)->push_back(raw_data);
              info = type + ": ";
            } else if (type == "DEBUG") {
              info = type + ": ";
            }

            info += data->get_string("msg") + "\n";

            // Password prompts are ignored
            if (verbose &&
                info.find("Enter the password for") == std::string::npos) {
              console->print(info);
            }
          }

          log_debug("DBA: mysqlprovision: %s", buf.c_str());

          full_output.append(buf);
          buf = "";
        } else {
          buf += c;
        }
      } else if (c == '\r') {
        buf += c;
      } else {
        buf += c;

        last_closed = c == '}';
      }
    }

    if (!buf.empty()) {
      if (verbose) console->print(buf);

      log_debug("DBA: mysqlprovision: %s", buf.c_str());

      full_output.append(buf);
    }
    stage_action = "terminating";
  } catch (const std::system_error &e) {
    log_warning("DBA: %s while %s mysqlprovision", e.what(),
                stage_action.c_str());
  }

  exit_code = p.wait();

  if (verbose) {
    std::string footer(78, '=');
    console->println(footer);
  }

#ifndef NDEBUG
  if ((getenv("TEST_DEBUG") && strcmp(getenv("TEST_DEBUG"), "2") >= 0)) {
    std::cerr << "mysqlprovision exited with code " << exit_code << ":\n"
              << "\t"
              << shcore::str_join(shcore::str_split(full_output, "\n"), "\n\t")
              << "\n";
  }
#endif
  /*
   * process launcher returns 128 if an ENOENT happened.
   */
  if (exit_code == 128) {
    throw shcore::Exception::runtime_error(
        "mysqlprovision not found. Please verify that mysqlsh is installed "
        "correctly.");
    /*
     * mysqlprovision returns 1 as exit-code for internal behaviour errors.
     * The logged message starts with "ERROR: "
     */
  } else if (exit_code == 1) {
    // Print full output if it wasn't already printed before because of verbose
    log_error("DBA: mysqlprovision exited with error code (%s) : %s ",
              std::to_string(exit_code).c_str(), full_output.c_str());

    /*
     * mysqlprovision returns 2 as exit-code for parameters parsing errors
     * The logged message starts with "mysqlprovision: error: "
     */
  } else if (exit_code == 2) {
    log_error("DBA: mysqlprovision exited with error code (%s) : %s ",
              std::to_string(exit_code).c_str(), full_output.c_str());

    // This error implies a wrong integratio nbetween the chell and MP
    std::string log_path = shcore::get_user_config_path();
    log_path += "mysqlsh.log";

    throw shcore::Exception::runtime_error(
        "Error calling mysqlprovision. For more details look at the log at: " +
        log_path);
  } else {
    log_info("DBA: mysqlprovision: Command returned exit code %i", exit_code);
  }
  return exit_code;
}

void ProvisioningInterface::set_ssl_args(
    const std::string &prefix,
    const mysqlshdk::db::Connection_options &instance,
    std::vector<const char *> *args) {
  std::string ssl_ca, ssl_cert, ssl_key;

  auto instance_ssl = instance.get_ssl_options();

  if (instance_ssl.has_ca())
    ssl_ca = "--" + prefix + "-ssl-ca=" + instance_ssl.get_ca();

  if (instance_ssl.has_cert())
    ssl_cert = "--" + prefix + "-ssl-cert=" + instance_ssl.get_cert();

  if (instance_ssl.has_key())
    ssl_key = "--" + prefix + "-ssl-key=" + instance_ssl.get_key();

  if (!ssl_ca.empty()) args->push_back(strdup(ssl_ca.c_str()));
  if (!ssl_cert.empty()) args->push_back(strdup(ssl_cert.c_str()));
  if (!ssl_key.empty()) args->push_back(strdup(ssl_key.c_str()));
}

int ProvisioningInterface::check(
    const mysqlshdk::db::Connection_options &connection_options,
    const std::string &cnfpath, const std::string &output_cnfpath, bool update,
    bool remote, shcore::Value::Array_type_ref *errors) {
  shcore::Argument_map kwargs;

  {
    auto map = get_connection_map(connection_options);
    if (map->has_key("password")) (*map)["passwd"] = (*map)["password"];
    kwargs["server"] = shcore::Value(map);
  }
  if (!cnfpath.empty()) {
    kwargs["option_file"] = shcore::Value(cnfpath);
  }
  if (!output_cnfpath.empty()) {
    kwargs["output_cnf_path"] = shcore::Value(output_cnfpath);
  }
  if (update) {
    kwargs["update"] = shcore::Value::True();
  }
  if (remote) {
    kwargs["remote"] = shcore::Value::True();
  }

  return execute_mysqlprovision("check", shcore::Argument_list(), kwargs,
                                errors, _verbose);
}

/*
 * Handles the execution for the MP "check" command
 *
 * @param connection_options the target instance connection options
 * @param mycnf_path the my.cnf file path
 * @param output_mycnf_path the output my.cnf file path
 * @param update boolean value to indicate whether configuration updates are to
 * be done or not
 * @param remote boolean value to indicate whether the operation is to be
 * executed remotely or locally
 *
 * @return a Map containing the information of the command result output
 */
shcore::Value ProvisioningInterface::exec_check_ret_handler(
    const mysqlshdk::db::Connection_options &connection_options,
    const std::string &mycnf_path, const std::string &output_mycnf_path,
    bool update, bool remote) {
  shcore::Value::Map_type_ref ret_val(new shcore::Value::Map_type());
  shcore::Value::Array_type_ref mp_errors;
  shcore::Value::Array_type_ref errors(new shcore::Value::Array_type());

  if ((check(connection_options, mycnf_path, output_mycnf_path, update, remote,
             &mp_errors) == 0)) {
    (*ret_val)["status"] = shcore::Value("ok");
  } else {
    (*ret_val)["status"] = shcore::Value("error");

    if (mp_errors) {
      for (auto error_object : *mp_errors) {
        auto map = error_object.as_map();

        std::string error_str;
        if (map->get_string("type") == "ERROR") {
          error_str = map->get_string("msg");

          if (error_str.find("The operation could not continue due to the "
                             "following requirements not being met") !=
              std::string::npos) {
            auto lines = shcore::split_string(error_str, "\n");

            bool loading_options = false;

            shcore::Value::Map_type_ref server_options(
                new shcore::Value::Map_type());
            std::string option_type;

            for (size_t index = 1; index < lines.size(); index++) {
              if (loading_options) {
                auto option_tokens =
                    shcore::split_string(lines[index], " ", true);

                if (option_tokens[1] == "<no") {
                  option_tokens[1] = "<no value>";
                  option_tokens.erase(option_tokens.begin() + 2);
                }

                if (option_tokens[2] == "<no") {
                  option_tokens[2] = "<no value>";
                  option_tokens.erase(option_tokens.begin() + 3);
                }

                if (option_tokens[2] == "<not") {
                  option_tokens[2] = "<not set>";
                  option_tokens.erase(option_tokens.begin() + 3);
                }

                if (option_tokens[1] == "<unique") {
                  option_tokens[1] = "<unique ID>";
                  option_tokens.erase(option_tokens.begin() + 2);
                }

                // The tokens describing each option have length of 5
                if (option_tokens.size() > 5) {
                  index--;
                  loading_options = false;
                } else {
                  shcore::Value::Map_type_ref option;
                  if (!server_options->has_key(option_tokens[0])) {
                    option.reset(new shcore::Value::Map_type());
                    (*server_options)[option_tokens[0]] = shcore::Value(option);
                  } else {
                    option = (*server_options)[option_tokens[0]].as_map();
                  }

                  (*option)["required"] =
                      shcore::Value(option_tokens[1]);  // The required value
                  (*option)[option_type] =
                      shcore::Value(option_tokens[2]);  // The current value
                }
              } else {
                if (lines[index].find("Some active options on server") !=
                    std::string::npos) {
                  option_type = "server";
                  loading_options = true;
                  // Skips to the actual option table
                  do {
                    index++;
                  } while (
                      lines[index].find("-------------------------------") ==
                      std::string::npos);
                } else if (lines[index].find("Some of the configuration "
                                             "values on your options file") !=
                           std::string::npos) {
                  option_type = "config";
                  loading_options = true;
                  // Skips to the actual option table
                  do {
                    index++;
                  } while (
                      lines[index].find("-------------------------------") ==
                      std::string::npos);
                }
              }
            }

            if (server_options->size()) {
              shcore::Value::Array_type_ref config_errors(
                  new shcore::Value::Array_type());
              for (auto option : *server_options) {
                auto state = option.second.as_map();

                std::string required_value = state->get_string("required");
                std::string server_value = state->get_string("server", "");
                std::string config_value = state->get_string("config", "");

                // Taken from MP, reading docs made me think all variables
                // should require restart Even several of them are dynamic, it
                // seems changing values may lead to problems An
                // extransaction_write_set_extraction which apparently is
                // reserved for future use So I just took what I saw on the MP
                // code Source:
                // http://dev.mysql.com/doc/refman/5.7/en/dynamic-system-variables.html
                std::vector<std::string> dynamic_variables = {
                    "binlog_format", "binlog_checksum", "slave_parallel_type",
                    "slave_preserve_commit_order"};

                bool dynamic =
                    std::find(dynamic_variables.begin(),
                              dynamic_variables.end(),
                              option.first) != dynamic_variables.end();

                std::string action;
                std::string current;
                if (!server_value.empty() &&
                    !config_value.empty()) {  // Both server and configuration
                                              // are wrong
                  if (dynamic) {
                    action = "server_update+config_update";
                  } else {
                    action = "config_update+restart";
                  }

                  current = server_value;
                } else if (!config_value.empty()) {  // Configuration is
                                                     // wrong, server is OK
                  action = "config_update";
                  current = config_value;
                } else if (!server_value.empty()) {  // Server is wrong,
                                                     // configuration is OK
                  if (dynamic) {
                    action = "server_update";
                  } else {
                    action = "restart";
                  }
                  current = server_value;
                }

                shcore::Value::Map_type_ref error(
                    new shcore::Value::Map_type());

                (*error)["option"] = shcore::Value(option.first);
                (*error)["current"] = shcore::Value(current);
                (*error)["required"] = shcore::Value(required_value);
                (*error)["action"] = shcore::Value(action);

                config_errors->push_back(shcore::Value(error));
              }

              (*ret_val)["config_errors"] = shcore::Value(config_errors);
            }
          } else {
            errors->push_back(shcore::Value(error_str));
          }
        }
      }
    }
    (*ret_val)["errors"] = shcore::Value(errors);
  }

  return shcore::Value(ret_val);
}

int ProvisioningInterface::exec_sandbox_op(
    const std::string &op, int port, int portx, const std::string &sandbox_dir,
    const shcore::Argument_map &extra_kwargs,
    shcore::Value::Array_type_ref *errors) {
  shcore::Argument_map kwargs(extra_kwargs);

  kwargs["sandbox_cmd"] = shcore::Value(op);
  kwargs["port"] = shcore::Value(std::to_string(port));
  if (portx != 0) {
    kwargs["mysqlx_port"] = shcore::Value(std::to_string(portx));
  }

  if (!sandbox_dir.empty()) {
    kwargs["sandbox_base_dir"] = shcore::Value(sandbox_dir);
  } else {
    std::string dir = mysqlsh::current_shell_options()->get().sandbox_directory;

    try {
      shcore::ensure_dir_exists(dir);

      kwargs["sandbox_base_dir"] = shcore::Value(dir);
    } catch (std::runtime_error &error) {
      log_warning("DBA: Unable to create default sandbox directory at %s.",
                  dir.c_str());
    }
  }

  return execute_mysqlprovision("sandbox", shcore::Argument_list(), kwargs,
                                errors, _verbose);
}

int ProvisioningInterface::create_sandbox(
    int port, int portx, const std::string &sandbox_dir,
    const std::string &password, const shcore::Value &mycnf_options, bool start,
    bool ignore_ssl_error, int timeout, shcore::Value::Array_type_ref *errors) {
  shcore::Argument_map kwargs;
  if (mycnf_options) {
    kwargs["opt"] = mycnf_options;
  }

  if (ignore_ssl_error) kwargs["ignore_ssl_error"] = shcore::Value::True();

  if (start) kwargs["start"] = shcore::Value::True();

  if (!password.empty()) kwargs["passwd"] = shcore::Value(password);

  if (timeout > 0) kwargs["timeout"] = shcore::Value(timeout);

  return exec_sandbox_op("create", port, portx, sandbox_dir, kwargs, errors);
}

int ProvisioningInterface::delete_sandbox(
    int port, const std::string &sandbox_dir,
    shcore::Value::Array_type_ref *errors) {
  return exec_sandbox_op("delete", port, 0, sandbox_dir, shcore::Argument_map(),
                         errors);
}

int ProvisioningInterface::kill_sandbox(int port,
                                        const std::string &sandbox_dir,
                                        shcore::Value::Array_type_ref *errors) {
  return exec_sandbox_op("kill", port, 0, sandbox_dir, shcore::Argument_map(),
                         errors);
}

int ProvisioningInterface::stop_sandbox(int port,
                                        const std::string &sandbox_dir,
                                        const std::string &password,
                                        shcore::Value::Array_type_ref *errors) {
  shcore::Argument_map kwargs;
  if (!password.empty()) kwargs["passwd"] = shcore::Value(password);
  return exec_sandbox_op("stop", port, 0, sandbox_dir, kwargs, errors);
}

int ProvisioningInterface::start_sandbox(
    int port, const std::string &sandbox_dir,
    shcore::Value::Array_type_ref *errors) {
  return exec_sandbox_op("start", port, 0, sandbox_dir, shcore::Argument_map(),
                         errors);
}

int ProvisioningInterface::start_replicaset(
    const mysqlshdk::db::Connection_options &instance,
    const std::string &repl_user, const std::string &repl_user_password,
    bool multi_primary, const std::string &ssl_mode,
    const std::string &ip_whitelist, const std::string &group_name,
    const std::string &gr_local_address, const std::string &gr_group_seeds,
    bool skip_rpl_user, shcore::Value::Array_type_ref *errors) {
  shcore::Argument_map kwargs;
  shcore::Argument_list args;

  {
    auto map = get_connection_map(instance);
    if (map->has_key("password")) (*map)["passwd"] = (*map)["password"];
    args.push_back(shcore::Value(map));
  }

  if (!repl_user.empty()) {
    kwargs["rep_user_passwd"] = shcore::Value(repl_user_password);
    kwargs["replication_user"] = shcore::Value(repl_user);
  }
  if (skip_rpl_user) {
    kwargs["skip_rpl_user"] = shcore::Value::True();
  }

  if (multi_primary) {
    kwargs["single_primary"] = shcore::Value("OFF");
  }
  if (!ssl_mode.empty()) {
    kwargs["ssl_mode"] = shcore::Value(ssl_mode);
  }
  if (!ip_whitelist.empty()) {
    kwargs["ip_whitelist"] = shcore::Value(ip_whitelist);
  }
  if (!group_name.empty()) {
    kwargs["group_name"] = shcore::Value(group_name);
  }
  if (!gr_local_address.empty()) {
    kwargs["gr_address"] = shcore::Value(gr_local_address);
  }
  if (!gr_group_seeds.empty()) {
    kwargs["group_seeds"] = shcore::Value(gr_group_seeds);
  }

  return execute_mysqlprovision("start-replicaset", args, kwargs, errors,
                                _verbose);
}

int ProvisioningInterface::join_replicaset(
    const mysqlshdk::db::Connection_options &instance,
    const mysqlshdk::db::Connection_options &peer, const std::string &repl_user,
    const std::string &repl_user_password, const std::string &ssl_mode,
    const std::string &ip_whitelist, const std::string &gr_local_address,
    const std::string &gr_group_seeds, bool skip_rpl_user,
    shcore::Value::Array_type_ref *errors) {
  shcore::Argument_map kwargs;
  shcore::Argument_list args;

  {
    auto map = get_connection_map(instance);
    if (map->has_key("password")) (*map)["passwd"] = (*map)["password"];
    args.push_back(shcore::Value(map));
  }
  {
    auto map = get_connection_map(peer);
    if (map->has_key("password")) (*map)["passwd"] = (*map)["password"];
    args.push_back(shcore::Value(map));
  }

  if (!repl_user.empty()) {
    kwargs["rep_user_passwd"] = shcore::Value(repl_user_password);
    kwargs["replication_user"] = shcore::Value(repl_user);
  }
  if (!ssl_mode.empty()) {
    kwargs["ssl_mode"] = shcore::Value(ssl_mode);
  }
  if (!ip_whitelist.empty()) {
    kwargs["ip_whitelist"] = shcore::Value(ip_whitelist);
  }

  if (!gr_local_address.empty()) {
    kwargs["gr_address"] = shcore::Value(gr_local_address);
  }
  if (!gr_group_seeds.empty()) {
    kwargs["group_seeds"] = shcore::Value(gr_group_seeds);
  }
  if (skip_rpl_user) {
    kwargs["skip_rpl_user"] = shcore::Value::True();
  }
  if (instance.has_host() &&
      mysqlshdk::utils::Net::is_local_address(instance.get_host())) {
    kwargs["target_is_local"] = shcore::Value::True();
  }

  return execute_mysqlprovision("join-replicaset", args, kwargs, errors,
                                _verbose);
}
