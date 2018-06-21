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
#include "modules/adminapi/mod_dba_common.h"
#include "modules/adminapi/mod_dba_metadata_storage.h"
#include "modules/adminapi/mod_dba_replicaset.h"
#include "modules/adminapi/mod_dba_sql.h"
#include "modules/mod_shell.h"
#include "mysqlshdk/libs/db/connection_options.h"
#include "mysqlshdk/libs/db/mysql/session.h"
#include "unittest/test_utils/admin_api_test.h"
#include "unittest/test_utils/mod_testutils.h"

using mysqlshdk::mysql::Instance;
using mysqlshdk::mysql::Var_qualifier;
namespace tests {

class Dba_replicaset_test : public Admin_api_test {
 public:
  static std::shared_ptr<mysqlshdk::db::ISession> create_session(
      int port, std::string user = "root") {
    auto session = mysqlshdk::db::mysql::Session::create();

    auto connection_options = shcore::get_connection_options(
        user + ":root@localhost:" + std::to_string(port), false);
    session->connect(connection_options);

    return session;
  }
  virtual void SetUp() {
    Admin_api_test::SetUp();
    reset_replayable_shell();

    execute("shell.connect('root:root@localhost:" +
            std::to_string(_mysql_sandbox_port1) + "')");

    auto dba = _interactive_shell->shell_context()
                   ->get_global("dba")
                   .as_object<mysqlsh::dba::Dba>();
    if (!dba) {
      auto idba = _interactive_shell->shell_context()
                      ->get_global("dba")
                      .as_object<shcore::Global_dba>();
      dba = std::dynamic_pointer_cast<mysqlsh::dba::Dba>(idba->get_target());
    }

    shcore::Argument_list args;
    args.emplace_back("sample");
    m_cluster = dba->get_cluster_(args).as_object<mysqlsh::dba::Cluster>();

    execute("session.close()");
  }

  virtual void TearDown() {
    shcore::Argument_list args;
    m_cluster->disconnect(args);
    Admin_api_test::TearDown();
  }

  static void SetUpTestCase() {
    SetUpSampleCluster("Dba_replicaset_test/SetUpTestCase");
  }

  static void TearDownTestCase() {
    TearDownSampleCluster("Dba_replicaset_test/TearDownTestCase");
  }

 protected:
  std::shared_ptr<mysqlshdk::db::ISession> create_base_session(int port) {
    mysqlshdk::db::Connection_options connection_options;

    connection_options.set_host("localhost");
    connection_options.set_port(port);
    connection_options.set_user("user");
    connection_options.set_password("");

    auto session = mysqlshdk::db::mysql::Session::create();

    session->connect(connection_options);

    return session;
  }

  std::shared_ptr<mysqlsh::dba::Cluster> m_cluster;
};

// If the information on the Metadata and the GR group
// P_S info matches, the rescan result should be empty
TEST_F(Dba_replicaset_test, rescan_cluster_with_no_changes) {
  try {
    shcore::Value ret_val = _replicaset->rescan({});
    auto result = ret_val.as_map();

    EXPECT_TRUE(result->has_key("name"));
    EXPECT_STREQ("default", result->get_string("name").c_str());

    EXPECT_TRUE(result->has_key("newlyDiscoveredInstances"));
    auto newly_discovered_instances =
        result->get_array("newlyDiscoveredInstances");
    EXPECT_TRUE(newly_discovered_instances->empty());

    EXPECT_TRUE(result->has_key("unavailableInstances"));
    auto unavailable_instances = result->get_array("unavailableInstances");
    EXPECT_TRUE(unavailable_instances->empty());
  } catch (const shcore::Exception &e) {
    std::string error = e.what();
  }
}

// as seen in GR group P_S info, the rescan
// result should include those on the unavailableInstances list
TEST_F(Dba_replicaset_test, rescan_cluster_with_unavailable_instances) {
  auto md_session = create_session(_mysql_sandbox_port1);

  // Insert a fake record for the third instance on the metadata
  std::string query =
      "insert into mysql_innodb_cluster_metadata.instances "
      "values (0, 1, " +
      std::to_string(_replicaset->get_id()) + ", '" + uuid_3 +
      "', 'localhost:<port>', "
      "'HA', NULL, '{\"mysqlX\": \"localhost:<port>0\", "
      "\"grLocal\": \"localhost:1<port>\", "
      "\"mysqlClassic\": \"localhost:<port>\"}', "
      "NULL, NULL, NULL)";

  query = shcore::str_replace(query, "<port>",
                              std::to_string(_mysql_sandbox_port3));

  md_session->query(query);

  try {
    shcore::Value ret_val = _replicaset->rescan({});
    auto result = ret_val.as_map();

    EXPECT_TRUE(result->has_key("name"));
    EXPECT_STREQ("default", result->get_string("name").c_str());

    EXPECT_TRUE(result->has_key("newlyDiscoveredInstances"));
    auto newly_discovered_instances =
        result->get_array("newlyDiscoveredInstances");
    EXPECT_TRUE(newly_discovered_instances->empty());

    auto unavailable_instances = result->get_array("unavailableInstances");
    auto instance = unavailable_instances.get()->at(0);
    auto instance_map = instance.as_map();

    EXPECT_STREQ(uuid_3.c_str(), instance_map->get_string("member_id").c_str());
    std::string host = "localhost:" + std::to_string(_mysql_sandbox_port3);
    EXPECT_STREQ(host.c_str(), instance_map->get_string("label").c_str());
    EXPECT_STREQ(host.c_str(), instance_map->get_string("host").c_str());
  } catch (const shcore::Exception &e) {
    std::string error = e.what();
  }

  md_session->query(
      "delete from mysql_innodb_cluster_metadata.instances "
      " where mysql_server_uuid = '" +
      uuid_3 + "'");
  md_session->close();
}

// If the GR group P_S info contains more instances than the ones
// as seen in Metadata, the rescan
// result should include those on the newlyDiscoveredInstances list
TEST_F(Dba_replicaset_test, rescan_cluster_with_new_instances) {
  auto md_session = create_session(_mysql_sandbox_port1);

  md_session->query(
      "delete from mysql_innodb_cluster_metadata.instances "
      " where mysql_server_uuid = '" +
      uuid_2 + "'");

  try {
    shcore::Value ret_val = _replicaset->rescan({});
    auto result = ret_val.as_map();

    EXPECT_TRUE(result->has_key("name"));
    EXPECT_STREQ("default", result->get_string("name").c_str());

    EXPECT_TRUE(result->has_key("newlyDiscoveredInstances"));
    auto newly_discovered_instances =
        result->get_array("newlyDiscoveredInstances");

    auto unknown_instances = result->get_array("newlyDiscoveredInstances");
    auto instance = unknown_instances.get()->at(0);
    auto instance_map = instance.as_map();

    EXPECT_STREQ(uuid_2.c_str(), instance_map->get_string("member_id").c_str());
    std::string host = "localhost:" + std::to_string(_mysql_sandbox_port2);
    EXPECT_STREQ(host.c_str(), instance_map->get_string("name").c_str());
    EXPECT_STREQ(host.c_str(), instance_map->get_string("host").c_str());

    EXPECT_TRUE(result->has_key("unavailableInstances"));
    auto unavailable_instances = result->get_array("unavailableInstances");
    EXPECT_TRUE(unavailable_instances->empty());
  } catch (const shcore::Exception &e) {
    std::string error = e.what();
  }

  md_session->close();
}

// Regression test for BUG#28219398
// INSTANCE ADDED TO CLUSTER STUCK IN RECOVERY PHASE DUE TO WRONG CHANGE MASTER
// CMD:
//
// Occasionally, when adding an instance to an existing cluster, the instance
// gets stuck in the distributed-recovery phase resulting in an immutable
// reported status of 'RECOVERY'.
//
// The issue happens every-time the generated password for the replication-user
// contains the following sequence of characters: '%s'.
TEST_F(Dba_replicaset_test, bug28219398) {
  std::string replication_user, replication_pwd;
  mysqlshdk::db::Connection_options connection_options;

  replication_user = "mysql_innodb_cluster_r1711111111";

  // The password must contain the sequence of chars '%s';
  replication_pwd = "|t+-*2]1@+T?e%&H;*.%s)>)F/U}9,$?";

  // Create the replication_user on the cluster
  auto md_session = create_session(_mysql_sandbox_port1);

  md_session->query("CREATE USER IF NOT EXISTS '" + replication_user +
                    "'@'localhost' IDENTIFIED BY /*(*/ '" + replication_pwd +
                    "' /*)*/ ");

  md_session->query("GRANT REPLICATION SLAVE ON *.* to /*(*/ '" +
                    replication_user + "'@'localhost' /*)*/");

  // Set the connection options for _mysql_sandbox_port3
  connection_options.set_host("localhost");
  connection_options.set_port(_mysql_sandbox_port3);
  connection_options.set_user("root");
  connection_options.set_password("root");

  try {
    m_cluster->get_default_replicaset()->add_instance(
        connection_options, {}, replication_user, replication_pwd, false, "",
        true, false);

    // Set the Shell global session
    execute("shell.connect('root:root@localhost:" +
            std::to_string(_mysql_sandbox_port3) + "')");

    testutil->wait_member_state(_mysql_sandbox_port3, "ONLINE", false);

    execute("session.close()");

    auto session = create_session(_mysql_sandbox_port3);
    auto instance_type = mysqlsh::dba::get_gr_instance_type(session);

    mysqlsh::dba::Cluster_check_info state =
        mysqlsh::dba::get_replication_group_state(session, instance_type);

    session->close();

    // The instance should be 'ONLINE' and not stuck in 'RECOVERING'
    EXPECT_EQ(state.source_state, mysqlsh::dba::ManagedInstance::OnlineRO);
  } catch (const shcore::Exception &e) {
    SCOPED_TRACE(e.what());
    SCOPED_TRACE("Unexpected failure adding instance.");
    ADD_FAILURE();
  }

  md_session->close();
}
}  // namespace tests
