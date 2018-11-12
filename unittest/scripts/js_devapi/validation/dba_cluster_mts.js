//@ Initialization
||

//@<OUT> check instance with invalid parallel type.
Validating local MySQL instance listening at port <<<__mysql_sandbox_port1>>> for use in an InnoDB cluster...
Instance detected as a sandbox.
Please note that sandbox instances are only suitable for deploying test clusters for use within the same host.

This instance reports its own address as <<<real_hostname>>>
Clients and other cluster members will communicate with it through this address by default. If this is not correct, the report_host MySQL system variable should be changed.

Checking whether existing tables comply with Group Replication requirements...
No incompatible tables detected

Checking instance configuration...

Some configuration options need to be fixed:
+---------------------+---------------+----------------+----------------------------+
| Variable            | Current Value | Required Value | Note                       |
+---------------------+---------------+----------------+----------------------------+
| slave_parallel_type | DATABASE      | LOGICAL_CLOCK  | Update the server variable |
+---------------------+---------------+----------------+----------------------------+

Please use the dba.configureInstance() command to repair these issues.

//@ Create cluster (fail: parallel type check fail).
||Dba.createCluster: Instance check failed (RuntimeError)

//@ fix config including parallel type
|The instance 'localhost:<<<__mysql_sandbox_port1>>>' was configured for InnoDB cluster usage.|

//@ Create cluster (succeed this time).
||

//@<OUT> check instance with invalid commit order.
Validating local MySQL instance listening at port <<<__mysql_sandbox_port2>>> for use in an InnoDB cluster...
Instance detected as a sandbox.
Please note that sandbox instances are only suitable for deploying test clusters for use within the same host.

This instance reports its own address as <<<real_hostname>>>
Clients and other cluster members will communicate with it through this address by default. If this is not correct, the report_host MySQL system variable should be changed.

Checking whether existing tables comply with Group Replication requirements...
No incompatible tables detected

Checking instance configuration...

Some configuration options need to be fixed:
+-----------------------------+---------------+----------------+----------------------------+
| Variable                    | Current Value | Required Value | Note                       |
+-----------------------------+---------------+----------------+----------------------------+
| slave_preserve_commit_order | OFF           | ON             | Update the server variable |
+-----------------------------+---------------+----------------+----------------------------+

Please use the dba.configureInstance() command to repair these issues.

//@ Adding instance to cluster (fail: commit order wrong).
|Please use the dba.configureInstance() command to repair these issues.|
||Cluster.addInstance: Instance check failed (RuntimeError)

//@<OUT> check instance with invalid type and commit order.
Validating local MySQL instance listening at port <<<__mysql_sandbox_port3>>> for use in an InnoDB cluster...
Instance detected as a sandbox.
Please note that sandbox instances are only suitable for deploying test clusters for use within the same host.

This instance reports its own address as <<<real_hostname>>>
Clients and other cluster members will communicate with it through this address by default. If this is not correct, the report_host MySQL system variable should be changed.

Checking whether existing tables comply with Group Replication requirements...
No incompatible tables detected

Checking instance configuration...

Some configuration options need to be fixed:
+-----------------------------+---------------+----------------+----------------------------+
| Variable                    | Current Value | Required Value | Note                       |
+-----------------------------+---------------+----------------+----------------------------+
| slave_parallel_type         | DATABASE      | LOGICAL_CLOCK  | Update the server variable |
| slave_preserve_commit_order | OFF           | ON             | Update the server variable |
+-----------------------------+---------------+----------------+----------------------------+

Please use the dba.configureInstance() command to repair these issues.


//@ configure instance and update type and commit order with valid values.
|WARNING: User 'root' can only connect from localhost.|
|The instance 'localhost:<<<__mysql_sandbox_port3>>>' was configured for InnoDB cluster usage.|

//@<OUT> check instance, no invalid values after configure.
Validating local MySQL instance listening at port <<<__mysql_sandbox_port3>>> for use in an InnoDB cluster...
Instance detected as a sandbox.
Please note that sandbox instances are only suitable for deploying test clusters for use within the same host.

This instance reports its own address as <<<real_hostname>>>
Clients and other cluster members will communicate with it through this address by default. If this is not correct, the report_host MySQL system variable should be changed.

Checking whether existing tables comply with Group Replication requirements...
No incompatible tables detected

Checking instance configuration...
Instance configuration is compatible with InnoDB cluster

The instance '<<<localhost>>>:<<<__mysql_sandbox_port3>>>' is valid for InnoDB cluster usage.

//@ Adding instance to cluster (succeed: nothing to update).
||

//@ Finalization
||
