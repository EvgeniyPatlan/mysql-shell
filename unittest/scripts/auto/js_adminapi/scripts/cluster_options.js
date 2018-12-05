// WL#11465 AdminAPI: AdminAPI: change cluster member options
//
// Currently, it's not possible to change a previously configuration option
// of a running Cluster or Cluster active member in the AdminAPI.
// Such settings can be set during the calls to:
//
//     - dba.createCluster()
//     - <Cluster.>addInstance()
//     - <Cluster.>rejoinInstance()
//
// In order to support changing configuration options of cluster members
// individually or globally, the AdminAPI was extended with two new commands:
//
//     - <Cluster.>setOption(option, value)
//     - <Cluster.>setInstanceOption(instance, option, value)
//
// Each command has a defined set of accepted options supported by the AdminAPI.
//
// On top of that, in order to verify which are the cluster configuration options
// in place, a new command was added to the AdminAPI:
//
//     - <Cluster.>options([options])

//@ WL#11465: Initialization
testutil.deploySandbox(__mysql_sandbox_port1, "root");
testutil.deploySandbox(__mysql_sandbox_port2, "root");
testutil.deploySandbox(__mysql_sandbox_port3, "root");

shell.connect(__sandbox_uri1);

//@ WL#11465: Create single-primary cluster with specific options
var __local_port1 = 30000 + __mysql_sandbox_port1;
var __local_port2 = 30000 + __mysql_sandbox_port2;
var __local_port3 = 30000 + __mysql_sandbox_port3;
var __cfg_local_address1 = localhost + ":" + __local_port1;
var __cfg_local_address2 = localhost + ":" + __local_port2;
var __cfg_local_address3 = localhost + ":" + __local_port3;
var __cfg_group_name = "bbbbbbbb-aaaa-aaaa-aaaa-aaaaaaaaaaaa";
var __cfg_group_seeds1 = __cfg_local_address2 + "," + __cfg_local_address3;
var __cfg_group_seeds2 = __cfg_local_address1 + "," + __cfg_local_address3;
var __cfg_group_seeds3 = __cfg_local_address1 + "," + __cfg_local_address2;

var cluster = dba.createCluster("testCluster", {groupName: __cfg_group_name, localAddress: __cfg_local_address1});

//@ WL#11465: Add instance 2 with specific options
cluster.addInstance(__sandbox_uri2, {localAddress: __cfg_local_address2, memberWeight: 75, exitStateAction: "ABORT_SERVER"});

//@ WL#11465: Add instance 3 with specific options
cluster.addInstance(__sandbox_uri3, {localAddress: __cfg_local_address3, memberWeight: 25, exitStateAction: "READ_ONLY"});

// F3.1 - On a successful 'options([options])' call, a JSON object with the
// result of the operation shall be returned:
// F3.1.1 - The JSON object shall contain the information about the global
// cluster and per instance configuration options.

//@<OUT> WL#11465: Get the cluster options {VER(>=8.0.13)}
cluster.options();

//@<OUT> WL#11465: Get the cluster options 5.7 {VER(>=5.7.24) && VER(<8.0.0)}
cluster.options();

// F3.2 - The function shall have an optional parameter 'all':
// F3.2.1 - The 'all' option shall be a boolean to indicate if the information
// about all the configuration options shall be included in the report.

//@ WL#11465: ArgumentErrors of cluster.options
cluster.options("foo", true)
cluster.options("foo")
cluster.options(1)
cluster.options({foo: true})
cluster.options({all: "foo"})

//@<OUT> WL#11465: Get the cluster options using 'all' {VER(>=8.0.13)}
cluster.options({all: true});

//@<OUT> WL#11465: Get the cluster options 5.7 using 'all' {VER(>=5.7.24) && VER(<8.0.0)}
cluster.options({all: true});

//@ Kill instances 2 and 3
testutil.killSandbox(__mysql_sandbox_port3);
testutil.waitMemberState(__mysql_sandbox_port3, "(MISSING)");
testutil.killSandbox(__mysql_sandbox_port2);
testutil.waitMemberState(__mysql_sandbox_port2, "UNREACHABLE");

//@<OUT> WL#11465: Get the cluster options with 2 members down 5.7 {VER(>=5.7.24) && VER(<8.0.0)}
cluster.options();

//@<OUT> WL#11465: Get the cluster options with 2 members down {VER(>=8.0.13)}
cluster.options();

//@ WL#11465: Finalization
cluster.disconnect();
session.close();
testutil.destroySandbox(__mysql_sandbox_port1);
testutil.destroySandbox(__mysql_sandbox_port2);
testutil.destroySandbox(__mysql_sandbox_port3);
