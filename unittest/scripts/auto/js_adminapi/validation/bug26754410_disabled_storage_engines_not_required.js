//@<OUT> checkInstanceConfiguration with disabled_storage_engines no error.
The instance 'localhost:<<<__mysql_sandbox_port1>>>' is valid for InnoDB cluster usage.

{
    "status": "ok"
}

//@<OUT> configureInstance disabled_storage_engines no error.
Configuring local MySQL instance listening at port <<<__mysql_sandbox_port1>>> for use in an InnoDB cluster...
Instance detected as a sandbox.
Please note that sandbox instances are only suitable for deploying test clusters for use within the same host.

This instance reports its own address as <<<hostname>>>

WARNING: User 'root' can only connect from localhost.
If you need to manage this instance while connected from other hosts, new account(s) with the proper source address specification must be created.

The instance 'localhost:<<<__mysql_sandbox_port1>>>' is valid for InnoDB cluster usage.

//@ Remove disabled_storage_engines option from configuration and restart.
||

//@ Restart sandbox.
||

//@<OUT> configureInstance still no issues after restart.
Configuring local MySQL instance listening at port <<<__mysql_sandbox_port1>>> for use in an InnoDB cluster...
Instance detected as a sandbox.
Please note that sandbox instances are only suitable for deploying test clusters for use within the same host.

This instance reports its own address as <<<hostname>>>

WARNING: User 'root' can only connect from localhost.
If you need to manage this instance while connected from other hosts, new account(s) with the proper source address specification must be created.

The instance 'localhost:<<<__mysql_sandbox_port1>>>' is valid for InnoDB cluster usage.
