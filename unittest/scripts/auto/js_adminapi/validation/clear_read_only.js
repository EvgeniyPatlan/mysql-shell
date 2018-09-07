
//@ Dba_create_cluster.clear_read_only_invalid
||Dba.createCluster: Option 'clearReadOnly': Invalid typecast: Bool expected, but value is String (TypeError)

//@<OUT> Dba_create_cluster.clear_read_only_unset
Validating instance at localhost:<<<__mysql_sandbox_port1>>>...
Instance detected as a sandbox.
Please note that sandbox instances are only suitable for deploying test clusters for use within the same host.

This instance reports its own address as <<<real_hostname>>>

Instance configuration is suitable.
Creating InnoDB cluster 'dev' on 'root@localhost:<<<__mysql_sandbox_port1>>>'...
ERROR: The MySQL instance at 'localhost:<<<__mysql_sandbox_port1>>>' currently has the super_read_only system variable set to protect it from inadvertent updates from applications. You must first unset it to be able to perform any changes to this instance. For more information see: https://dev.mysql.com/doc/refman/en/server-system-variables.html#sysvar_super_read_only.

//@<ERR> Dba_create_cluster.clear_read_only_unset
Dba.createCluster: Server in SUPER_READ_ONLY mode (RuntimeError)

//@<OUT> Dba_create_cluster.clear_read_only_false
Validating instance at localhost:<<<__mysql_sandbox_port1>>>...
Instance detected as a sandbox.
Please note that sandbox instances are only suitable for deploying test clusters for use within the same host.

This instance reports its own address as <<<real_hostname>>>

Instance configuration is suitable.
Creating InnoDB cluster 'dev' on 'root@localhost:<<<__mysql_sandbox_port1>>>'...
ERROR: The MySQL instance at 'localhost:<<<__mysql_sandbox_port1>>>' currently has the super_read_only system variable set to protect it from inadvertent updates from applications. You must first unset it to be able to perform any changes to this instance. For more information see: https://dev.mysql.com/doc/refman/en/server-system-variables.html#sysvar_super_read_only.

//@<ERR> Dba_create_cluster.clear_read_only_false
Dba.createCluster: Server in SUPER_READ_ONLY mode (RuntimeError)

//@ Check unchanged
||

//@ Dba_configure_local_instance.clear_read_only_invalid
||Dba.configureLocalInstance: Option 'clearReadOnly': Invalid typecast: Bool expected, but value is String


//@<OUT> Dba_configure_local_instance.clear_read_only_unset
Configuring local MySQL instance listening at port <<<__mysql_sandbox_port1>>> for use in an InnoDB cluster...
Instance detected as a sandbox.
Please note that sandbox instances are only suitable for deploying test clusters for use within the same host.

This instance reports its own address as <<<real_hostname>>>
Clients and other cluster members will communicate with it through this address by default. If this is not correct, the report_host MySQL system variable should be changed.
Assuming full account name 'root'@'%' for root
WARNING: MySQL configuration file /path/to/file.cnf does not exist. Skipping file verification.

The instance 'localhost:<<<__mysql_sandbox_port1>>>' is valid for InnoDB cluster usage.
ERROR: The MySQL instance at 'localhost:<<<__mysql_sandbox_port1>>>' currently has the super_read_only system variable set to protect it from inadvertent updates from applications. You must first unset it to be able to perform any changes to this instance. For more information see: https://dev.mysql.com/doc/refman/en/server-system-variables.html#sysvar_super_read_only.

//@<ERR> Dba_configure_local_instance.clear_read_only_unset
Dba.configureLocalInstance: Server in SUPER_READ_ONLY mode (RuntimeError)

//@<OUT> Dba_configure_local_instance.clear_read_only_false
Configuring local MySQL instance listening at port <<<__mysql_sandbox_port1>>> for use in an InnoDB cluster...
Instance detected as a sandbox.
Please note that sandbox instances are only suitable for deploying test clusters for use within the same host.

This instance reports its own address as <<<real_hostname>>>
Clients and other cluster members will communicate with it through this address by default. If this is not correct, the report_host MySQL system variable should be changed.
Assuming full account name 'root'@'%' for root
WARNING: MySQL configuration file /path/to/file.cnf does not exist. Skipping file verification.

The instance 'localhost:<<<__mysql_sandbox_port1>>>' is valid for InnoDB cluster usage.
ERROR: The MySQL instance at 'localhost:<<<__mysql_sandbox_port1>>>' currently has the super_read_only system variable set to protect it from inadvertent updates from applications. You must first unset it to be able to perform any changes to this instance. For more information see: https://dev.mysql.com/doc/refman/en/server-system-variables.html#sysvar_super_read_only.

//@<ERR> Dba_configure_local_instance.clear_read_only_false
Dba.configureLocalInstance: Server in SUPER_READ_ONLY mode (RuntimeError)

//@ Dba_drop_metadata.clear_read_only_invalid
||Dba.dropMetadataSchema: Argument 'clearReadOnly' is expected to be a bool


//@<OUT> Dba_drop_metadata.clear_read_only_unset
ERROR: The MySQL instance at 'localhost:<<<__mysql_sandbox_port1>>>' currently has the super_read_only system variable set to protect it from inadvertent updates from applications. You must first unset it to be able to perform any changes to this instance. For more information see: https://dev.mysql.com/doc/refman/en/server-system-variables.html#sysvar_super_read_only.

//@<ERR> Dba_drop_metadata.clear_read_only_unset
Dba.dropMetadataSchema: Server in SUPER_READ_ONLY mode (RuntimeError)

//@<OUT> Dba_drop_metadata.clear_read_only_false
ERROR: The MySQL instance at 'localhost:<<<__mysql_sandbox_port1>>>' currently has the super_read_only system variable set to protect it from inadvertent updates from applications. You must first unset it to be able to perform any changes to this instance. For more information see: https://dev.mysql.com/doc/refman/en/server-system-variables.html#sysvar_super_read_only.

//@<ERR> Dba_drop_metadata.clear_read_only_false
Dba.dropMetadataSchema: Server in SUPER_READ_ONLY mode (RuntimeError)

//@ Dba_reboot_cluster.clear_read_only_invalid
||Dba.rebootClusterFromCompleteOutage: Argument 'clearReadOnly' is expected to be a bool

//@<OUT> Dba_reboot_cluster.clear_read_only_unset
ERROR: The MySQL instance at 'localhost:<<<__mysql_sandbox_port1>>>' currently has the super_read_only system variable set to protect it from inadvertent updates from applications. You must first unset it to be able to perform any changes to this instance. For more information see: https://dev.mysql.com/doc/refman/en/server-system-variables.html#sysvar_super_read_only.

//@<ERR> Dba_reboot_cluster.clear_read_only_unset
Dba.rebootClusterFromCompleteOutage: Server in SUPER_READ_ONLY mode (RuntimeError)
