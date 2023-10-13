# System Views 

Greenplum Database provides the following system views:

-   [gp_backend_memory_contexts](#gp_backend_memory_contexts)
-   [gp_config](#gp_config)
-   [gp_cursors](#gp_cursors)
-   [gp_distributed_log](#gp_distributed_log)
-   [gp_distributed_xacts](#gp_distributed_xacts)
-   [gp_endpoints](#gp_endpoints)
-   [gp_file_settings](#gp_file_settings)
-   [gp_pgdatabase](#gp_pgdatabase)
-   [gp_replication_origin_status](#gp_replication_origin_status)
-   [gp_replication_slots](#gp_replication_slots)
-   [gp_resgroup_config](#gp_resgroup_config)
-   [gp_resgroup_iostats_per_host](#gp_resgroup_iostats_per_host)
-   [gp_resgroup_status](#gp_resgroup_status)
-   [gp_resgroup_status_per_host](#gp_resgroup_status_per_host)
-   [gp_resgroup_status_per_segment](#gp_resgroup_status_per_segment)
-   [gp_resqueue_status](#gp_resqueue_status)
-   [gp_segment_endpoints](#gp_segment_endpoints)
-   [gp_session_endpoints](#gp_session_endpoints)
-   [gp_settings](#gp_settings)
-   [gp_suboverflowed_backend](#gp_suboverflowed_backend)
-   [gp_stat_activity](#gp_stat_activity)
-   [gp_stat_all_indexes](#gp_stat_all_indexes)
-   [gp_stat_all_tables](#gp_stat_all_tables)
-   [gp_stat_archiver](#gp_stat_archiver)
-   [gp_stat_bgwriter](#gp_stat_bgwriter)
-   [gp_stat_database](#gp_stat_database)
-   [gp_stat_database_conflicts](#gp_stat_database_conflicts)
-   [gp_stat_gssapi](#gp_stat_gssapi)
-   [gp_stat_operations](#gp_stat_operations)
-   [gp_stat_progress_analyze](#gp_stat_progress_analyze)
-   [gp_stat_progress_basebackup](#gp_stat_progress_basebackup)
-   [gp_stat_progress_cluster](#gp_stat_progress_cluster)
-   [gp_stat_progress_copy](#gp_stat_progress_copy)
-   [gp_stat_progress_create_index](#gp_stat_progress_create_index)
-   [gp_stat_progress_vacuum](#gp_stat_progress_vacuum)
-   [gp_stat_replication](#gp_stat_replication)
-   [gp_stat_resqueues](#gp_stat_resqueues)
-   [gp_stat_slru](#gp_stat_slru)
-   [gp_stat_ssl](#gp_stat_ssl)
-   [gp_stat_subscription](#gp_stat_subscription)
-   [gp_stat_sys_indexes](#gp_stat_sys_indexes)
-   [gp_stat_sys_tables](#gp_stat_sys_tables)
-   [gp_stat_user_functions](#gp_stat_user_functions)
-   [gp_stat_user_indexes](#gp_stat_user_indexes)
-   [gp_stat_user_tables](#gp_stat_user_tables)
-   [gp_stat_wal_receiver](#gp_stat_wal_receiver)
-   [gp_stat_wal](#gp_stat_wal)
-   [gp_stat_xact_all_tables](#gp_stat_xact_all_tables)
-   [gp_stat_xact_sys_tables](#gp_stat_xact_sys_tables)
-   [gp_stat_xact_user_functions](#gp_stat_xact_user_functions)
-   [gp_stat_xact_user_tables](#gp_stat_xact_user_tables)
-   [gp_statio_all_indexes](#gp_statio_all_indexes)
-   [gp_statio_all_sequences](#gp_statio_all_sequences)
-   [gp_statio_all_tables](#gp_statio_all_tables)
-   [gp_statio_sys_indexes](#gp_statio_sys_indexes)
-   [gp_statio_sys_sequences](#gp_statio_sys_sequences)
-   [gp_statio_sys_tables](#gp_statio_sys_tables)
-   [gp_statio_user_indexes](#gp_statio_user_indexes)
-   [gp_statio_user_sequences](#gp_statio_user_sequences)
-   [gp_statio_user_tables](#gp_statio_user_tables)
-   [gp_stats](#gp_stats)
-   [gp_stats_ext](#gp_stats_ext)
-   [gp_transaction_log](#gp_transaction_log)
-   [gpexpand.expansion_progress](#gpexpandexpansion_progress)
-   [pg_backend_memory_contexts](#pg_backend_memory_contexts)
-   [pg_cursors](#pg_cursors)
-   [pg_exttable](#pg_exttable)
-   [pg_matviews](#pg_matviews)
-   [pg_max_external_files](#pg_max_external_files)
-   [pg_policies](#pg_policies)
-   [pg_resqueue_attributes](#pg_resqueue_attributes)
-   pg_resqueue_status (Deprecated. Use [gp_toolkit.gp_resqueue_status](../gp_toolkit.html).)
-   [pg_stat_activity](#pg_stat_activity)
-   [pg_stat_all_indexes](#pg_stat_all_indexes)
-   [pg_stat_all_tables](#pg_stat_all_tables)
-   [pg_stat_operations](#pg_stat_operations)
-   [pg_stat_replication](#pg_stat_replication)
-   [pg_stat_resqueues](#pg_stat_resqueues)
-   [pg_stat_slru](#pg_stat_slru)
-   [pg_stat_wal](#pg_stat_wal)
-   [pg_stats](#pg-stats)
-   [pg_stats_ext](#pg_stats_ext)
-   session_level_memory_consumption (See [Monitoring a Greenplum System](../../admin_guide/managing/monitor.html#topic_slt_ddv_1q).)

**Summary Views**

For more information on summary views, see [Summary Views](#summary_views), below.

- gp_stat_all_indexes_summary
- gp_stat_all_tables_summary
- gp_stat_archiver_summary
- gp_stat_bgwriter_summary
- gp_stat_database_summary
- gp_stat_progress_analyze_summary
- gp_stat_progress_basebackup_summary
- gp_stat_progress_cluster_summary
- gp_stat_progress_copy_summary
- gp_stat_progress_create_index_summary
- gp_stat_progress_vacuum_summary
- gp_stat_slru_summary
- gp_stat_sys_indexes_summary
- gp_stat_user_functions_summary
- gp_stat_user_indexes_summary
- gp_stat_wal_summary
- gp_stat_xact_all_tables_summary
- gp_stat_xact_sys_tables_summary
- gp_stat_xact_user_functions_summary
- gp_stat_xact_user_tables_summary
- gp_statio_all_indexes_summary
- gp_statio_all_sequences_summary
- gp_statio_all_tables_summary
- gp_statio_sys_indexes_summary
- gp_statio_sys_sequences_summary
- gp_statio_sys_tables_summary
- gp_statio_user_indexes_summary
- gp_statio_user_sequences_summary
- gp_statio_user_tables_summary

For more information about the standard system views supported in PostgreSQL and Greenplum Database, see the following sections of the PostgreSQL documentation:

-   [System Views](https://www.postgresql.org/docs/12/views-overview.html)
-   [Statistics Collector Views](https://www.postgresql.org/docs/12/monitoring-stats.html#MONITORING-STATS-VIEWS)
-   [The Information Schema](https://www.postgresql.org/docs/12/information-schema.html)

## <a id="gp_backend_memory_contexts"></a>gp_backend_memory_contexts

The `gp_backend_memory_contexts` view is a cluster-wide view that displays the [`pg_backend_memory_contexts`](#pg_backend_memory_contexts) information from every primary segment.

|column|type|references|description|
|------|----|----------|-----------|
|`gp_segment_id`|integer| |Unique identifier of a segment (or coordinator) instance.|
|`name`|text| |The name of the memory context.|
|`ident`|text| |Identification information of the memory context. This field is truncated at 1024 bytes.|
|`parent`|text| |The name of the parent of this memory context.|
|`level`|int4| |The distance from `TopMemoryContext` in context tree.|
|`total_bytes`|int8| |The total number of bytes allocated for this memory context.|
|`total_nblocks`|int8| |The total number of blocks allocated for this memory context.|
|`free_bytes`|int8| |Free space in bytes.|
|`free_chunks`|int8| |The total number of free chunks.|
|`used_bytes`|int8| |Used space in bytes.|

## <a id="gp_config"></a>gp_config

The `gp_config` view is a cluster-wide view that displays the `pg_config`information from every primary segment.

|column|type|references|description|
|------|----|----------|-----------|
|`gp_segment_id`|integer| |Unique identifier of a segment (or coordinator) instance.|
|`name`|text| |The parameter name.|
|`setting`|text| |The parameter value.|

## <a id="gp_cursors"></a>gp_cursors

The `gp_cursors` view is a cluster-wide view that displays the [`pg_config`](#pg_cursors) information from every primary segment.

|name|type|references|description|
|----|----|----------|-----------|
|`gp_segment_id`|integer| |Unique identifier of a segment (or coordinator) instance.|
|`name`|text| |The name of the cursor.|
|`statement`|text| |The verbatim query string submitted to declare this cursor.|
|`is_holdable`|boolean| |`true` if the cursor is holdable \(that is, it can be accessed after the transaction that declared the cursor has committed\); `false` otherwise.<br/><br/> **Note** Greenplum Database does not support holdable parallel retrieve cursors, this value is always `false` for such cursors.|
|`is_binary`|boolean| |`true` if the cursor was declared `BINARY`; `false` otherwise.|
|`is_scrollable`|boolean| |`true` if the cursor is scrollable \(that is, it allows rows to be retrieved in a nonsequential manner\); `false` otherwise.<br/><br/> **Note** Greenplum Database does not support scrollable cursors, this value is always `false`.|
|`creation_time`|timestamptz| |The time at which the cursor was declared.|
|`is_parallel`|boolean| |`true` if the cursor was declared `PARALLEL RETRIEVE`; `false` otherwise.|

## <a id="gp_distributed_log"></a>gp_distributed_log 

The `gp_distributed_log` view contains status information about distributed transactions and their associated local transactions. A distributed transaction is a transaction that involves modifying data on the segment instances. Greenplum's distributed transaction manager ensures that the segments stay in synch. This view allows you to see the status of distributed transactions.

|column|type|references|description|
|------|----|----------|-----------|
|`segment_id`|smallint|gp\_segment\_configuration.content|The content id of the segment. The coordinator is always -1 \(no content\).|
|`dbid`|smallint|gp\_segment\_configuration.dbid|The unique id of the segment instance.|
|`distributed_xid`|xid| |The global transaction id.|
|`distributed_id`|text| |A system assigned ID for a distributed transaction.|
|`status`|text| |The status of the distributed transaction \(Committed or Aborted\).|
|`local_transaction`|xid| |The local transaction ID.|

## <a id="gp_distributed_xacts"></a>gp_distributed_xacts 

The `gp_distributed_xacts` view contains information about Greenplum Database distributed transactions. A distributed transaction is a transaction that involves modifying data on the segment instances. Greenplum's distributed transaction manager ensures that the segments stay in synch. This view allows you to see the currently active sessions and their associated distributed transactions.

|column|type|references|description|
|------|----|----------|-----------|
|`distributed_xid`|xid| |The transaction ID used by the distributed transaction across the Greenplum Database array.|
|`distributed_id`|text| |The distributed transaction identifier. It has 2 parts — a unique timestamp and the distributed transaction number.|
|`state`|text| |The current state of this session with regards to distributed transactions.|
|`gp_session_id`|int| |The ID number of the Greenplum Database session associated with this transaction.|
|`xmin_distributed _snapshot`|xid| |The minimum distributed transaction number found among all open transactions when this transaction was started. It is used for MVCC distributed snapshot purposes.|

## <a id="gp_endpoints"></a>gp_endpoints 

The `gp_endpoints` view lists the endpoints created for all active parallel retrieve cursors declared by the current session user in the current database. When the Greenplum Database superuser accesses this view, it returns a list of all endpoints created for all parallel retrieve cursors declared by all users in the current database.

Endpoints exist only for the duration of the transaction that defines the parallel retrieve cursor, or until the cursor is closed.

|name|type|references|description|
|----|----|----------|-----------|
|gp\_segment\_id|integer| |The QE's endpoint `gp_segment_id`.|
|auth\_token|text| |The authentication token for a retrieve session.|
|cursorname|text| |The name of the parallel retrieve cursor.|
|sessionid|integer| |The identifier of the session in which the parallel retrieve cursor was created.|
|hostname|varchar\(64\)| |The name of the host from which to retrieve the data for the endpoint.|
|port|integer| |The port number from which to retrieve the data for the endpoint.|
|username|text| |The name of the session user \(not the current user\); *you must initiate the retrieve session as this user*.|
|state|text| |The state of the endpoint; the valid states are:<br/><br/>READY: The endpoint is ready to be retrieved.<br/><br/>ATTACHED: The endpoint is attached to a retrieve connection.<br/><br/>RETRIEVING: A retrieve session is retrieving data from the endpoint at this moment.<br/><br/>FINISHED: The endpoint has been fully retrieved.<br/><br/>RELEASED: Due to an error, the endpoint has been released and the connection closed.|
|endpointname|text| |The endpoint identifier; you provide this identifier to the `RETRIEVE` command.|

## <a id="gp_file_settings"></a>gp_file_settings

The `gp_file_settings` view is a cluster-wide view that displays the `pg_file_settings`information from every primary segment.

|name|type|references|description|
|----|----|----------|-----------|
|`gp_segment_id`|integer| |Unique identifier of a segment (or coordinator) instance.|
|`sourcefile`|text| |Full path name of the configuration file.|
|`sourceline`|integer| |Line number within the configuration file where the entry appears.|
|`seqno`|integer| |Order in which the entries are processed (1..n).|
|`name`|text| |Configuration parameter name.|
|`setting`|text| |Value to be assigned to the parameter.|
|`applied`|boolean| |True if the value can be applied successfully.|
|`error`|text| |If not null, an error message indicating why this entry could not be applied.|

## <a id="gp_pgdatabase"></a>gp_pgdatabase

The `gp_pgdatabase` view displays the status of Greenplum segment instances and whether they are acting as the mirror or the primary. The Greenplum fault detection and recovery utilities use this view internally to identify failed segments.

|column|type|references|description|
|------|----|----------|-----------|
|`dbid`|smallint|gp\_segment\_configuration.dbid|System-assigned ID. The unique identifier of a segment \(or coordinator\) instance.|
|`isprimary`|boolean|gp\_segment\_configuration.role|Whether or not this instance is active. Is it currently acting as the primary segment \(as opposed to the mirror\).|
|`content`|smallint|gp\_segment\_configuration.content|The ID for the portion of data on an instance. A primary segment instance and its mirror will have the same content ID.<br/><br/>For a segment the value is from 0-*N-1*, where *N* is the number of segments in Greenplum Database.<br/><br/>For the coordinator, the value is -1.|
|`valid`|boolean|gp\_segment\_configuration.mode|Whether or not this instance is up and the mode is either *s* (synchronized) or *n* (not in sync).|
|`definedprimary`|boolean|gp\_segment\_ configuration.preferred\_role|Whether or not this instance was defined as the primary \(as opposed to the mirror\) at the time the system was initialized.|

## <a id="gp_replication_origin_status"></a>gp_replication_origin_status

The `gp_replication_origin_status` view is a cluster-wide view that displays the `pg_replication_origin_status` information from every primary segment.

name|type|references|description|
|----|----|----------|-----------|
|`gp_segment_id`|integer| |Unique identifier of a segment (or coordinator) instance.|
|`local_id`|oid|pg_replication_origin.roident|Internal node identifier.|
|`external_id`|text|pg_replication_origin.roname|External node identifier.|
|`remote_lsn`|pg_lsn| |The origin node's LSN up to which data has been replicated.|
|`local_lsn`|pg_lsn| |This node's LSN at which remote_lsn has been replicated. Used to flush commit records before persisting data to disk when using asynchronous commits.|

## <a id="gp_replication_slots"></a>gp_replication_slots

The `gp_replication_slots` view is a cluster-wide view that displays the `pg_replication_slots` information from every primary segment.

name|type|references|description|
|----|----|----------|-----------|
|`gp_segment_id`|integer| |Unique identifier of a segment (or coordinator) instance.|
|`slot_name`|name| |A unique, cluster-wide identifier for the replication slot.|
|`plugin`|name| |The base name of the shared object containing the output plugin this logical slot is using, or null for physical slots.|
|`slot_type`|text| |The slot type - physical or logical.|
|`datoid`|oid|pg_database.oid|The OID of the database this slot is associated with, or null. Only logical slots have an associated database.|
|`database`|text|pg_database.datname|The name of the database this slot is associated with, or null. Only logical slots have an associated database.|
|`temporary`|boolean| |True if this is a temporary replication slot. Temporary slots are not saved to disk and are automatically dropped on error or when the session has finished.|
|`active`|boolean| |True if this slot is currently actively being used.|
|`active_pid`|integer| |The process ID of the session using this slot if the slot is currently actively being used. `NULL` if inactive.|
|`xmin`|xid| |The oldest transaction that this slot needs the database to retain. VACUUM cannot remove tuples deleted by any later transaction.|
|`catalog_xmin`|xid| |The oldest transaction affecting the system catalogs that this slot needs the database to retain. VACUUM cannot remove catalog tuples deleted by any later transaction.|
|`restart_ls`n|pg_lsn| |The address (LSN) of oldest WAL which still might be required by the consumer of this slot and thus won't be automatically removed during checkpoints. `NULL` if the LSN of this slot has never been reserved.|
|`confirmed_flush_lsn`|pg_lsn| |The address (LSN) up to which the logical slot's consumer has confirmed receiving data. Data older than this is not available anymore. NULL for physical slots.|

## <a id="gp_resgroup_config"></a>gp_resgroup_config

The `gp_toolkit.gp_resgroup_config` view allows administrators to see the current CPU, memory, and concurrency limits for a resource group.

> **Note** The `gp_resgroup_config` view is valid only when resource group-based resource management is active.

|column|type|references|description|
|------|----|----------|-----------|
|`groupid`|oid|pg\_resgroup.oid|The ID of the resource group.|
|`groupname`|name|pg\_resgroup.rsgname|The name of the resource group.|
|`concurrency`|text|pg\_resgroupcapability.value for pg\_resgroupcapability.reslimittype = 1|The concurrency \(`CONCURRENCY`\) value specified for the resource group.|
|`cpu_max_percent`|text|pg\_resgroupcapability.value for pg\_resgroupcapability.reslimittype = 2|The CPU limit \(`CPU_MAX_PERCENT`\) value specified for the resource group, or -1.|
|`cpu_weight`|text|pg\_resgroupcapability.value for pg\_resgroupcapability.reslimittype = 3|The scheduling priority of the resource group (CPU_WEIGHT).|
|`cpuset`|text|pg\_resgroupcapability.value for pg\_resgroupcapability.reslimittype = 4|The CPU cores reserved for the resource group (CPUSET), or -1.|
|`memory_limit`|text|pg\_resgroupcapability.value for pg\_resgroupcapability.reslimittype = 5|The memory limit \(`MEMORY_LIMIT`\) value specified for the resource group.|
|`min_cost`|text|pg\_resgroupcapability.value for pg\_resgroupcapability.reslimittype = 6|The minimum cost of a query plan to be included in the resource group (MIN_COST).|
|`io_limit`|text|pg\_resgroupcapability.value for pg\_resgroupcapability.reslimittype = 7|The maximum read/write sequential disk I/O throughput, and the maximum read/write I/O operations per second for the queries assigned to a specific tablespace (shown as the tablespace oid) and resource group (IO_LIMIT).|

## <a id="gp_resgroup_iostats_per_host"></a>gp_resgroup_iostats_per_host

The `gp_toolkit.gp_resgroup_iostats_per_host` view allows administrators to see current disk I/O  usage for each resource group on a per-host basis.

Memory amounts are specified in MBs.

> **Note** The `gp_resgroup_iostats_per_host` view is valid only when resource group-based resource management is active.

|column|type|references|description|
|------|----|----------|-----------|
|`rsgname`|name| pg_resgroup.rsgname|The name of the resource group|
|`hostname`|text|gp_segment_configuration.hostname|The hostname of the segment host|
|`tablespace`|name|pg_tablespace.spcname|The name of the tablespace|
|`rbps`|bigint||The real-time read sequential disk I/O throughput by the resource group on a host, in Bytes/s|
|`wbps`|bigint||The real-time write sequential disk I/O throughput by the resource group on a host, in Bytes/s|
|`riops`|bigint||The real-time read I/O operations per second by the resource group on a host|
|`wiops`|bigint||The real-time write I/O operations per second by the resource group on a host|

Sample output for the `gp_resgroup_iostats_per_host` view:

```
SELECT * from gp_toolkit.gp_resgroup_iostats_per_host;
 rsgname        | hostname | tablespace       | rbps | wbps | riops | wiops  
----------------+----------+------------------+------------------+------------------+-------------+-------------
 rg_test_group1 | mtspc    | pg_default       | 21356347                | 29369067                | 162           | 36           
 rg_test_group2 | mtspc    | pg_default       | 0                | 0                | 0           | 0           
 rg_test_group3 | mtspc    | pg_default       | 0                | 0                | 0           | 0           
 rg_test_group4 | mtspc    | *                | 0                | 0                | 0           | 0           
 rg_test_group5 | mtspc    | rg_io_limit_ts_1 | 0                | 0                | 0           | 0           
(5 rows)
```

## <a id="gp_resgroup_iostats_per_host"></a>gp_resgroup_iostats_per_host

The `gp_toolkit.gp_resgroup_iostats_per_host` view allows administrators to see current disk I/O  usage for each resource group on a per-host basis.

Memory amounts are specified in MBs.

> **Note** The `gp_resgroup_iostats_per_host` view is valid only when resource group-based resource management is active.

|column|type|references|description|
|------|----|----------|-----------|
|`rsgname`|name| pg_resgroup.rsgname|The name of the resource group.|
|`hostname`|text|gp_segment_configuration.hostname|The hostname of the segment host.|
|`tablespace`|
|`rbps`|
|`



|`cpu_usage`|float| |The real-time CPU core usage by the resource group on a host. The value is the sum of the percentages of the CPU cores that are used by the resource group on the host.|
|`memory_usage`|float| |The real-time memory usage of the resource group on each Greenplum Database segment's host, in MB.|

Sample output for the `gp_resgroup_iostats_per_host` view:

```
select * from gp_toolkit.gp_resgroup_status_per_host;
 rsgname       | hostname | tablespace | rbps (MB_read/s)
---------------+----------+-----------+--------------
 admin_group   | zero     | pg_default | 80
 default_group | zero     | pg_default | 500
 system_group  | zero     | pg_default | 300
 rg_new_group  | zero     | 
(4 rows)
```

## <a id="gp_resgroup_status"></a>gp_resgroup_status

The `gp_resgroup_status` view allows administrators to see status and activity for a resource group. It shows how many queries are waiting to run and how many queries are currently active in the system for each resource group. The view also displays current memory and CPU usage for the resource group.

> **Note** Resource groups use the Linux control groups \(cgroups\) configured on the host systems. The cgroups are used to manage host system resources. When resource groups use cgroups that are as part of a nested set of cgroups, resource group limits are relative to the parent cgroup allotment. For information about nested cgroups and Greenplum Database resource group limits, see [Using Resource Groups](../../admin_guide/workload_mgmt_resgroups.html#topic8339intro).

This view is accessible to all users.

|column|type|references|description|
|------|----|----------|-----------|
|rsgname|name| pg_resgroup.rsgname|The name of the resource group.|
|groupid|oid|pg_resgroup.oid|The ID of the resource group.|
|num\_running|integer| |The number of transactions currently running in the resource group.|
|num\_queueing|integer| |The number of currently queued transactions for the resource group.|
|num\_queued|integer| |The total number of queued transactions for the resource group since the Greenplum Database cluster was last started, excluding the num\_queueing.|
|num\_executed|integer| |The total number of transactions run in the resource group since the Greenplum Database cluster was last started, excluding the num\_running.|
|total\_queue\_duration|interval| |The total time any transaction was queued since the Greenplum Database cluster was last started.|

Sample output for the `gp_resgroup_status` view:

```
select * from gp_toolkit.gp_resgroup_status;
 rsgname       | groupid | num_running | num_queueing | num_queued | num_executed | total_queue_duration |
---------------+---------+-------------+--------------+------------+-------------------------------------
 default_group | 6437    | 0           | 0            | 0          | 0            | @ 0                  |
 admin_group   | 6438    | 1           | 0            | 0          | 13           | @ 0                  |
 system_group  | 6441    | 0           | 0            | 0          | 0            | @ 0                  |
(3 rows)
```

## <a id="gp_resgroup_status_per_host"></a>gp_resgroup_status_per_host

The `gp_toolkit.gp_resgroup_status_per_host` view allows administrators to see current memory and CPU usage and allocation for each resource group on a per-host basis.

Memory amounts are specified in MBs.

> **Note** The `gp_resgroup_status_per_host` view is valid only when resource group-based resource management is active.

|column|type|references|description|
|------|----|----------|-----------|
|rsgname|name| pg_resgroup.rsgname|The name of the resource group.|
|groupid|oid|pg_resgroup.oid|The ID of the resource group.|
|`hostname`|text|gp_segment_configuration.hostname|The hostname of the segment host.|
|`cpu_usage`|float| |The real-time CPU core usage by the resource group on a host. The value is the sum of the percentages of the CPU cores that are used by the resource group on the host.|
|`memory_usage`|float| |The real-time memory usage of the resource group on each Greenplum Database segment's host, in MB.|

Sample output for the `gp_resgroup_status_per_host` view:

```
select * from gp_toolkit.gp_resgroup_status_per_host;
 rsgname       | groupid | hostname | cpu_usage | memory_usage
---------------+---------+----------+-----------+--------------
 admin_group   | 6438    | zero     | 0.07      | 91.92
 default_group | 6437    | zero     | 0.00      | 0.00
 system_group  | 6441    | zero     | 0.02      | 53.04
(3 rows)
```

## <a id="gp_resgroup_status_per_segment"></a>gp_resgroup_status_per_segment

The `gp_toolkit.gp_resgroup_status_per_segment` view allows administrators to see current memory usage usage calculated by vmem tracker and grouped by segment.

Memory amounts are specified in MBs.

> **Note** The `gp_resgroup_status_per_segment` view is valid only when resource group-based resource management is active.

|column|type|references|description|
|------|----|----------|-----------|
|`groupid`|oid|pg\_resgroup.oid|The ID of the resource group.|
|`groupname`|name|pg\_resgroup.rsgname|The name of the resource group.|
|`segment_id`|smallint|gp\_segment\_configuration.content|The content ID for a segment instance on the segment host.|
|`vmem_usage`|The real-time memory usage of the resource group on each segment, in MB.|

## <a id="gp_resqueue_status"></a>gp_resqueue_status

The `gp_toolkit.gp_resqueue_status` view allows administrators to see status and activity for a resource queue. It shows how many queries are waiting to run and how many queries are currently active in the system from a particular resource queue.

> **Note** The `gp_resqueue_status` view is valid only when resource queue-based resource management is active.

|column|type|references|description|
|------|----|----------|-----------|
|`queueid`|oid|gp\_toolkit.gp\_resqueue\_ queueid|The ID of the resource queue.|
|`rsqname`|name|gp\_toolkit.gp\_resqueue\_ rsqname|The name of the resource queue.|
|`rsqcountlimit`|real|gp\_toolkit.gp\_resqueue\_ rsqcountlimit|The active query threshold of the resource queue. A value of -1 means no limit.|
|`rsqcountvalue`|real|gp\_toolkit.gp\_resqueue\_ rsqcountvalue|The number of active query slots currently being used in the resource queue.|
|`rsqcostlimit`|real|gp\_toolkit.gp\_resqueue\_ rsqcostlimit|The query cost threshold of the resource queue. A value of -1 means no limit.|
|`rsqcostvalue`|real|gp\_toolkit.gp\_resqueue\_ rsqcostvalue|The total cost of all statements currently in the resource queue.|
|`rsqmemorylimit`|real|gp\_toolkit.gp\_resqueue\_ rsqmemorylimit|The memory limit for the resource queue.|
|`rsqmemoryvalue`|real|gp\_toolkit.gp\_resqueue\_ rsqmemoryvalue|The total memory used by all statements currently in the resource queue.|
|`rsqwaiters`|integer|gp\_toolkit.gp\_resqueue\_ rsqwaiter|The number of statements currently waiting in the resource queue.|
|`rsqholders`|integer|gp\_toolkit.gp\_resqueue\_ rsqholders|The number of statements currently running on the system from this resource queue.|

## <a id="gp_segment_endpoints"></a>gp_segment_endpoints

The `gp_segment_endpoints` view lists the endpoints created in the QE for all active parallel retrieve cursors declared by the current session user. When the Greenplum Database superuser accesses this view, it returns a list of all endpoints on the QE created for all parallel retrieve cursors declared by all users.

Endpoints exist only for the duration of the transaction that defines the parallel retrieve cursor, or until the cursor is closed.

|column|type|references|description|
|----|----|----------|-----------|
|auth\_token|text| |The authentication token for the retrieve session.|
|databaseid|oid| |The identifier of the database in which the parallel retrieve cursor was created.|
|senderpid|integer| |The identifier of the process sending the query results.|
|receiverpid|integer| |The process identifier of the retrieve session that is receiving the query results.|
|state|text| |The state of the endpoint; the valid states are:<br/><br/>READY: The endpoint is ready to be retrieved.<br/><br/>ATTACHED: The endpoint is attached to a retrieve connection.<br/><br/>RETRIEVING: A retrieve session is retrieving data from the endpoint at this moment.<br/><br/>FINISHED: The endpoint has been fully retrieved.<br/><br/>RELEASED: Due to an error, the endpoint has been released and the connection closed.|
|gp\_segment\_id|integer| |The QE's endpoint `gp_segment_id`.|
|sessionid|integer| |The identifier of the session in which the parallel retrieve cursor was created.|
|username|text| |The name of the session user \(not the current user\); *you must initiate the retrieve session as this user*.|
|endpointname|text| |The endpoint identifier; you provide this identifier to the `RETRIEVE` command.|
|cursorname|text| |The name of the parallel retrieve cursor.|

## <a id="gp_session_endpoints"></a>gp_session_endpoints

The `gp_session_endpoints` view lists the endpoints created for all active parallel retrieve cursors declared by the current session user in the current session.

Endpoints exist only for the duration of the transaction that defines the parallel retrieve cursor, or until the cursor is closed.

|column|type|references|description|
|----|----|----------|-----------|
|gp\_segment\_id|integer| |The QE's endpoint `gp_segment_id`.|
|auth\_token|text| |The authentication token for a retrieve session.|
|cursorname|text| |The name of the parallel retrieve cursor.|
|sessionid|integer| |The identifier of the session in which the parallel retrieve cursor was created.|
|hostname|varchar\(64\)| |The name of the host from which to retrieve the data for the endpoint.|
|port|integer| |The port number from which to retrieve the data for the endpoint.|
|username|text| |The name of the session user \(not the current user\); *you must initiate the retrieve session as this user*.|
|state|text| |The state of the endpoint; the valid states are:<br/><br/>READY: The endpoint is ready to be retrieved.<br/><br/>ATTACHED: The endpoint is attached to a retrieve connection.<br/><br/>RETRIEVING: A retrieve session is retrieving data from the endpoint at this moment.<br/><br/>FINISHED: The endpoint has been fully retrieved.<br/><br/>RELEASED: Due to an error, the endpoint has been released and the connection closed.|
|endpointname|text| |The endpoint identifier; you provide this identifier to the `RETRIEVE` command.|

## <a id="gp_settings"></a>gp_settings

The `gp_settings` view is a cluster-wide view that displays the `pg_settings` information from every primary segment.

|name|type|references|description|
|----|----|----------|-----------|
|`gp_segment_id`|integer| |Unique identifier of a segment (or coordinator) instance.|
|`name`|text| |Runtime configuration parameter name.
|`setting`|text| |Current value of the parameter.|
|`unit`|text| |Implicit unit of the parameter.|
|`category`|text| |Logical group of the parameter.|
|`short_desc`|text| |A brief description of the parameter.
|`extra_desc`|text| |Additional, more detailed, description of the parameter.|
|`context`|text| |Context required to set the parameter's value.|
|`vartype`|text| |Parameter type (bool, enum, integer, real, or string)v
|`source`|text| |Source of the current parameter value.v
|`min_val`|text| |Minimum allowed value of the parameter (null for non-numeric values).|
|`max_val`|text| |Maximum allowed value of the parameter (null for non-numeric values).|
|`enumvals`|text[]| |Permitted values of an enum parameter (null for non-enum values).|
|`boot_val`|text| |Parameter value assumed at server startup if the parameter is not otherwise set.|
|`reset_val`|text| |Value that RESET would reset the parameter to in the current session.|
|`sourcefile`|text| |Configuration file the current value was set in (null for values set from sources other than configuration files, or when examined by a user who is neither a superuser or a member of `pg_read_all_settings`); helpful when using include directives in configuration files.|
|`sourceline`|integer| |Line number within the configuration file the current value was set at (null for values set from sources other than configuration files, or when examined by a user who is neither a superuser or a member of pg_read_all_settings).|
|`pending_restart`|boolean| |`true` if the value has been changed in the configuration file but needs a restart; otherwise `false`.|

## <a id="gp_suboverflowed_backend"></a>gp_suboverflowed_backend

The `gp_suboverflowed_backend` view allows administrators to identify sessions in which a backend has subtransaction overflows, which can cause query performance degradation in the system, including catalog queries.

|column|type|description|
|------|----|----------|
|`segid`|integer|The id of the segment containing the suboverflowed backend.|
|`pids`|integer[]|A list of the pids of all suboverflowed backends on this segment.|

For more information on handling suboverflowed backends to prevent performance issues, see [Checking for and Terminating Overflowed Backends](../../admin_guide/managing/monitor.html#overflowed_backends).

## <a id="gp_transaction_log"></a>gp_transaction_log

The `gp_transaction_log` view contains status information about transactions local to a particular segment. This view allows you to see the status of local transactions.

|column|type|references|description|
|------|----|----------|-----------|
|`segment_id`|smallint|gp\_segment\_configuration.content|The content id of the segment. The coordinator is always -1 \(no content\).|
|`dbid`|smallint|gp\_segment\_configuration.dbid|The unique id of the segment instance.|
|`transaction`|xid| |The local transaction ID.|
|`status`|text| |The status of the local transaction \(Committed or Aborted\).|

## <a id="gpexpandexpansion_progress"></a>gpexpand.expansion_progress

The `gpexpand.expansion_progress` view contains information about the status of a system expansion operation. The view provides calculations of the estimated rate of table redistribution and estimated time to completion.

Status for specific tables involved in the expansion is stored in [gpexpand.status\_detail](gp_expansion_tables.html).

|column|type|references|description|
|------|----|----------|-----------|
|`name`|text| |Name for the data field provided. Includes:<br/><br/>Bytes Left<br/><br/>Bytes Done<br/><br/>Estimated Expansion Rate<br/><br/>Estimated Time to Completion<br/><br/>Tables Expanded<br/><br/>Tables Left|
|`value`|text| |The value for the progress data. For example: `Estimated Expansion Rate - 9.75667095996092 MB/s`|

## <a id="gp_stat_activity"></a>gp_stat_activity

The `gp_stat_activity` view is a cluster-wide view that displays the [`pg_stat_activity` ](#pg_stat_activity) information from every primary segment. 

|column|type|references|description|
|------|----|----------|-----------|
|`gp_segment_id`|integer| |Unique identifier of a segment (or coordinator) instance.|
|`datid`|oid|pg\_database.oid|Database OID|
|`datname`|name| |Database name|
|`pid`|integer| |Process ID of this backend|
|`sess_id`|integer| |Session ID|
|`usesysid`|oid|pg\_authid.oid|OID of the user logged into this backend|
|`usename`|name| |Name of the user logged into this backend|
|`application_name`|text| |Name of the application that is connected to this backend|
|`client_addr`|inet| |IP address of the client connected to this backend. If this field is null, it indicates either that the client is connected via a Unix socket on the server machine or that this is an internal process such as autovacuum.|
|`client_hostname`|text| |Host name of the connected client, as reported by a reverse DNS lookup of `client_addr`. This field will only be non-null for IP connections, and only when log\_hostname is enabled.|
|`client_port`|integer| |TCP port number that the client is using for communication with this backend, or -1 if a Unix socket is used|
|`backend_start`|timestamptz| |Time backend process was started|
|`xact_start`|timestamptz| |Transaction start time|
|`query_start`|timestamptz| |Time query began execution|
|`state_change`|timestampz| |Time when the `state` was last changed|
|`wait_event_type`|text| |Type of event for which the backend is waiting|
|`wait_event`|text| |Wait event name if backend is currently waiting|
|`state`|text| |Current overall state of this backend. Possible values are:<br/><br/>-   `active`: The backend is running a query.<br/><br/>-   `idle`: The backend is waiting for a new client command.<br/><br/>-   `idle in transaction`: The backend is in a transaction, but is not currently running a query.<br/><br/>-   `idle in transaction (aborted)`: This state is similar to idle in transaction, except one of the statements in the transaction caused an error.<br/><br/>-   `fastpath function call`: The backend is running a fast-path function.<br/><br/>-   `disabled`: This state is reported if `track_activities` is deactivated in this backend.|
|`query`|text| |Text of this backend's most recent query. If `state` is active this field shows the currently running query. In all other states, it shows the last query that was run.|
|`rsgid`|oid|pg\_resgroup.oid|Resource group OID or `0`.<br/><br/>See [Note](#rsg_note).|
|`rsgname`|text|pg\_resgroup.rsgname|Resource group name or `unknown`.<br/><br/>See [Note](#rsg_note).|

## <a id="gp_stat_all_indexes"></a>gp_stat_all_indexes

The `gp_stat_all_indexes` view is a cluster-wide view that displays the [`pg_stat_indexes`](#pg_stat_all_indexes) information from every primary segment. 

|column|type|references|description|
|------|----|----------|-----------|
|`gp_segment_id`|integer| |Unique identifier of a segment (or coordinator) instance.|
|`relid`|oid| |ID of the table for this index.|
|`indexrelid`|oid| |OID of this index.|
|`schemaname`|name| |Name of the schema this index is in.|
|`relname`|name| |Name of the table for this index.|
|`indexrelname`|name| |Name for this index.|
|`idx_scan`|bigint| |Number of index scans initiated on this index.|
|`idx_tup_read`|bigint| |Number of index entries returned by scans on this index.|
|`idx_tup_fetch`|bigint| |Number of live table rows fetched by simple index scans using this index|

This system view is summarized in the `gp_stat_all_indexes_summary` system view.

## <a id="gp_stat_all_tables"></a>gp_stat_all_tables

The `gp_stat_all_tables` view is a cluster-wide view that displays the [`pg_stat_tables`](#pg_stat_all_tables) information from every primary segment. 

|column|type|references|description|
|------|----|----------|-----------|
|`gp_segment_id`|integer| |Unique identifier of a segment (or coordinator) instance.|
|`relid`|oid| |OID of a table.|
|`schemaname`|name| |Name of the schema this table is on.|
|`relname`|name| |Name of this table.|
|`seq_scan`|bigint| |Number of sequential scans initiated on this table.|
|`seq_tup_read`|bigint| |Number of live rows fetched by sequential scans.|
|`idx_scan`|bigint| |Number of index scans initiated on this table.|
|`idx_tup_fetch`|bigint| |Number of live rows fetched by index scans.|
|`n_tup_ins`|bigint| |Number of rows inserted.|
|`n_tup_upd`|bigint| |Number of rows updated (includes HOT updated rows).|
|`n_tup_del`|bigint| |Number of rows deleted.|
|`n_tup_hot_upd`|bigint| |Number of rows HOT updated (that is, with no separate index update required).|
|`n_live_tup`|bigint| |Estimated number of live rows.|
|`n_dead_tup`|bigint| |Estimated number of dead rows.|
|`n_mod_since_analyze`|bigint| |Estimated number of rows modified since this table was last analyzed.|
|`n_ins_since_vacuum`|bigint| |Estimated number of rows inserted since this table was last vacuumed.|
|`last_vacuum`|timestamp with time zone| |Last time at which this table was manually vacuumed (not counting `VACUUM FULL`).|
|`last_autovacuum`|timestamp with time zone| |Last time at which this table was vacuumed by the autovacuum daemon.|
|`last_analyze`|timestamp with time zone| |Last time at which this table was manually analyzed.|
|`last_autoanalyze`|timestamp with time zone| |Last time at which this table was analyzed by the autovacuum daemon.|
|`vacuum_count`|bigint| |Number of times this table has been manually vacuumed (not counting `VACUUM FULL`).|
|`autovacuum_count`|bigint| |Number of times this table has been vacuumed by the autovacuum daemon.|
|`analyze_count`|bigint| |Number of times this table has been manually analyzed.|
|`autoanalyze_count`|bigint| |Number of times this table has been analyzed by the autovacuum daemon.|

This system view is summarized in the `gp_stat_all_tables_summary` system view.

## <a id="gp_stat_archiver"></a>gp_stat_archiver

|column|type|references|description|
|------|----|----------|-----------|
|`gp_segment_id`|integer| |Unique identifier of a segment (or coordinator) instance.|
|`archived_count`|bigint| |Number of WAL files that have been successfully archived.|
|`last_archived_wal`|text| |Name of the WAL file most recently successfully archived.|
|`last_archived_time `|timestamp with time zone| |Time of the most recent successful archive operation.|
|`failed_count`|bigint| |Number of failed attempts for archiving WAL files.|
|`last_failed_wal`|timestamp with time zone| |Name of the WAL file of the most recent failed archival operation.|
|`last_failed_time`|bigint| |Time of the most recent failed archival operation.|
|`stats_reset`|timestamp with time zone| |Time at which these statistics were last reset.|

This system view is summarized in the `gp_stat_archiver_summary` system view.

## <a id="gp_stat_bgwriter"></a>gp_stat_bgwriter

|column|type|references|description|
|------|----|----------|-----------|
|`gp_segment_id`|integer| |Unique identifier of a segment (or coordinator) instance.|
|`checkpoints_timed`|bigint| |Number of scheduled checkpoints that have been performed.|
|`checkpoints_req`|bigint| |Number of requested checkpoints that have been performed.|
|`checkpoint_write_time`|double precision| |Total amount of time that has been spent in the portion of checkpoint processing where files are written to disk, in milliseconds.|
|`checkpoint_sync_time`|double precision| |Total amount of time that has been spent in the portion of checkpoint processing where files are synchronized to disk, in milliseconds.|
|`buffers_checkpoint`|bigint| |Number of buffers written during checkpoints.|
|`buffers_clean`|bigint| |Number of buffers written by the background writer.|
|`maxwritten_clean` bigint| |Number of times the background writer stopped a cleaning scan because it had written too many buffers.|
|`buffers_backend` bigint| |Number of buffers written directly by a backend.|
|`buffers_backend_fsync` bigint| |Number of times the background writer stopped a cleaning scan because it had written too many buffers.|
|`buffers_alloc`|bigint| |Number of buffers allocated.|
|`stats_reset`|timestamp with time zone| |Time at which these statistics were last reset.|

This system view is summarized in the `gp_stat_bgwriter_summary` system view.

## <a id="gp_stat_database"></a>gp_stat_database

|column|type|references|description|
|------|----|----------|-----------|
|`gp_segment_id`|integer| |Unique identifier of a segment (or coordinator) instance.|
|`datid`|oid| |OID of this database, or 0 for objects belonging to a shared relation.|
|`datname`|name| |Name of this database, or NULL for shared objects.|
|`numbackends`|integer| |Number of backends currently connected to this database, or NULL for shared objects. This is the only column in this view that returns a value reflecting current state; all other columns return the accumulated values since the last reset.|
|`xact_commit`|bigint| |Number of transactions in this database that have been committed.|
|`xact_rollback`|bigint| |Number of transactions in this database that have been rolled back.|
|`blks_read`|bigint| |Number of disk blocks read in this database.|
|`blks_hit`|bigint| |Number of times disk blocks were found already in the buffer cache, so that a read was not necessary (this only includes hits in the PostgreSQL buffer cache, not the operating system's file system cache).|
|`tup_returned`|bigint| |Number of live rows fetched by sequential scans and index entries returned by index scans in this database.|
|`tup_fetched `|bigint| |Number of live rows fetched by index scans in this database.|
|`tup_inserted`|bigint| |Number of rows inserted by queries in this database.|
|`tup_updated`|bigint| |Number of rows updated by queries in this database.|
|`tup_deleted bigint`|bigint| |Number of rows deleted by queries in this database.|
|`conflicts`|bigint| |Number of queries canceled due to conflicts with recovery in this database.|
|`temp_files`|bigint| |Number of temporary files created by queries in this database. All temporary files are counted, regardless of why the temporary file was created (for example, sorting or hashing), and regardless of the `log_temp_files` setting.|
|`temp_bytes`|bigint| |Total amount of data written to temporary files by queries in this database. All temporary files are counted, regardless of why the temporary file was created, and regardless of the log_temp_files setting.|
|`deadlocks`|bigint| |Number of deadlocks detected in this database.|
|`checksum_failures`|bigint| |Number of data page checksum failures detected in this database (or on a shared object), or NULL if data checksums are not enabled.|
|`checksum_last_failure`|timestamp with time zone| |Time at which the last data page checksum failure was detected in this database (or on a shared object), or NULL if data checksums are not enabled.|
|`blk_read_time`|double precision| |Time spent reading data file blocks by backends in this database, in milliseconds (if track_io_timing is enabled, otherwise zero).|
|`session_time`|double precision| |Time spent by database sessions in this database, in milliseconds (note that statistics are only updated when the state of a session changes, so if sessions have been idle for a long time, this idle time won't be included).|
|`active_time`|double precision| |Time spent executing SQL statements in this database, in milliseconds (this corresponds to the states active and fastpath function call in pg_stat_activity).|
|`idle_in_transaction_time`|double precision| |Time spent idling while in a transaction in this database, in milliseconds (this corresponds to the states idle in transaction and idle in transaction (aborted) in `gp_stat_activity`)|
|`sessions`|bigint| |Total number of sessions established to this database.|
|`sessions_abandoned`|bigint| |Number of database sessions to this database that were terminated because connection to the client was lost.|
|`sessions_fatal`|bigint| |Number of database sessions to this database that were terminated by fatal errors.|
|`sessions_killed`|bigint| |Number of database sessions to this database that were terminated by operator intervention.|
|`stats_reset`|timestamp with time zone| |Time at which these statistics were last reset.|

This system view is summarized in the `gp_stat_database_summary` system view.

## <a id="gp_stat_database_conflicts"></a>gp_stat_database_conflicts

|column|type|references|description|
|------|----|----------|-----------|
|`gp_segment_id`|integer| |Unique identifier of a segment (or coordinator) instance.|
|`datid`|oid| |OID of a database.|
|`datname`|name| |Name of this database.|
|`confl_tablespace`|bigint| |Number of queries in this database that have been canceled due to dropped tablespaces.|
|`confl_lock`|bigint| |Number of queries in this database that have been canceled due to lock timeouts.|
|`confl_snapshot`|bigint| |Number of queries in this database that have been canceled due to old snapshots.|
|`client_port`|integer| |Client port number.|
|`confl_bufferpin`|bigint| |Number of queries in this database that have been canceled due to pinned buffers.|
|`confl_deadlock`|bigint| |Number of queries in this database that have been canceled due to deadlocks.|

## <a id="gp_stat_gssapi"></a>gp_stat_gssapi

|column|type|references|description|
|------|----|----------|-----------|
|`gp_segment_id`|integer| |Unique identifier of a segment (or coordinator) instance.|
|`pid`|integer| |Process ID of a backend.|
|`gss_authenticated boolean`|boolean| |True if GSSAPI authentication was used for this connection.|
|`principal`|text| |Principal used to authenticate this connection, or NULL if GSSAPI was not used to authenticate this connection. This field is truncated if the principal is longer than `NAMEDATALEN` (64 characters in a standard build).|
|`encrypted`|boolean| |True if GSSAPI encryption is in use on this connection.|

## <a id="gp_stat_operations"></a>gp_stat_operations

The view `gp_stat_operations` shows details about the last operation performed on a database object \(such as a table, index, view or database\) or a global object \(such as a role\).

|column|type|references|description|
|------|----|----------|-----------|
|`gp_segment_id`|integer| |Unique identifier of a segment (or coordinator) instance.|
|`classname`|text| |The name of the system table in the `pg_catalog` schema where the record about this object is stored \(`pg_class`=relations, `pg_database`=databases,`pg_namespace`=schemas, `pg_authid`=roles\)|
|`objname`|name| |The name of the object.|
|`objid`|oid| |The OID of the object.|
|`schemaname`|name| |The name of the schema where the object resides.|
|`usestatus`|text| |The status of the role who performed the last operation on the object \(`CURRENT`=a currently active role in the system, `DROPPED`=a role that no longer exists in the system, `CHANGED`=a role name that exists in the system, but has changed since the last operation was performed\).|
|`usename`|name| |The name of the role that performed the operation on this object.|
|`actionname`|name| |The action that was taken on the object.|
|`subtype`|text| |The type of object operated on or the subclass of operation performed.|
|`statime`|timestamptz| |The timestamp of the operation. This is the same timestamp that is written to the Greenplum Database server log files in case you need to look up more detailed information about the operation in the logs.|

### <a id="gp_stat_progress_analyze"></a>gp_stat_progress_analyze

The `gp_stat_progress_analyze` view is a cluster-wide view that displays the [pg_stat_progress_analyze](https://www.postgresql.org/docs/15/progress-reporting.html#ANALYZE-PROGRESS-REPORTING) information from every primary segment for all currently-running `ANALYZE` operations.

The `gp_stat_progress_analyze_summary` view aggregates across the Greenplum Database cluster the metrics reported by `gp_stat_progress_analyze`.

|Column|Type|Description|
|------|----|-----------|
|`gp_segment_id`|integer| Unique identifier of a segment \(or coordinator\) instance. (This column is not present in the `gp_stat_progress_analyze_summary` view.) |
| `pid` | integer | The process identifier of the backend, or the coordinator process identifier if the `gp_stat_progress_analyze_summary` view. |
| `datid` | oid | The object identifier of the database to which this backend is connected. |
| `datname` | name | Name of the database to which this backend is connected. |
| `relid` | oid | The object identifier of the table being analyzed. |
| `phase` | text | Current processing phase. Refer to [ANALYZE Progress Reporting](../../admin_guide/managing/progress_reporting.html#analyze_progress) for detailed information about the phases. |
| `sample_blks_total` | bigint | Total number of heap blocks that will be sampled. |
| `sample_blks_scanned` | bigint | Number of heap blocks scanned. |
| `ext_stats_total` | bigint | Number of extended statistics. |
| `ext_stats_computed` | bigint | Number of extended statistics computed. This counter only advances when the phase is computing extended statistics. |
| `child_tables_total` | bigint | Number of child tables. |
| `child_tables_done` | bigint | Number of child tables scanned. This counter only advances when the phase is acquiring inherited sample rows. |
| `current_child_table_relid` | oid | The object identifier of the child table currently being scanned. This field is only valid when the phase is acquiring inherited sample rows. (This column is not present in the for `gp_stat_progress_analyze_summary` view.)|

### <a id="gp_stat_progress_basebackup"></a>gp_stat_progress_basebackup

The `gp_stat_progress_basebackup` view is a cluster-wide view that displays the [pg_stat_progress_basebackup](https://www.postgresql.org/docs/15/progress-reporting.html#BASEBACKUP-PROGRESS-REPORTING) information from every primary segment for all currently-running base backup operations (`gprecoverseg`).

The `gp_stat_progress_basebackup_summary` view aggregates across the Greenplum Database cluster the metrics reported by `gp_stat_progress_basebackup`.

|Column|Type|Description|
|------|----|-----------|
|`gp_segment_id`|integer| Unique identifier of a segment \(or coordinator\) instance. (This column is not present in the `gp_stat_progress_basebackup_summary` view.)|
| `pid` | integer | The process identifier of a WAL sender process, or the coordinator process identifier if the `gp_stat_progress_basebackup_summary` view. |
| `phase` | text | Current processing phase. Refer to [Base Backup Progress Reporting](../../admin_guide/managing/progress_reporting.html#basebackup_progress) for detailed information about the phases. |
| `backup_total` | bigint | Total amount of data that will be streamed. This is estimated and reported as of the beginning of streaming database files phase. Note that this is only an approximation since the database may change during streaming database files phase and WAL log may be included in the backup later. This is always the same value as backup_streamed once the amount of data streamed exceeds the estimated total size. NULL if the estimation is disabled in `pg_basebackup`. |
| `backup_streamed` | bigint | Amount of data streamed. This counter only advances when the phase is streaming database files or transferring wal files. |
| `tablespaces_total` | bigint | Total number of tablespaces that will be streamed. |
| `tablespaces_streamed` | bigint | Number of tablespaces streamed. This counter only advances when the phase is streaming database files. |

### <a id="gp_stat_progress_cluster"></a>gp_stat_progress_cluster

The `gp_stat_progress_cluster` view is a cluster-wide view that displays the [pg_stat_progress_cluster](https://www.postgresql.org/docs/15/progress-reporting.html#CLUSTER-PROGRESS-REPORTING) information from every primary segment for all currently-running `CLUSTER` and `VACUUM FULL` (on a heap table) operations.

The `gp_stat_progress_cluster_summary` view aggregates across the Greenplum Database cluster the metrics reported by `gp_stat_progress_cluster`.

|Column|Type|Description|
|------|----|-----------|
|`gp_segment_id`|integer| Unique identifier of a segment \(or coordinator\) instance. (This column is not present in the `gp_stat_progress_cluster_summary` view.)|
| `pid` | integer | Process identifier of the backend, or the coordinator process identifier if the `gp_stat_progress_cluster_summary` view. |
| `datid` | oid | The object identifier of the database to which this backend is connected. |
| `datname` | name | Name of the database to which this backend is connected. |
| `relid` | `oid` | The object identifier of the table being clustered. |
| `command` | text | The name of the command that is running. Either `CLUSTER` or `VACUUM FULL`. |
| `phase` | text | Current processing phase. Refer to [CLUSTER and VACUUM FULL Progress Reporting](../../admin_guide/managing/progress_reporting.html#cluster_progress) for detailed information about the phases. |
| `cluster_index_relid` | oid | If the table is being scanned using an index, this is the object identifier of the index being used; otherwise, it is zero. This field is not applicable to AO/CO tables. |
| `heap_tuples_scanned` | bigint | For heap tables, `heap_tuples_scanned` records the number of tuples scanned, including both live and dead tuples. For AO tables, `heap_tuples_scanned` records the number of live tuples scanned, excluding the dead tuples. This counter only advances when the phase is `seq scanning append-optimized`, `seq scanning heap`, `index scanning heap`, or `writing new heap`. For AO/CO tables, Greenplum converts byte size into equivalent heap blocks in size. |
| `heap_tuples_written` | bigint | Number of tuples written. This counter only advances when the phase is `seq scanning heap`, `index scanning heap`, `writing new append-optimized`, or `writing new heap`. |
| `heap_blks_total` | bigint | Total number of heap blocks in the table. This number is reported as of the beginning of `seq scanning heap`. For AO/CO tables, Greenplum converts byte size into equivalent heap blocks in size. |
| `heap_blks_scanned` | bigint | Number of heap blocks scanned. This counter only advances when the phase is `seq scanning heap`. For AO/CO tables, Greenplum converts byte size into equivalent heap blocks in size. |
| `index_rebuild_count` | bigint | Number of indexes rebuilt. This counter only advances when the phase is `rebuilding index`, and is not applicable to AO/CO tables. |

### <a id="gp_stat_progress_copy"></a>gp_stat_progress_copy

The `gp_stat_progress_copy` view is a cluster-wide view that displays the [pg_stat_progress_copy](https://www.postgresql.org/docs/15/progress-reporting.html#COPY-PROGRESS-REPORTING) information from every primary segment for all currently-running `COPY` operations.

The `gp_stat_progress_copy_summary` view aggregates across the Greenplum Database cluster the metrics reported by `gp_stat_progress_copy`.

|Column|Type|Description|
|------|----|-----------|
|`gp_segment_id`|integer| Unique identifier of a segment \(or coordinator\) instance. (This column is not present in the `gp_stat_progress_copy_summary` view.)|
| `pid` | integer | Process identifier of the backend, or the coordinator process identifier if the `gp_stat_progress_copy_summary` view. |
| `datid` | oid | The object identifier of the database to which this backend is connected. |
| `datname` | name | Name of the database to which this backend is connected. |
| `relid` | oid | The object identifier of the table on which the `COPY` command is executed. It is set to `0` if copying from a `SELECT` query. |
| `command` | text | The command that is running: `COPY FROM`, `COPY TO`, `COPY FROM ON SEGMENT`, or `COPY TO ON SEGMENT`. |
| `type` | text | The io type that the data is read from or written to: `FILE`, `PROGRAM`, `PIPE` (for `COPY FROM STDIN` and `COPY TO STDOUT`), or `CALLBACK` (used for example during the initial table synchronization in logical replication). |
| `bytes_processed` | bigint | Number of bytes already processed by `COPY` command. |
| `bytes_total` | bigint | Size of source file for `COPY FROM` command in bytes. It is set to `0` if not available. |
| `tuples_processed` | bigint | Number of tuples already processed by `COPY` command. |
| `tuples_excluded` | bigint | Number of tuples not processed because they were excluded by the `WHERE` clause of the `COPY` command. |

### <a id="gp_stat_progress_create_index"></a>gp_stat_progress_create_index

The `gp_stat_progress_create_index` view is a cluster-wide view that displays the [pg_stat_progress_create_index](https://www.postgresql.org/docs/15/progress-reporting.html#CREATE-INDEX-PROGRESS-REPORTING) information from every primary segment for all currently-running `CREATE INDEX` and `REINDEX` operations.

The `gp_stat_progress_create_index_summary` view aggregates across the Greenplum Database cluster the metrics reported by `gp_stat_progress_create_index`.

|Column|Type|Description|
|------|----|-----------|
|`gp_segment_id`|integer| Unique identifier of a segment \(or coordinator\) instance. (This column is not present in the `gp_stat_progress_create_index_summary` view.)|
| `pid` | integer | Process identifier of the backend, or the coordinator process identifier if the `gp_stat_progress_create_index_summary` view. |
| `datid` | oid | The object identifer of the database to which this backend is connected. |
| `datname` | name | Name of the database to which this backend is connected. |
| `relid` | oid | The object identifer of the table on which the index is being created. |
| `index_relid` | oid | The object identifer of the index being created or reindexed. Because Greenplum Database does not support concurrent (re)indexing, this value is always `0`. |
| `command` | text | The name of the command that is running: `CREATE INDEX` or `REINDEX`. |
| `phase` | text | Current processing phase of index creation. Refer to [CREATE INDEX Progress Reporting](../../admin_guide/managing/progress_reporting.html#create_index_progress) for detailed information about the phases. |
| `lockers_total` | bigint | Total number of lockers to wait for, when applicable. |
| `lockers_done` | bigint | Number of lockers already waited for. |
| `current_locker_pid` | bigint | The process identifier of the locker currently being waited for. |
| `blocks_total` | bigint | Total number of blocks to be processed in the current phase. |
| `blocks_done` | bigint | Number of blocks already processed in the current phase. |
| `tuples_total` | bigint | Total number of tuples to be processed in the current phase. |
| `tuples_done` | bigint | Number of tuples already processed in the current phase. |
| `partitions_total` | bigint | When creating an index on a partitioned table, this column is set to the total number of partitions on which the index is to be created. This field is 0 during a `REINDEX`. |
| `partitions_done` | bigint | When creating an index on a partitioned table, this column is set to the number of partitions on which the index has been completed. This field is 0 during a `REINDEX`. |

### <a id="gp_stat_progress_vacuum"></a>gp_stat_progress_vacuum

The `gp_stat_progress_vacuum` view is a cluster-wide view that displays the [pg_stat_progress_vacuum](https://www.postgresql.org/docs/15/progress-reporting.html#VACUUM-PROGRESS-REPORTING) information from every primary segment for all currently-running `VACUUM` and `vacuumdb` operations.

The `gp_stat_progress_vacuum_summary` view aggregates across the Greenplum Database cluster the metrics reported by `gp_stat_progress_vacuum`.

|Column|Type|Description|
|------|----|-----------|
|`gp_segment_id`|integer| Unique identifier of a segment \(or coordinator\) instance. (This column is not present in the `gp_stat_progress_vacuum_summary` view.)|
| `pid` | integer | Process identifier of the backend, or the coordinator process identifier if the `gp_stat_progress_vacuum_summary` view. |
| `datid` | oid | The object identifier of the database to which this backend is connected. |
| `datname` | name | Name of the database to which this backend is connected. |
| `relid` | oid | The object identifier of the table being vacuumed. |
| `phase` | text | Current processing phase of vacuum. Refer to [VACUUM Progress Reporting](../../admin_guide/managing/progress_reporting.html#vacuum_progress) for detailed information about the phases. |
| `heap_blks_total` | bigint | *Heap tables*: Total number of heap blocks in the table. This number is reported as of the beginning of the scan; blocks added later will not be (and need not be) visited by this `VACUUM`.</br></br>*AO/CO tables*<sup>1</sup>: Collected at the beginning of the `append-optimized pre-cleanup` phase by adding up the on-disk file sizes of all segment files of the relation, and converting the size into the number of heap-equivalent blocks. The value should not change while `VACUUM` progresses. |
| `heap_blks_scanned` | bigint | *Heap tables*: Number of heap blocks scanned. Because the visibility map is used to optimize scans, some blocks will be skipped without inspection; skipped blocks are included in this total, so that this number will eventually become equal to `heap_blks_total` when the vacuum is complete. This counter only advances when the phase is `scanning heap`. </br></br> *AO/CO tables*:<sup>1</sup> The counter only advances when the phase is `append-optimized compact`. For `ao_row` tables, updated every time Greenplum finishes scanning a segment file. For `ao_column` tables, updated every time Greenplum moves a tuple. `heap_blks_scanned` can be less than or equal to `heap_blks_total` at the end of the `VACUUM` operation because Greenplum does not need to scan blocks after the logical EOF of a segment file. |
| `heap_blks_vacuumed` | bigint | *Heap tables*: Number of heap blocks vacuumed. Unless the table has no indexes, this counter only advances when the phase is `vacuuming heap`. Blocks that contain no dead tuples are skipped, so the counter may sometimes skip forward in large increments. </br></br> *AO/CO tables*<sup>1</sup>: The counter advances when Greenplum truncates a segment file, which may happen during both `append-optimized pre-cleanup` and `append-optimized post-cleanup` phases. Because Greenplum Database truncates physical blocks after the logical EOF in a segment file, `heap_blks_vacuumed` may be either smaller or larger than `heap_blks_scanned`. |
| `index_vacuum_count` | bigint | *Heap tables*: Number of completed index vacuum cycles. </br></br> *AO/CO tables*: Collected when Greenplum recycles a dead segment file, which may happen both, any, or neither, during `append-optimized pre-cleanup` phase and `append-optimized post-cleanup phase`. |
| `max_dead_tuples` | bigint | *Heap tables*: Number of dead tuples that we can store before needing to perform an index vacuum cycle, based on [maintenance_work_mem](../config_params/guc-list.html#maintenance_work_mem). </br></br> *AO/CO tables*: Collected at the beginning of the `append-optimized pre-cleanup` phase, this is the total number of tuples before the logical EOF of all segment files. The value should not change while `VACUUM` progresses. |
| `num_dead_tuples` | bigint | *Heap tables*: Number of dead tuples collected since the last index vacuum cycle. </br></br> *AO/CO tables*: Collected during `append-optimized compact` phase. For `ao_row` tables, updated every time Greenplum discares a dead tuple. For `ao_column` tables, updated every time Greenplum moves a live tuple, and also when the number of dead tuples advances. |

<sup>1</sup> In Greenplum Database, an AO/CO table vacuum behaves differently than a heap table vacuum. Because Greenplum stores the logical EOF for each segment file, it does not need to scan physical blocks after the logical EOF, and Greenplum can truncate them. Because of this, for AO/CO tables, `heap_blks_vacuumed` could be either smaller or larger than `heap_blks_scanned`. Neither `heap_blks_vacuumed` nor `heap_blks_scanned` can be larger than `heap_blks_total`. Similarly, `heap_blks_scanned` can be less than or equal to `heap_blks_total` at the end of `VACUUM` for AO/CO tables - there is no need to scan blocks after the logical EOF of a segment file.


## <a id="gp_stat_replication"></a>gp_stat_replication

The `gp_stat_replication` view contains replication statistics of the `walsender` process that is used for Greenplum Database Write-Ahead Logging \(WAL\) replication when coordinator or segment mirroring is enabled.

|column|type|references|description|
|------|----|----------|-----------|
|`gp_segment_id`|integer| |Unique identifier of a segment \(or coordinator\) instance.|
|`pid`|integer| |Process ID of the `walsender` backend process.|
|`usesysid`|oid| |User system ID that runs the `walsender` backend process.|
|`usename`|name| |User name that runs the `walsender` backend process.|
|`application_name`|text| |Client application name.|
|`client_addr`|inet| |Client IP address.|
|`client_hostname`|text| |Client host name.|
|`client_port`|integer| |Client port number.|
|`backend_start`|timestamp| |Operation start timestamp.|
|`backend_xmin`|xid| |The current backend's `xmin` horizon.|
|`state`|text| |`walsender` state. The value can be:<br/><br/>`startup`<br/><br/>`backup`<br/><br/>`catchup`<br/><br/>`streaming`|
|`sent_location`|text| |`walsender` xlog record sent location.|
|`write_location`|text| |`walreceiver` xlog record write location.|
|`flush_location`|text| |`walreceiver` xlog record flush location.|
|`replay_location`|text| |Coordinator standby or segment mirror xlog record replay location.|
|`sync_priority`|integer| |Priority. The value is `1`.|
|`sync_state`|text| |`walsender`synchronization state. The value is `sync`.|
|`sync_error`|text| |`walsender` synchronization error. `none` if no error.|

## <a id="gp_stat_resqueues"></a>gp_stat_resqueues

The `gp_stat_resqueues` view is a cluster-wide view that displays the [`pg_stat_resqueues`](#pg_stat_resqueues) information from every primary segment.

|column|type|references|description|
|------|----|----------|-----------|
|`gp_segment_id`|integer| |Unique identifier of a segment (or coordinator) instance.|
|`queueid`|oid| |The OID of the resource queue.|
|`queuename`|name| |The name of the resource queue.|
|`n_queries_exec`|bigint| |Number of queries submitted for execution from this resource queue.|
|`n_queries_wait`|bigint| |Number of queries submitted to this resource queue that had to wait before they could run.|
|`elapsed_exec`|bigint| |Total elapsed execution time for statements submitted through this resource queue.|
|`elapsed_wait`|bigint| |Total elapsed time that statements submitted through this resource queue had to wait before they were run.|

## <a id="gp_stat_slru"></a>gp_stat_slru

The `gp_stat_slru` view is a cluster-wide view that displays the [`pg_stat_slru`](#pg_stat_slru) information from every primary segment.

|column|type|references|description|
|------|----|----------|-----------|
|`gp_segment_id`|integer| |Unique identifier of a segment (or coordinator) instance.|
|`name`|text| |Name of the SLRU.|
|`blks_zeroed`|bigint| |Number of blocks zeroed during initializations.|
|`blks_hit`|bigint| |Number of times disk blocks were found already in the SLRU, so that a read was not necessary (this only includes hits in the SLR, not the operating system's file system cache).|
|`blks_read`|bigint| |Number of disk blocks read for this SLRU.|
|`blks_written`|biging| |Number of disk blocks written for this SLRU.|
|`blks_exists`|biging| |Number of blocks checked for existence for this SLRU.|
|`flushes`|bigint| |Number of flushes of dirty data for this SLRU.|
|`truncates`|bigint| |Number of truncates for this SLRU.|
|`stats_reset`|timestamp with time zone| |Time at which these statistics were last reset.|

This system view is summarized in the `gp_stat_slru_summary` system view.

## <a id="gp_stat_ssl"></a>gp_stat_ssl

The `gp_stat_ssl` view is a cluster-wide view that displays the `pg_stat_ssl` information from every primary segment.

|column|type|references|description|
|------|----|----------|-----------|
|`gp_segment_id`|integer| |Unique identifier of a segment (or coordinator) instance.|
|`pid`|integer| |Process ID of a backend or WAL sender process.|
|`ssl`|boolean| |True if SSL is used on this connection|
|`version`|text| |Version of SSL in use, or `NULL` if SSL is not in use on this connection.|
|`cipher`|text| |Name of SSL cipher in use, or `NULL` if SSL is not in use on this connection.|
|`bits`|integer| |Number of bits in the encryption algorithm used, or `NULL` if SSL is not used on this connection.|
|`client_dn`|text| |Distinguished Name (DN) field from the client certificate used, or `NUL`L if no client certificate was supplied or if SSL is not in use on this connection. This field is truncated if the DN field is longer than `NAMEDATALEN` (64 characters in a standard build).|
|`client_serial`|numeric| |Serial number of the client certificate, or `NULL` if no client certificate was supplied or if SSL is not in use on this connection. The combination of certificate serial number and certificate issuer uniquely identifies a certificate (unless the issuer erroneously reuses serial numbers).|
|`issuer_dn`|text| |DN of the issuer of the client certificate, or `NULL` if no client certificate was supplied or if SSL is not in use on this connection. This field is truncated like `client_dn` is.|

## <a id="gp_stat_subscription"></a>gp_stat_subscription

The `gp_stat_subscription` view is a cluster-wide view that displays the `pg_stat_subscription` information from every primary segment.

|column|type|references|description|
|------|----|----------|-----------|
|`gp_segment_id`|integer| |Unique identifier of a segment (or coordinator) instance.|
|`subid`|oid| |OID of the subscription.|
|`subname`|name| |Name of the subscription.|
|`pid`|integer| |Process ID of the subscription worker process.|
|`relid`|oid| |OID of the relation that the worker is synchronizing; `NULL` for the main apply worker.|
|`received_lsn`|pg_lsn| |Last write-ahead log location received, the initial value of this field being `0`.|
|`last_msg_send_time`|timestamp with time zone| |Send time of last message received from origin WAL sender.|
|`last_msg_receipt_time`|timestamp with time zone| |Receipt time of last message received from origin WAL sender.|
|`latest_end_lsn`|pg_lsn| |Last write-ahead log location reported to origin WAL sender.|
|`latest_end_time`|timestamp with time zone| |Time of last write-ahead log location reported to origin WAL sender.

## <a id="gp_stat_sys_indexes"></a>gp_stat_sys_indexes

The `gp_stat_sys_indexes` view is a cluster-wide view that displays the `pg_stat_sys_indexes` information from every primary segment. It shows the same information as [`gp_stat_all_indexes`](#gp_stat_all_indexes) but filtered to show system indexes only.

This system view is summarized in the `gp_stat_sys_indexes_summary` system view.

## <a id="gp_stat_sys_tables"></a>gp_stat_sys_tables

The `gp_stat_sys_tables` view is a cluster-wide view that displays the `pg_stat_sys_tables` information from every primary segment. It shows the same information as [`gp_stat_all_tables`](#gp_stat_all_tables) but filtered to show system tables only.

## <a id="gp_stat_user_functions"></a>gp_stat_user_functions

The `gp_stat_user_functions` view is a cluster-wide view that displays the `pg_stat_user_functions` information from every primary segment.

|column|type|references|description|
|------|----|----------|-----------|
|`gp_segment_id`|integer| |Unique identifier of a segment (or coordinator) instance.|
|`funcid`|oid| |OID of a function.|
|`schemaname`|name| |Name of the schema this function is in.|
|`funcname`|name| |Name of this function.|
|`calls`|bigint| |Number of times this function has been called.|
|`total_time`|double precision| |Total time spent in this function and all other functions called by it, in milliseconds.|
|`self_time`|double precision| |Total time spent in this function itself, not including other functions called by it, in milliseconds.|

This system view is summarized in the `gp_stat_user_functions_summary` system view.

## <a id="gp_stat_user_indexes"></a>gp_stat_user_indexes

The `gp_stat_user_indexes` view is a cluster-wide view that displays the `pg_stat_user_indexes` information from every primary segment. It shows the same information as [`gp_stat_all_indexes`](#gp_stat_all_indexes) but filtered to show indexes on user tables only.

This system view is summarized in the `gp_stat_user_functions_summary` system view.

## <a id="gp_stat_user_tables"></a>gp_stat_user_tables

The `gp_stat_user_tables` view is a cluster-wide view that displays the `pg_stat_user_tables` information from every primary segment. It shows the same information as [`gp_stat_all_tables`](#gp_stat_all_tables) but filtered to show only user tables.

## <a id="gp_stat_wal"></a>gp_stat_wal

The `gp_stat_wal` view is a cluster-wide view that displays the [`pg_stat_wal`](#pg_stat_wal) information from every primary segment.

|column|type|references|description|
|------|----|----------|-----------|
|`gp_segment_id`|integer| |Unique identifier of a segment (or coordinator) instance.|
|`wal_records`|bigint| |Total number of WAL records generated.|
|`wal_fpw`|bigint| |Total number of WAL full page images generated.|
|`wal_bytes`|numeric| |Total amount of WAL generated in bytes.|
|`wal_buffers_full`|bigint| |Number of times WAL data was written to disk because WAL buffers became full.|
|`wal_write`|bigint| |Number of times WAL buffers were written out to disk.|
|`wal_sync`|bigint||Number of times WAL files were synced to disk.|
|`wal_write_time`|double precision| |Total amount of time spent writing WAL buffers to disk, in milliseconds (if [track_wal_io_timing](../config_params/guc-list.html#track_wal_io_timing) is enabled, otherwise zero).|
|`wal_sync_time`|double precision| |Total amount of time spent syncing WAL files to disk, in milliseconds (if `track_wal_io_timing` is enabled, otherwise zero).|
|`stats_reset`|timestamp with time zone| |Time at which these statistics were last reset.|

This system view is summarized in the `gp_stat_wal_summary` system view.

## <a id="gp_stat_wal_receiver"></a>gp_stat_wal_receiver

The `gp_stat_wal_receiver` view is a cluster-wide view that displays the `pg_stat_wal_receiver` information from every primary segment.

|column|type|references|description|
|------|----|----------|-----------|
|`gp_segment_id`|integer| |Unique identifier of a segment (or coordinator) instance.|
|`pid`|integer| |Process ID of the WAL receiver process.|
|`status`|text| |Activity status of the WAL receiver process.|
|`receive_start_lsn`|pg_lsn| |First write-ahead log location used when WAL receiver is started.|
|`receive_start_tli`|integer.| |First timeline number used when WAL receiver is started.|
|`written_lsn`|pg_lsn.| |Last write-ahead log location already received and written to disk, but not flushed. This should not be used for data integrity checks.|
|`flushed_lsn`|pg_lsn| |Last write-ahead log location already received and flushed to disk, the initial value of this field being the first log location used when WAL receiver is started.|
|`received_tli`|integer| |Timeline number of last write-ahead log location received and flushed to disk, the initial value of this field being the timeline number of the first log location used when WAL receiver is started.|
|`last_msg_send_time`|timestamp with time zone| |Send time of last message received from origin WAL sender.|
|`last_msg_receipt_time`|timestamp with time zone| |Receipt time of last message received from origin WAL sender.|
|`latest_end_lsn`|pg_lsn| |Last write-ahead log location reported to origin WAL sender.|
|`latest_end_time`|timestamp with time zone| |Time of last write-ahead log location reported to origin WAL sender.|
|`slot_name`|text| |Replication slot name used by this WAL receiver.|
|`sender_host`|text| |Host of the PostgreSQL instance this WAL receiver is connected to. This can be a host name, an IP address, or a directory path if the connection is via Unix socket. (The path case can be distinguished because it will always be an absolute path, beginning with `/.`).|
|`sender_port`|integer| |Port number of the PostgreSQL instance this WAL receiver is connected to.|
|`conninfo`|text| |Connection string used by this WAL receiver, with security-sensitive fields obfuscated.|

## <a id="gp_stat_xact_all_tables"></a>gp_stat_xact_all_tables

The `gp_stat_xact_all_tables` view is a cluster-wide view that displays the `pg_stat_xact_all_tables` information from every primary segment. It shows the same information as [`gp_stat_all_tables`](#gp_stat_all_tables) but counts actions taken so far within the current transaction (which are not yet included in `gp_stat_all_tables` and related views). The columns for numbers of live and dead rows and vacuum and analyze actions are not present in this view.

This system view is summarized in the `gp_stat_xact_all_tables_summary` system view.

## <a id="gp_stat_xact_sys_tables"></a>gp_stat_xact_sys_tables

The `gp_stat_xact_sys_tables` view is a cluster-wide view that displays the `pg_stat_xact_sys_tables` information from every primary segment. It shows the same information as [`gp_stat_xact_all_tables`](#gp_stat_xact_all_tables) but filtered to show system tables only.

This system view is summarized in the `gp_stat_xact_sys_tables_summary` system view.

## <a id="gp_stat_xact_user_functions"></a>gp_stat_xact_user_functions

The `gp_stat_xact_user_functions` view is a cluster-wide view that displays the `pg_stat_xact_user_functions` information from every primary segment. It shows the same information as [`gp_stat_xact_user_functions`](#gp_stat_user_functions) but counts only calls during the current transaction (which are not yet included in `gp_stat_user_functions`).

This system view is summarized in the `gp_stat_xact_user_functions_summary` system view.

## <a id="gp_stat_xact_user_tables"></a>gp_stat_xact_user_tables

The `gp_stat_xact_user_tables` view is a cluster-wide view that displays the `pg_stat_xact_user_tables` information from every primary segment. It shows the same information as [`gp_stat_xact_all_tables`](#gp_stat_xact_all_tables) but filtered to show user tables only.

This system view is summarized in the `gp_stat_xact_user_tables_summary` system view.

## <a id="gp_statio_all_indexes"></a>gp_statio_all_indexes

The `gp_statio_all_indexes` view is a cluster-wide view that displays the `pg_statio_all_indexes` information from every primary segment.

|column|type|references|description|
|------|----|----------|-----------|
|`gp_segment_id`|integer| |Unique identifier of a segment (or coordinator) instance.|
|`relid`|oid| |OID of the table for this index.|
|`indexrelid`|oid| |OID of this index.|
|`schemaname`|name| |Name of the schema containing this index.|
|`relname`|name| |Name of the table for this index.|
|`indexrelname`|name| |Name of this index.|
|`idx_blks_read`|bigint| |Number of disk blocks read from this index.|
|`idx_blks_hit`|bigint| |Number of buffer hits in this index.|

This system view is summarized in the `gp_statio_all_indexes_summary` system view.

## <a id="gp_statio_all_sequences"></a>gp_statio_all_sequences

The `gp_statio_all_sequences` view is a cluster-wide view that displays the `pg_statio_all_sequences` information from every primary segment.

|column|type|references|description|
|------|----|----------|-----------|
|`gp_segment_id`|integer| |Unique identifier of a segment (or coordinator) instance.|
|`relid`|oid| |OID of a sequence.|
|`schemaname`|name| |Name of the schema this sequence is in.|
|`relname`|name| |Name of this sequence.|
|`blks_read`|bigint| |Number of disk blocks read from this sequence.|
|`blks_hit`|bigint| |Number of buffer hits in this sequence.|

This system view is summarized in the `gp_statio_all_sequences_summary` system view.

## <a id="gp_statio_all_tables"></a>gp_statio_all_tables

The `gp_statio_all_tables` view is a cluster-wide view that displays the `pg_statio_all_tables` information from every primary segment.

|column|type|references|description|
|------|----|----------|-----------|
|`gp_segment_id`|integer| |Unique identifier of a segment (or coordinator) 
|reli|oid|OID of a table.|
|`schemaname`|name| |Name of the schema containing this table.|
|`relname`|name| |Name of this table.|
|`heap_blks_read`|bigint| |Number of disk blocks read from this table.|
|`heap_blks_hit`|bigint| |Number of buffer hits in this table.|
|`idx_blks_read`|bigint| |Number of disk blocks read from all indexes on this table.|
|`idx_blks_hit`|bigint| |Number of buffer hits in all indexes on this table.|
|`toast_blks_read`|bigint.| |Number of disk blocks read from this table's TOAST table (if any).|
|`toast_blks_hit`|bigint| |Number of buffer hits in this table's TOAST table (if any).|
|`tidx_blks_read`|bigint| |Number of disk blocks read from this table's TOAST table indexes (if any).|
|`tidx_blks_hit`|bigint| |Number of buffer hits in this table's TOAST table indexes (if any).|

This system view is summarized in the `gp_statio_all_tables_summary` system view.

## <a id="gp_statio_sys_indexes"></a>gp_statio_sys_indexes

The `gp_statio_sys_indexes` view is a cluster-wide view that displays the `pg_statio_sys_indexes` information from every primary segment. It shows the same information as [`gp_statio_all_indexes`](#gp_statio_all_indexes) but filtered to show indexes on system tables only.

This system view is summarized in the `gp_statio_sys_indexes_summary` system view.

## <a id="gp_statio_sys_sequences"></a>gp_statio_sys_sequences

The `gp_statio_sys_sequences` view is a cluster-wide view that displays the `pg_statio_sys_sequences` information from every primary segment. It shows the same information as [`gp_statio_all_sequences`](#gp_statio_all_sequences) but filtered to system sequences only.

This system view is summarized in the `gp_statio_sys_indexes_summary` system view.

## <a id="gp_statio_sys_tables"></a>gp_statio_sys_tables

The `gp_statio_sys_tables` view is a cluster-wide view that displays the `pg_statio_sys_tables` information from every primary segment. It shows the same information as [`gp_statio_all_tables`](#gp_statio_all_tables) but filtered to show system tables only.

This system view is summarized in the `gp_statio_sys_tables_summary` system view.

## <a id="gp_statio_user_indexes"></a>gp_statio_user_indexes

The `gp_statio_user_indexes` view is a cluster-wide view that displays the `pg_statio_user_indexes` information from every primary segment. It shows the same information as [`gp_statio_all_indexes`](#gp_statio_all_indexes) but filtered to show indexes on user tables only.

This system view is summarized in the `gp_statio_sys_user_indexes_summary` system view.

## <a id="gp_statio_user_sequences"></a>gp_statio_user_sequences

The `gp_statio_user_sequences` view is a cluster-wide view that displays the `pg_statio_user_sequences` information from every primary segment. It shows the same information as [`gp_statio_all_sequences`](#gp_statio_all_sequences) but filtered to show user sequences only.

This system view is summarized in the `gp_statio_sys_user_sequences_summary` system view.

## <a id="gp_statio_user_tables"></a>gp_statio_user_tables

The `gp_statio_user_tables` view is a cluster-wide view that displays the `pg_statio_user_tables` information from every primary segment. It shows the same information as [`gp_statio_all_tables`](#gp_statio_all_tables) but filtered to show user tables only.

This system view is summarized in the `gp_statio_sys_user_tables_summary` system view.

## <a id="gp_stats"></a>gp_stats

The `gp_stats` view is a cluster-wide view that displays the `pg_stats` information from every primary segment.

|column|type|references|description|
|------|----|----------|-----------|
|`gp_segment_id`|integer| |Unique identifier of a segment (or coordinator) 
|`schemaname`|name|[pg\_namespace](pg_namespace.html).nspname| The name of the schema containing table.|
|`tablename`|name|[pg\_class](pg_class.html).relname|The name of the table.|
|`attname`|name|[pg\_attribute](pg_attribute.html).attname|The name of the column described by this row.|
|`inherited`|bool| |If true, this row includes inheritance child columns, not just the values in the specified table.|
|`null_frac`|real| |The fraction of column entries that are null.|
|`avg_width`|integer| |The average width in bytes of column's entries.|
|`n_distinct`|real| |If greater than zero, the estimated number of distinct values in the column. If less than zero, the negative of the number of distinct values divided by the number of rows. \(The negated form is used when ANALYZE believes that the number of distinct values is likely to increase as the table grows; the positive form is used when the column seems to have a fixed number of possible values.\) For example, `-1` indicates a unique column in which the number of distinct values is the same as the number of rows.|
|`most_common_vals`|anyarray| |A list of the most common values in the column. \(Null if no values seem to be more common than any others.\)|
|`most_common_freqs`|real[]| |A list of the frequencies of the most common values, i.e., number of occurrences of each divided by total number of rows. \(Null when `most_common_vals` is.\)|
|`histogram_bounds`|anyarray| |A list of values that divide the column's values into groups of approximately equal population. The values in `most_common_vals`, if present, are omitted from this histogram calculation. \(This column is null if the column data type does not have a `<` operator or if the `most_common_vals` list accounts for the entire population.\)|
|`correlation`|real| |Statistical correlation between physical row ordering and logical ordering of the column values. This ranges from -1 to +1. When the value is near -1 or +1, an index scan on the column will be estimated to be cheaper than when it is near zero, due to reduction of random access to the disk. \(This column is null if the column data type does not have a `<` operator.\)|
|`most_common_elems`|anyarray| |A list of non-null element values most often appearing within values of the column. \(Null for scalar types.\)|
|`most_common_elem_freqs`|real[]| |A list of the frequencies of the most common element values, i.e., the fraction of rows containing at least one instance of the given value. Two or three additional values follow the per-element frequencies; these are the minimum and maximum of the preceding per-element frequencies, and optionally the frequency of null elements. \(Null when most_common_elems is.\)|
|`element_count_histogram`|real[]| |A histogram of the counts of distinct non-null element values within the values of the column, followed by the average number of distinct non-null elements. \(Null for scalar types.\)|

The maximum number of entries in the array fields can be controlled on a column-by-column basis using the `ALTER TABLE SET STATISTICS` command, or globally by setting the [default\_statistics\_target](../config_params/guc-list.html#default_statistics_target) run-time configuration parameter.

## <a id="gp_stats_ext"></a>gp_stats_ext

The `gp_stats_ext` view is a cluster-wide view that displays the `pg_stats_ext` information from every primary segment.

|column|type|references|description|
|------|----|----------|-----------|
|`gp_segment_id`|integer| |Unique identifier of a segment (or coordinator) 
|`schemaname`|name|[pg\_namespace](pg_namespace.html).nspname|The name of the schema containing table.|
|`tablename`|name|[pg\_class](pg_class.html).relname|The name of the table.|
|`statistics_schemaname`|name|[pg\_namespace](pg_namespace.html).nspname|The name of the schema containing the extended statistic.|
|`statistics_name`|name|[pg\_statistic_ext](pg_statistic_ext.html).stxname|The name of the extended statistic.|
|`statistics_owner`|oid|[pg\_authid](pg_authid.html).oid|The owner of the extended statistic.|
|`attnames`|name[]|[pg\_attribute](pg_attribute.html).attname|The names of the columns on which the extended statistics is defined.|
|`kinds`|text[]| |The types of extended statistics enabled for this record.|
|`n_distinct`|pg_ndistinct| | N-distinct counts for combinations of column values. If greater than zero, the estimated number of distinct values in the combination. If less than zero, the negative of the number of distinct values divided by the number of rows. \(The negated form is used when `ANALYZE` believes that the number of distinct values is likely to increase as the table grows; the positive form is used when the column seems to have a fixed number of possible values.\) For example, `-1` indicates a unique combination of columns in which the number of distinct combinations is the same as the number of rows. |
|`dependencies`|pg_dependencies| | Functional dependency statistics. |
|`most_common_vals`|anyarray| |A list of the most common values in the column. \(Null if no combinations seem to be more common than any others.\)|
|`most_common_vals_null`|anyarray| |A list of NULL flags for the most combinations of values. \(Null when `most_common_vals` is.\) |
|`most_common_freqs`|real[]| |A list of the frequencies of the most common combinations, i.e., number of occurrences of each divided by total number of rows. \(Null when `most_common_vals` is.\)|
|`most_common_base_freqs`|real[]| |A list of the base frequencies of the most common combinations, i.e., product of per-value frequencies. \(Null when `most_common_vals` is.\)|

The maximum number of entries in the array fields can be controlled on a column-by-column basis using the `ALTER TABLE SET STATISTICS` command, or globally by setting the [default\_statistics\_target](../config_params/guc-list.html#default_statistics_target) run-time configuration parameter.

## <a id="pg_backend_memory_contexts"></a>pg_backend_memory_contexts

The `pg_backend_memory_contexts` system view displays all of the memory contexts in use by the server process attached to the current session.

`pg_backend_memory_contexts` contains one row for each memory context.

|column|type|description|
|------|----|-----------|
|`name`|text| The name of the memory context.|
|`ident`|text| Identification information of the memory context. This field is truncated at 1024 bytes.|
|`parent`|text| The name of the parent of this memory context.|
|`level`|int4| The distance from `TopMemoryContext` in context tree.|
|`total_bytes`|int8| The total number of bytes allocated for this memory context.|
|`total_nblocks`|int8| The total number of blocks allocated for this memory context.|
|`free_bytes`|int8| Free space in bytes.|
|`free_chunks`|int8| The total number of free chunks.|
|`used_bytes`|int8| Used space in bytes.|

## <a id="pg_cursors"></a>pg_cursors

The `pg_cursors` view lists the currently available cursors. Cursors can be defined in one of the following ways:

-   via the DECLARE SQL statement
-   via the Bind message in the frontend/backend protocol
-   via the Server Programming Interface \(SPI\)

    > **Note** Greenplum Database does not support the definition, or access of, parallel retrieve cursors via SPI.


Cursors exist only for the duration of the transaction that defines them, unless they have been declared `WITH HOLD`. Non-holdable cursors are only present in the view until the end of their creating transaction.

> **Note** Greenplum Database does not support holdable parallel retrieve cursors.

|name|type|references|description|
|----|----|----------|-----------|
|`name`|text| |The name of the cursor.|
|`statement`|text| |The verbatim query string submitted to declare this cursor.|
|`is_holdable`|boolean| |`true` if the cursor is holdable \(that is, it can be accessed after the transaction that declared the cursor has committed\); `false` otherwise.<br/><br/>> **Note** Greenplum Database does not support holdable parallel retrieve cursors, this value is always `false` for such cursors.|
|`is_binary`|boolean| |`true` if the cursor was declared `BINARY`; `false` otherwise.|
|`is_scrollable`|boolean| |`true` if the cursor is scrollable \(that is, it allows rows to be retrieved in a nonsequential manner\); `false` otherwise.<br/><br/>> **Note** Greenplum Database does not support scrollable cursors, this value is always `false`.|
|`creation_time`|timestamptz| |The time at which the cursor was declared.|
|`is_parallel`|boolean| |`true` if the cursor was declared `PARALLEL RETRIEVE`; `false` otherwise.|

## <a id="pg_exttable"></a>pg_exttable

The `pg_exttable` system catalog view is used to track external tables and web tables created by the `CREATE EXTERNAL TABLE` command.

|column|type|references|description|
|------|----|----------|-----------|
|`reloid`|oid|pg\_class.oid|The OID of this external table.|
|`urilocation`|text\[\]| |The URI location\(s\) of the external table files.|
|`execlocation`|text\[\]| |The ON segment locations defined for the external table.|
|`fmttype`|char| |Format of the external table files: `t` for text, `c` for csv, or `b` for custom. |
|`fmtopts`|text| |Formatting options of the external table files, such as the field delimiter, null string, escape character, etc.|
|`options`|text\[\]| |The options defined for the external table.|
|`command`|text| |The OS command to run when the external table is accessed.|
|`rejectlimit`|integer| |The per segment reject limit for rows with errors, after which the load will fail.|
|`rejectlimittype`|char| |Type of reject limit threshold: `r` for number of rows, or `p` for percent.|
|`logerrors`|bool| |`1` to log errors, `0` to not.|
|`encoding`|text| |The client encoding.|
|`writable`|boolean| |`false` for readable external tables, `true` for writable external tables.|

## <a id="pg_matviews"></a>pg_matviews

The view `pg_matviews` provides access to information about each materialized view in the database.

|column|type|references|description|
|------|----|----------|-----------|
|`schemaname`|name|pg\_namespace.nspname|Name of the schema containing the materialized view|
|`matviewname`|name|pg\_class.relname|Name of the materialized view|
|`matviewowner`|name|pg\_authid.rolname|Name of the materialized view's owner|
|`tablespace`|name|pg\_tablespace.spcname|Name of the tablespace containing the materialized view \(NULL if default for the database\)|
|`hasindexes`|boolean||True if the materialized view has \(or recently had\) any indexes|
|`ispopulated`|boolean||True if the materialized view is currently populated|
|`definition`|text||Materialized view definition \(a reconstructed `SELECT` command\)|

## <a id="pg_max_external_files"></a>pg_max_external_files

The `pg_max_external_files` view shows the maximum number of external table files allowed per segment host when using the external table `file` protocol.

|column|type|references|description|
|------|----|----------|-----------|
|`hostname`|name| |The host name used to access a particular segment instance on a segment host.|
|`maxfiles`|bigint| |Number of primary segment instances on the host.|

## <a id="pg_policies"></a>pg_policies

The `pg_policies` view provides access to information about each row-level security policy in the database.

|Column Name|Type|References|Description|
|------|----|----------|-----------|
|`schemaname`|name|pg\_namespace.nspname |The name of schema that contas the table the policy is on|
|`tablename`|name|pg\_class.relname|The name of the table the policy is on|
|`policyname`|name|pg\_policy.polname |The name of policy|
|`polpermissive`|text| |Is the policy permissive or restrictive?|
|`roles`|name[]| |The roles to which this policy applies|
|`cmd`|text| |The command type to which the policy is applied|
|`qual`|text| |The expression added to the security barrier qualifications for queries to which this policy applies|
|`with_check`|text| |The expression added to the `WITH CHECK` qualifications for queries that attempt to add rows to this table|

> **Note**  Policies stored in `pg_policy` are applied only when `pg_class.relrowsecurity` is set for their table.

## <a id="pg_resqueue_attributes"></a>pg_resqueue_attributes

> **Note** The `pg_resqueue_attributes` view is valid only when resource queue-based resource management is active.

The `pg_resqueue_attributes` view allows administrators to see the attributes set for a resource queue, such as its active statement limit, query cost limits, and priority.

|column|type|references|description|
|------|----|----------|-----------|
|`rsqname`|name|pg\_resqueue.rsqname|The name of the resource queue.|
|`resname`|text| |The name of the resource queue attribute.|
|`resetting`|text| |The current value of a resource queue attribute.|
|`restypid`|integer| |System assigned resource type id.|

## <a id="pg_stat_activity"></a>pg_stat_activity

The `pg_stat_activity` view shows one row per server process with details about the associated user session and query. The columns that report data on the current query are available unless the parameter `stats_command_string` has been turned off. Furthermore, these columns are only visible if the user examining the view is a superuser or the same as the user owning the process being reported on.

The maximum length of the query text string stored in the column `query` can be controlled with the server configuration parameter `track_activity_query_size`.

|column|type|references|description|
|------|----|----------|-----------|
|`datid`|oid|pg\_database.oid|Database OID|
|`datname`|name| |Database name|
|`pid`|integer| |Process ID of this backend|
|`sess_id`|integer| |Session ID|
|`usesysid`|oid|pg\_authid.oid|OID of the user logged into this backend|
|`usename`|name| |Name of the user logged into this backend|
|`application_name`|text| |Name of the application that is connected to this backend|
|`client_addr`|inet| |IP address of the client connected to this backend. If this field is null, it indicates either that the client is connected via a Unix socket on the server machine or that this is an internal process such as autovacuum.|
|`client_hostname`|text| |Host name of the connected client, as reported by a reverse DNS lookup of `client_addr`. This field will only be non-null for IP connections, and only when log\_hostname is enabled.|
|`client_port`|integer| |TCP port number that the client is using for communication with this backend, or -1 if a Unix socket is used|
|`backend_start`|timestamptz| |Time backend process was started|
|`xact_start`|timestamptz| |Transaction start time|
|`query_start`|timestamptz| |Time query began execution|
|`state_change`|timestampz| |Time when the `state` was last changed|
|`wait_event_type`|text| |Type of event for which the backend is waiting|
|`wait_event`|text| |Wait event name if backend is currently waiting|
|`state`|text| |Current overall state of this backend. Possible values are:<br/><br/>-   `active`: The backend is running a query.<br/><br/>-   `idle`: The backend is waiting for a new client command.<br/><br/>-   `idle in transaction`: The backend is in a transaction, but is not currently running a query.<br/><br/>-   `idle in transaction (aborted)`: This state is similar to idle in transaction, except one of the statements in the transaction caused an error.<br/><br/>-   `fastpath function call`: The backend is running a fast-path function.<br/><br/>-   `disabled`: This state is reported if `track_activities` is deactivated in this backend.|
|`backend_xid`|xid||The top-level transaction identifier of this backend, if any.|
|`backend_xmin`|xid||The current backend's `xmin` horizon.| 
|`query`|text| |Text of this backend's most recent query. If `state` is active this field shows the currently running query. In all other states, it shows the last query that was run.|
|`backend_type`|text||The type of the current backend.|
|`rsgid`|oid|pg\_resgroup.oid|Resource group OID or `0`.<br/><br/>See [Note](#rsg_note).|
|`rsgname`|text|pg\_resgroup.rsgname|Resource group name or `unknown`.<br/><br/>See [Note](#rsg_note).|

> **Note**
> When resource groups are enabled. Only query dispatcher (QD) processes will have a `rsgid` and `rsgname`. Other server processes such as a query executer (QE) process or session connection processes will have a `rsgid` value of `0` and a `rsgname` value of `unknown`. QE processes are managed by the same resource group as the dispatching QD process.

## <a id="pg_stat_all_indexes"></a>pg_stat_all_indexes

The `pg_stat_all_indexes` view shows one row for each index in the current database that displays statistics about accesses to that specific index.

The `pg_stat_user_indexes` and `pg_stat_sys_indexes` views contain the same information, but filtered to only show user and system indexes respectively.

|Column|Type|Description|
|------|----|-----------|
|`relid`|oid|OID of the table for this index|
|`indexrelid`|oid|OID of this index|
|`schemaname`|name|Name of the schema this index is in|
|`relname`|name|Name of the table for this index|
|`indexrelname`|name|Name of this index|
|`idx_scan`|bigint|Total number of index scans initiated on this index from all segment instances|
|`idx_tup_read`|bigint|Number of index entries returned by scans on this index|
|`idx_tup_fetch`|bigint|Number of live table rows fetched by simple index scans using this index|

## <a id="pg_stat_all_tables"></a>pg_stat_all_tables

The `pg_stat_all_tables` view shows one row for each table in the current database \(including TOAST tables\) to display statistics about accesses to that specific table.

The `pg_stat_user_tables` and `pg_stat_sys_table` views contain the same information, but filtered to only show user and system tables respectively.

|Column|Type|Description|
|------|----|-----------|
|`relid`|oid|OID of a table|
|`schemaname`|name|Name of the schema that this table is in|
|`relname`|name|Name of this table|
|`seq_scan`|bigint|Total number of sequential scans initiated on this table from all segment instances|
|`seq_tup_read`|bigint|Number of live rows fetched by sequential scans|
|`idx_scan`|bigint|Total number of index scans initiated on this table from all segment instances|
|`idx_tup_fetch`|bigint|Number of live rows fetched by index scans|
|`n_tup_ins`|bigint|Number of rows inserted|
|`n_tup_upd`|bigint|Number of rows updated \(includes HOT updated rows\)|
|`n_tup_del`|bigint|Number of rows deleted|
|`n_tup_hot_upd`|bigint|Number of rows HOT updated \(i.e., with no separate index update required\)|
|`n_live_tup`|bigint|Estimated number of live rows|
|`n_dead_tup`|bigint|Estimated number of dead rows|
|`n_mod_since_analyze`|bigint|Estimated number of rows modified since this table was last analyzed|
|`last_vacuum`|timestamp with time zone|Last time this table was manually vacuumed \(not counting `VACUUM FULL`\)|
|`last_autovacuum`|timestamp with time zone|Last time this table was vacuumed by the autovacuum daemon|
|`last_analyze`|timestamp with time zone|Last time this table was manually analyzed|
|`last_autoanalyze`|timestamp with time zone|Last time this table was analyzed by the autovacuum daemon|
|`vacuum_count`|bigint|Number of times this table has been manually vacuumed \(not counting `VACUUM FULL`\)|
|`autovacuum_count`|bigint|Number of times this table has been vacuumed by the autovacuum daemon|
|`analyze_count`|bigint|Number of times this table has been manually analyzed|
|`autoanalyze_count`|bigint|Number of times this table has been analyzed by the autovacuum daemon|

## <a id="pg_stat_operations"></a>pg_stat_operations

The view `pg_stat_operations` shows details about the last operation performed on a database object \(such as a table, index, view or database\) or a global object \(such as a role\).

|column|type|references|description|
|------|----|----------|-----------|
|`classname`|text| |The name of the system table in the `pg_catalog` schema where the record about this object is stored \(`pg_class`=relations, `pg_database`=databases,`pg_namespace`=schemas, `pg_authid`=roles\)|
|`objname`|name| |The name of the object.|
|`objid`|oid| |The OID of the object.|
|`schemaname`|name| |The name of the schema where the object resides.|
|`usestatus`|text| |The status of the role who performed the last operation on the object \(`CURRENT`=a currently active role in the system, `DROPPED`=a role that no longer exists in the system, `CHANGED`=a role name that exists in the system, but has changed since the last operation was performed\).|
|`usename`|name| |The name of the role that performed the operation on this object.|
|`actionname`|name| |The action that was taken on the object.|
|`subtype`|text| |The type of object operated on or the subclass of operation performed.|
|`statime`|timestamptz| |The timestamp of the operation. This is the same timestamp that is written to the Greenplum Database server log files in case you need to look up more detailed information about the operation in the logs.|

## <a id="pg_stat_replication"></a>pg_stat_replication

The `pg_stat_replication` view contains metadata of the `walsender` process that is used for Greenplum Database coordinator mirroring.

|column|type|references|description|
|------|----|----------|-----------|
|`pid`|integer| |Process ID of WAL sender backend process.|
|`usesysid`|integer| |User system ID that runs the WAL sender backend process|
|`usename`|name| |User name that runs WAL sender backend process.|
|`application_name`|oid| |Client application name.|
|`client_addr`|name| |Client IP address.|
|`client_hostname`|text| |The host name of the client machine.|
|`client_port`|integer| |Client port number.|
|`backend_start`|timestamp| |Operation start timestamp.|
|`backend_xmin`|xid| |The current backend's `xmin` horizon.|
|`state`|text| |WAL sender state. The value can be:<br/><br/>`startup`<br/><br/>`backup`<br/><br/>`catchup`<br/><br/>`streaming`|
|`sent_location`|text| |WAL sender xlog record sent location.|
|`write_location`|text| |WAL receiver xlog record write location.|
|`flush_location`|text| |WAL receiver xlog record flush location.|
|`replay_location`|text| |Standby xlog record replay location.|
|`sync_priority`|text| |Priority. the value is `1`.|
|`sync_state`|text| |WAL sender synchronization state. The value is `sync`.|

## <a id="pg_stat_resqueues"></a>pg_stat_resqueues

> **Note** The `pg_stat_resqueues` view is valid only when resource queue-based resource management is active.

The `pg_stat_resqueues` view allows administrators to view metrics about a resource queue's workload over time. To allow statistics to be collected for this view, you must enable the `stats_queue_level` server configuration parameter on the Greenplum Database coordinator instance. Enabling the collection of these metrics does incur a small performance penalty, as each statement submitted through a resource queue must be logged in the system catalog tables.

|column|type|references|description|
|------|----|----------|-----------|
|`queueid`|oid| |The OID of the resource queue.|
|`queuename`|name| |The name of the resource queue.|
|`n_queries_exec`|bigint| |Number of queries submitted for execution from this resource queue.|
|`n_queries_wait`|bigint| |Number of queries submitted to this resource queue that had to wait before they could run.|
|`elapsed_exec`|bigint| |Total elapsed execution time for statements submitted through this resource queue.|
|`elapsed_wait`|bigint| |Total elapsed time that statements submitted through this resource queue had to wait before they were run.|

## <a id="pg_stat_slru"></a>pg_stat_slru

Greenplum Database accesses certain on-disk information via SLRU (simple least-recently-used) caches. The `pg_stat_slru` view contains one row for each tracked SLRU cache, showing statistics about access to cached pages.

|column|type|references|description|
|------|----|----------|-----------|
|`name`|text| |Name of the SLRU.|
|`blks_zeroed`|bigint| |Number of blocks zeroed during initializations.|
|`blks_hit`|bigint| |Number of times disk blocks were found already in the SLRU, so that a read was not necessary (this only includes hits in the SLR, not the operating system's file system cache).|
|`blks_read`|bigint| |Number of disk blocks read for this SLRU.|
|`blks_written`|biging| |Number of disk blocks written for this SLRU.|
|`blks_exists`|biging| |Number of blocks checked for existence for this SLRU.|
|`flushes`|bigint| |Number of flushes of dirty data for this SLRU.|
|`truncates`|bigint| |Number of truncates for this SLRU.|
|`stats_reset`|timestamp with time zone| |Time at which these statistics were last reset.|

## <a id="pg_stat_wal"></a>pg_stat_wal

The `pg_stat_wal` view shows data about the WAL activity of the cluster. It contains always a single row.

|column|type|references|description|
|------|----|----------|-----------|
|`wal_records`|bigint| |Total number of WAL records generated.|
|`wal_fpw`|bigint| |Total number of WAL full page images generated.|
|`wal_bytes`|numeric| |Total amount of WAL generated in bytes.|
|`wal_buffers_full`|bigint| |Number of times WAL data was written to disk because WAL buffers became full.|
|`wal_write`|bigint| |Number of times WAL buffers were written out to disk.|
|`wal_sync`|bigint||Number of times WAL files were synced to disk.|
|`wal_write_time`|double precision| |Total amount of time spent writing WAL buffers to disk, in milliseconds (if [track_wal_io_timing](../config_params/guc-list.html#track_wal_io_timing) is enabled, otherwise zero).|
|`wal_sync_time`|double precision| |Total amount of time spent syncing WAL files to disk, in milliseconds (if `track_wal_io_timing` is enabled, otherwise zero).|
|`stats_reset`|timestamp with time zone| |Time at which these statistics were last reset.|

## <a id="pg_stats"></a>pg_stats

The `pg_stats` view provides access to the information stored in the `pg_statistic` catalog. This view allows access only to rows of `pg_statistic` that correspond to tables the user has permission to read, and therefore it is safe to allow public read access to this view.

`pg_stats` is also designed to present the information in a more readable format than the underlying catalog — at the cost that its schema must be extended whenever new slot types are defined for `pg_statistic`.

|column|type|references|description|
|------|----|----------|-----------|
|`schemaname`|name|[pg\_namespace](pg_namespace.html).nspname| The name of the schema containing table.|
|`tablename`|name|[pg\_class](pg_class.html).relname|The name of the table.|
|`attname`|name|[pg\_attribute](pg_attribute.html).attname|The name of the column described by this row.|
|`inherited`|bool| |If true, this row includes inheritance child columns, not just the values in the specified table.|
|`null_frac`|real| |The fraction of column entries that are null.|
|`avg_width`|integer| |The average width in bytes of column's entries.|
|`n_distinct`|real| |If greater than zero, the estimated number of distinct values in the column. If less than zero, the negative of the number of distinct values divided by the number of rows. \(The negated form is used when ANALYZE believes that the number of distinct values is likely to increase as the table grows; the positive form is used when the column seems to have a fixed number of possible values.\) For example, `-1` indicates a unique column in which the number of distinct values is the same as the number of rows.|
|`most_common_vals`|anyarray| |A list of the most common values in the column. \(Null if no values seem to be more common than any others.\)|
|`most_common_freqs`|real[]| |A list of the frequencies of the most common values, i.e., number of occurrences of each divided by total number of rows. \(Null when `most_common_vals` is.\)|
|`histogram_bounds`|anyarray| |A list of values that divide the column's values into groups of approximately equal population. The values in `most_common_vals`, if present, are omitted from this histogram calculation. \(This column is null if the column data type does not have a `<` operator or if the `most_common_vals` list accounts for the entire population.\)|
|`correlation`|real| |Statistical correlation between physical row ordering and logical ordering of the column values. This ranges from -1 to +1. When the value is near -1 or +1, an index scan on the column will be estimated to be cheaper than when it is near zero, due to reduction of random access to the disk. \(This column is null if the column data type does not have a `<` operator.\)|
|`most_common_elems`|anyarray| |A list of non-null element values most often appearing within values of the column. \(Null for scalar types.\)|
|`most_common_elem_freqs`|real[]| |A list of the frequencies of the most common element values, i.e., the fraction of rows containing at least one instance of the given value. Two or three additional values follow the per-element frequencies; these are the minimum and maximum of the preceding per-element frequencies, and optionally the frequency of null elements. \(Null when most_common_elems is.\)|
|`element_count_histogram`|real[]| |A histogram of the counts of distinct non-null element values within the values of the column, followed by the average number of distinct non-null elements. \(Null for scalar types.\)|

The maximum number of entries in the array fields can be controlled on a column-by-column basis using the `ALTER TABLE SET STATISTICS` command, or globally by setting the [default\_statistics\_target](../config_params/guc-list.html#default_statistics_target) run-time configuration parameter.

## <a id="pg_stats_ext"></a>pg_stats_ext

The `pg_stats` view provides access to the information stored in the `pg_statistic_ext` and `pg_statistic_ext_data` catalog tables. This view allows access only to rows of `pg_statistic_ext` and `pg_statistic_ext_data` that correspond to tables the user has permission to read, and therefore it is safe to allow public read access to this view.

`pg_stats_ext` is also designed to present the information in a more readable format than the underlying catalogs — at the cost that its schema must be extended whenever new types of extended statistics are added to `pg_statistic_ext`.

|column|type|references|description|
|------|----|----------|-----------|
|`schemaname`|name|[pg\_namespace](pg_namespace.html).nspname|The name of the schema containing table.|
|`tablename`|name|[pg\_class](pg_class.html).relname|The name of the table.|
|`statistics_schemaname`|name|[pg\_namespace](pg_namespace.html).nspname|The name of the schema containing the extended statistic.|
|`statistics_name`|name|[pg\_statistic_ext](pg_statistic_ext.html).stxname|The name of the extended statistic.|
|`statistics_owner`|oid|[pg\_authid](pg_authid.html).oid|The owner of the extended statistic.|
|`attnames`|name[]|[pg\_attribute](pg_attribute.html).attname|The names of the columns on which the extended statistics is defined.|
|`kinds`|text[]| |The types of extended statistics enabled for this record.|
|`n_distinct`|pg_ndistinct| | N-distinct counts for combinations of column values. If greater than zero, the estimated number of distinct values in the combination. If less than zero, the negative of the number of distinct values divided by the number of rows. \(The negated form is used when `ANALYZE` believes that the number of distinct values is likely to increase as the table grows; the positive form is used when the column seems to have a fixed number of possible values.\) For example, `-1` indicates a unique combination of columns in which the number of distinct combinations is the same as the number of rows. |
|`dependencies`|pg_dependencies| | Functional dependency statistics. |
|`most_common_vals`|anyarray| |A list of the most common values in the column. \(Null if no combinations seem to be more common than any others.\)|
|`most_common_vals_null`|anyarray| |A list of NULL flags for the most combinations of values. \(Null when `most_common_vals` is.\) |
|`most_common_freqs`|real[]| |A list of the frequencies of the most common combinations, i.e., number of occurrences of each divided by total number of rows. \(Null when `most_common_vals` is.\)|
|`most_common_base_freqs`|real[]| |A list of the base frequencies of the most common combinations, i.e., product of per-value frequencies. \(Null when `most_common_vals` is.\)|

The maximum number of entries in the array fields can be controlled on a column-by-column basis using the `ALTER TABLE SET STATISTICS` command, or globally by setting the [default\_statistics\_target](../config_params/guc-list.html#default_statistics_target) run-time configuration parameter.

## <a id="summary_views"></a>Summary Views

Greenplum Database includes a number of summary views -- all related to collected statistics -- which aggregate across the Greenplum cluster the metrics reported by their corresponding `gp_` view. For example, `gp_stat_archiver_summary` aggregates the metrics reported by `gp_stat_archiver`. These metrics are reported as sum, average, or last, depending on the column. For more information, see the [summary view code in Github](https://github.com/greenplum-db/gpdb/blob/main/src/backend/catalog/system_views_gp_summary.sql).

The following is a list of summary views:

- gp_stat_all_indexes_summary
- gp_stat_all_tables_summary
- gp_stat_archiver_summary
- gp_stat_bgwriter_summary
- gp_stat_database_summary
- gp_stat_slru_summary
- gp_stat_progress_analyze_summary
- gp_stat_progress_basebackup_summary
- gp_stat_progress_cluster_summary
- gp_stat_progress_copy_summary
- gp_stat_progress_create_index_summary
- gp_stat_progress_vacuum_summary
- gp_stat_sys_indexes_summary
- gp_stat_user_functions_summary
- gp_stat_user_indexes_summary
- gp_stat_wal_summary
- gp_stat_xact_all_tables_summary
- gp_stat_xact_sys_tables_summary
- gp_stat_xact_user_functions_summary
- gp_stat_xact_user_tables_summary
- gp_statio_all_indexes_summary
- gp_statio_all_sequences_summary
- gp_statio_all_tables_summary
- gp_statio_sys_indexes_summary
- gp_statio_sys_sequences_summary
- gp_statio_sys_tables_summary
- gp_statio_user_indexes_summary
- gp_statio_user_sequences_summary
- gp_statio_user_tables_summary

**Parent topic:** [System Catalogs](../system_catalogs/catalog_ref.html)
