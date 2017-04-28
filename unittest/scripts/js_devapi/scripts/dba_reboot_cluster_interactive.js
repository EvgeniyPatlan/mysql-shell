// Assumptions: smart deployment rountines available
//@ Initialization
var deployed_here = reset_or_deploy_sandboxes();

shell.connect({scheme: 'mysql', host: localhost, port: __mysql_sandbox_port1, user: 'root', password: 'root'});
var clusterSession = session;

//@<OUT> create cluster
if (__have_ssl)
  var cluster = dba.createCluster('dev', {memberSslMode:'REQUIRED'});
else
  var cluster = dba.createCluster('dev');

// session is stored on the cluster object so changing the global session should not affect cluster operations
shell.connect({scheme: 'mysql', host: "localhost", port: __mysql_sandbox_port2, user: 'root', password: 'root'});

cluster.status();

//@ Add instance 2
add_instance_to_cluster(cluster, __mysql_sandbox_port2);

// Waiting for the second added instance to become online
wait_slave_state(cluster, uri2, "ONLINE");

//@ Add instance 3
add_instance_to_cluster(cluster, __mysql_sandbox_port3);

// Waiting for the third added instance to become online
wait_slave_state(cluster, uri3, "ONLINE");

//@ Dba.rebootClusterFromCompleteOutage errors
dba.rebootClusterFromCompleteOutage("dev");
dba.rebootClusterFromCompleteOutage("dev", {invalidOpt: "foobar"});

// Kill all the instances

// Kill instance 2
if (__sandbox_dir)
  dba.killSandboxInstance(__mysql_sandbox_port2, {sandboxDir:__sandbox_dir});
else
  dba.killSandboxInstance(__mysql_sandbox_port2);

// Since the cluster has quorum, the instance will be kicked off the
// Cluster going OFFLINE->UNREACHABLE->(MISSING)
wait_slave_state(cluster, uri2, "(MISSING)");

// Kill instance 3
if (__sandbox_dir)
  dba.killSandboxInstance(__mysql_sandbox_port3, {sandboxDir:__sandbox_dir});
else
  dba.killSandboxInstance(__mysql_sandbox_port3);

// Waiting for the third added instance to become unreachable
// Will remain unreachable since there's no quorum to kick it off
wait_slave_state(cluster, uri3, "UNREACHABLE");

// Kill instance 1
if (__sandbox_dir)
  dba.killSandboxInstance(__mysql_sandbox_port1, {sandboxDir:__sandbox_dir});
else
  dba.killSandboxInstance(__mysql_sandbox_port1);

// Re-start all the instances except instance 3

// Start instance 2
if (__sandbox_dir)
  dba.startSandboxInstance(__mysql_sandbox_port2, {sandboxDir:__sandbox_dir});
else
  dba.startSandboxInstance(__mysql_sandbox_port2);

// Start instance 1
if (__sandbox_dir)
  dba.startSandboxInstance(__mysql_sandbox_port1, {sandboxDir:__sandbox_dir});
else
  dba.startSandboxInstance(__mysql_sandbox_port1);

session.close();

// Re-establish the connection to instance 1
shell.connect({scheme: 'mysql', host: localhost, port: __mysql_sandbox_port1, user: 'root', password: 'root'});

//@ Dba.rebootClusterFromCompleteOutage success
cluster = dba.rebootClusterFromCompleteOutage("dev");

// Waiting for the second added instance to become online
wait_slave_state(cluster, uri2, "ONLINE");

//@<OUT> cluster status after reboot
cluster.status();
session.close();

//@ Finalization
//  Will close opened sessions and delete the sandboxes ONLY if this test was executed standalone
clusterSession.close();
if (deployed_here)
  cleanup_sandboxes(true);
