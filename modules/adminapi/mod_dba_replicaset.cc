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
#include "modules/adminapi/mod_dba_replicaset.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "modules/adminapi/dba/remove_instance.h"
#include "modules/adminapi/dba/validations.h"
#include "modules/adminapi/mod_dba_common.h"
#include "modules/adminapi/mod_dba_metadata_storage.h"
#include "modules/adminapi/mod_dba_sql.h"
#include "modules/mod_mysql_resultset.h"
#include "modules/mod_mysql_session.h"
#include "modules/mod_shell.h"
#include "modules/mysqlxtest_utils.h"
#include "mysqlshdk/libs/mysql/group_replication.h"
#include "mysqlshdk/libs/mysql/instance.h"
#include "shellcore/base_session.h"
#include "utils/utils_general.h"
#include "utils/utils_net.h"
#include "utils/utils_sqlstring.h"
#include "utils/utils_string.h"

using namespace std::placeholders;
using namespace mysqlsh;
using namespace mysqlsh::dba;
using namespace shcore;
using mysqlshdk::db::uri::formats::only_transport;
using mysqlshdk::db::uri::formats::user_transport;
std::set<std::string> ReplicaSet::_add_instance_opts = {
    "label",       "password",     "memberSslMode",
    "ipWhitelist", "localAddress", "groupSeeds"};

// TODO(nelson): update the values to sm and mp respectively on the next version
// bump
char const *ReplicaSet::kTopologySinglePrimary = "pm";
char const *ReplicaSet::kTopologyMultiPrimary = "mm";

static const std::string kSandboxDatadir = "sandboxdata";

const char *kWarningDeprecateSslMode =
    "Option 'memberSslMode' is deprecated for this operation and it will be "
    "removed in a future release. This option is not needed because the SSL "
    "mode is automatically obtained from the cluster. Please do not use it "
    "here.";

enum class Gr_seeds_change_type {
  ADD,
  REMOVE,
  OVERRIDE,
};

ReplicaSet::ReplicaSet(const std::string &name,
                       const std::string &topology_type,
                       const std::string &group_name,
                       std::shared_ptr<MetadataStorage> metadata_storage,
                       std::shared_ptr<IConsole> console_handler)
    : _name(name),
      _topology_type(topology_type),
      _group_name(group_name),
      _metadata_storage(metadata_storage),
      m_console(console_handler) {
  assert(topology_type == kTopologyMultiPrimary ||
         topology_type == kTopologySinglePrimary);
  init();
}

ReplicaSet::~ReplicaSet() {}

std::string &ReplicaSet::append_descr(std::string &s_out, int UNUSED(indent),
                                      int UNUSED(quote_strings)) const {
  s_out.append("<" + class_name() + ":" + _name + ">");
  return s_out;
}

static void append_member_status(const shcore::Value::Map_type_ref &node,
                                 const Instance_definition &instance,
                                 bool read_write,
                                 bool active_session_instance) {
  (*node)["address"] = shcore::Value(instance.endpoint);

  (*node)["status"] = instance.state.empty() ? shcore::Value("(MISSING)")
                                             : shcore::Value(instance.state);
  (*node)["role"] = shcore::Value(instance.role);
  (*node)["mode"] = shcore::Value(read_write ? "R/W" : "R/O");
}

bool ReplicaSet::operator==(const Object_bridge &other) const {
  return class_name() == other.class_name() && this == &other;
}

#if DOXYGEN_CPP
/**
 * Use this function to retrieve an valid member of this class exposed to the
 * scripting languages. \param prop : A string containing the name of the member
 * to be returned
 *
 * This function returns a Value that wraps the object returned by this
 * function. The content of the returned value depends on the property being
 * requested. The next list shows the valid properties as well as the returned
 * value for each of them:
 *
 * \li name: returns a String object with the name of this ReplicaSet object.
 */
#else
/**
 * Returns the name of this ReplicaSet object.
 * \return the name as an String object.
 */
#if DOXYGEN_JS
String ReplicaSet::getName() {}
#elif DOXYGEN_PY
str ReplicaSet::get_name() {}
#endif
#endif
shcore::Value ReplicaSet::get_member(const std::string &prop) const {
  shcore::Value ret_val;
  if (prop == "name")
    ret_val = shcore::Value(_name);
  else
    ret_val = shcore::Cpp_object_bridge::get_member(prop);

  return ret_val;
}

void ReplicaSet::init() {
  add_property("name", "getName");
  add_varargs_method("addInstance",
                     std::bind(&ReplicaSet::add_instance_, this, _1));
  add_varargs_method("rejoinInstance",
                     std::bind(&ReplicaSet::rejoin_instance_, this, _1));
  add_varargs_method("removeInstance",
                     std::bind(&ReplicaSet::remove_instance, this, _1));
  add_varargs_method("disable", std::bind(&ReplicaSet::disable, this, _1));
  add_varargs_method("dissolve", std::bind(&ReplicaSet::dissolve, this, _1));
  add_varargs_method("checkInstanceState",
                     std::bind(&ReplicaSet::check_instance_state, this, _1));
  add_varargs_method(
      "forceQuorumUsingPartitionOf",
      std::bind(&ReplicaSet::force_quorum_using_partition_of_, this, _1));
}

/*
 * Verify if the topology type changed and issue an error if needed.
 */
void ReplicaSet::verify_topology_type_change() const {
  // Get GR single primary mode value.
  int gr_primary_mode;
  auto classic(_metadata_storage->get_session());
  get_server_variable(classic, "group_replication_single_primary_mode",
                      gr_primary_mode);

  // Check if the topology type matches the real settings used by the
  // cluster instance, otherwise an error is issued.
  // NOTE: The GR primary mode is guaranteed (by GR) to be the same for all
  // instance of the same group.
  if (gr_primary_mode == 1 && _topology_type == kTopologyMultiPrimary)
    throw shcore::Exception::runtime_error(
        "The InnoDB Cluster topology type (Multi-Primary) does not match the "
        "current Group Replication configuration (Single-Primary). Please "
        "use <cluster>.rescan() or change the Group Replication "
        "configuration accordingly.");
  else if (gr_primary_mode == 0 && _topology_type == kTopologySinglePrimary)
    throw shcore::Exception::runtime_error(
        "The InnoDB Cluster topology type (Single-Primary) does not match the "
        "current Group Replication configuration (Multi-Primary). Please "
        "use <cluster>.rescan() or change the Group Replication "
        "configuration accordingly.");
}

void ReplicaSet::adopt_from_gr() {
  shcore::Value ret_val;

  auto newly_discovered_instances_list(
      get_newly_discovered_instances(_metadata_storage, _id));

  // Add all instances to the cluster metadata
  for (NewInstanceInfo &instance : newly_discovered_instances_list) {
    mysqlshdk::db::Connection_options newly_discovered_instance;

    newly_discovered_instance.set_host(instance.host);
    newly_discovered_instance.set_port(instance.port);

    log_info("Adopting member %s:%d from existing group", instance.host.c_str(),
             instance.port);

    // TODO(somebody): what if the password is different on each server?
    // And what if is different from the current session?
    auto session = _metadata_storage->get_session();

    auto session_data = session->get_connection_options();

    newly_discovered_instance.set_user(session_data.get_user());
    newly_discovered_instance.set_password(session_data.get_password());

    add_instance_metadata(newly_discovered_instance);
  }
}

/**
 * Auxiliary function to update the group_replication_group_seeds variable.
 *
 * @param gr_address address to override or add/remove from the
 * instance's group_replication_group_seeds variable
 * @param Change_type Type of change we want to do to the group_seeds
 * variable. If Add or Remove, we respectively add or remove the gr_address
 * value received as argument to the group_replication_group_seeds variable of
 * the instance whose session we got. If Override, we set the
 * group_replication_group_seeds with the value of gr_address received as
 * argument.
 * @param session to the instance whose group_replication_group_seeds variable
 * is to be changed.
 * @param naming_style Naming style being used, required for the logged
 * warning messages.
 */
static void update_group_replication_group_seeds(
    const std::string &gr_address, const Gr_seeds_change_type change_type,
    std::shared_ptr<mysqlshdk::db::ISession> session,
    const NamingStyle naming_style,
    std::shared_ptr<mysqlsh::IConsole> console_handler) {
  std::string gr_group_seeds_new_value;
  std::string address =
      session->get_connection_options().as_uri(only_transport());
  // create an instance object for the provided session
  auto instance = mysqlshdk::mysql::Instance(session);
  auto gr_group_seeds = instance.get_sysvar_string(
      "group_replication_group_seeds", mysqlshdk::mysql::Var_qualifier::GLOBAL);
  auto gr_group_seeds_vector = shcore::split_string(*gr_group_seeds, ",");

  switch (change_type) {
    case Gr_seeds_change_type::ADD:
      // get the group_replication_group_seeds value from the instance
      if (!gr_group_seeds->empty()) {
        // if the group_seeds value is not empty, add the gr_address to it.
        // if it is not already there.
        if (std::find(gr_group_seeds_vector.begin(),
                      gr_group_seeds_vector.end(),
                      gr_address) == gr_group_seeds_vector.end()) {
          gr_group_seeds_vector.push_back(gr_address);
        }
        gr_group_seeds_new_value = shcore::str_join(gr_group_seeds_vector, ",");
      } else {
        // If the instance had no group_seeds yet defined, just set it as the
        // value the gr_address argument.
        gr_group_seeds_new_value = gr_address;
      }
      break;
    case Gr_seeds_change_type::REMOVE:
      gr_group_seeds_vector.erase(
          std::remove(gr_group_seeds_vector.begin(),
                      gr_group_seeds_vector.end(), gr_address),
          gr_group_seeds_vector.end());
      gr_group_seeds_new_value = shcore::str_join(gr_group_seeds_vector, ",");
      break;
    case Gr_seeds_change_type::OVERRIDE:
      gr_group_seeds_new_value = gr_address;
      break;
  }

  // Update group_replication_group_seeds variable
  // If server version >= 8.0.11 use set persist, otherwise use set global
  // and warn users that they should use configureLocalInstance to persist
  // the value of the variables
  if (instance.get_version() >= mysqlshdk::utils::Version(8, 0, 11)) {
    bool persist_load = *instance.get_sysvar_bool(
        "persisted_globals_load", mysqlshdk::mysql::Var_qualifier::GLOBAL);
    if (!persist_load) {
      std::string warn_msg =
          "On instance '" + address +
          "' the persisted cluster configuration will not be loaded upon "
          "reboot since 'persisted-globals-load' is set "
          "to 'OFF'. Please use the <Dba>." +
          get_member_name("configureLocalInstance", naming_style) +
          " command locally to persist the changes or set "
          "'persisted-globals-load' to 'ON' on the configuration file.";
      console_handler->print_warning(warn_msg);
    }
    instance.set_sysvar("group_replication_group_seeds",
                        gr_group_seeds_new_value,
                        mysqlshdk::mysql::Var_qualifier::PERSIST);
  } else {
    instance.set_sysvar("group_replication_group_seeds",
                        gr_group_seeds_new_value,
                        mysqlshdk::mysql::Var_qualifier::GLOBAL);
    std::string warn_msg =
        "On instance '" + address +
        "' membership change cannot be persisted since MySQL version " +
        instance.get_version().get_base() +
        " does not support the SET PERSIST command "
        "(MySQL version >= 8.0.11 required). Please use the <Dba>." +
        get_member_name("configureLocalInstance", naming_style) +
        " command locally to persist the changes.";
    console_handler->print_warning(warn_msg);
  }
}

/**
 * Adds a Instance to the ReplicaSet
 * \param conn The Connection String or URI of the Instance to be added
 */
#if DOXYGEN_JS
Undefined ReplicaSet::addInstance(InstanceDef instance, Dictionary options) {}
#elif DOXYGEN_PY
None ReplicaSet::add_instance(InstanceDef instance, doptions) {}
#endif
shcore::Value ReplicaSet::add_instance_(const shcore::Argument_list &args) {
  shcore::Value ret_val;
  args.ensure_count(1, 2, get_function_name("addInstance").c_str());

  // Check if the ReplicaSet is empty
  if (_metadata_storage->is_replicaset_empty(get_id()))
    throw shcore::Exception::runtime_error(
        "ReplicaSet not initialized. Please add the Seed Instance using: "
        "addSeedInstance().");

  // Add the Instance to the Default ReplicaSet
  try {
    auto instance_def =
        mysqlsh::get_connection_options(args, mysqlsh::PasswordFormat::OPTIONS);

    shcore::Argument_list rest;
    if (args.size() == 2) rest.push_back(args.at(1));

    ret_val = add_instance(instance_def, rest);
  }
  CATCH_AND_TRANSLATE_FUNCTION_EXCEPTION(get_function_name("addInstance"));

  return ret_val;
}

void ReplicaSet::validate_instance_address(
    std::shared_ptr<mysqlshdk::db::ISession> session,
    const std::string &hostname, int port) {
  if (mysqlshdk::utils::Net::is_loopback(hostname)) {
    // if the address is local (localhost or 127.0.0.1), we know it's local and
    // so can be used with sandboxes only
    std::string datadir =
        session->query("SELECT @@datadir")->fetch_one()->get_as_string(0);
    if (!datadir.empty() && (datadir[datadir.size() - 1] == '/' ||
                             datadir[datadir.size() - 1] == '\\'))
      datadir.pop_back();
    if (datadir.length() < kSandboxDatadir.length() ||
        datadir.compare(datadir.length() - kSandboxDatadir.length(),
                        kSandboxDatadir.length(), kSandboxDatadir) != 0) {
      log_info("'%s' is a local address but not in a sandbox (datadir %s)",
               hostname.c_str(), datadir.c_str());
      throw shcore::Exception::runtime_error(
          "To add an instance to the cluster, please use a valid, non-local "
          "hostname or IP. " +
          hostname + " can only be used with sandbox MySQL instances.");
    }
  }
}

void set_group_replication_member_options(
    std::shared_ptr<mysqlshdk::db::ISession> session,
    const std::string &ssl_mode) {
  if (session->get_server_version() >= mysqlshdk::utils::Version(8, 0, 5) &&
      ssl_mode == dba::kMemberSSLModeDisabled) {
    // We need to install the GR plugin to have GR sysvars available
    mysqlshdk::mysql::Instance instance(session);
    mysqlshdk::gr::install_plugin(instance);

    // This option required to connect using the new caching_sha256_password
    // authentication method without SSL
    session->query("SET PERSIST group_replication_recovery_get_public_key=1");
  }
}

shcore::Value ReplicaSet::add_instance(
    const mysqlshdk::db::Connection_options &connection_options,
    const shcore::Argument_list &args,
    const std::string &existing_replication_user,
    const std::string &existing_replication_password, bool overwrite_seed,
    const std::string &group_name, bool skip_instance_check,
    bool skip_rpl_user) {
  shcore::Value ret_val;
  bool success = false;

  bool seed_instance = false;
  std::string ssl_mode = dba::kMemberSSLModeAuto;  // SSL Mode AUTO by default
  std::string ip_whitelist, instance_label, local_address, group_seeds;

  // NOTE: This function is called from either the add_instance_ on this class
  //       or the add_instance in Cluster class, hence this just throws
  //       exceptions and the proper handling is done on the caller functions
  //       (to append the called function name)

  // Check if we're on a addSeedInstance or not
  if (_metadata_storage->is_replicaset_empty(_id)) seed_instance = true;

  // Check if we need to overwrite the seed instance
  if (overwrite_seed) seed_instance = true;

  // Retrieves the add options
  if (args.size() == 1) {
    auto add_options = args.map_at(0);
    shcore::Argument_map add_instance_map(*add_options);
    add_instance_map.ensure_keys({}, _add_instance_opts, " options");

    // Validate SSL options for the cluster instance
    validate_ssl_instance_options(add_options);

    // Validate ip whitelist option
    validate_ip_whitelist_option(add_options);

    // Validate local address option
    validate_local_address_option(add_options);

    // Validate group seeds option
    validate_group_seeds_option(add_options);

    if (add_options->has_key("memberSslMode")) {
      ssl_mode = add_options->get_string("memberSslMode");
      if (!seed_instance) {
        m_console->print_warning(kWarningDeprecateSslMode);
        m_console->println();
      }
    }

    if (add_options->has_key("ipWhitelist"))
      ip_whitelist = add_options->get_string("ipWhitelist");

    if (add_options->has_key("label")) {
      instance_label = add_options->get_string("label");
      mysqlsh::dba::validate_label(instance_label);

      if (!_metadata_storage->is_instance_label_unique(get_id(),
                                                       instance_label))
        throw shcore::Exception::argument_error(
            "An instance with label '" + instance_label +
            "' is already part of this InnoDB cluster");
    }

    if (add_options->has_key("localAddress"))
      local_address = add_options->get_string("localAddress");

    if (add_options->has_key("groupSeeds"))
      group_seeds = add_options->get_string("groupSeeds");
  }

  std::shared_ptr<mysqlshdk::db::ISession> session{establish_mysql_session(
      connection_options, current_shell_options()->get().wizards)};
  mysqlshdk::mysql::Instance target_instance(session);
  target_instance.cache_global_sysvars();

  // Retrieves the instance definition
  auto instance_cnx_opt = session->get_connection_options();

  std::string user = instance_cnx_opt.get_user();
  std::string super_user_password = instance_cnx_opt.get_password();
  std::string joiner_host = instance_cnx_opt.get_host();

  std::string instance_address = instance_cnx_opt.as_uri(only_transport());

  // Check whether the address being used is not in a known not-good case
  validate_instance_address(session, joiner_host, instance_cnx_opt.get_port());

  // Check instance configuration and state, like dba.checkInstance
  // But don't do it if it was already done by the caller
  if (!skip_instance_check) {
    std::shared_ptr<Cluster> cluster(_cluster.lock());
    if (!cluster)
      throw shcore::Exception::runtime_error(
          "Cluster object is no longer valid");
    ensure_instance_configuration_valid(
        &target_instance, cluster->get_provisioning_interface(), m_console);
  }

  // Check replication filters before creating the Metadata.
  validate_replication_filters(session);

  // Resolve the SSL Mode to use to configure the instance.
  std::string new_ssl_mode;
  std::string target;
  if (seed_instance) {
    new_ssl_mode = resolve_cluster_ssl_mode(session, ssl_mode);
    target = "cluster";
  } else {
    auto peer_session = _metadata_storage->get_session();
    new_ssl_mode = resolve_instance_ssl_mode(session, peer_session, ssl_mode);
    target = "instance";
  }

  if (new_ssl_mode != ssl_mode) {
    ssl_mode = new_ssl_mode;
    log_warning("SSL mode used to configure the %s: '%s'", target.c_str(),
                ssl_mode.c_str());
  }

  GRInstanceType type = get_gr_instance_type(session);

  if (type != GRInstanceType::Standalone &&
      type != GRInstanceType::StandaloneWithMetadata) {
    // Retrieves the new instance UUID
    std::string uuid;
    get_server_variable(session, "server_uuid", uuid);

    // Verifies if the instance is part of the cluster replication group
    auto cluster_session = _metadata_storage->get_session();

    // Verifies if this UUID is part of the current replication group
    if (is_server_on_replication_group(cluster_session, uuid)) {
      if (type == GRInstanceType::InnoDBCluster) {
        log_debug("Instance '%s' already managed by InnoDB cluster",
                  instance_address.c_str());
        throw shcore::Exception::runtime_error(
            "The instance '" + instance_address +
            "' is already part of this InnoDB cluster");
      } else {
        log_debug(
            "Instance '%s' is already part of a Replication Group, but not "
            "managed",
            instance_address.c_str());
      }
    } else {
      if (type == GRInstanceType::InnoDBCluster)
        throw shcore::Exception::runtime_error(
            "The instance '" + instance_address +
            "' is already part of another InnoDB cluster");
      else
        throw shcore::Exception::runtime_error(
            "The instance '" + instance_address +
            "' is already part of another Replication Group");
    }
  }

  // Check instance server UUID (must be unique among the cluster members).
  validate_server_uuid(session);

  bool is_instance_on_md =
      _metadata_storage->is_instance_on_replicaset(get_id(), instance_address);

  log_debug("RS %lu: Adding instance '%s' to replicaset%s",
            static_cast<unsigned long>(_id), instance_address.c_str(),
            is_instance_on_md ? " (already in MD)" : "");

  if (type == GRInstanceType::Standalone ||
      type == GRInstanceType::StandaloneWithMetadata) {
    log_debug("Instance '%s' is not yet in the cluster",
              instance_address.c_str());

    std::string replication_user(existing_replication_user);
    std::string replication_user_password(existing_replication_password);

    // Creates the replication user ONLY if not already given and if
    // skip_rpl_user was not set to true.
    if (!skip_rpl_user && replication_user.empty()) {
      // TODO(.) Replication user is not a metadata thing... all the GR
      // things should be moved out of metadata class
      _metadata_storage->create_repl_account(replication_user,
                                             replication_user_password);
      log_debug("Created replication user '%s'", replication_user.c_str());
    }

    // Call the gadget to bootstrap the group with this instance
    if (seed_instance) {
      log_info("Joining '%s' to group using account %s@%s",
               instance_address.c_str(), user.c_str(),
               instance_address.c_str());
      log_info("Using Group Replication group name: %s", group_name.c_str());
      log_info("Using Group Replication local address: %s",
               local_address.c_str());
      log_info("Using Group Replication group seeds: %s", group_seeds.c_str());

      set_group_replication_member_options(session, ssl_mode);

      // Call mysqlprovision to bootstrap the group using "start"
      success = do_join_replicaset(
          instance_cnx_opt, nullptr, super_user_password, replication_user,
          replication_user_password, ssl_mode, ip_whitelist, group_name,
          local_address, group_seeds, skip_rpl_user);
    } else {
      // We need to retrieve a peer instance, so let's use the Seed one
      // NOTE(rennox): In add instance this function is not used, the peer
      // instance is the session from the _metadata_storage: why the difference?
      std::string peer_instance = get_peer_instance();

      mysqlshdk::db::Connection_options peer =
          shcore::get_connection_options(peer_instance, false);

      // Sets the same user as the added instance
      peer.set_user(user);

      // Get SSL values to connect to peer instance
      auto md_ssl = _metadata_storage->get_session()
                        ->get_connection_options()
                        .get_ssl_options();
      if (md_ssl.has_data()) {
        auto peer_ssl = peer.get_ssl_options();
        if (md_ssl.has_ca()) peer_ssl.set_ca(md_ssl.get_ca());

        if (md_ssl.has_cert()) peer_ssl.set_cert(md_ssl.get_cert());

        if (md_ssl.has_key()) peer_ssl.set_key(md_ssl.get_key());
      }

      set_group_replication_member_options(session, ssl_mode);

      // if no group_seeds value was provided by the user, then,
      // before joining instance to cluster, get the values of the
      // gr_local_address from all the active members of the cluster
      if (group_seeds.empty()) group_seeds = get_cluster_group_seeds();
      log_info("Joining '%s' to group using account %s@%s to peer '%s'",
               instance_address.c_str(), user.c_str(), instance_address.c_str(),
               peer_instance.c_str());
      // Call mysqlprovision to do the work
      success = do_join_replicaset(instance_cnx_opt, &peer, super_user_password,
                                   replication_user, replication_user_password,
                                   ssl_mode, ip_whitelist, group_name,
                                   local_address, group_seeds, skip_rpl_user);
    }
  }
  if (success) {
    // If the instance is not on the Metadata, we must add it
    if (!is_instance_on_md)
      add_instance_metadata(instance_cnx_opt, instance_label);

    // Get the gr_address of the instance being added
    auto added_instance = mysqlshdk::mysql::Instance(session);
    std::string added_instance_address =
        session->get_connection_options().as_uri(only_transport());
    std::string added_instance_gr_address = *added_instance.get_sysvar_string(
        "group_replication_local_address",
        mysqlshdk::mysql::Var_qualifier::GLOBAL);

    // Update the group_seeds of the instance that was just added
    // If the groupSeeds option was used (not empty), we use
    // that value, otherwise we use the value of all the
    // group_replication_local_address of all the active instances
    update_group_replication_group_seeds(group_seeds,
                                         Gr_seeds_change_type::OVERRIDE,
                                         session, naming_style, m_console);
    // Update the group_replication_group_seeds of the members that
    // already belonged to the cluster and are either ONLINE or recovering
    // by adding the gr_local_address of the instance that was just added.
    std::vector<std::string> ignore_instances_vec = {instance_address};
    Gr_seeds_change_type change_type = Gr_seeds_change_type::ADD;

    execute_in_members(
        {"'ONLINE'", "'RECOVERING'"}, instance_cnx_opt, ignore_instances_vec,
        [added_instance_gr_address, change_type,
         this](std::shared_ptr<mysqlshdk::db::ISession> session) {
          update_group_replication_group_seeds(
              added_instance_gr_address, change_type, session,
              this->naming_style, this->m_console);
        });
    log_debug("Instance add finished");
  }

  return ret_val;
}

bool ReplicaSet::do_join_replicaset(
    const mysqlshdk::db::Connection_options &instance,
    mysqlshdk::db::Connection_options *peer,
    const std::string &super_user_password, const std::string &repl_user,
    const std::string &repl_user_password, const std::string &ssl_mode,
    const std::string &ip_whitelist, const std::string &group_name,
    const std::string &local_address, const std::string &group_seeds,
    bool skip_rpl_user) {
  shcore::Value ret_val;
  int exit_code = -1;

  bool is_seed_instance = peer ? false : true;

  shcore::Value::Array_type_ref errors, warnings;

  std::shared_ptr<Cluster> cluster(_cluster.lock());
  if (!cluster)
    throw shcore::Exception::runtime_error("Cluster object is no longer valid");

  if (is_seed_instance) {
    exit_code = cluster->get_provisioning_interface()->start_replicaset(
        instance, repl_user, super_user_password, repl_user_password,
        _topology_type == kTopologyMultiPrimary, ssl_mode, ip_whitelist,
        group_name, local_address, group_seeds, skip_rpl_user, &errors);
  } else {
    exit_code = cluster->get_provisioning_interface()->join_replicaset(
        instance, *peer, repl_user, super_user_password, repl_user_password,
        ssl_mode, ip_whitelist, local_address, group_seeds, skip_rpl_user,
        &errors);
  }

  if (exit_code == 0) {
    auto instance_url = instance.as_uri(user_transport());
    // If the exit_code is zero but there are errors
    // it means they're warnings and we must log them first
    if (errors) {
      for (auto error_object : *errors) {
        auto map = error_object.as_map();
        std::string error_str = map->get_string("msg");
        log_warning("DBA: %s : %s", instance_url.c_str(), error_str.c_str());
      }
    }
    if (is_seed_instance)
      ret_val = shcore::Value(
          "The instance '" + instance_url +
          "' was successfully added as seeding instance to the MySQL Cluster.");
    else
      ret_val = shcore::Value("The instance '" + instance_url +
                              "' was successfully added to the MySQL Cluster.");
  } else {
    throw shcore::Exception::runtime_error(
        get_mysqlprovision_error_string(errors));
  }

  return exit_code == 0;
}

#if DOXYGEN_CPP
/**
 * Use this function to rejoin an Instance to the ReplicaSet
 * \param args : A list of values to be used to add a Instance to the
 * ReplicaSet.
 *
 * This function returns an empty Value.
 */
#else
/**
 * Rejoin a Instance to the ReplicaSet
 * \param name The name of the Instance to be rejoined
 */
#if DOXYGEN_JS
Undefined ReplicaSet::rejoinInstance(String name, Dictionary options) {}
#elif DOXYGEN_PY
None ReplicaSet::rejoin_instance(str name, Dictionary options) {}
#endif
#endif  // DOXYGEN_CPP
shcore::Value ReplicaSet::rejoin_instance_(const shcore::Argument_list &args) {
  shcore::Value ret_val;
  args.ensure_count(1, 2, get_function_name("rejoinInstance").c_str());

  // Check if the ReplicaSet is empty
  if (_metadata_storage->is_replicaset_empty(get_id()))
    throw shcore::Exception::runtime_error(
        "ReplicaSet not initialized. Please add the Seed Instance using: "
        "addSeedInstance().");

  // Rejoin the Instance to the Default ReplicaSet
  try {
    auto instance_def =
        mysqlsh::get_connection_options(args, mysqlsh::PasswordFormat::OPTIONS);

    shcore::Value::Map_type_ref options;

    if (args.size() == 2) options = args.map_at(1);

    ret_val = rejoin_instance(&instance_def, options);
  }
  CATCH_AND_TRANSLATE_FUNCTION_EXCEPTION(get_function_name("rejoinInstance"));

  return ret_val;
}

/**
 * Get an up-to-date group seeds value based on the current list of active
 * members.
 *
 * An optional instance_session parameter can be provide that will be used to
 * get its current GR group seeds value and add the local address from the
 * active members (avoiding duplicates) to that initial value, allowing to
 * preserve the GR group seeds of the specified instance. If no
 * instance_session is given (nullptr) then the returned groups seeds value
 * will only be based on the currently active members, disregarding any current
 * GR group seed value on any instance (allowing to reset the group seed only
 * based on the currently active members).
 *
 * @param instance_session Session to the target instance to get the group
 *                         seeds for.
 * @return a string with a comma separated list of all the GR local address
 *         values of the currently active (ONLINE or RECOVERING) instances in
 *         the replicaset.
 */
std::string ReplicaSet::get_cluster_group_seeds(
    std::shared_ptr<mysqlshdk::db::ISession> instance_session) {
  // Get connection option for the metadata.
  std::shared_ptr<Cluster> cluster(_cluster.lock());
  if (!cluster)
    throw shcore::Exception::runtime_error("Cluster object is no longer valid");
  std::shared_ptr<mysqlshdk::db::ISession> cluster_session =
      cluster->get_group_session();
  Connection_options cluster_cnx_opt =
      cluster_session->get_connection_options();

  // Get list of active instances (ONLINE or RECOVERING)
  std::vector<Instance_definition> active_instances =
      _metadata_storage->get_replicaset_active_instances(_id);

  std::vector<std::string> gr_group_seeds_list;
  // If the target instance is provided, use its current GR group seed variable
  // value as starting point to append new (missing) values to it.
  if (instance_session) {
    auto instance = mysqlshdk::mysql::Instance(instance_session);
    // Get the instance GR group seeds and save it to the GR group seeds list.
    std::string gr_group_seeds =
        instance.get_sysvar_string("group_replication_group_seeds",
                                   mysqlshdk::mysql::Var_qualifier::GLOBAL);
    if (!gr_group_seeds.empty()) {
      gr_group_seeds_list = shcore::split_string(gr_group_seeds, ",");
    }
  }

  // Get the update GR group seed from local address of all active instances.
  for (Instance_definition &instance_def : active_instances) {
    std::string instance_address = instance_def.endpoint;
    Connection_options instance_cnx_opt =
        shcore::get_connection_options(instance_address, false);
    // It is assumed that the same user and password is used by all members.
    if (cluster_cnx_opt.has_user())
      instance_cnx_opt.set_user(cluster_cnx_opt.get_user());
    if (cluster_cnx_opt.has_password())
      instance_cnx_opt.set_password(cluster_cnx_opt.get_password());
    // Connect to the instance.
    std::shared_ptr<mysqlshdk::db::ISession> session;
    try {
      log_debug(
          "Connecting to instance '%s' to get its value for the "
          "group_replication_local_address variable.",
          instance_address.c_str());
      session = get_session(instance_cnx_opt);
    } catch (std::exception &e) {
      // Do not issue an error if we are unable to connect to the instance,
      // it might have failed in the meantime, just skip the use of its GR
      // local address.
      log_info(
          "Could not connect to instance '%s', its local address will not "
          "be used for the group seeds: %s",
          instance_address.c_str(), e.what());
      break;
    }
    auto instance = mysqlshdk::mysql::Instance(session);
    // Get the instance GR local address and add it to the GR group seeds list.
    std::string local_address =
        instance.get_sysvar_string("group_replication_local_address",
                                   mysqlshdk::mysql::Var_qualifier::GLOBAL);
    if (std::find(gr_group_seeds_list.begin(), gr_group_seeds_list.end(),
                  local_address) == gr_group_seeds_list.end()) {
      // Only add the local address if not already in the group seed list,
      // avoiding duplicates.
      gr_group_seeds_list.push_back(local_address);
    }
    session->close();
  }
  return shcore::str_join(gr_group_seeds_list, ",");
}

shcore::Value ReplicaSet::rejoin_instance(
    mysqlshdk::db::Connection_options *instance_def,
    const shcore::Value::Map_type_ref &rejoin_options) {
  std::shared_ptr<Cluster> cluster(_cluster.lock());
  if (!cluster)
    throw shcore::Exception::runtime_error("Cluster object is no longer valid");

  shcore::Value ret_val;
  // SSL Mode AUTO by default
  std::string ssl_mode = mysqlsh::dba::kMemberSSLModeAuto;
  std::string ip_whitelist, user, password;
  shcore::Value::Array_type_ref errors;
  std::shared_ptr<mysqlshdk::db::ISession> session, seed_session;

  // Retrieves the options
  if (rejoin_options) {
    shcore::Argument_map rejoin_instance_map(*rejoin_options);
    rejoin_instance_map.ensure_keys({}, _add_instance_opts, " options");

    // Validate SSL options for the cluster instance
    validate_ssl_instance_options(rejoin_options);

    // Validate ip whitelist option
    validate_ip_whitelist_option(rejoin_options);

    if (rejoin_options->has_key("memberSslMode")) {
      ssl_mode = rejoin_options->get_string("memberSslMode");
      m_console->print_warning(kWarningDeprecateSslMode);
      m_console->println();
    }

    if (rejoin_options->has_key("ipWhitelist"))
      ip_whitelist = rejoin_options->get_string("ipWhitelist");
  }

  if (!instance_def->has_port())
    instance_def->set_port(mysqlshdk::db::k_default_mysql_port);

  instance_def->set_default_connection_data();

  std::string instance_address = instance_def->as_uri(only_transport());

  // Check if the instance is part of the Metadata
  if (!_metadata_storage->is_instance_on_replicaset(get_id(),
                                                    instance_address)) {
    std::string message = "The instance '" + instance_address + "' " +
                          "does not belong to the ReplicaSet: '" +
                          get_member("name").as_string() + "'.";

    throw shcore::Exception::runtime_error(message);
  }

  // Before rejoining an instance we must verify if the instance's
  // 'group_replication_group_name' matches the one registered in the
  // Metadata (BUG #26159339)
  //
  // Before rejoining an instance we must also verify if the group has quorum
  // and if the gr plugin is active otherwise we may end up hanging the system

  // Validate 'group_replication_group_name'
  {
    try {
      log_info("Opening a new session to the rejoining instance %s",
               instance_address.c_str());
      session = establish_mysql_session(*instance_def,
                                        current_shell_options()->get().wizards);
    } catch (std::exception &e) {
      log_error("Could not open connection to '%s': %s",
                instance_address.c_str(), e.what());
      throw;
    }

    if (!validate_replicaset_group_name(session, get_group_name())) {
      std::string nice_error =
          "The instance '" + instance_address +
          "' "
          "may belong to a different ReplicaSet as the one registered "
          "in the Metadata since the value of "
          "'group_replication_group_name' does not match the one "
          "registered in the ReplicaSet's Metadata: possible split-brain "
          "scenario. Please remove the instance from the cluster.";

      session->close();

      throw shcore::Exception::runtime_error(nice_error);
    }
  }

  // In order to be able to rejoin the instance to the cluster we need the seed
  // instance.

  // Get the seed instance
  std::string seed_instance = get_peer_instance();

  auto connection_options =
      shcore::get_connection_options(seed_instance, false);

  // To be able to establish a session to the seed instance we need a username
  // and password. Taking into consideration the assumption that all instances
  // of the cluster use the same credentials we can obtain the ones of the
  // current metadata session

  // Get the current cluster session from the metadata
  auto md_session = _metadata_storage->get_session();
  auto md_connection_options = md_session->get_connection_options();

  if (md_connection_options.has_user()) user = md_connection_options.get_user();

  if (md_connection_options.has_password())
    password = md_connection_options.get_password();

  connection_options.set_user(user);
  connection_options.set_password(password);

  // Establish a session to the seed instance
  try {
    log_info("Opening a new session to seed instance: %s",
             seed_instance.c_str());
    seed_session = get_session(connection_options);
  } catch (std::exception &e) {
    throw Exception::runtime_error("Could not open a connection to " +
                                   seed_instance + ": " + e.what() + ".");
  }

  // Verify if the group_replication plugin is active on the seed instance
  {
    log_info(
        "Verifying if the group_replication plugin is active on the seed "
        "instance %s",
        seed_instance.c_str());

    std::string plugin_status =
        get_plugin_status(seed_session, "group_replication");

    if (plugin_status != "ACTIVE") {
      throw shcore::Exception::runtime_error(
          "Cannot rejoin instance. The seed instance doesn't have "
          "group-replication active.");
    }
  }

  // Verify if the instance being added is MISSING, otherwise throw an error
  // Bug#26870329
  {
    // get server_uuid from the instance that we're trying to rejoin
    if (!validate_instance_rejoinable(session, _metadata_storage, _id)) {
      // instance not missing, so throw an error
      auto instance = mysqlshdk::mysql::Instance(session);
      auto member_state =
          mysqlshdk::gr::to_string(mysqlshdk::gr::get_member_state(instance));
      std::string nice_error_msg = "Cannot rejoin instance '" +
                                   instance_address + "' to the ReplicaSet '" +
                                   get_member("name").as_string() +
                                   "' since it is an active (" + member_state +
                                   ") member of the ReplicaSet.";
      session->close();
      throw shcore::Exception::runtime_error(nice_error_msg);
    }
  }

  // Get the up-to-date GR group seeds values (with the GR local address from
  // all currently active instances).
  std::string gr_group_seeds = get_cluster_group_seeds(session);

  // join Instance to cluster
  {
    int exit_code;

    // Check replication filters before creating the Metadata.
    validate_replication_filters(session);

    std::string new_ssl_mode;
    // Resolve the SSL Mode to use to configure the instance.
    new_ssl_mode = resolve_instance_ssl_mode(session, seed_session, ssl_mode);
    if (new_ssl_mode != ssl_mode) {
      ssl_mode = new_ssl_mode;
      log_warning("SSL mode used to configure the instance: '%s'",
                  ssl_mode.c_str());
    }

    // Get SSL values to connect to peer instance
    auto seed_instance_def = seed_session->get_connection_options();

    // Stop group-replication
    log_info("Stopping group-replication at instance %s",
             instance_address.c_str());
    session->execute("STOP GROUP_REPLICATION");

    // Get the seed session connection data
    // use mysqlprovision to rejoin the cluster.
    exit_code = cluster->get_provisioning_interface()->join_replicaset(
        session->get_connection_options(), seed_instance_def, "",
        session->get_connection_options().get_password(), "", ssl_mode,
        ip_whitelist, "", gr_group_seeds, true, &errors);
    if (exit_code == 0) {
      log_info("The instance '%s' was successfully rejoined on the cluster.",
               instance_address.c_str());
    } else {
      throw shcore::Exception::runtime_error(
          get_mysqlprovision_error_string(errors));
    }
  }
  return ret_val;
}

#if DOXYGEN_CPP
/**
 * Use this function to remove a Instance from the ReplicaSet object
 * \param args : A list of values to be used to remove a Instance to the
 * Cluster.
 *
 * This function returns an empty Value.
 */
#else
/**
 * Removes a Instance from the ReplicaSet
 * \param name The name of the Instance to be removed
 */
#if DOXYGEN_JS
Undefined ReplicaSet::removeInstance(String name) {}
#elif DOXYGEN_PY
None ReplicaSet::remove_instance(str name) {}
#endif

/**
 * Removes a Instance from the ReplicaSet
 * \param doc The Document representing the Instance to be removed
 */
#if DOXYGEN_JS
Undefined ReplicaSet::removeInstance(Document doc) {}
#elif DOXYGEN_PY
None ReplicaSet::remove_instance(Document doc) {}
#endif
#endif

shcore::Value ReplicaSet::remove_instance(const shcore::Argument_list &args) {
  mysqlshdk::utils::nullable<bool> force;
  bool interactive;
  std::string password;
  mysqlshdk::db::Connection_options instance_cnx_opts;

  // Get target instance connection options.
  instance_cnx_opts =
      mysqlsh::get_connection_options(args, mysqlsh::PasswordFormat::OPTIONS);

  std::shared_ptr<Cluster> cluster(_cluster.lock());
  if (!cluster)
    throw shcore::Exception::runtime_error("Cluster object is no longer valid");
  interactive = cluster->is_interactive();

  // Get optional options.
  if (args.size() == 2) {
    Unpack_options(args.map_at(1))
        .optional("force", &force)
        .optional("interactive", &interactive)
        .optional_ci("password", &password)
        .end();
  }

  // Overide password if provided in options dictionary.
  if (!password.empty()) {
    instance_cnx_opts.set_password(password);
  }

  // Remove the Instance from the ReplicaSet
  try {
    // Create the remove_instance command and execute it.
    Remove_instance op_remove_instance(instance_cnx_opts, interactive, force,
                                       cluster, shared_from_this(),
                                       this->naming_style, m_console);
    // Always execute finish when leaving "try catch".
    auto finally = shcore::on_leave_scope(
        [&op_remove_instance]() { op_remove_instance.finish(); });
    // Prepare the remove_instance command execution (validations).
    op_remove_instance.prepare();
    // Execute remove_instance operations.
    op_remove_instance.execute();
  } catch (...) {
    throw;
  }

  return shcore::Value();
}

/**
 * Auxiliary function to re-enable super_read_only.
 *
 * @param super_read_only boolean with the initial super_read_only value.
 * @param instance target instance object.
 * @param instance_address string with the target instance address.
 */
static void reenable_super_read_only(
    const mysqlshdk::utils::nullable<bool> &super_read_only,
    const mysqlshdk::mysql::Instance &instance,
    const std::string &instance_address) {
  // Re-enable super_read_only if previously enabled.
  if (*super_read_only) {
    log_debug("Re-enabling super_read_only on instance '%s'.",
              instance_address.c_str());
    instance.set_sysvar("super_read_only", true,
                        mysqlshdk::mysql::Var_qualifier::GLOBAL);
  }
}

void ReplicaSet::update_group_members_for_removed_member(
    const std::string &local_gr_address,
    const mysqlshdk::mysql::Instance &instance, bool remove_rpl_user_on_group) {
  // Iterate through all ONLINE and RECOVERING cluster members and update their
  // group_replication_group_seeds value by removing the gr_local_address
  // of the instance that was removed
  std::vector<std::string> ignore_instances_vec;
  Gr_seeds_change_type change_type = Gr_seeds_change_type::REMOVE;

  // It is assumed that the same connection credentials can be used to all the
  // instances in the cluster.
  Connection_options instances_cnx_opts = instance.get_connection_options();

  log_debug("Updating group_replication_group_seeds of cluster members");
  execute_in_members({"'ONLINE'", "'RECOVERING'"}, instances_cnx_opts,
                     ignore_instances_vec,
                     [local_gr_address, change_type,
                      this](std::shared_ptr<mysqlshdk::db::ISession> session) {
                       update_group_replication_group_seeds(
                           local_gr_address, change_type, session,
                           this->naming_style, this->m_console);
                     });

  // Remove the replication users on the instance and members if
  // remove_rpl_user_on_group = true.
  if (remove_rpl_user_on_group) {
    log_debug("Removing replication user on instance and replicaset members");
  } else {
    log_debug("Removing replication user on instance");
  }
  remove_replication_users(instance, remove_rpl_user_on_group);
}

void ReplicaSet::remove_replication_users(
    const mysqlshdk::mysql::Instance &instance, bool remove_rpl_user_on_group) {
  std::string instance_address =
      instance.get_connection_options().as_uri(only_transport());
  // Check if super_read_only is enabled and disable it to remove replication
  // users and metadata.
  mysqlshdk::utils::nullable<bool> super_read_only = instance.get_sysvar_bool(
      "super_read_only", mysqlshdk::mysql::Var_qualifier::GLOBAL);
  if (*super_read_only) {
    log_debug(
        "Disabling super_read_only to remove replication users on "
        "instance '%s'.",
        instance_address.c_str());
    instance.set_sysvar("super_read_only", false,
                        mysqlshdk::mysql::Var_qualifier::GLOBAL);
  }

  // Remove all replication (recovery users) on the removed instance,
  // disabling binary logging (avoid being replicated).
  try {
    // Re-enable super_read_only if previously enabled when leaving "try catch".
    auto finally =
        shcore::on_leave_scope([super_read_only, instance, instance_address]() {
          reenable_super_read_only(super_read_only, instance, instance_address);
        });
    instance.set_sysvar("sql_log_bin", static_cast<const int64_t>(0),
                        mysqlshdk::mysql::Var_qualifier::SESSION);

    log_debug("Removing InnoDB Cluster replication users on instance '%s'.",
              instance_address.c_str());
    instance.drop_users_with_regexp("'mysql_innodb_cluster_r[0-9]{10}.*");

    instance.set_sysvar("sql_log_bin", static_cast<const int64_t>(1),
                        mysqlshdk::mysql::Var_qualifier::SESSION);
  } catch (shcore::Exception &err) {
    throw;
  }

  if (remove_rpl_user_on_group) {
    // Get replication user (recovery) used by the instance to remove
    // on remaining members.
    std::string rpl_user = mysqlshdk::gr::get_recovery_user(instance);

    Connection_options instance_cnx_opts = instance.get_connection_options();
    // Get a primary (master) instance.
    std::string primary_instance_address = get_peer_instance();
    mysqlshdk::db::Connection_options primary_cnx_opts =
        shcore::get_connection_options(primary_instance_address, false);
    // If user credentials are missing get them from the removed instance.
    // It is assumed that the same user and password is used by all members.
    if (!primary_cnx_opts.has_user() || !primary_cnx_opts.has_password()) {
      primary_cnx_opts.set_user(instance_cnx_opts.get_user());
      primary_cnx_opts.set_password(instance_cnx_opts.get_password());
    }
    log_debug(
        "Opening a new session to instance '%s' to remove the "
        "replication user for instance '%s'.",
        primary_instance_address.c_str(), instance_address.c_str());
    std::shared_ptr<mysqlshdk::db::ISession> _peer_session =
        mysqlshdk::db::mysql::Session::create();
    _peer_session->connect(primary_cnx_opts);
    mysqlshdk::mysql::Instance primary_instance(_peer_session);

    // Remove the replication user used by the removed instance on all
    // cluster members through the primary (using replication).
    primary_instance.drop_users_with_regexp(rpl_user + ".*");

    primary_instance.close_session();
  }
}

/**
 * Finalize removal of an instance after leaving the replicaset.
 *
 * This function performs operations to finalize the removal of an instance
 * after leaving the replicaset (GR is stopped), more specifically:
 *   - Update the group_replication_group_seeds on the other members of the
 *     cluster;
 *   - Disable GR start on boot (to avoid instance from rejoining the group);
 *   - Reset and persist the values for group_replication_bootstrap_group,
 *     group_replication_force_members, group_replication_group_seeds and
 *     group_replication_local_address.
 *   - Remove replication (recovery) users;
 *
 * @param instance_cnx_opts Connection options for the instance to remove.
 * @param remove_rpl_user_on_group boolean indicating if the replication
 *        (recovery) user used by the instance should be removed on the
 *        remaining members of the GR group (replicaset). If true then remove
 *        the recovery user used by the instance on the other members through
 *        a primary instance, otherwise skip it (just remove replication users
 *        on the target instance).
 */
void ReplicaSet::finalize_instance_removal(
    const mysqlshdk::db::Connection_options &instance_cnx_opts,
    bool remove_rpl_user_on_group) {
  // Create and open an instance session.
  std::string instance_address = instance_cnx_opts.as_uri(only_transport());
  log_debug(
      "Opening a new session to instance: '%s' to disable and persist "
      "'group_replication_start_on_boot', if supported by the server.",
      instance_address.c_str());
  std::shared_ptr<mysqlshdk::db::ISession> _session =
      get_session(instance_cnx_opts);
  mysqlshdk::mysql::Instance instance(_session);

  // get the group_replication_local address value of the instance we will
  // remove from the replicaset (to use later to update other members seeds).
  std::string local_gr_address =
      *instance.get_sysvar_string("group_replication_local_address",
                                  mysqlshdk::mysql::Var_qualifier::GLOBAL);

  // Disable and persist GR start on boot if leave-replicaset succeed and
  // reset values for group_replication_bootstrap_group,
  // group_replication_force_members, group_replication_group_seeds and
  // group_replication_local_address
  // NOTE: Only for server supporting SET PERSIST, version must be >= 8.0.11
  // due to BUG#26495619.
  if (instance.get_version() >= mysqlshdk::utils::Version(8, 0, 11)) {
    instance.set_sysvar("group_replication_start_on_boot", false,
                        mysqlshdk::mysql::Var_qualifier::PERSIST);

    const char *k_gr_remove_instance_vars_default[]{
        "group_replication_bootstrap_group", "group_replication_force_members",
        "group_replication_group_seeds", "group_replication_local_address"};
    for (auto gr_var : k_gr_remove_instance_vars_default) {
      instance.set_sysvar_default(gr_var,
                                  mysqlshdk::mysql::Var_qualifier::PERSIST);
    }

    bool persist_load = *instance.get_sysvar_bool(
        "persisted_globals_load", mysqlshdk::mysql::Var_qualifier::GLOBAL);
    if (!persist_load) {
      std::string warn_msg =
          "On instance '" + instance_address +
          "' the persisted cluster configuration will not be loaded upon "
          "reboot since 'persisted-globals-load' is set to 'OFF'. Please set "
          "'persisted-globals-load' to 'ON' on the configuration file or set "
          "the 'group_replication_start_on_boot' variable to 'OFF' in the "
          "server configuration file, otherwise it might rejoin the cluster "
          "upon restart.";

      m_console->print_warning(warn_msg);
    }
  } else {
    // NOTE: mysqlprovision already set group_replication_start_on_boot=OFF
    std::string warn_msg =
        "On instance '" + instance_address +
        "' configuration cannot be persisted since MySQL version " +
        _session->get_server_version().get_base() +
        " does not support the SET PERSIST command (MySQL version >= 8.0.11 "
        "required). Please set the 'group_replication_start_on_boot' variable "
        "to 'OFF' in the server configuration file, otherwise it might rejoin "
        "the cluster upon restart.";

    m_console->print_warning(warn_msg);
  }

  // If remove operation went well, update the group_replication_group_seeds
  // value on all replicaset members by removing the gr_local_address
  // of the instance that was removed and remove replication users.
  update_group_members_for_removed_member(local_gr_address, instance,
                                          remove_rpl_user_on_group);

  instance.close_session();
}

shcore::Value ReplicaSet::dissolve(const shcore::Argument_list &args) {
  shcore::Value ret_val;
  args.ensure_count(0, 1, get_function_name("dissolve").c_str());

  try {
    bool force = false;
    shcore::Value::Map_type_ref options;

    if (args.size() == 1) options = args.map_at(0);

    if (options) {
      shcore::Argument_map opt_map(*options);

      opt_map.ensure_keys({}, {"force"}, "dissolve options");

      if (opt_map.has_key("force")) force = opt_map.bool_at("force");
    }

    if (!force && _metadata_storage->is_replicaset_active(get_id()))
      throw shcore::Exception::runtime_error(
          "Cannot dissolve the ReplicaSet: the ReplicaSet is active.");

    MetadataStorage::Transaction tx(_metadata_storage);

    uint64_t rset_id = get_id();

    // remove all the instances from the ReplicaSet
    auto instances = _metadata_storage->get_replicaset_instances(rset_id);

    _metadata_storage->drop_replicaset(rset_id);

    tx.commit();

    remove_instances_from_gr(instances);
  }
  CATCH_AND_TRANSLATE_FUNCTION_EXCEPTION(get_function_name("dissolve"));

  return ret_val;
}

void ReplicaSet::remove_instances_from_gr(
    const std::vector<Instance_definition> &instances) {
  MetadataStorage::Transaction tx(_metadata_storage);

  auto instance_session(_metadata_storage->get_session());
  auto connection_options = instance_session->get_connection_options();

  /* This function usually starts by removing from the replicaset the R/W
   * instance, which usually is the first on the instances list, and on
   * primary-master mode that implies a new master election. So to avoid GR
   * BUG#24818604 , we must leave the R/W instance for last.
   */

  // Get the R/W instance
  std::string master_uuid, master_instance;
  get_status_variable(instance_session, "group_replication_primary_member",
                      master_uuid, false);

  if (!master_uuid.empty()) {
    for (const auto &value : instances) {
      if (value.uuid == master_uuid) {
        master_instance = value.endpoint;
      }
    }
  }

  for (const auto &value : instances) {
    std::string instance_str = value.endpoint;

    if (instance_str != master_instance) {
      remove_instance_from_gr(instance_str, connection_options);
    }
  }

  // Remove the master instance
  if (!master_uuid.empty()) {
    remove_instance_from_gr(master_instance, connection_options);
  }

  tx.commit();
}

void ReplicaSet::remove_instance_from_gr(
    const std::string &instance_str,
    const mysqlshdk::db::Connection_options &data) {
  std::shared_ptr<Cluster> cluster(_cluster.lock());
  if (!cluster)
    throw shcore::Exception::runtime_error("Cluster object is no longer valid");

  auto instance_cnx_opts = shcore::get_connection_options(instance_str, false);
  instance_cnx_opts.set_user(data.get_user());
  instance_cnx_opts.set_password(data.get_password());

  if (data.get_ssl_options().has_data()) {
    auto cluster_ssl = data.get_ssl_options();
    auto instance_ssl = instance_cnx_opts.get_ssl_options();

    if (cluster_ssl.has_ca()) instance_ssl.set_ca(cluster_ssl.get_ca());
    if (cluster_ssl.has_cert()) instance_ssl.set_cert(cluster_ssl.get_cert());
    if (cluster_ssl.has_key()) instance_ssl.set_key(cluster_ssl.get_key());
  }

  shcore::Value::Array_type_ref errors;

  // Leave the replicaset
  int exit_code = cluster->get_provisioning_interface()->leave_replicaset(
      instance_cnx_opts, &errors);
  if (exit_code != 0) {
    throw shcore::Exception::runtime_error(
        get_mysqlprovision_error_string(errors));
  } else {
    // Finalize instance removal after successfully leaving replicaset.
    // - Disable and persist GR start on boot;
    // - Remove replication (recovery) users;
    // NOTE: This function is used only to dissolve a cluster, removing
    // all the replication users on all members, therefore there is no need to
    // specifically get the replication user used by the removed instance to
    // drop it on the remaining cluster members.
    // In addition, the current implementation drops the cluster metadata before
    // executing this function on all members, not making it possible afterwards
    // to obtain the primary member information (connection) from the metadata.
    // The primary member connection information would had to be obtained
    // before removing the metadata and passed as parameter, but it is not
    // needed currently. However, this is something to keep in mind for future
    // improvements.
    finalize_instance_removal(instance_cnx_opts, false);
  }
}

shcore::Value ReplicaSet::disable(const shcore::Argument_list &args) {
  shcore::Value ret_val;

  args.ensure_count(0, get_function_name("disable").c_str());

  try {
    MetadataStorage::Transaction tx(_metadata_storage);

    // Get all instances of the replicaset
    auto instances = _metadata_storage->get_replicaset_instances(get_id());

    // Update the metadata to turn 'active' off
    _metadata_storage->disable_replicaset(get_id());

    tx.commit();

    remove_instances_from_gr(instances);
  }
  CATCH_AND_TRANSLATE_FUNCTION_EXCEPTION(get_function_name("disable"));

  return ret_val;
}

shcore::Value ReplicaSet::rescan(const shcore::Argument_list &args) {
  shcore::Value ret_val;

  try {
    ret_val = shcore::Value(_rescan(args));
  }
  CATCH_AND_TRANSLATE_FUNCTION_EXCEPTION(get_function_name("rescan"));

  return ret_val;
}

shcore::Value::Map_type_ref ReplicaSet::_rescan(
    const shcore::Argument_list &args) {
  shcore::Value::Map_type_ref ret_val(new shcore::Value::Map_type());

  // Set the ReplicaSet name on the result map
  (*ret_val)["name"] = shcore::Value(_name);

  std::vector<NewInstanceInfo> newly_discovered_instances_list =
      get_newly_discovered_instances(_metadata_storage, _id);

  // Creates the newlyDiscoveredInstances map
  shcore::Value::Array_type_ref newly_discovered_instances(
      new shcore::Value::Array_type());

  for (auto &instance : newly_discovered_instances_list) {
    shcore::Value::Map_type_ref newly_discovered_instance(
        new shcore::Value::Map_type());
    (*newly_discovered_instance)["member_id"] =
        shcore::Value(instance.member_id);
    (*newly_discovered_instance)["name"] = shcore::Value::Null();

    std::string instance_address =
        instance.host + ":" + std::to_string(instance.port);

    (*newly_discovered_instance)["host"] = shcore::Value(instance_address);
    newly_discovered_instances->push_back(
        shcore::Value(newly_discovered_instance));
  }
  // Add the newly_discovered_instances list to the result Map
  (*ret_val)["newlyDiscoveredInstances"] =
      shcore::Value(newly_discovered_instances);

  shcore::Value unavailable_instances_result;

  std::vector<MissingInstanceInfo> unavailable_instances_list =
      get_unavailable_instances(_metadata_storage, _id);

  // Creates the unavailableInstances array
  shcore::Value::Array_type_ref unavailable_instances(
      new shcore::Value::Array_type());

  for (auto &instance : unavailable_instances_list) {
    shcore::Value::Map_type_ref unavailable_instance(
        new shcore::Value::Map_type());
    (*unavailable_instance)["member_id"] = shcore::Value(instance.id);
    (*unavailable_instance)["label"] = shcore::Value(instance.label);
    (*unavailable_instance)["host"] = shcore::Value(instance.host);

    unavailable_instances->push_back(shcore::Value(unavailable_instance));
  }
  // Add the missing_instances list to the result Map
  (*ret_val)["unavailableInstances"] = shcore::Value(unavailable_instances);

  return ret_val;
}

std::string ReplicaSet::get_peer_instance() {
  std::string master_uuid, master_instance;
  std::vector<Instance_definition> instances;

  // We need to retrieve a peer instance, so let's use the Seed one
  // If using single-primary mode the Seed instance is the primary
  std::shared_ptr<Cluster> cluster(_cluster.lock());
  if (!cluster)
    throw shcore::Exception::runtime_error("Cluster object is no longer valid");
  std::shared_ptr<mysqlshdk::db::ISession> cluster_session =
      cluster->get_group_session();
  Connection_options cluster_cnx_opt =
      cluster_session->get_connection_options();

  get_status_variable(cluster_session, "group_replication_primary_member",
                      master_uuid, false);

  // If the primary UUID is empty, instance might have left the cluster and
  // the primary UUID needs to be obtained from another "online" instance.
  if (master_uuid.empty()) {
    // Get all known instances from metadata.
    instances =
        _metadata_storage->get_replicaset_instances(get_id(), false, {});
    for (auto &instance : instances) {
      // Connect to instance.
      mysqlshdk::db::Connection_options instance_cnx_opts =
          shcore::get_connection_options(instance.endpoint, false);
      // If user credentials are missing get them from the cluster instance.
      // It is assumed that the same user and password is used by all members.
      if (!instance_cnx_opts.has_user() || !instance_cnx_opts.has_password()) {
        instance_cnx_opts.set_user(cluster_cnx_opt.get_user());
        instance_cnx_opts.set_password(cluster_cnx_opt.get_password());
      }
      try {
        log_debug(
            "Opening a new session to instance '%s' to find the primary "
            "member.",
            instance_cnx_opts.as_uri(only_transport()).c_str());
        std::shared_ptr<mysqlshdk::db::ISession> _instance_session =
            mysqlshdk::db::mysql::Session::create();
        _instance_session->connect(instance_cnx_opts);
        // Get GR primary member UUID.
        get_status_variable(_instance_session,
                            "group_replication_primary_member", master_uuid,
                            false);
        if (!master_uuid.empty()) {
          // Primary UUID found. Update list of online members using this
          // alternative instance.
          instances = _metadata_storage->get_replicaset_online_instances(
              get_id(), _instance_session);
          break;  // Primary UUID found (no need to look on other instances).
        }
        _instance_session->close();
      } catch (const std::exception &) {
        // Ignore error connecting to instance, since it might not be available.
        continue;
      }
    }
  } else {
    // Get ther list of online members.
    instances = _metadata_storage->get_replicaset_online_instances(get_id());
  }

  if (!master_uuid.empty()) {
    for (auto &instance : instances) {
      if (instance.uuid == master_uuid) {
        master_instance = instance.endpoint;
        break;  // Found matching "online" instance.
      }
    }
  } else if (!instances.empty()) {
    // If in multi-primary mode, any instance works
    // so we can get the first one that is online
    auto instance = instances[0];
    master_instance = instance.endpoint;
  }

  return master_instance;
}

shcore::Value ReplicaSet::check_instance_state(
    const shcore::Argument_list &args) {
  shcore::Value ret_val;
  args.ensure_count(1, 2, get_function_name("checkInstanceState").c_str());

  // Verifies the transaction state of the instance ins relation to the cluster
  try {
    ret_val = retrieve_instance_state(args);
  }
  CATCH_AND_TRANSLATE_FUNCTION_EXCEPTION(get_function_name("getInstanceState"));

  return ret_val;
}

shcore::Value ReplicaSet::retrieve_instance_state(
    const shcore::Argument_list &args) {
  auto instance_def =
      mysqlsh::get_connection_options(args, PasswordFormat::STRING);

  validate_connection_options(instance_def);

  if (!instance_def.has_port())
    instance_def.set_port(mysqlshdk::db::k_default_mysql_port);

  auto instance_session = establish_mysql_session(
      instance_def, current_shell_options()->get().wizards);

  // We will work with the session saved on the metadata which points to the
  // cluster Assuming it is the R/W instance
  auto master_session = _metadata_storage->get_session();

  // We have to retrieve these variables to do the actual state validation
  std::string master_gtid_executed;
  std::string master_gtid_purged;
  std::string instance_gtid_executed;
  std::string instance_gtid_purged;

  get_gtid_state_variables(master_session, master_gtid_executed,
                           master_gtid_purged);
  get_gtid_state_variables(instance_session, instance_gtid_executed,
                           instance_gtid_purged);

  // Now we perform the validation
  SlaveReplicationState state =
      get_slave_replication_state(master_session, instance_gtid_executed);

  std::string reason;
  std::string status;
  switch (state) {
    case SlaveReplicationState::Diverged:
      status = "error";
      reason = "diverged";
      break;
    case SlaveReplicationState::Irrecoverable:
      status = "error";
      reason = "lost_transactions";
      break;
    case SlaveReplicationState::Recoverable:
      status = "ok";
      reason = "recoverable";
      break;
    case SlaveReplicationState::New:
      status = "ok";
      reason = "new";
      break;
  }

  shcore::Value::Map_type_ref ret_val(new shcore::Value::Map_type());

  (*ret_val)["state"] = shcore::Value(status);
  (*ret_val)["reason"] = shcore::Value(reason);

  return shcore::Value(ret_val);
}

void ReplicaSet::add_instance_metadata(
    const mysqlshdk::db::Connection_options &instance_definition,
    const std::string &label) {
  log_debug("Adding instance to metadata");

  MetadataStorage::Transaction tx(_metadata_storage);

  int port = -1;
  int xport = -1;
  std::string local_gr_address;

  std::string joiner_host;

  // Check if the instance was already added
  std::string instance_address = instance_definition.as_uri(only_transport());

  std::string mysql_server_uuid;
  std::string mysql_server_address;

  log_debug("Connecting to '%s' to query for metadata information...",
            instance_address.c_str());
  // get the server_uuid from the joining instance
  {
    std::shared_ptr<mysqlshdk::db::ISession> classic;
    std::string joiner_user;
    try {
      classic = establish_mysql_session(instance_definition,
                                        current_shell_options()->get().wizards);

      auto options = classic->get_connection_options();
      port = options.get_port();
      xport = port * 10;
      joiner_host = options.get_host();
      instance_address = options.as_uri(only_transport());
      joiner_user = options.get_user();
    } catch (Exception &e) {
      std::stringstream ss;
      ss << "Error opening session to '" << instance_address
         << "': " << e.what();
      log_warning("%s", ss.str().c_str());

      // Check if we're adopting a GR cluster, if so, it could happen that
      // we can't connect to it because root@localhost exists but root@hostname
      // doesn't (GR keeps the hostname in the members table)
      if (e.is_mysql() && e.code() == 1045) {  // access denied
        std::stringstream se;
        se << "Access denied connecting to new instance " << instance_address
           << ".\n"
           << "Please ensure all instances in the same group/replicaset have"
              " the same password for account '"
              ""
           << joiner_user
           << "' and that it is accessible from the host mysqlsh is running "
              "from.";
        throw Exception::runtime_error(se.str());
      }
      throw Exception::runtime_error(ss.str());
    }
    {
      // Query UUID of the member and its public hostname
      auto result = classic->query("SELECT @@server_uuid");
      auto row = result->fetch_one();
      if (row) {
        mysql_server_uuid = row->get_as_string(0);
      } else {
        throw Exception::runtime_error("@@server_uuid could not be queried");
      }
    }
    try {
      auto result = classic->query("SELECT @@mysqlx_port");
      auto xport_row = result->fetch_one();
      if (xport_row) xport = xport_row->get_int(0);
    } catch (std::exception &e) {
      log_info("Could not query xplugin port, using default value: %s",
               e.what());
    }

    // Loads the local HR host data
    get_server_variable(classic, "group_replication_local_address",
                        local_gr_address, false);

    // NOTE(rennox): This validation is completely weird, mysql_server_address
    // has not been used so far...
    if (!mysql_server_address.empty() && mysql_server_address != joiner_host) {
      log_info("Normalized address of '%s' to '%s'", joiner_host.c_str(),
               mysql_server_address.c_str());

      instance_address = mysql_server_address + ":" + std::to_string(port);
    } else {
      mysql_server_address = joiner_host;
    }
  }
  std::string instance_xaddress;
  instance_xaddress = mysql_server_address + ":" + std::to_string(xport);
  Instance_definition instance;

  instance.role = "HA";
  instance.endpoint = instance_address;
  instance.xendpoint = instance_xaddress;
  instance.grendpoint = local_gr_address;
  instance.uuid = mysql_server_uuid;

  instance.label = label.empty() ? instance_address : label;

  // update the metadata with the host
  // NOTE(rennox): the old insert_host function was reading from the map
  // the keys location and id_host (<--- no, is not a typo, id_host)
  // where does those values come from????
  uint32_t host_id = _metadata_storage->insert_host(joiner_host, "", "");

  instance.host_id = host_id;
  instance.replicaset_id = get_id();

  // And the instance
  _metadata_storage->insert_instance(instance);

  tx.commit();
}

void ReplicaSet::remove_instance_metadata(
    const mysqlshdk::db::Connection_options &instance_def) {
  log_debug("Removing instance from metadata");

  MetadataStorage::Transaction tx(_metadata_storage);

  std::string port = std::to_string(instance_def.get_port());

  std::string host = instance_def.get_host();

  // Check if the instance was already added
  std::string instance_address = host + ":" + port;

  _metadata_storage->remove_instance(instance_address);

  tx.commit();
}

std::vector<std::string> ReplicaSet::get_online_instances() {
  std::vector<std::string> online_instances_array;

  auto online_instances =
      _metadata_storage->get_replicaset_online_instances(_id);

  for (auto &instance : online_instances) {
    // TODO: Review if end point is the right thing
    std::string instance_host = instance.endpoint;
    online_instances_array.push_back(instance_host);
  }

  return online_instances_array;
}

#if DOXYGEN_CPP
/**
 * Use this function to restore a ReplicaSet from a Quorum loss scenario
 * \param args : A list of values to be used to use the partition from
 * an Instance to restore the ReplicaSet.
 *
 * This function returns an empty Value.
 */
#else
/**
 * Forces the quorum on ReplicaSet with Quorum loss
 * \param name The name of the Instance to be used as partition to force the
 * quorum on the ReplicaSet
 */
#if DOXYGEN_JS
Undefined ReplicaSet::forceQuorumUsingPartitionOf(InstanceDef instance);
#elif DOXYGEN_PY
None ReplicaSet::force_quorum_using_partition_of(InstanceDef instance);
#endif
#endif  // DOXYGEN_CPP
shcore::Value ReplicaSet::force_quorum_using_partition_of_(
    const shcore::Argument_list &args) {
  shcore::Value ret_val;
  args.ensure_count(1, 2,
                    get_function_name("forceQuorumUsingPartitionOf").c_str());

  // Check if the ReplicaSet is empty
  if (_metadata_storage->is_replicaset_empty(get_id()))
    throw shcore::Exception::runtime_error("ReplicaSet not initialized.");

  // Rejoin the Instance to the Default ReplicaSet
  try {
    ret_val = force_quorum_using_partition_of(args);
  }
  CATCH_AND_TRANSLATE_FUNCTION_EXCEPTION(
      get_function_name("forceQuorumUsingPartitionOf"));

  return ret_val;
}

shcore::Value ReplicaSet::force_quorum_using_partition_of(
    const shcore::Argument_list &args) {
  shcore::Value ret_val;
  uint64_t rset_id = get_id();
  std::shared_ptr<mysqlshdk::db::ISession> session;

  auto instance_def =
      mysqlsh::get_connection_options(args, PasswordFormat::STRING);

  validate_connection_options(instance_def);

  if (!instance_def.has_port())
    instance_def.set_port(mysqlshdk::db::k_default_mysql_port);

  instance_def.set_default_connection_data();

  std::string instance_address = instance_def.as_uri(only_transport());

  // TODO(miguel): test if there's already quorum and add a 'force' option to be
  // used if so

  // TODO(miguel): test if the instance if part of the current cluster, for the
  // scenario of restoring a cluster quorum from another

  // Check if the instance belongs to the ReplicaSet on the Metadata
  if (!_metadata_storage->is_instance_on_replicaset(rset_id,
                                                    instance_address)) {
    std::string message = "The instance '" + instance_address + "'";

    message.append(" does not belong to the ReplicaSet: '" +
                   get_member("name").as_string() + "'.");

    throw shcore::Exception::runtime_error(message);
  }

  // Before rejoining an instance we must verify if the instance's
  // 'group_replication_group_name' matches the one registered in the
  // Metadata (BUG #26159339)
  {
    try {
      log_info("Opening a new session to the partition instance %s",
               instance_address.c_str());
      session = establish_mysql_session(instance_def,
                                        current_shell_options()->get().wizards);
      instance_def = session->get_connection_options();
    } catch (std::exception &e) {
      log_error("Could not open connection to '%s': %s",
                instance_address.c_str(), e.what());
      throw;
    }

    if (!validate_replicaset_group_name(session, get_group_name())) {
      std::string nice_error =
          "The instance '" + instance_address +
          "' "
          "cannot be used to restore the cluster as it "
          "may belong to a different ReplicaSet as the one registered "
          "in the Metadata since the value of "
          "'group_replication_group_name' does not match the one "
          "registered in the ReplicaSet's Metadata: possible split-brain "
          "scenario.";

      session->close();

      throw shcore::Exception::runtime_error(nice_error);
    }
  }

  // Get the instance state
  Cluster_check_info state;

  auto instance_type = get_gr_instance_type(session);

  if (instance_type != GRInstanceType::Standalone &&
      instance_type != GRInstanceType::StandaloneWithMetadata) {
    state = get_replication_group_state(session, instance_type);

    if (state.source_state != ManagedInstance::OnlineRW &&
        state.source_state != ManagedInstance::OnlineRO) {
      std::string message = "The instance '" + instance_address + "'";
      message.append(" cannot be used to restore the cluster as it is on a ");
      message.append(ManagedInstance::describe(
          static_cast<ManagedInstance::State>(state.source_state)));
      message.append(" state, and should be ONLINE");

      session->close();

      throw shcore::Exception::runtime_error(message);
    }
  } else {
    std::string message = "The instance '" + instance_address + "'";
    message.append(
        " cannot be used to restore the cluster as it is not an active member "
        "of replication group.");

    session->close();

    throw shcore::Exception::runtime_error(message);
  }

  // Check if there is quorum to issue an error.
  mysqlshdk::mysql::Instance target_instance(session);
  if (mysqlshdk::gr::has_quorum(target_instance, nullptr, nullptr)) {
    m_console->print_error(
        "Cannot perform operation on an healthy cluster because it can only "
        "be used to restore a cluster from quorum loss.");

    target_instance.close_session();

    throw shcore::Exception::runtime_error(
        "The cluster has quorum according to instance '" + instance_address +
        "'");
  }

  // Get the online instances of the ReplicaSet to user as group_peers
  auto online_instances =
      _metadata_storage->get_replicaset_online_instances(rset_id);

  if (online_instances.empty()) {
    session->close();
    throw shcore::Exception::logic_error(
        "No online instances are visible from the given one.");
  }

  std::string group_peers;

  for (auto &instance : online_instances) {
    std::string instance_host = instance.endpoint;
    auto instance_cnx_opts =
        shcore::get_connection_options(instance_host, false);
    // We assume the login credentials are the same on all instances
    instance_cnx_opts.set_login_options_from(instance_def);

    std::shared_ptr<mysqlshdk::db::ISession> instance_session;
    try {
      log_info(
          "Opening a new session to a group_peer instance to obtain the XCOM "
          "address %s",
          instance_host.c_str());
      instance_session = get_session(instance_cnx_opts);
    } catch (std::exception &e) {
      log_error("Could not open connection to %s: %s", instance_address.c_str(),
                e.what());
      session->close();
      throw;
    }

    std::string group_peer_instance_xcom_address;

    // Get @@group_replication_local_address
    get_server_variable(instance_session, "group_replication_local_address",
                        group_peer_instance_xcom_address);

    group_peers.append(group_peer_instance_xcom_address);
    group_peers.append(",");

    instance_session->close();
  }

  // Force the reconfiguration of the GR group
  {
    // Remove the trailing comma of group_peers
    if (group_peers.back() == ',') group_peers.pop_back();

    log_info("Setting the group_replication_force_members at instance %s",
             instance_address.c_str());

    // Setting the group_replication_force_members will force a new group
    // membership, triggering the necessary actions from GR upon being set to
    // force the quorum. Therefore, the variable can be cleared immediately
    // after it is set.
    set_global_variable(session, "group_replication_force_members",
                        group_peers);

    // Clear group_replication_force_members at the end to allow GR to be
    // restarted later on the instance (without error).
    set_global_variable(session, "group_replication_force_members", "");

    session->close();
  }

  return ret_val;
}

Cluster_check_info ReplicaSet::check_preconditions(
    std::shared_ptr<mysqlshdk::db::ISession> group_session,
    const std::string &function_name) const {
  try {
    return check_function_preconditions("ReplicaSet." + function_name,
                                        group_session);
  }
  CATCH_AND_TRANSLATE_FUNCTION_EXCEPTION(get_function_name(function_name));
  return Cluster_check_info{};
}

shcore::Value ReplicaSet::get_description() const {
  shcore::Value ret_val = shcore::Value::new_map();
  auto description = ret_val.as_map();

  // First we identify the master instance
  auto instances = _metadata_storage->get_replicaset_instances(_id);

  (*description)["name"] = shcore::Value(_name);
  (*description)["topology"] = shcore::Value::new_array();

  auto instance_list = description->get_array("topology");

  for (const auto &value : instances) {
    auto instance = shcore::Value::new_map();
    auto instance_obj = instance.as_map();

    (*instance_obj)["label"] = shcore::Value(value.label);
    (*instance_obj)["address"] = shcore::Value(value.endpoint);
    (*instance_obj)["role"] = shcore::Value(value.role);

    instance_list->push_back(instance);
  }

  return ret_val;
}

shcore::Value ReplicaSet::get_status(
    const mysqlsh::dba::Cluster_check_info &state) const {
  shcore::Value ret_val = shcore::Value::new_map();
  auto status = ret_val.as_map();

  // First, check if the topology type matchs the current state in order to
  // retrieve the status correctly, otherwise issue an error.
  this->verify_topology_type_change();

  bool single_primary_mode = _topology_type == kTopologySinglePrimary;

  // get the current cluster session from the metadata
  auto instance_session = _metadata_storage->get_session();

  // Identifies the master node
  std::string master_uuid;
  if (single_primary_mode) {
    get_status_variable(instance_session, "group_replication_primary_member",
                        master_uuid, false);
  }

  // Get SSL Mode used by the cluster (same on all members of the replicaset).
  std::string gr_ssl_mode;
  get_server_variable(instance_session, "group_replication_ssl_mode",
                      gr_ssl_mode, true);

  auto instances = _metadata_storage->get_replicaset_instances(_id, true);

  Instance_definition master;
  bool master_found = false;
  int online_count = 0, total_count = 0;

  for (const auto &value : instances) {
    total_count++;
    if (value.uuid == master_uuid) {
      master = value;
      master_found = true;
    }

    if (value.state == "ONLINE") online_count++;
  }

  /*
   * Theory:
   *
   * unreachable_instances = COUNT(member_state = 'UNREACHABLE')
   * quorum = unreachable_instances < (total_instances / 2)
   * total_ha_instances = 2 * (number_of_failures) + 1
   * number_of_failures = (total_ha_instances - 1) / 2
   */

  int number_of_failures = (online_count - 1) / 2;
  int non_active = total_count - online_count;

  std::string desc_status;
  ReplicaSetStatus::Status rs_status;

  // Get the current cluster session from the metadata
  auto session = _metadata_storage->get_session();
  auto options = session->get_connection_options();

  std::string active_session_address = options.as_uri(only_transport());

  if (state.quorum == ReplicationQuorum::Quorumless) {
    rs_status = ReplicaSetStatus::NO_QUORUM;
    desc_status = "Cluster has no quorum as visible from '" +
                  active_session_address +
                  "' and cannot process write transactions.";
  } else {
    if (number_of_failures == 0) {
      rs_status = ReplicaSetStatus::OK_NO_TOLERANCE;

      desc_status = "Cluster is NOT tolerant to any failures.";
    } else {
      if (non_active > 0)
        rs_status = ReplicaSetStatus::OK_PARTIAL;
      else
        rs_status = ReplicaSetStatus::OK;

      if (number_of_failures == 1)
        desc_status = "Cluster is ONLINE and can tolerate up to ONE failure.";
      else
        desc_status = "Cluster is ONLINE and can tolerate up to " +
                      std::to_string(number_of_failures) + " failures.";
    }
  }

  if (non_active > 0) {
    if (non_active == 1)
      desc_status.append(" " + std::to_string(non_active) +
                         " member is not active");
    else
      desc_status.append(" " + std::to_string(non_active) +
                         " members are not active");
  }

  (*status)["name"] = shcore::Value(_name);
  (*status)["statusText"] = shcore::Value(desc_status);
  (*status)["status"] = shcore::Value(ReplicaSetStatus::describe(rs_status));
  (*status)["ssl"] = shcore::Value(gr_ssl_mode);

  // In single primary mode we need to add the "primary" field
  if (single_primary_mode && master_found)
    (*status)["primary"] = shcore::Value(master.endpoint);

  // Creates the topology node
  (*status)["topology"] = shcore::Value::new_map();
  auto instance_owner_node = status->get_map("topology");

  // Inserts the instances
  for (const auto &value : instances) {
    // Gets each row
    auto instance_label = value.label;
    (*instance_owner_node)[instance_label] = shcore::Value::new_map();
    auto instance_node = instance_owner_node->get_map(instance_label);

    // check if it is the active session instance
    bool active_session_instance = false;

    if (active_session_address == value.endpoint)
      active_session_instance = true;

    // We compare the server's uuid to match instances, which means that
    // if uuid for an instances changes, the status will look off.
    // re-syncing uuid should be done in a separate step
    if (value.uuid == master.uuid && single_primary_mode)
      append_member_status(instance_node, value, true, active_session_instance);
    else
      append_member_status(instance_node, value,
                           single_primary_mode ? false : true,
                           active_session_instance);

    (*instance_node)["readReplicas"] = shcore::Value::new_map();
  }

  return ret_val;
}

void ReplicaSet::remove_instances(
    const std::vector<std::string> &remove_instances) {
  if (!remove_instances.empty()) {
    for (auto instance : remove_instances) {
      // verify if the instance is on the metadata
      if (_metadata_storage->is_instance_on_replicaset(_id, instance)) {
        Value::Map_type_ref options(new shcore::Value::Map_type);

        auto connection_options =
            shcore::get_connection_options(instance, false);
        remove_instance_metadata(connection_options);
      } else {
        std::string message = "The instance '" + instance + "'";
        message.append(" does not belong to the ReplicaSet: '" +
                       get_member("name").as_string() + "'.");
        throw shcore::Exception::runtime_error(message);
      }
    }
  }
}

void ReplicaSet::rejoin_instances(
    const std::vector<std::string> &rejoin_instances,
    const shcore::Value::Map_type_ref &options) {
  auto instance_session(_metadata_storage->get_session());
  auto instance_data = instance_session->get_connection_options();

  if (!rejoin_instances.empty()) {
    // Get the user and password from the options
    // or from the instance session
    if (options) {
      // Check if the password is specified on the options and if not prompt it
      mysqlsh::set_user_from_map(&instance_data, options);
      mysqlsh::set_password_from_map(&instance_data, options);
    }

    for (auto instance : rejoin_instances) {
      // verify if the instance is on the metadata
      if (_metadata_storage->is_instance_on_replicaset(_id, instance)) {
        auto connection_options =
            shcore::get_connection_options(instance, false);

        connection_options.set_user(instance_data.get_user());
        connection_options.set_password(instance_data.get_password());

        // If rejoinInstance fails we don't want to stop the execution of the
        // function, but to log the error.
        try {
          std::string msg = "Rejoining the instance '" + instance +
                            "' to the "
                            "cluster's default replicaset.";
          log_warning("%s", msg.c_str());
          rejoin_instance(&connection_options, shcore::Value::Map_type_ref());
        } catch (shcore::Exception &e) {
          log_error("Failed to rejoin instance: %s", e.what());
        }
      } else {
        std::string msg =
            "The instance '" + instance +
            "' does not "
            "belong to the cluster. Skipping rejoin to the Cluster.";
        throw shcore::Exception::runtime_error(msg);
      }
    }
  }
}

/**
 * Check the instance server UUID of the specified intance.
 *
 * The server UUID must be unique for all instances in a cluster. This function
 * checks if the server_uuid of the target instance is unique among all active
 * members of the cluster.
 *
 * @param instance_session Session to the target instance to check its server
 *                         UUID.
 */
void ReplicaSet::validate_server_uuid(
    std::shared_ptr<mysqlshdk::db::ISession> instance_session) {
  // Get the server_uuid of the target instance.
  auto instance = mysqlshdk::mysql::Instance(instance_session);
  std::string server_uuid = instance.get_sysvar_string(
      "server_uuid", mysqlshdk::mysql::Var_qualifier::GLOBAL);

  // Get connection option for the metadata.
  std::shared_ptr<Cluster> cluster(_cluster.lock());
  if (!cluster) {
    throw shcore::Exception::runtime_error("Cluster object is no longer valid");
  }
  std::shared_ptr<mysqlshdk::db::ISession> cluster_session =
      cluster->get_group_session();
  Connection_options cluster_cnx_opt =
      cluster_session->get_connection_options();

  // Get list of instances in the metadata
  std::vector<Instance_definition> metadata_instances =
      _metadata_storage->get_replicaset_active_instances(_id);

  // Get and compare the server UUID of all instances with the one of
  // the target instance.
  for (Instance_definition &instance_def : metadata_instances) {
    if (server_uuid == instance_def.uuid) {
      // Raise an error if the server uuid is the same of a cluster member.
      throw Exception::runtime_error(
          "Cannot add an instance with the same server UUID (" + server_uuid +
          ") of an active member of the cluster '" + instance_def.endpoint +
          "'. Please change the server UUID of the instance to add, all "
          "members must have a unique server UUID.");
    }
  }
}

std::shared_ptr<mysqlshdk::db::ISession> ReplicaSet::get_session(
    const mysqlshdk::db::Connection_options &args) {
  std::shared_ptr<mysqlshdk::db::ISession> ret_val;

  ret_val = mysqlshdk::db::mysql::Session::create();

  ret_val->connect(args);

  return ret_val;
}

std::vector<Instance_definition> ReplicaSet::get_instances_from_metadata() {
  return _metadata_storage->get_replicaset_instances(get_id());
}

/** Iterates through all the cluster members in a given state calling the given
 * function on each of then.
 * @param states Vector of strings with the states of members on which the
 * functor will be called.
 * @param cnx_opt Connection options to be used to connect to the cluster
 * members
 * @param ignore_instances_vector Vector with addresses of instances to be
 * ignored even if their state is specified in the states vector.
 * @param functor Function that is called on each member of the cluster whose
 * state is specified in the states vector.
 */

void ReplicaSet::execute_in_members(
    const std::vector<std::string> &states,
    const mysqlshdk::db::Connection_options &cnx_opt,
    const std::vector<std::string> &ignore_instances_vector,
    std::function<void(std::shared_ptr<mysqlshdk::db::ISession> session)>
        functor) {
  std::shared_ptr<mysqlshdk::db::ISession> instance_session;
  // Note (nelson): should we handle the super_read_only behavior here or should
  // it be the responsibility of the functor?
  auto instance_definitions =
      _metadata_storage->get_replicaset_instances(_id, false, states);

  for (auto &instance_def : instance_definitions) {
    std::string instance_address = instance_def.endpoint;
    // if instance is on the list of instances to be ignored, skip it
    if (std::find(ignore_instances_vector.begin(),
                  ignore_instances_vector.end(),
                  instance_address) != ignore_instances_vector.end())
      continue;
    auto instance_cnx_opt =
        shcore::get_connection_options(instance_address, false);

    instance_cnx_opt.set_login_options_from(cnx_opt);
    try {
      log_debug(
          "Opening a new session to instance '%s' while iterating "
          "cluster members",
          instance_address.c_str());
      instance_session = get_session(instance_cnx_opt);
    } catch (std::exception &e) {
      log_error("Could not open connection to '%s': %s",
                instance_address.c_str(), e.what());
      throw;
    }
    functor(instance_session);
    instance_session->close();
  }
}

void ReplicaSet::set_group_name(const std::string &group_name) {
  _group_name = group_name;
  _metadata_storage->set_replicaset_group_name(shared_from_this(), group_name);
}
