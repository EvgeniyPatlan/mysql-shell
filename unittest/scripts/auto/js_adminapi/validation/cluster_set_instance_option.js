//@<OUT> WL#11465: Create single-primary cluster
Creating a Classic session to 'root@localhost:<<<__mysql_sandbox_port1>>>'

//@ WL#11465: ArgumentErrors of setInstanceOption
||Invalid number of arguments, expected 3 but got 0 (ArgumentError)
||Invalid URI: empty. (ArgumentError)
||Argument #1 is expected to be a map (ArgumentError)
||Invalid connection options, no options provided. (ArgumentError)
||Invalid number of arguments, expected 3 but got 1 (ArgumentError)
||Invalid number of arguments, expected 3 but got 2 (ArgumentError)
||Option 'foobar' not supported. (ArgumentError)
||Invalid connection options, no options provided. (ArgumentError)

//@<ERR> WL#11465: Error when executing setInstanceOption for a target instance that does not belong to the cluster
Cluster.setInstanceOption: The instance 'localhost:3350' does not belong to the ReplicaSet: 'default'. (RuntimeError)

//@<ERR> WL#11465: Error when executing setInstanceOption when the target instance is not ONLINE
Cluster.setInstanceOption: The instance 'localhost:<<<__mysql_sandbox_port3>>>' is not ONLINE. (RuntimeError)

//@<ERR> WL#11465: Error when executing setInstanceOption on a cluster with no visible quorum {VER(>=8.0.14)}
Cluster.setInstanceOption: There is no quorum to perform the operation (RuntimeError)

//@<ERR> WL#11465: Error when executing setInstanceOption on a cluster with no visible quorum 5.7 {VER(>=5.7.24) && VER(<8.0.0)}
Cluster.setInstanceOption: There is no quorum to perform the operation (RuntimeError)

//@ WL#11465: Re-create the cluster
||

//@<ERR> WL#11465: setInstanceOption label with invalid value for label 1
Cluster.setInstanceOption: The label can only start with an alphanumeric or the '_' character. (ArgumentError)

//@<ERR> WL#11465: setInstanceOption label with invalid value for label 2
Cluster.setInstanceOption: An instance with label 'localhost:<<<__mysql_sandbox_port1>>>' is already part of this InnoDB cluster (ArgumentError)

//@<OUT> WL#11465: setInstanceOption label
Setting the value of 'label' to 'newLabel' in the instance: 'localhost:<<<__mysql_sandbox_port2>>>' ...

Successfully set the value of 'label' to 'newLabel' in the 'default' ReplicaSet member: 'localhost:<<<__mysql_sandbox_port2>>>'.

//@<OUT> WL#11465: Verify label changed correctly
newLabel

//@<OUT> WL#11465: setInstanceOption memberWeight
Setting the value of 'memberWeight' to '25' in the instance: 'localhost:<<<__mysql_sandbox_port2>>>' ...

Successfully set the value of 'memberWeight' to '25' in the 'default' ReplicaSet member: 'localhost:<<<__mysql_sandbox_port2>>>'.

//@<OUT> WL#11465: memberWeight label changed correctly
25

//@<ERR> WL#11465: setInstanceOption exitStateAction with invalid value
Cluster.setInstanceOption: Variable 'group_replication_exit_state_action' can't be set to the value of 'ABORT' (RuntimeError)

//@<OUT> WL#11465: setInstanceOption exitStateAction
Setting the value of 'exitStateAction' to 'ABORT_SERVER' in the instance: 'localhost:<<<__mysql_sandbox_port2>>>' ...

Successfully set the value of 'exitStateAction' to 'ABORT_SERVER' in the 'default' ReplicaSet member: 'localhost:<<<__mysql_sandbox_port2>>>'.

//@<OUT> WL#11465: exitStateAction label changed correctly
ABORT_SERVER

//@ WL#11465: Finalization
||