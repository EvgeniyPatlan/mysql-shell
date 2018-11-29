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

#include <string>
#include <vector>

#include "modules/adminapi/dba/replicaset_switch_to_multi_primary_mode.h"
#include "modules/adminapi/mod_dba_metadata_storage.h"
#include "modules/adminapi/mod_dba_sql.h"
#include "mysqlshdk/include/shellcore/console.h"
#include "mysqlshdk/libs/innodbcluster/cluster_metadata.h"
#include "mysqlshdk/libs/mysql/group_replication.h"
#include "mysqlshdk/libs/utils/utils_general.h"

namespace mysqlsh {
namespace dba {

Switch_to_multi_primary_mode::Switch_to_multi_primary_mode(
    std::shared_ptr<Cluster> cluster, std::shared_ptr<ReplicaSet> replicaset,
    const shcore::NamingStyle &naming_style)
    : Topology_configuration_command(cluster, replicaset, naming_style) {
  assert(cluster);
  assert(replicaset);
}

Switch_to_multi_primary_mode::~Switch_to_multi_primary_mode() {}

void Switch_to_multi_primary_mode::prepare() {
  // Do the base class validations
  Topology_configuration_command::prepare();

  // Set the internal configuration object.
  prepare_config_object();
}

shcore::Value Switch_to_multi_primary_mode::execute() {
  auto console = mysqlsh::current_console();
  console->print_info("Switching cluster '" + m_cluster->get_name() +
                      "' to Multi-Primary mode...");
  console->println();

  // Execute the UDF: SELECT group_replication_switch_to_multi_primary_mode()
  mysqlshdk::gr::switch_to_multi_primary_mode(*m_cluster_session_instance);

  // Update the auto-increment values in all replicaset members:
  //   - auto_increment_increment = 7
  //   - auto_increment_offset = 1 + server_id % 7
  {
    log_debug("Updating auto_increment values of replicaset members");

    // Call update_auto_increment to do the job in all instances
    mysqlshdk::gr::update_auto_increment(
        m_cfg.get(), mysqlshdk::gr::Topology_mode::MULTI_PRIMARY);

    m_cfg->apply();
  }

  // Update the Metadata schema to change the replicasets.topology_type value to
  // "pm"
  {
    log_debug(
        "Updating Replicaset value of topology_type to single-primary in the "
        "Medatada.");

    update_topology_mode_metadata(mysqlshdk::gr::Topology_mode::MULTI_PRIMARY);

    // Update the Replicaset object topology_type
    m_replicaset->set_topology_type(ReplicaSet::kTopologyMultiPrimary);
  }

  // Print information about the instances role changes
  print_replicaset_members_role_changes();

  console->print_info(
      "The cluster successfully switched to Multi-Primary mode.");

  return shcore::Value();
}

void Switch_to_multi_primary_mode::rollback() {}

void Switch_to_multi_primary_mode::finish() {}

}  // namespace dba
}  // namespace mysqlsh
