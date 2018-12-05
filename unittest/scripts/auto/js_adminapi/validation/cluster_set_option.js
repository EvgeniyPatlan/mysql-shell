//@<OUT> WL#11465: Create single-primary cluster
Creating a Classic session to 'root@localhost:<<<__mysql_sandbox_port1>>>'

//@ WL#11465: ArgumentErrors of setOption
||Invalid number of arguments, expected 2 but got 0 (ArgumentError)
||Invalid number of arguments, expected 2 but got 1 (ArgumentError)
||Option 'foobar' not supported. (ArgumentError)

//@ WL#11465: Error when executing setOption on a cluster with 1 or more members not ONLINE
|ERROR: The instance 'localhost:<<<__mysql_sandbox_port3>>>' has the status: '(MISSING)'. All members must be ONLINE.|One or more instances of the cluster are not ONLINE. (RuntimeError)

//@<ERR> WL#11465: Error when executing setOption on a cluster with no visible quorum {VER(>=8.0.14)}
Cluster.setOption: There is no quorum to perform the operation (RuntimeError)

//@<ERR> WL#11465: Error when executing setOption on a cluster with no visible quorum 5.7 {VER(>=5.7.24) && VER(<8.0.0)}
Cluster.setOption: There is no quorum to perform the operation (RuntimeError)

//@ WL#11465: Re-create the cluster
||

//@ WL#11465: setOption clusterName with invalid value for cluster-name
||The Cluster name can only start with an alphabetic or the '_' character. (ArgumentError)

//@<OUT> WL#11465: setOption clusterName
Setting the value of 'clusterName' to 'newName' in the Cluster ...

Successfully set the value of 'clusterName' to 'newName' in the Cluster: 'cluster'.

//@<OUT> WL#11465: Verify clusterName changed correctly
newName

//@<OUT> WL#11465: setOption memberWeight
Setting the value of 'memberWeight' to '25' in all ReplicaSet members ...

Successfully set the value of 'memberWeight' to '25' in the 'default' ReplicaSet.

//@<OUT> WL#11465: Verify memberWeight changed correctly in instance 1
25

//@<OUT> WL#11465: Verify memberWeight changed correctly in instance 2
25

//@<OUT> WL#11465: Verify memberWeight changed correctly in instance 3
25

//@<ERR> WL#11465: setOption exitStateAction with invalid value
Cluster.setOption: Variable 'group_replication_exit_state_action' can't be set to the value of 'ABORT' (RuntimeError)

//@<OUT> WL#11465: setOption exitStateAction
Setting the value of 'exitStateAction' to 'ABORT_SERVER' in all ReplicaSet members ...

Successfully set the value of 'exitStateAction' to 'ABORT_SERVER' in the 'default' ReplicaSet.

//@<OUT> WL#11465: Verify exitStateAction changed correctly in instance 1
ABORT_SERVER

//@<OUT> WL#11465: Verify exitStateAction changed correctly in instance 2
ABORT_SERVER

//@<OUT> WL#11465: Verify exitStateAction changed correctly in instance 3
ABORT_SERVER

//@<OUT> WL#11465: setOption failoverConsistency {VER(>=8.0.14)}
Setting the value of 'failoverConsistency' to 'BEFORE_ON_PRIMARY_FAILOVER' in all ReplicaSet members ...

Successfully set the value of 'failoverConsistency' to 'BEFORE_ON_PRIMARY_FAILOVER' in the 'default' ReplicaSet.

//@<OUT> WL#11465: Verify failoverConsistency changed correctly in instance 1 {VER(>=8.0.14)}
BEFORE_ON_PRIMARY_FAILOVER

//@<OUT> WL#11465: Verify failoverConsistency changed correctly in instance 2 {VER(>=8.0.14)}
BEFORE_ON_PRIMARY_FAILOVER

//@<OUT> WL#11465: Verify failoverConsistency changed correctly in instance 3 {VER(>=8.0.14)}
BEFORE_ON_PRIMARY_FAILOVER

//@<OUT> WL#11465: setOption expelTimeout {VER(>=8.0.14)}
Setting the value of 'expelTimeout' to '3500' in all ReplicaSet members ...

Successfully set the value of 'expelTimeout' to '3500' in the 'default' ReplicaSet.

//@<OUT> WL#11465: Verify expelTimeout changed correctly in instance 1 {VER(>=8.0.14)}
3500

//@<OUT> WL#11465: Verify expelTimeout changed correctly in instance 2 {VER(>=8.0.14)}
3500

//@<OUT> WL#11465: Verify expelTimeout changed correctly in instance 3 {VER(>=8.0.14)}
3500

//@ WL#11465: Finalization
||