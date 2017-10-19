// Assumptions: smart deployment rountines available
//@ Initialization
var deployed_here = reset_or_deploy_sandboxes();

//@ create first cluster
shell.connect({scheme: 'mysql', host: localhost, port: __mysql_sandbox_port1, user: 'root', password: 'root'});
var singleSession = session;

if (__have_ssl)
  var single = dba.createCluster('single', {memberSslMode:'REQUIRED'});
else
  var single = dba.createCluster('single', {memberSslMode:'DISABLED'});

//@ Success adding instance
add_instance_to_cluster(single, __mysql_sandbox_port2);

// Waiting for the second added instance to become online
wait_slave_state(single, uri2, "ONLINE");

// Wait for the second added instance to fetch all the replication data
wait_sandbox_in_metadata(__mysql_sandbox_port2);

// Connect to the future new seed node
shell.connect({scheme: 'mysql', host: localhost, port: __mysql_sandbox_port2, user: 'root', password: 'root'});
var singleSession2 = session;

//@ Check auto_increment values for single-primary
singleSession.runSql("show global variables like 'auto_increment_%'").fetchAll();
singleSession2.runSql("show global variables like 'auto_increment_%'").fetchAll();

//@ Get the cluster back
var single = dba.getCluster();

// Kill the seed instance
if (__sandbox_dir)
  dba.killSandboxInstance(__mysql_sandbox_port1, {sandboxDir:__sandbox_dir});
else
  dba.killSandboxInstance(__mysql_sandbox_port1);

wait_slave_state(single, uri1, ["(MISSING)"]);

//@ Restore the quorum
single.forceQuorumUsingPartitionOf({host: localhost, port: __mysql_sandbox_port2, user: 'root', password:'root'});

//@ Success adding instance to the single cluster
add_instance_to_cluster(single, __mysql_sandbox_port3);
shell.connect({SCHEME: 'mysql', Host: localhost, PoRt: __mysql_sandbox_port3, UsEr: 'root', PassWord: 'root'});

//@ Remove the instance from the cluster
single.removeInstance({host: localhost, port: __mysql_sandbox_port3});

//@ create second cluster
shell.connect({scheme: 'mysql', host: localhost, port: __mysql_sandbox_port3, user: 'root', password: 'root'});
var multiSession = session;

// We must use clearReadOnly because the instance 3 was removed from the cluster before
// (BUG#26422638)
if (__have_ssl)
  var multi = dba.createCluster('multi', {memberSslMode:'REQUIRED', multiMaster:true, force:true, clearReadOnly: true});
else
  var multi = dba.createCluster('multi', {memberSslMode:'DISABLED', multiMaster:true, force:true, clearReadOnly: true});

//@ Failure adding instance from multi cluster into single
add_instance_options['port'] = __mysql_sandbox_port3;
single.addInstance(add_instance_options);

// Drops the metadata on the multi cluster letting a non managed replication group
dba.dropMetadataSchema({force:true});

//@ Failure adding instance from an unmanaged replication group into single
add_instance_options['port'] = __mysql_sandbox_port3;
single.addInstance(add_instance_options);

//@ Failure adding instance already in the InnoDB cluster
add_instance_options['port'] = __mysql_sandbox_port2;
single.addInstance(add_instance_options);

//@ Finalization
// Will close opened sessions and delete the sandboxes ONLY if this test was executed standalone
singleSession.close();
singleSession2.close();
multiSession.close();
if (deployed_here)
  cleanup_sandboxes(true);
