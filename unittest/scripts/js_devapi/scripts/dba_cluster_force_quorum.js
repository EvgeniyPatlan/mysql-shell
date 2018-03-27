// Assumptions: smart deployment rountines available

// Due to the fixes for BUG #26159339: SHELL: ADMINAPI DOES NOT TAKE
// GROUP_NAME INTO ACCOUNT
// We must reset the instances to reset 'group_replication_group_name' since on
// the previous test Shell_js_dba_tests.configure_local_instance the instances
// configurations are persisted on my.cnf so upon the restart of any instance
// it can bootstrap a new group, changing the value of
// 'group_replication_group_name' and leading to a failure on forceQuorum
// cleanup_sandboxes(true);

//@ Initialization
testutil.deploySandbox(__mysql_sandbox_port1, "root");
testutil.deploySandbox(__mysql_sandbox_port2, "root");
testutil.deploySandbox(__mysql_sandbox_port3, "root");

shell.connect({scheme:'mysql', host: localhost, port: __mysql_sandbox_port1, user: 'root', password: 'root'});
var clusterSession = session;

//@<OUT> create cluster
if (__have_ssl)
  var cluster = dba.createCluster('dev', {memberSslMode: 'REQUIRED'});
else
  var cluster = dba.createCluster('dev', {memberSslMode: 'DISABLED'});

cluster.status();

//@ Add instance 2
add_instance_to_cluster(cluster, __mysql_sandbox_port2);

// Waiting for the second added instance to become online
testutil.waitMemberState(__mysql_sandbox_port2, "ONLINE");

//@ Add instance 3
add_instance_to_cluster(cluster, __mysql_sandbox_port3);

// Waiting for the third added instance to become online
testutil.waitMemberState(__mysql_sandbox_port3, "ONLINE");

//@ Disable group_replication_start_on_boot on second instance {VER(>=8.0.5)}
// If we don't set the start_on_boot variable to OFF, it is possible that instance 2 will
// be still trying to join the cluster from the moment it was started again until
// the cluster is unlocked after the forceQuorumUsingPartitionOf command
var s2 = mysql.getSession({host:localhost, port: __mysql_sandbox_port2, user: 'root', password: 'root'});
s2.runSql("SET PERSIST group_replication_start_on_boot = 0");
s2.close();

//@ Disable group_replication_start_on_boot on third instance {VER(>=8.0.5)}
// If we don't set the start_on_boot variable to OFF, it is possible that instance 3 will
// be still trying to join the cluster from the moment it was started again until
// the cluster is unlocked after the forceQuorumUsingPartitionOf command
var s3 = mysql.getSession({host:localhost, port: __mysql_sandbox_port3, user: 'root', password: 'root'});
s3.runSql("SET PERSIST group_replication_start_on_boot = 0");
s3.close();

//@ Kill instance 2
testutil.killSandbox(__mysql_sandbox_port2);

// Since the cluster has quorum, the instance will be kicked off the
// Cluster going OFFLINE->UNREACHABLE->(MISSING)
testutil.waitMemberState(__mysql_sandbox_port2, "(MISSING)");

//@ Kill instance 3
testutil.killSandbox(__mysql_sandbox_port3);

// Waiting for the third added instance to become unreachable
// Will remain unreachable since there's no quorum to kick it off
testutil.waitMemberState(__mysql_sandbox_port3, "UNREACHABLE");

//@ Start instance 2
testutil.startSandbox(__mysql_sandbox_port2);

//@ Start instance 3
testutil.startSandbox(__mysql_sandbox_port3);

//@<OUT> Cluster status
cluster.status();

//@ Cluster.forceQuorumUsingPartitionOf errors
cluster.forceQuorumUsingPartitionOf();
cluster.forceQuorumUsingPartitionOf(1);
cluster.forceQuorumUsingPartitionOf("");
cluster.forceQuorumUsingPartitionOf(1, "");
cluster.forceQuorumUsingPartitionOf({host:localhost, port: __mysql_sandbox_port2, password:'root'});

//@ Cluster.forceQuorumUsingPartitionOf success
cluster.forceQuorumUsingPartitionOf({host:localhost, port: __mysql_sandbox_port1, password:'root'});

//@<OUT> Cluster status after force quorum
cluster.status();

//@ Rejoin instance 2
if (__have_ssl)
  cluster.rejoinInstance({host:localhost, port: __mysql_sandbox_port2, password:'root'}, {memberSslMode: 'REQUIRED'});
else
  cluster.rejoinInstance({host:localhost, port: __mysql_sandbox_port2, password:'root'}, {memberSslMode: 'DISABLED'});

// Waiting for the second rejoined instance to become online
testutil.waitMemberState(__mysql_sandbox_port2, "ONLINE");

//@ Rejoin instance 3
if (__have_ssl)
  cluster.rejoinInstance({host:localhost, port: __mysql_sandbox_port3, password:'root'}, {memberSslMode: 'REQUIRED'});
else
  cluster.rejoinInstance({host:localhost, port: __mysql_sandbox_port3, password:'root'}, {memberSslMode: 'DISABLED'});

// Waiting for the third rejoined instance to become online
testutil.waitMemberState(__mysql_sandbox_port3, "ONLINE");

//@<OUT> Cluster status after rejoins
cluster.status();

//@ Finalization
//  Will close opened sessions and delete the sandboxes ONLY if this test was executed standalone
clusterSession.close();
cluster.disconnect();
testutil.destroySandbox(__mysql_sandbox_port1);
testutil.destroySandbox(__mysql_sandbox_port2);
testutil.destroySandbox(__mysql_sandbox_port3);
