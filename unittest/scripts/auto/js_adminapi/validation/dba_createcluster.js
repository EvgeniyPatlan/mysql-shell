//@ WL#12049: Initialization
||

//@ WL#12049: Unsupported server version {VER(<5.7.24)}
||Option 'exitStateAction' not supported on target server version: '<<<__version>>>'

//@ WL#12049: Create cluster errors using exitStateAction option {VER(>=5.7.24)}
||Invalid value for exitStateAction, string value cannot be empty.
||Invalid value for exitStateAction, string value cannot be empty.
||Error starting cluster: Invalid value for exitStateAction, can't be set to the value of ':'
||Error starting cluster: Invalid value for exitStateAction, can't be set to the value of 'AB'
||Error starting cluster: Invalid value for exitStateAction, can't be set to the value of '10'

//@ WL#12049: Create cluster specifying a valid value for exitStateAction (ABORT_SERVER) {VER(>=5.7.24)}
||

//@<OUT> WL#12049: Confirm group_replication_exit_state_action is set correctly (ABORT_SERVER) {VER(>=5.7.24)}
ABORT_SERVER

//@ WL#12049: Dissolve cluster 1 {VER(>=5.7.24)}
||

//@ WL#12049: Create cluster specifying a valid value for exitStateAction (READ_ONLY) {VER(>=5.7.24)}
||

//@<OUT> WL#12049: Confirm group_replication_exit_state_action is set correctly (READ_ONLY) {VER(>=5.7.24)}
READ_ONLY

//@ WL#12049: Dissolve cluster 2 {VER(>=5.7.24)}
||

//@ WL#12049: Create cluster specifying a valid value for exitStateAction (1) {VER(>=5.7.24)}
||

//@<OUT> WL#12049: Confirm group_replication_exit_state_action is set correctly (1) {VER(>=5.7.24)}
ABORT_SERVER

//@ WL#12049: Dissolve cluster 3 {VER(>=5.7.24)}
||

//@ WL#12049: Create cluster specifying a valid value for exitStateAction (0) {VER(>=5.7.24)}
||

//@<OUT> WL#12049: Confirm group_replication_exit_state_action is set correctly (0) {VER(>=5.7.24)}
READ_ONLY

//@ WL#12049: Dissolve cluster 4 {VER(>=5.7.24)}
||

//@ WL#12049: Create cluster {VER(>=8.0.12)}
||

//@<OUT> WL#12049: exitStateAction must be persisted on mysql >= 8.0.12 {VER(>=8.0.12)}
group_replication_bootstrap_group = OFF
group_replication_exit_state_action = READ_ONLY
group_replication_force_members =
group_replication_group_name = ca94447b-e6fc-11e7-b69d-4485005154dc
group_replication_group_seeds =
group_replication_local_address = <<<localhost>>>:<<<__mysql_sandbox_gr_port1>>>
group_replication_recovery_use_ssl = ON
group_replication_single_primary_mode = ON
group_replication_ssl_mode = REQUIRED
group_replication_start_on_boot = ON

//@ WL#12049: Dissolve cluster 6 {VER(>=8.0.12)}
||

//@ WL#12049: Initialize new instance
||

//@ WL#12049: Create cluster 2 {VER(>=8.0.12)}
||

//@<OUT> BUG#28701263: DEFAULT VALUE OF EXITSTATEACTION TOO DRASTIC {VER(>=8.0.12)}
group_replication_bootstrap_group = OFF
group_replication_exit_state_action = READ_ONLY
group_replication_group_name = ca94447b-e6fc-11e7-b69d-4485005154dc
group_replication_group_seeds =
group_replication_local_address = <<<localhost>>>:<<<__mysql_sandbox_gr_port1>>>
group_replication_recovery_use_ssl = ON
group_replication_single_primary_mode = ON
group_replication_ssl_mode = REQUIRED
group_replication_start_on_boot = ON


//@ WL#12049: Finalization
||

//@ WL#11032: Initialization
||

//@ WL#11032: Unsupported server version {VER(<5.7.20)}
||Option 'memberWeight' not available for target server version.

//@ WL#11032: Create cluster errors using memberWeight option {VER(>=5.7.20)}
||Option 'memberWeight' Integer expected, but value is String (TypeError)
||Option 'memberWeight' is expected to be of type Integer, but is Bool (TypeError)
||Option 'memberWeight' Integer expected, but value is String (TypeError)
||Option 'memberWeight' is expected to be of type Integer, but is Float (TypeError)

//@ WL#11032: Create cluster specifying a valid value for memberWeight (25) {VER(>=5.7.20)}
||

//@<OUT> WL#11032: Confirm group_replication_member_weight is set correctly (25) {VER(>=5.7.20)}
25

//@ WL#11032: Dissolve cluster 1 {VER(>=5.7.20)}
||

//@ WL#11032: Create cluster specifying a valid value for memberWeight (100) {VER(>=5.7.20)}
||

//@<OUT> WL#11032: Confirm group_replication_member_weight is set correctly (100) {VER(>=5.7.20)}
100

//@ WL#11032: Dissolve cluster 2 {VER(>=5.7.20)}
||

//@ WL#11032: Create cluster specifying a valid value for memberWeight (-50) {VER(>=5.7.20)}
||

//@<OUT> WL#11032: Confirm group_replication_member_weight is set correctly (0) {VER(>=5.7.20)}
0

//@ WL#11032: Dissolve cluster 3 {VER(>=5.7.20)}
||

//@ WL#11032: Create cluster {VER(>=8.0.11)}
||

//@<OUT> WL#11032: memberWeight must be persisted on mysql >= 8.0.11 {VER(>=8.0.12)}
group_replication_bootstrap_group = OFF
group_replication_exit_state_action = READ_ONLY
group_replication_force_members =
group_replication_group_name = ca94447b-e6fc-11e7-b69d-4485005154dc
group_replication_group_seeds =
group_replication_local_address = <<<localhost>>>:<<<__mysql_sandbox_gr_port1>>>
group_replication_member_weight = 75
group_replication_recovery_use_ssl = ON
group_replication_single_primary_mode = ON
group_replication_ssl_mode = REQUIRED
group_replication_start_on_boot = ON

//@ WL#11032: Dissolve cluster 6 {VER(>=8.0.11)}
||

//@ WL#11032: Initialize new instance
||

//@ WL#11032: Create cluster 2 {VER(>=8.0.11)}
||

//@<OUT> WL#11032: memberWeight must not be persisted on mysql >= 8.0.11 if not set {VER(>=8.0.12)}
group_replication_bootstrap_group = OFF
group_replication_exit_state_action = READ_ONLY
group_replication_group_name = ca94447b-e6fc-11e7-b69d-4485005154dc
group_replication_group_seeds =
group_replication_local_address = <<<localhost>>>:<<<__mysql_sandbox_gr_port1>>>
group_replication_recovery_use_ssl = ON
group_replication_single_primary_mode = ON
group_replication_ssl_mode = REQUIRED
group_replication_start_on_boot = ON

//@ WL#11032: Finalization
||

//@ WL#12067: Initialization
||

//@ WL#12067: TSF1_6 Unsupported server version {VER(<8.0.14)}
||Option 'failoverConsistency' not supported on target server version: '<<<__version>>>'

//@ WL#12067: Create cluster errors using failoverConsistency option {VER(>=8.0.14)}
||Invalid value for failoverConsistency, string value cannot be empty.
||Invalid value for failoverConsistency, string value cannot be empty.
||Error starting cluster: Invalid value for failoverConsistency, can't be set to the value of ':'
||Error starting cluster: Invalid value for failoverConsistency, can't be set to the value of 'AB'
||Error starting cluster: Invalid value for failoverConsistency, can't be set to the value of '10'
||Option 'failoverConsistency' is expected to be of type String, but is Integer (TypeError)

//@ WL#12067: TSF1_1 Create cluster using BEFORE_ON_PRIMARY_FAILOVER as value for failoverConsistency {VER(>=8.0.14)}
||

//@<OUT> WL#12067: TSF1_1 Confirm group_replication_consistency is set correctly (BEFORE_ON_PRIMARY_FAILOVER) {VER(>=8.0.14)}
BEFORE_ON_PRIMARY_FAILOVER

//@<OUT> WL#12067: TSF1_1 Confirm group_replication_consistency was correctly persisted. {VER(>=8.0.14)}
group_replication_consistency = BEFORE_ON_PRIMARY_FAILOVER

//@ WL#12067: Dissolve cluster 1 {VER(>=8.0.14)}
||

//@ WL#12067: TSF1_2 Create cluster using EVENTUAL as value for failoverConsistency {VER(>=8.0.14)}
||

//@<OUT> WL#12067: TSF1_2 Confirm group_replication_consistency is set correctly (EVENTUAL) {VER(>=8.0.14)}
EVENTUAL

//@<OUT> WL#12067: TSF1_2 Confirm group_replication_consistency was correctly persisted. {VER(>=8.0.14)}
group_replication_consistency = EVENTUAL

//@ WL#12067: Dissolve cluster 2 {VER(>=8.0.14)}
||

//@ WL#12067: TSF1_1 Create cluster using 1 as value for failoverConsistency {VER(>=8.0.14)}
||

//@<OUT> WL#12067: TSF1_1 Confirm group_replication_consistency is set correctly (1) {VER(>=8.0.14)}
BEFORE_ON_PRIMARY_FAILOVER

//@ WL#12067: Dissolve cluster 3 {VER(>=8.0.14)}
||

//@ WL#12067: TSF1_2 Create cluster using 0 as value for failoverConsistency {VER(>=8.0.14)}
||

//@<OUT> WL#12067: TSF1_2 Confirm group_replication_consistency is set correctly (0) {VER(>=8.0.14)}
EVENTUAL

//@ WL#12067: Dissolve cluster 4 {VER(>=8.0.14)}
||

//@ WL#12067: TSF1_3 Create cluster using no value for failoverConsistency {VER(>=8.0.14)}
||

//@<OUT> WL#12067: TSF1_3 Confirm without failoverConsistency group_replication_consistency is set to default (EVENTUAL) {VER(>=8.0.14)}
EVENTUAL

//@ WL#12067: Dissolve cluster 5 {VER(>=8.0.14)}
||

//@ WL#12067: TSF1_7 Create cluster using evenTual as value for failoverConsistency throws no exception (case insensitive) {VER(>=8.0.14)}
||

//@<OUT> WL#12067: TSF1_7 Confirm group_replication_consistency is set correctly (EVENTUAL) {VER(>=8.0.14)}
EVENTUAL

//@ WL#12067: Dissolve cluster 6 {VER(>=8.0.14)}
||

//@ WL#12067: TSF1_8 Create cluster using Before_ON_PriMary_FailoveR as value for failoverConsistency throws no exception (case insensitive) {VER(>=8.0.14)}
||

//@<OUT> WL#12067: TSF1_8 Confirm group_replication_consistency is set correctly (BEFORE_ON_PRIMARY_FAILOVER) {VER(>=8.0.14)}
BEFORE_ON_PRIMARY_FAILOVER

//@ WL#12067: Dissolve cluster 7 {VER(>=8.0.14)}
||

//@ WL#12067: Initialize new instance {VER(>=8.0.14)}
||

//@ WL#12067: Create cluster 2 {VER(>=8.0.14)}
||

//@<OUT> WL#12067: failoverConsistency must not be persisted on mysql >= 8.0.14 if not set {VER(>=8.0.14)}

//@ WL#12067: Finalization
||

//@ WL#12050: Initialization
||

//@ WL#12050: TSF1_5 Unsupported server version {VER(<8.0.13)}
||Option 'expelTimeout' not supported on target server version: '<<<__version>>>'

//@ WL#12050: Create cluster errors using expelTimeout option {VER(>=8.0.13)}
// TSF1_3, TSF1_4, TSF1_6
||Option 'expelTimeout' Integer expected, but value is String (TypeError)
||Option 'expelTimeout' Integer expected, but value is String (TypeError)
||Option 'expelTimeout' is expected to be of type Integer, but is Float (TypeError)
||Option 'expelTimeout' is expected to be of type Integer, but is Bool (TypeError)
||Invalid value for expelTimeout, integer value must be in the range: [0, 3600] (ArgumentError)
||Invalid value for expelTimeout, integer value must be in the range: [0, 3600] (ArgumentError)

//@ WL#12050: TSF1_1 Create cluster using 12 as value for expelTimeout {VER(>=8.0.13)}
||

//@ WL#12050: TSF1_1 Confirm group_replication_member_expel_timeout is set correctly (12) {VER(>=8.0.13)}
|12|

//@<OUT> WL#12050: TSF1_1 Confirm group_replication_consistency was correctly persisted. {VER(>=8.0.13)}
group_replication_member_expel_timeout = 12

//@ WL#12050: Dissolve cluster 1 {VER(>=8.0.13)}
||

//@ WL#12050: TSF1_2 Initialize new instance {VER(>=8.0.13)}
||

//@ WL#12050: TSF1_2 Create cluster using no value for expelTimeout, confirm it has the default value {VER(>=8.0.13)}
||

//@ WL#12050: TSF1_2 Confirm group_replication_member_expel_timeout is set correctly (0) {VER(>=8.0.13)}
|0|

//@<OUT> WL#12050: TSF1_2 Confirm group_replication_member_expel_timeout was not persisted since no value was provided. {VER(>=8.0.13)}

//@ WL#12050: Finalization
||
