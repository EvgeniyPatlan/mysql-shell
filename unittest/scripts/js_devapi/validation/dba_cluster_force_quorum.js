//@ Initialization
||

//@<OUT> create cluster
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

//@ Add instance 2
||

//@ Add instance 3
||

//@ Disable group_replication_start_on_boot on second instance {VER(>=8.0.11)}
||

//@ Disable group_replication_start_on_boot on third instance {VER(>=8.0.11)}
||

//@ Kill instance 2
||

//@ Kill instance 3
||

//@ Start instance 2
||

//@ Start instance 3
||

//@<OUT> Cluster status
{
    "clusterName": "dev",
    "defaultReplicaSet": {
        "name": "default",
        "primary": "<<<localhost>>>:<<<__mysql_sandbox_port1>>>",
        "ssl": "<<<__ssl_mode>>>",
        "status": "NO_QUORUM",
        "statusText": "Cluster has no quorum as visible from '<<<localhost>>>:<<<__mysql_sandbox_port1>>>' and cannot process write transactions. 2 members are not active",
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
                "status": "UNREACHABLE"
            }
        }
    },
    "groupInformationSourceMember": "mysql://root@<<<localhost>>>:<<<__mysql_sandbox_port1>>>"
}

//@ Disconnect and reconnect to instance
||

//@<OUT> Get cluster operation must show a warning because there is no quorum
WARNING: Cluster has no quorum and cannot process write transactions: 1 out of 2 members of the InnoDB cluster are unreachable from the member we’re connected to, which is not sufficient for a quorum to be reached.

//@ Cluster.forceQuorumUsingPartitionOf errors
||Invalid number of arguments in Cluster.forceQuorumUsingPartitionOf, expected 1 to 2 but got 0
||Cluster.forceQuorumUsingPartitionOf: Invalid connection options, expected either a URI or a Dictionary
||Cluster.forceQuorumUsingPartitionOf: Invalid URI: empty.
||Cluster.forceQuorumUsingPartitionOf: Invalid connection options, expected either a URI or a Dictionary
||Cluster.forceQuorumUsingPartitionOf: The instance '<<<localhost>>>:<<<__mysql_sandbox_port2>>>' cannot be used to restore the cluster as it is not an active member of replication group.


//@ Cluster.forceQuorumUsingPartitionOf success
||

//@<OUT> Cluster status after force quorum
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

//@ Rejoin instance 2
||

//@ Rejoin instance 3
||

//@<OUT> Cluster status after rejoins
{
    "clusterName": "dev",
    "defaultReplicaSet": {
        "name": "default",
        "primary": "<<<localhost>>>:<<<__mysql_sandbox_port1>>>",
        "ssl": "<<<__ssl_mode>>>",
        "status": "OK",
        "statusText": "Cluster is ONLINE and can tolerate up to ONE failure.",
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
            },
            "<<<localhost>>>:<<<__mysql_sandbox_port3>>>": {
                "address": "<<<localhost>>>:<<<__mysql_sandbox_port3>>>",
                "mode": "R/O",
                "readReplicas": {},
                "role": "HA",
                "status": "ONLINE"
            }
        }
    },
    "groupInformationSourceMember": "mysql://root@<<<localhost>>>:<<<__mysql_sandbox_port1>>>"
}

//@ STOP group_replication on instance where forceQuorumUsingPartitionOf() was executed.
||

//@ Start group_replication on instance the same instance succeeds because group_replication_force_members is empty.
||

//@ Finalization
||
