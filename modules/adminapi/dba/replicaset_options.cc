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

#include "modules/adminapi/dba/replicaset_options.h"
#include "modules/adminapi/mod_dba_common.h"
#include "mysqlshdk/libs/mysql/group_replication.h"

namespace mysqlsh {
namespace dba {

Replicaset_options::Replicaset_options(std::shared_ptr<ReplicaSet> replicaset,
                                       mysqlshdk::utils::nullable<bool> all)
    : m_replicaset(replicaset), m_all(all) {
  assert(replicaset);
}

Replicaset_options::~Replicaset_options() {}

void Replicaset_options::prepare() {
  // Get the current members list
  m_instances = m_replicaset->get_instances();

  // Connect to every ReplicaSet member and populate:
  //   - m_member_sessions
  //   - m_member_connect_errors
  connect_to_members();
}

/**
 * Connects to all members of the ReplicaSet
 *
 * This function tries to connect to all registered members of the ReplicaSet
 * and:
 *  - If the connection is established successfully add the session object to
 * m_member_sessions
 *  - If the connection cannot be established, add the connection error to
 * m_member_connect_errors
 */
void Replicaset_options::connect_to_members() {
  auto group_session = m_replicaset->get_cluster()->get_group_session();

  mysqlshdk::db::Connection_options group_session_copts(
      group_session->get_connection_options());

  for (const auto &inst : m_instances) {
    mysqlshdk::db::Connection_options opts(inst.classic_endpoint);
    if (opts.uri_endpoint() == group_session_copts.uri_endpoint()) {
      m_member_sessions[inst.classic_endpoint] = group_session;
    } else {
      opts.set_login_options_from(group_session_copts);

      try {
        m_member_sessions[inst.classic_endpoint] =
            mysqlshdk::db::mysql::open_session(opts);
      } catch (mysqlshdk::db::Error &e) {
        m_member_connect_errors[inst.classic_endpoint] = e.format();
      }
    }
  }
}

/**
 * Get the global ReplicaSet configuration options configuration options of
 * the ReplicaSet
 *
 * This function gets the global ReplicaSet configuration options and builds
 * an Array containing a Dictionary with the format:
 * {
 *   "option": "option_xxx",
 *   "value": "xxx-xxx",
 *   "variable": "group_replication_xxx"
 * }
 *
 * @return a shcore::Array_t containing a dictionary object with the global
 * ReplicaSet configuration options information
 */
shcore::Array_t Replicaset_options::collect_global_options() {
  shcore::Array_t array = shcore::make_array();

  auto group_session = m_replicaset->get_cluster()->get_group_session();
  mysqlshdk::mysql::Instance group_instance(group_session);

  for (const auto &cfg : k_global_options) {
    shcore::Dictionary_t option = shcore::make_dict();
    std::string value = group_instance.get_sysvar_string(
        cfg.second, mysqlshdk::mysql::Var_qualifier::GLOBAL);

    (*option)["option"] = shcore::Value(cfg.first);
    (*option)["variable"] = shcore::Value(cfg.second);
    (*option)["value"] = shcore::Value(value);

    array->push_back(shcore::Value(option));
  }

  return array;
}

/**
 * Get the ReplicaSet configuration options configuration options of
 * a ReplicaSet member
 *
 * This function gets the instance ReplicaSet configuration options and builds
 * an Array containing a Dictionary with the format:
 * {
 *   "option": "option_xxx",
 *   "value": "xxx-xxx",
 *   "variable": "group_replication_xxx"
 * }
 *
 * NOTE: If the option 'all' was enabled, the function will query ALL Group
 * Replication options
 *
 * @param instance Target instance to query for the configuration options
 *
 * @return a shcore::Array_t containing a dictionary object with the instance
 * ReplicaSet configuration options information
 */
shcore::Array_t Replicaset_options::get_instance_options(
    const mysqlshdk::mysql::Instance &instance) {
  shcore::Array_t array = shcore::make_array();

  if (m_all.is_null() || *m_all == false) {
    for (const auto &cfg : k_instance_options) {
      shcore::Dictionary_t option = shcore::make_dict();
      mysqlshdk::utils::nullable<std::string> value =
          instance.get_sysvar_string(cfg.second,
                                     mysqlshdk::mysql::Var_qualifier::GLOBAL);

      (*option)["option"] = shcore::Value(cfg.first);
      (*option)["variable"] = shcore::Value(cfg.second);

      // Check if the option exists in the target server
      if (!value.is_null()) {
        (*option)["value"] = shcore::Value(*value);
      } else {
        (*option)["value"] = shcore::Value::Null();
      }

      array->push_back(shcore::Value(option));
    }
  } else {
    // Get all GR configurations.
    log_debug("Get all group replication configurations.");
    std::map<std::string, mysqlshdk::utils::nullable<std::string>> gr_cfgs =
        mysqlshdk::gr::get_all_configurations(instance);

    for (const auto &cfg : gr_cfgs) {
      shcore::Dictionary_t option = shcore::make_dict();

      // Check if the option is supported by the AdminAPI so we use its name
      auto it = k_instance_options.cbegin();
      while (it != k_instance_options.cend()) {
        if (it->second == cfg.first) {
          (*option)["option"] = shcore::Value(it->first);
          break;
        }
        it++;
      }

      (*option)["variable"] = shcore::Value(cfg.first);

      if (!cfg.second.is_null()) {
        (*option)["value"] = shcore::Value(*cfg.second);
      }

      array->push_back(shcore::Value(option));
    }
  }

  return array;
}

/**
 * Get the full ReplicaSet Options information
 *
 * This function gets the ReplicaSet configuration options and builds
 * an Dictionary containing the following fields:
 *   - "globalOptions": content of collect_global_options()
 *   - "topology": an array with every member of the cluster identified by its
 * label. Each member of the array contains the result from
 * get_instance_options(instance)
 *
 *
 * @return a shcore::Dictionary_t containing a dictionary object with the full
 * ReplicaSet configuration options information
 */
shcore::Dictionary_t Replicaset_options::collect_replicaset_options() {
  shcore::Dictionary_t tmp = shcore::make_dict();
  shcore::Dictionary_t ret = shcore::make_dict();

  // Get global options
  (*ret)["globalOptions"] = shcore::Value(collect_global_options());

  connect_to_members();

  for (const auto &inst : m_instances) {
    shcore::Dictionary_t option = shcore::make_dict();

    mysqlshdk::mysql::Instance instance(
        m_member_sessions[inst.classic_endpoint]);

    if (!instance.get_session()) {
      (*option)["shellConnectError"] =
          shcore::Value(m_member_connect_errors[inst.classic_endpoint]);
      (*tmp)[inst.name] = shcore::Value(option);
    } else {
      (*tmp)[inst.name] = shcore::Value(get_instance_options(instance));
    }
  }

  (*ret)["topology"] = shcore::Value(tmp);

  return ret;
}

shcore::Value Replicaset_options::execute() {
  // Get the ReplicaSet options
  shcore::Dictionary_t replicaset_dict;

  replicaset_dict = collect_replicaset_options();

  return shcore::Value(replicaset_dict);
}

void Replicaset_options::rollback() {
  // Do nothing right now, but it might be used in the future when
  // transactional command execution feature will be available.
}

void Replicaset_options::finish() {
  // Reset all auxiliary (temporary) data used for the operation execution.
  m_member_sessions.clear();
  m_instances.clear();
  m_member_connect_errors.clear();
}

}  // namespace dba
}  // namespace mysqlsh
