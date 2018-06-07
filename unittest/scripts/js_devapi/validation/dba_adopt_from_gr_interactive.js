//@ Initialization
||

//@ it's not possible to adopt from GR without existing group replication
||Dba.createCluster: The adoptFromGR option is set to true, but there is no replication group to adopt (ArgumentError)

//@ Create cluster
||

//@ Adding instance to cluster
||

//@<OUT> Drop Metadata
Are you sure you want to remove the Metadata? [y/N]: Metadata Schema successfully removed.

//@ Check cluster status after drop metadata schema
||Cluster.status: This function is not available through a session to an instance belonging to an unmanaged replication group (RuntimeError)

//@<OUT> Create cluster adopting from GR - answer 'yes' to prompt
You are connected to an instance that belongs to an unmanaged replication group.
Do you want to setup an InnoDB cluster based on this replication group? [Y/n]: A new InnoDB cluster will be created based on the existing replication group on instance 'root@<<<real_hostname>>>:<<<__mysql_sandbox_port1>>>'.

Creating InnoDB cluster 'testCluster' on 'root@<<<real_hostname>>>:<<<__mysql_sandbox_port1>>>'...
Adding Seed Instance...
Adding Instance '<<<real_hostname>>>:<<<__mysql_sandbox_port2>>>'...

Cluster successfully created based on existing replication group.

//@<OUT> Check cluster status - success
{
    "clusterName": "testCluster",
    "defaultReplicaSet": {
        "name": "default",
        "primary": "<<<real_hostname>>>:<<<__mysql_sandbox_port1>>>",
        "ssl": "DISABLED",
        "status": "OK_NO_TOLERANCE",
        "statusText": "Cluster is NOT tolerant to any failures.",
        "topology": {
            "<<<real_hostname>>>:<<<__mysql_sandbox_port1>>>": {
                "address": "<<<real_hostname>>>:<<<__mysql_sandbox_port1>>>",
                "mode": "R/W",
                "readReplicas": {},
                "role": "HA",
                "status": "ONLINE"
            },
            "<<<real_hostname>>>:<<<__mysql_sandbox_port2>>>": {
                "address": "<<<real_hostname>>>:<<<__mysql_sandbox_port2>>>",
                "mode": "R/O",
                "readReplicas": {},
                "role": "HA",
                "status": "ONLINE"
            }
        }
    },
    "groupInformationSourceMember": "mysql://root@<<<real_hostname>>>:<<<__mysql_sandbox_port1>>>"
}

//@ Create cluster adopting from GR - answer 'no' to prompt
||Dba.createCluster: Creating a cluster on an unmanaged replication group requires adoptFromGR option to be true (ArgumentError)

//@ Check cluster status - failure
||The cluster object is disconnected. Please call Cluster.getCluster to obtain a fresh cluster handle. (RuntimeError)

//@<OUT> Create cluster adopting from GR - use 'adoptFromGR' option
A new InnoDB cluster will be created based on the existing replication group on instance 'root@<<<real_hostname>>>:<<<__mysql_sandbox_port1>>>'.

Creating InnoDB cluster 'testCluster' on 'root@<<<real_hostname>>>:<<<__mysql_sandbox_port1>>>'...
Adding Seed Instance...
Adding Instance '<<<real_hostname>>>:<<<__mysql_sandbox_port2>>>'...

Cluster successfully created based on existing replication group.

//@<OUT> Check cluster status - success - 'adoptFromGR'
{
    "clusterName": "testCluster",
    "defaultReplicaSet": {
        "name": "default",
        "primary": "<<<real_hostname>>>:<<<__mysql_sandbox_port1>>>",
        "ssl": "DISABLED",
        "status": "OK_NO_TOLERANCE",
        "statusText": "Cluster is NOT tolerant to any failures.",
        "topology": {
            "<<<real_hostname>>>:<<<__mysql_sandbox_port1>>>": {
                "address": "<<<real_hostname>>>:<<<__mysql_sandbox_port1>>>",
                "mode": "R/W",
                "readReplicas": {},
                "role": "HA",
                "status": "ONLINE"
            },
            "<<<real_hostname>>>:<<<__mysql_sandbox_port2>>>": {
                "address": "<<<real_hostname>>>:<<<__mysql_sandbox_port2>>>",
                "mode": "R/O",
                "readReplicas": {},
                "role": "HA",
                "status": "ONLINE"
            }
        }
    },
    "groupInformationSourceMember": "mysql://root@<<<real_hostname>>>:<<<__mysql_sandbox_port1>>>"
}

//@<OUT> Create cluster adopting from multi-primary GR - use 'adoptFromGR' option
A new InnoDB cluster will be created based on the existing replication group on instance 'root@<<<real_hostname>>>:<<<__mysql_sandbox_port1>>>'.

Creating InnoDB cluster 'testCluster' on 'root@<<<real_hostname>>>:<<<__mysql_sandbox_port1>>>'...
Adding Instance '<<<real_hostname>>>:<<<__mysql_sandbox_port1>>>'...
Adding Instance '<<<real_hostname>>>:<<<__mysql_sandbox_port2>>>'...

Cluster successfully created based on existing replication group.

//@<OUT> Check cluster status - success - adopt from multi-primary
{
    "clusterName": "testCluster",
    "defaultReplicaSet": {
        "name": "default",
        "ssl": "<<<__ssl_mode>>>",
        "status": "OK_NO_TOLERANCE",
        "statusText": "Cluster is NOT tolerant to any failures.",
        "topology": {
            "<<<real_hostname>>>:<<<__mysql_sandbox_port1>>>": {
                "address": "<<<real_hostname>>>:<<<__mysql_sandbox_port1>>>",
                "mode": "R/W",
                "readReplicas": {},
                "role": "HA",
                "status": "ONLINE"
            },
            "<<<real_hostname>>>:<<<__mysql_sandbox_port2>>>": {
                "address": "<<<real_hostname>>>:<<<__mysql_sandbox_port2>>>",
                "mode": "R/W",
                "readReplicas": {},
                "role": "HA",
                "status": "ONLINE"
            }
        }
    },
    "groupInformationSourceMember": "mysql://root@<<<real_hostname>>>:<<<__mysql_sandbox_port1>>>"
}

//@ dissolve the cluster
|The cluster was successfully dissolved.|

//@ it's not possible to adopt from GR when cluster was dissolved
||Dba.createCluster: The adoptFromGR option is set to true, but there is no replication group to adopt (ArgumentError)

//@ Finalization
||
