/*
 * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
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

#include "modules/adminapi/dba/replicaset_set_option.h"
#include "modules/adminapi/dba/validations.h"
#include "modules/adminapi/mod_dba_metadata_storage.h"
#include "mysqlshdk/include/shellcore/console.h"
#include "mysqlshdk/libs/config/config_server_handler.h"
#include "mysqlshdk/libs/mysql/group_replication.h"
#include "mysqlshdk/libs/utils/utils_general.h"

namespace mysqlsh {
namespace dba {
Replicaset_set_option::Replicaset_set_option(
    std::shared_ptr<ReplicaSet> replicaset, const std::string &option,
    const std::string &value)
    : m_replicaset(std::move(replicaset)),
      m_option(option),
      m_value_str(value) {
  assert(m_replicaset);

  m_cluster_session_instance = shcore::make_unique<mysqlshdk::mysql::Instance>(
      m_replicaset->get_cluster()->get_group_session());
}

Replicaset_set_option::Replicaset_set_option(
    std::shared_ptr<ReplicaSet> replicaset, const std::string &option,
    int64_t value)
    : m_replicaset(std::move(replicaset)),
      m_option(option),
      m_value_int(value) {
  assert(m_replicaset);

  m_cluster_session_instance = shcore::make_unique<mysqlshdk::mysql::Instance>(
      m_replicaset->get_cluster()->get_group_session());
}

Replicaset_set_option::~Replicaset_set_option() {}

void Replicaset_set_option::prepare_config_object() {
  auto console = mysqlsh::current_console();
  m_cfg = shcore::make_unique<mysqlshdk::config::Config>();

  for (const auto &instance : m_cluster_instances) {
    // Add server configuration handler depending on SET PERSIST support.
    std::string instance_address = instance->descr();

    bool is_set_persist_supported =
        !instance->is_set_persist_supported().is_null() &&
        *instance->is_set_persist_supported();

    // Create server configuration handler depending on SET PERSIST support.
    std::unique_ptr<mysqlshdk::config::IConfig_handler> config_handler =
        shcore::make_unique<mysqlshdk::config::Config_server_handler>(
            instance.get(), is_set_persist_supported
                                ? mysqlshdk::mysql::Var_qualifier::PERSIST
                                : mysqlshdk::mysql::Var_qualifier::GLOBAL);

    // Add the server configuration to the configuration object
    m_cfg->add_handler(instance_address, std::move(config_handler));
  }
}

void Replicaset_set_option::ensure_option_valid() {
  /* - Validate if the option is valid, being the accepted values:
   *     - exitStateAction
   *     - memberWeight
   *     - failoverConsistency
   *     - expelTimeout
   */
  if (k_global_replicaset_supported_options.count(m_option) == 0) {
    throw shcore::Exception::argument_error("Option '" + m_option +
                                            "' not supported.");
  }
}

void Replicaset_set_option::ensure_all_members_replicaset_online() {
  auto console = mysqlsh::current_console();

  log_debug("Checking if all members of the Replicaset are ONLINE.");

  // Get all cluster instances
  std::vector<Instance_definition> instance_defs =
      m_replicaset->get_cluster()
          ->get_metadata_storage()
          ->get_replicaset_instances(m_replicaset->get_id(), true);

  // Get cluster session to use the same authentication credentials for all
  // replicaset instances.
  Connection_options cluster_cnx_opt =
      m_cluster_session_instance->get_connection_options();

  // Check if all instances have the ONLINE state
  for (const Instance_definition &instance_def : instance_defs) {
    mysqlshdk::gr::Member_state state =
        mysqlshdk::gr::to_member_state(instance_def.state);

    if (state != mysqlshdk::gr::Member_state::ONLINE) {
      console->print_error(
          "The instance '" + instance_def.endpoint + "' has the status: '" +
          mysqlshdk::gr::to_string(state) + "'. All members must be ONLINE.");

      throw shcore::Exception::runtime_error(
          "One or more instances of the cluster are not ONLINE.");
    } else {
      // Instance is ONLINE, initialize and populate the internal replicaset
      // instances list

      // Establish a session to the instance
      // Set login credentials to connect to instance.
      // NOTE: It is assumed that the same login credentials can be used to
      // connect to all replicaset instances.
      std::string instance_address = instance_def.endpoint;

      Connection_options instance_cnx_opts =
          shcore::get_connection_options(instance_address, false);
      instance_cnx_opts.set_login_options_from(cluster_cnx_opt);

      log_debug("Connecting to instance '%s'.", instance_address.c_str());
      std::shared_ptr<mysqlshdk::db::ISession> session;

      mysqlshdk::mysql::Instance instance;

      // Establish a session to the instance
      try {
        session = mysqlshdk::db::mysql::Session::create();
        session->connect(instance_cnx_opts);

        // Add the instance to instances internal list
        m_cluster_instances.emplace_back(
            shcore::make_unique<mysqlshdk::mysql::Instance>(session));
      } catch (const std::exception &err) {
        log_debug("Failed to connect to instance: %s", err.what());

        console->print_error(
            "Unable to connect to instance '" + instance_address +
            "'. Please, verify connection credentials and make sure the "
            "instance is available.");

        throw shcore::Exception::runtime_error(err.what());
      }
    }
  }
}

void Replicaset_set_option::ensure_option_supported_all_members_replicaset() {
  auto console = mysqlsh::current_console();

  log_debug("Checking if all members of the Replicaset support the option '%s'",
            m_option.c_str());

  mysqlshdk::utils::Version support_in_80 =
      k_global_replicaset_supported_options.at(m_option).support_in_80;
  mysqlshdk::utils::Version support_in_57 =
      k_global_replicaset_supported_options.at(m_option).support_in_57;

  for (const auto &instance : m_cluster_instances) {
    std::string instance_address = instance->descr();

    // Verify if the instance version is supported
    mysqlshdk::utils::Version instance_version = instance->get_version();

    if ((instance_version < mysqlshdk::utils::Version("8.0.0") &&
         instance_version > mysqlshdk::utils::Version("5.7.0") &&
         instance_version < support_in_57) ||
        (instance_version > mysqlshdk::utils::Version("8.0.0") &&
         instance_version < support_in_80)) {
      console->print_error(
          "The instance '" + instance_address + "' has the version " +
          instance->get_version().get_full() +
          " which does not support the option '" + m_option + "'.");

      throw shcore::Exception::runtime_error(
          "One or more instances of the cluster have a version that does not "
          "support this operation.");
    }
  }
}

void Replicaset_set_option::prepare() {
  // Validate if the option is valid
  ensure_option_valid();

  // Verify user privileges to execute operation;
  ensure_user_privileges(*m_cluster_session_instance);

  // Verify if all the cluster members are ONLINE
  ensure_all_members_replicaset_online();

  // Verify if all cluster members support the option
  // NOTE: clusterName does not require this validation
  ensure_option_supported_all_members_replicaset();

  // Set the internal configuration object.
  prepare_config_object();
}

shcore::Value Replicaset_set_option::execute() {
  auto console = mysqlsh::current_console();

  // Update the option values in all ReplicaSet members:
  std::string option_gr_variable =
      k_global_replicaset_supported_options.at(m_option).option_variable;

  console->print_info(
      "Setting the value of '" + m_option + "' to '" +
      (m_value_str.is_null() ? std::to_string(*m_value_int) : *m_value_str) +
      "' in all ReplicaSet members ...");
  console->println();

  if (!m_value_str.is_null()) {
    m_cfg->set(option_gr_variable, m_value_str);
  } else {
    m_cfg->set(option_gr_variable, m_value_int);
  }

  m_cfg->apply();

  console->print_info(
      "Successfully set the value of '" + m_option + "' to '" +
      ((m_value_str.is_null() ? std::to_string(*m_value_int) : *m_value_str)) +
      "' in the '" + m_replicaset->get_name() + "' ReplicaSet.");

  return shcore::Value();
}

void Replicaset_set_option::rollback() {}

void Replicaset_set_option::finish() {
  // Close all sessions to cluster instances.
  for (const auto &instance : m_cluster_instances) {
    instance->close_session();
  }

  // Reset all auxiliary (temporary) data used for the operation execution.
  m_cluster_session_instance.reset();
}

}  // namespace dba
}  // namespace mysqlsh
