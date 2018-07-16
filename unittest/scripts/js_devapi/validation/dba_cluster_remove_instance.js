//@ Initialization
||

//@ Connect
||

//@ create cluster
||

//@ Adding instance
||

//@<OUT> Number of instance according to GR.
2

//@ Failure: remove_instance - invalid uri
||Invalid URI: Missing user information

//@<OUT> Cluster status
{
    "clusterName": "dev",
    "defaultReplicaSet": {
        "name": "default",
        "primary": "<<<localhost>>>:<<<__mysql_sandbox_port1>>>",
        "ssl": "<<<__ssl_mode>>>",
        "status": "OK_NO_TOLERANCE",
        "statusText": "Cluster is NOT tolerant to any failures.",
        "topology": {
            "<<<localhost>>>:<<<__mysql_sandbox_port1>>>": {
                "address": "<<<localhost>>>:<<<__mysql_sandbox_port1>>>",
                "mode": "R/W",
                "readReplicas": {},
                "role": "HA",
                "status": "ONLINE"
            },
            "<<<localhost>>>:<<<__mysql_sandbox_port2>>>": {
                "address": "<<<localhost>>>:<<<__mysql_sandbox_port2>>>",
                "mode": "R/O",
                "readReplicas": {},
                "role": "HA",
                "status": "ONLINE"
            }
        }
    },
    "groupInformationSourceMember": "mysql://root@<<<localhost>>>:<<<__mysql_sandbox_port1>>>"
}

//@<OUT> Remove instance failure due to wrong credentials
ERROR: Unable to connect to instance '<<<localhost>>>:<<<__mysql_sandbox_port2>>>'. Please, verify connection credentials and make sure the instance is available.

//@<ERR> Remove instance failure due to wrong credentials
Cluster.removeInstance: Access denied for user 'foo'@'localhost' (using password: YES) (RuntimeError)

//@<OUT> Cluster status after remove failed
{
    "clusterName": "dev",
    "defaultReplicaSet": {
        "name": "default",
        "primary": "<<<localhost>>>:<<<__mysql_sandbox_port1>>>",
        "ssl": "<<<__ssl_mode>>>",
        "status": "OK_NO_TOLERANCE",
        "statusText": "Cluster is NOT tolerant to any failures.",
        "topology": {
            "<<<localhost>>>:<<<__mysql_sandbox_port1>>>": {
                "address": "<<<localhost>>>:<<<__mysql_sandbox_port1>>>",
                "mode": "R/W",
                "readReplicas": {},
                "role": "HA",
                "status": "ONLINE"
            },
            "<<<localhost>>>:<<<__mysql_sandbox_port2>>>": {
                "address": "<<<localhost>>>:<<<__mysql_sandbox_port2>>>",
                "mode": "R/O",
                "readReplicas": {},
                "role": "HA",
                "status": "ONLINE"
            }
        }
    },
    "groupInformationSourceMember": "mysql://root@<<<localhost>>>:<<<__mysql_sandbox_port1>>>"
}

//@<OUT> Removing instance
The instance will be removed from the InnoDB cluster. Depending on the instance
being the Seed or not, the Metadata session might become invalid. If so, please
start a new session to the Metadata Storage R/W instance.

Instance '<<<localhost>>>:<<<__mysql_sandbox_port2>>>' is attempting to leave the cluster...
<<<(__version_num<80011)?"WARNING: On instance 'localhost:"+__mysql_sandbox_port2+"' configuration cannot be persisted since MySQL version "+__version+" does not support the SET PERSIST command (MySQL version >= 8.0.11 required). Please set the 'group_replication_start_on_boot' variable to 'OFF' in the server configuration file, otherwise it might rejoin the cluster upon restart.\n":""\>>>
<<<(__version_num<80011)?"WARNING: On instance 'localhost:"+__mysql_sandbox_port1+"' membership change cannot be persisted since MySQL version "+__version+" does not support the SET PERSIST command (MySQL version >= 8.0.11 required). Please use the <Dba>.configureLocalInstance command locally to persist the changes.\n":""\>>>

The instance '<<<localhost>>>:<<<__mysql_sandbox_port2>>>' was successfully removed from the cluster.


//@<OUT> Cluster status after removal
{
    "clusterName": "dev",
    "defaultReplicaSet": {
        "name": "default",
        "primary": "<<<localhost>>>:<<<__mysql_sandbox_port1>>>",
        "ssl": "<<<__ssl_mode>>>",
        "status": "OK_NO_TOLERANCE",
        "statusText": "Cluster is NOT tolerant to any failures.",
        "topology": {
            "<<<localhost>>>:<<<__mysql_sandbox_port1>>>": {
                "address": "<<<localhost>>>:<<<__mysql_sandbox_port1>>>",
                "mode": "R/W",
                "readReplicas": {},
                "role": "HA",
                "status": "ONLINE"
            }
        }
    },
    "groupInformationSourceMember": "mysql://root@<<<localhost>>>:<<<__mysql_sandbox_port1>>>"
}

//@ Adding instance on port2 back (interactive use)
||

//@ Adding instance on port3 (interactive use)
||

//@<OUT> Removing instance (interactive: true)
The instance will be removed from the InnoDB cluster. Depending on the instance
being the Seed or not, the Metadata session might become invalid. If so, please
start a new session to the Metadata Storage R/W instance.

Instance '<<<localhost>>>:<<<__mysql_sandbox_port2>>>' is attempting to leave the cluster...
<<<(__version_num<80011)?"WARNING: On instance 'localhost:"+__mysql_sandbox_port2+"' configuration cannot be persisted since MySQL version "+__version+" does not support the SET PERSIST command (MySQL version >= 8.0.11 required). Please set the 'group_replication_start_on_boot' variable to 'OFF' in the server configuration file, otherwise it might rejoin the cluster upon restart.\n":""\>>>
<<<(__version_num<80011)?"WARNING: On instance 'localhost:"+__mysql_sandbox_port1+"' membership change cannot be persisted since MySQL version "+__version+" does not support the SET PERSIST command (MySQL version >= 8.0.11 required). Please use the <Dba>.configureLocalInstance command locally to persist the changes.\n":""\>>>
<<<(__version_num<80011)?"WARNING: On instance 'localhost:"+__mysql_sandbox_port3+"' membership change cannot be persisted since MySQL version "+__version+" does not support the SET PERSIST command (MySQL version >= 8.0.11 required). Please use the <Dba>.configureLocalInstance command locally to persist the changes.\n":""\>>>

The instance '<<<localhost>>>:<<<__mysql_sandbox_port2>>>' was successfully removed from the cluster.

//@<OUT> Removing instance (interactive: false)
The instance will be removed from the InnoDB cluster. Depending on the instance
being the Seed or not, the Metadata session might become invalid. If so, please
start a new session to the Metadata Storage R/W instance.

Instance '<<<localhost>>>:<<<__mysql_sandbox_port3>>>' is attempting to leave the cluster...
<<<(__version_num<80011)?"WARNING: On instance 'localhost:"+__mysql_sandbox_port3+"' configuration cannot be persisted since MySQL version "+__version+" does not support the SET PERSIST command (MySQL version >= 8.0.11 required). Please set the 'group_replication_start_on_boot' variable to 'OFF' in the server configuration file, otherwise it might rejoin the cluster upon restart.\n":""\>>>
<<<(__version_num<80011)?"WARNING: On instance 'localhost:"+__mysql_sandbox_port1+"' membership change cannot be persisted since MySQL version "+__version+" does not support the SET PERSIST command (MySQL version >= 8.0.11 required). Please use the <Dba>.configureLocalInstance command locally to persist the changes.\n":""\>>>

The instance '<<<localhost>>>:<<<__mysql_sandbox_port3>>>' was successfully removed from the cluster.

//@ Adding instance on port2 back (interactive: true, force: false)
||

//@ Adding instance on port3 back (interactive: true, force: true)
||

//@<OUT> Removing instance (interactive: true, force: false)
The instance will be removed from the InnoDB cluster. Depending on the instance
being the Seed or not, the Metadata session might become invalid. If so, please
start a new session to the Metadata Storage R/W instance.

Instance '<<<localhost>>>:<<<__mysql_sandbox_port2>>>' is attempting to leave the cluster...
<<<(__version_num<80011)?"WARNING: On instance 'localhost:"+__mysql_sandbox_port2+"' configuration cannot be persisted since MySQL version "+__version+" does not support the SET PERSIST command (MySQL version >= 8.0.11 required). Please set the 'group_replication_start_on_boot' variable to 'OFF' in the server configuration file, otherwise it might rejoin the cluster upon restart.\n":""\>>>
<<<(__version_num<80011)?"WARNING: On instance 'localhost:"+__mysql_sandbox_port1+"' membership change cannot be persisted since MySQL version "+__version+" does not support the SET PERSIST command (MySQL version >= 8.0.11 required). Please use the <Dba>.configureLocalInstance command locally to persist the changes.\n":""\>>>
<<<(__version_num<80011)?"WARNING: On instance 'localhost:"+__mysql_sandbox_port3+"' membership change cannot be persisted since MySQL version "+__version+" does not support the SET PERSIST command (MySQL version >= 8.0.11 required). Please use the <Dba>.configureLocalInstance command locally to persist the changes.\n":""\>>>

The instance '<<<localhost>>>:<<<__mysql_sandbox_port2>>>' was successfully removed from the cluster.

//@<OUT> Removing instance (interactive: true, force: true)
The instance will be removed from the InnoDB cluster. Depending on the instance
being the Seed or not, the Metadata session might become invalid. If so, please
start a new session to the Metadata Storage R/W instance.

Instance '<<<localhost>>>:<<<__mysql_sandbox_port3>>>' is attempting to leave the cluster...
<<<(__version_num<80011)?"WARNING: On instance 'localhost:"+__mysql_sandbox_port3+"' configuration cannot be persisted since MySQL version "+__version+" does not support the SET PERSIST command (MySQL version >= 8.0.11 required). Please set the 'group_replication_start_on_boot' variable to 'OFF' in the server configuration file, otherwise it might rejoin the cluster upon restart.\n":""\>>>
<<<(__version_num<80011)?"WARNING: On instance 'localhost:"+__mysql_sandbox_port1+"' membership change cannot be persisted since MySQL version "+__version+" does not support the SET PERSIST command (MySQL version >= 8.0.11 required). Please use the <Dba>.configureLocalInstance command locally to persist the changes.\n":""\>>>

The instance '<<<localhost>>>:<<<__mysql_sandbox_port3>>>' was successfully removed from the cluster.

//@ Adding instance on port2 back (interactive: false, force: false)
||

//@ Adding instance on port3 back (interactive: false, force: true)
||

//@<OUT> Removing instance (interactive: false, force: false)
The instance will be removed from the InnoDB cluster. Depending on the instance
being the Seed or not, the Metadata session might become invalid. If so, please
start a new session to the Metadata Storage R/W instance.

Instance '<<<localhost>>>:<<<__mysql_sandbox_port2>>>' is attempting to leave the cluster...
<<<(__version_num<80011)?"WARNING: On instance 'localhost:"+__mysql_sandbox_port2+"' configuration cannot be persisted since MySQL version "+__version+" does not support the SET PERSIST command (MySQL version >= 8.0.11 required). Please set the 'group_replication_start_on_boot' variable to 'OFF' in the server configuration file, otherwise it might rejoin the cluster upon restart.\n":""\>>>
<<<(__version_num<80011)?"WARNING: On instance 'localhost:"+__mysql_sandbox_port1+"' membership change cannot be persisted since MySQL version "+__version+" does not support the SET PERSIST command (MySQL version >= 8.0.11 required). Please use the <Dba>.configureLocalInstance command locally to persist the changes.\n":""\>>>
<<<(__version_num<80011)?"WARNING: On instance 'localhost:"+__mysql_sandbox_port3+"' membership change cannot be persisted since MySQL version "+__version+" does not support the SET PERSIST command (MySQL version >= 8.0.11 required). Please use the <Dba>.configureLocalInstance command locally to persist the changes.\n":""\>>>

The instance '<<<localhost>>>:<<<__mysql_sandbox_port2>>>' was successfully removed from the cluster.

//@<OUT> Removing instance (interactive: false, force: true)
The instance will be removed from the InnoDB cluster. Depending on the instance
being the Seed or not, the Metadata session might become invalid. If so, please
start a new session to the Metadata Storage R/W instance.

Instance '<<<localhost>>>:<<<__mysql_sandbox_port3>>>' is attempting to leave the cluster...
<<<(__version_num<80011)?"WARNING: On instance 'localhost:"+__mysql_sandbox_port3+"' configuration cannot be persisted since MySQL version "+__version+" does not support the SET PERSIST command (MySQL version >= 8.0.11 required). Please set the 'group_replication_start_on_boot' variable to 'OFF' in the server configuration file, otherwise it might rejoin the cluster upon restart.\n":""\>>>
<<<(__version_num<80011)?"WARNING: On instance 'localhost:"+__mysql_sandbox_port1+"' membership change cannot be persisted since MySQL version "+__version+" does not support the SET PERSIST command (MySQL version >= 8.0.11 required). Please use the <Dba>.configureLocalInstance command locally to persist the changes.\n":""\>>>

The instance '<<<localhost>>>:<<<__mysql_sandbox_port3>>>' was successfully removed from the cluster.

//@ Adding instance on port2 back (force: false)
||

//@ Adding instance on port3 back (force: true)
||

//@<OUT> Removing instance (force: false)
The instance will be removed from the InnoDB cluster. Depending on the instance
being the Seed or not, the Metadata session might become invalid. If so, please
start a new session to the Metadata Storage R/W instance.

Instance '<<<localhost>>>:<<<__mysql_sandbox_port2>>>' is attempting to leave the cluster...
<<<(__version_num<80011)?"WARNING: On instance 'localhost:"+__mysql_sandbox_port2+"' configuration cannot be persisted since MySQL version "+__version+" does not support the SET PERSIST command (MySQL version >= 8.0.11 required). Please set the 'group_replication_start_on_boot' variable to 'OFF' in the server configuration file, otherwise it might rejoin the cluster upon restart.\n":""\>>>
<<<(__version_num<80011)?"WARNING: On instance 'localhost:"+__mysql_sandbox_port1+"' membership change cannot be persisted since MySQL version "+__version+" does not support the SET PERSIST command (MySQL version >= 8.0.11 required). Please use the <Dba>.configureLocalInstance command locally to persist the changes.\n":""\>>>
<<<(__version_num<80011)?"WARNING: On instance 'localhost:"+__mysql_sandbox_port3+"' membership change cannot be persisted since MySQL version "+__version+" does not support the SET PERSIST command (MySQL version >= 8.0.11 required). Please use the <Dba>.configureLocalInstance command locally to persist the changes.\n":""\>>>

The instance '<<<localhost>>>:<<<__mysql_sandbox_port2>>>' was successfully removed from the cluster.

//@<OUT> Instance (force: false) does not exist in cluster metadata
false

//@<OUT> Removing instance (force: true)
The instance will be removed from the InnoDB cluster. Depending on the instance
being the Seed or not, the Metadata session might become invalid. If so, please
start a new session to the Metadata Storage R/W instance.

Instance '<<<localhost>>>:<<<__mysql_sandbox_port3>>>' is attempting to leave the cluster...
<<<(__version_num<80011)?"WARNING: On instance 'localhost:"+__mysql_sandbox_port3+"' configuration cannot be persisted since MySQL version "+__version+" does not support the SET PERSIST command (MySQL version >= 8.0.11 required). Please set the 'group_replication_start_on_boot' variable to 'OFF' in the server configuration file, otherwise it might rejoin the cluster upon restart.\n":""\>>>
<<<(__version_num<80011)?"WARNING: On instance 'localhost:"+__mysql_sandbox_port1+"' membership change cannot be persisted since MySQL version "+__version+" does not support the SET PERSIST command (MySQL version >= 8.0.11 required). Please use the <Dba>.configureLocalInstance command locally to persist the changes.\n":""\>>>

The instance '<<<localhost>>>:<<<__mysql_sandbox_port3>>>' was successfully removed from the cluster.

//@<OUT> Instance (force: true) does not exist in cluster metadata
false

//@<OUT> Number of instance according to GR after removal.
1

//@ Stop instance on port2.
||

//@ Restart instance on port2.
||

//@ Connect to restarted instance3.
||

//@<OUT> Removed instance3 does not exist on its Metadata.
false

//@ Connect to restarted instance2.
||

//@<OUT> Removed instance2 does not exist on its Metadata.
false

//@<OUT> Confirm that GR start on boot is disabled {VER(>=8.0.11)}.
OFF

//@ Connect back to seed instance and get cluster.
||

//@ Adding instance on port2 back
||

//@ Adding instance on port3 back
||

//@ Reset persisted gr_start_on_boot on instances {VER(>=8.0.11)}
||

//@ Stop instance on port2
||

//@ Stop instance on port3
||

//@<OUT> Cluster status after instance on port2 is stopped
{
    "clusterName": "dev",
    "defaultReplicaSet": {
        "name": "default",
        "primary": "<<<localhost>>>:<<<__mysql_sandbox_port1>>>",
        "ssl": "<<<__ssl_mode>>>",
        "status": "OK_NO_TOLERANCE",
        "statusText": "Cluster is NOT tolerant to any failures. 2 members are not active",
        "topology": {
            "<<<localhost>>>:<<<__mysql_sandbox_port1>>>": {
                "address": "<<<localhost>>>:<<<__mysql_sandbox_port1>>>",
                "mode": "R/W",
                "readReplicas": {},
                "role": "HA",
                "status": "ONLINE"
            },
            "<<<localhost>>>:<<<__mysql_sandbox_port2>>>": {
                "address": "<<<localhost>>>:<<<__mysql_sandbox_port2>>>",
                "mode": "R/O",
                "readReplicas": {},
                "role": "HA",
                "status": "(MISSING)"
            },
            "<<<localhost>>>:<<<__mysql_sandbox_port3>>>": {
                "address": "<<<localhost>>>:<<<__mysql_sandbox_port3>>>",
                "mode": "R/O",
                "readReplicas": {},
                "role": "HA",
                "status": "(MISSING)"
            }
        }
    },
    "groupInformationSourceMember": "mysql://root@<<<localhost>>>:<<<__mysql_sandbox_port1>>>"
}

//@ Error removing stopped instance on port2
|ERROR: The instance '<<<localhost>>>:<<<__mysql_sandbox_port2>>>' cannot be removed because it is on a '(Missing)' state. Please bring the instance back ONLINE and try to remove it again. If the instance is permanently not reachable, then please use <Cluster>.removeInstance() with the force option set to true to proceed with the operation and only remove the instance from the Cluster Metadata.|The instance '<<<localhost>>>:<<<__mysql_sandbox_port2>>>' is '(Missing)' (RuntimeError)

//@ Remove stopped instance on port2 with force option
||

//@<OUT> Confirm instance2 is not in cluster metadata
false

//@<OUT> Remove unreachable instance (interactive: false, force: false) - error
ERROR: The instance '<<<localhost>>>:<<<__mysql_sandbox_port3>>>' cannot be removed because it is on a '(Missing)' state. Please bring the instance back ONLINE and try to remove it again. If the instance is permanently not reachable, then please use <Cluster>.removeInstance() with the force option set to true to proceed with the operation and only remove the instance from the Cluster Metadata.

//@<ERR> Remove unreachable instance (interactive: false, force: false) - error
Cluster.removeInstance: The instance '<<<localhost>>>:<<<__mysql_sandbox_port3>>>' is '(Missing)' (RuntimeError)

//@<OUT> Remove unreachable instance (interactive: false, force: true) - success
The instance '<<<localhost>>>:<<<__mysql_sandbox_port3>>>' is not reachable and it will only be removed from the metadata. Please take any necessary actions to make sure that the instance will not rejoin the cluster if brought back online.

The instance will be removed from the InnoDB cluster. Depending on the instance
being the Seed or not, the Metadata session might become invalid. If so, please
start a new session to the Metadata Storage R/W instance.


The instance '<<<localhost>>>:<<<__mysql_sandbox_port3>>>' was successfully removed from the cluster.

//@<OUT> Cluster status after removal of instance on port2 and port3
{
    "clusterName": "dev",
    "defaultReplicaSet": {
        "name": "default",
        "primary": "<<<localhost>>>:<<<__mysql_sandbox_port1>>>",
        "ssl": "<<<__ssl_mode>>>",
        "status": "OK_NO_TOLERANCE",
        "statusText": "Cluster is NOT tolerant to any failures.",
        "topology": {
            "<<<localhost>>>:<<<__mysql_sandbox_port1>>>": {
                "address": "<<<localhost>>>:<<<__mysql_sandbox_port1>>>",
                "mode": "R/W",
                "readReplicas": {},
                "role": "HA",
                "status": "ONLINE"
            }
        }
    },
    "groupInformationSourceMember": "mysql://root@<<<localhost>>>:<<<__mysql_sandbox_port1>>>"
}

//@ Remove instances post actions since remove when unreachable (ensure they do not rejoin cluster)
||

//@ Restart instance on port2 again.
||

//@ Restart instance on port3 again.
||

//@ Connect to instance2 (removed unreachable)
||

//@<OUT> Confirm instance2 is still in its metadata
true

//@ Connect to seed instance and get cluster again
||

//@ Adding instance on port2 again
||

//@ Reset persisted gr_start_on_boot on instance2 {VER(>=8.0.11)}
||

//@ Stop instance on port2 again
||

//@ Adding instance on port3 again
||

//@ Stop instance on port3 again
||

//@<OUT> Remove unreachable instance (interactive: true, force: false) - error
ERROR: The instance '<<<localhost>>>:<<<__mysql_sandbox_port2>>>' cannot be removed because it is on a '(Missing)' state. Please bring the instance back ONLINE and try to remove it again. If the instance is permanently not reachable, then please use <Cluster>.removeInstance() with the force option set to true to proceed with the operation and only remove the instance from the Cluster Metadata.

//@<ERR> Remove unreachable instance (interactive: true, force: false) - error
Cluster.removeInstance: The instance '<<<localhost>>>:<<<__mysql_sandbox_port2>>>' is '(Missing)' (RuntimeError)

//@<OUT> Remove unreachable instance (interactive: false) - error
ERROR: The instance '<<<localhost>>>:<<<__mysql_sandbox_port3>>>' cannot be removed because it is on a '(Missing)' state. Please bring the instance back ONLINE and try to remove it again. If the instance is permanently not reachable, then please use <Cluster>.removeInstance() with the force option set to true to proceed with the operation and only remove the instance from the Cluster Metadata.

//@<ERR> Remove unreachable instance (interactive: false) - error
Cluster.removeInstance: The instance '<<<localhost>>>:<<<__mysql_sandbox_port3>>>' is '(Missing)' (RuntimeError)

//@<OUT> Remove unreachable instance (interactive: true, answer NO) - error
ERROR: The instance '<<<localhost>>>:<<<__mysql_sandbox_port2>>>' cannot be removed because it is on a '(Missing)' state. Please bring the instance back ONLINE and try to remove it again. If the instance is permanently not reachable, then you can choose to proceed with the operation and only remove the instance from the Cluster Metadata.

Do you want to continue anyway (only the instance metadata will be removed)? [y/N]:

//@<ERR> Remove unreachable instance (interactive: true, answer NO) - error
Cluster.removeInstance: The instance '<<<localhost>>>:<<<__mysql_sandbox_port2>>>' is '(Missing)' (RuntimeError)

//@<OUT> Remove unreachable instance (interactive: true, answer YES) - success
ERROR: The instance '<<<localhost>>>:<<<__mysql_sandbox_port2>>>' cannot be removed because it is on a '(Missing)' state. Please bring the instance back ONLINE and try to remove it again. If the instance is permanently not reachable, then you can choose to proceed with the operation and only remove the instance from the Cluster Metadata.

Do you want to continue anyway (only the instance metadata will be removed)? [y/N]:
The instance '<<<localhost>>>:<<<__mysql_sandbox_port2>>>' is not reachable and it will only be removed from the metadata. Please take any necessary actions to make sure that the instance will not rejoin the cluster if brought back online.

The instance will be removed from the InnoDB cluster. Depending on the instance
being the Seed or not, the Metadata session might become invalid. If so, please
start a new session to the Metadata Storage R/W instance.


The instance '<<<localhost>>>:<<<__mysql_sandbox_port2>>>' was successfully removed from the cluster.

//@<OUT> Remove unreachable instance (interactive: true, force: true) - success
The instance '<<<localhost>>>:<<<__mysql_sandbox_port3>>>' is not reachable and it will only be removed from the metadata. Please take any necessary actions to make sure that the instance will not rejoin the cluster if brought back online.

The instance will be removed from the InnoDB cluster. Depending on the instance
being the Seed or not, the Metadata session might become invalid. If so, please
start a new session to the Metadata Storage R/W instance.


The instance '<<<localhost>>>:<<<__mysql_sandbox_port3>>>' was successfully removed from the cluster.

//@ Remove instance post actions (ensure they do not rejoin cluster)
||

//@ Restart instance on port2 one last time
||

//@ Adding instance on port2 one last time
||

//@ Connect to instance2 to introduce a replication error
||

//@ Introduce a replication error at instance 2
||

//@ Connect to seed instance and get cluster on last time
||

//@ Change shell option dba.gtidWaitTimeout to 1 second
||

//@<OUT> Remove instance with replication error - error
The instance will be removed from the InnoDB cluster. Depending on the instance
being the Seed or not, the Metadata session might become invalid. If so, please
start a new session to the Metadata Storage R/W instance.

ERROR: The instance '<<<localhost>>>:<<<__mysql_sandbox_port2>>>' was unable to catch up with cluster transactions. There might be too many transactions to apply or some replication error. In the former case, you can retry the operation (using a higher timeout value by setting the global shell option 'dba.gtidWaitTimeout'). In the later case, analyze and fix any replication error. You can also choose to skip this error using the 'force: true' option, but it might leave the instance in an inconsistent state and lead to errors if you want to reuse it.
//@<ERR> Remove instance with replication error - error
Cluster.removeInstance: Timeout reached waiting for cluster transactions to be applied on instance '<<<localhost>>>:<<<__mysql_sandbox_port2>>>' (RuntimeError)

//@ Change shell option dba.gtidWaitTimeout to 5 second
||

//@<OUT> Remove instance with replication error (force: true) - success
The instance will be removed from the InnoDB cluster. Depending on the instance
being the Seed or not, the Metadata session might become invalid. If so, please
start a new session to the Metadata Storage R/W instance.

WARNING: An error occured when trying to catch up with cluster transactions and the instance might have been left in an inconsistent state that will lead to errors if it is reused.

Instance '<<<localhost>>>:<<<__mysql_sandbox_port2>>>' is attempting to leave the cluster...
<<<(__version_num<80011)?"WARNING: On instance 'localhost:"+__mysql_sandbox_port2+"' configuration cannot be persisted since MySQL version "+__version+" does not support the SET PERSIST command (MySQL version >= 8.0.11 required). Please set the 'group_replication_start_on_boot' variable to 'OFF' in the server configuration file, otherwise it might rejoin the cluster upon restart.\n":""\>>>
<<<(__version_num<80011)?"WARNING: On instance 'localhost:"+__mysql_sandbox_port1+"' membership change cannot be persisted since MySQL version "+__version+" does not support the SET PERSIST command (MySQL version >= 8.0.11 required). Please use the <Dba>.configureLocalInstance command locally to persist the changes.\n":""\>>>

The instance '<<<localhost>>>:<<<__mysql_sandbox_port2>>>' was successfully removed from the cluster.

//@ Error removing last instance
|ERROR: The instance '<<<localhost>>>:<<<__mysql_sandbox_port1>>>' cannot be removed because it is the only member of the Cluster. Please use <Cluster>.dissolve() instead to remove the last instance and dissolve the Cluster.|Cluster.removeInstance: The instance '<<<localhost>>>:<<<__mysql_sandbox_port1>>>' is the last member of the ReplicaSet (LogicError)

//@ Dissolve cluster with success
||

//@ Cluster re-created with success
||

//@ Finalization
||
