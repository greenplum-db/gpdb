# System Views 

Greenplum Database provides the following system views.

-   [gp_distributed_log](#gp_distributed_log)
-   [gp_distributed_xacts](#gp_distributed_xacts)
-   [gp_endpoints](#gp_endpoints)
-   [gp_pgdatabase](#gp_pgdatabase)
-   [gp_resgroup_config](#gp_resgroup_config)
-   [gp_resgroup_status](#gp_resgroup_status)
-   [gp_resgroup_status_per_host](#gp_resgroup_status_per_host)
-   [gp_resqueue_status](#gp_resqueue_status)
-   [gp_segment_endpoints](#gp_segment_endpoints)
-   [gp_session_endpoints](#gp_session_endpoints)
-   [gp_transaction_log](#gp_transaction_log)
-   [gpexpand.expansion_progress](#gpexpandexpansion_progress)
-   [gp_stat_activity](#gp_stat_activity)
-   gp_st_all_indexes
-   gp_stat_all_tables
-   gp_stat_archiver
-   gp_stat_bgwriter
-   gp_stat_database
-   gp_stat_database_conflicts
-   gp_stat_gssapi
-   gp_stat_operations
-   [gp_stat_replication](#gp_stat_replication)
-   gp_stat_resqueues
-   gp_stat_slru
-   gp_stat_ssl
-   gp_stat_subscription
-   gp_stat_sys_indexes
-   gp_stat_sys_tablest
-   gp_stat_user_functions
-   gp_stat_user_indexes
-   gp_stat_user_tables
-   gp_stat_wal_receiver
-   gp_stat_xact_all_tables
-   gp_stat_xact_sys_tables
-   gp_stat_xact_user_functions
-   gp_stat_xact_user_tables
-   gp_stat_wal
-   gp_stat_writer
-   gp_statio_all_indexes
-   gp_statio_all_sequences
-   gp_statio_all_tables
-   gp_statio_sys_indexes
-   gp_statio_sys_sequences
-   gp_statio_sys_tables
-   gp_statio_user_indexes
-   gp_statio_user_sequences
-   gp_statio_user_tables
-   gp-stats
-   gp_stats_ext
-   [pg_backend_memory_contexts](#pg_backend_memory_contexts)
-   [pg_cursors](#pg_cursors)
-   [pg_matviews](#pg_matviews)
-   [pg_max_external_files](#pg_max_external_files)
-   [pg_partitions](#pg_partitions)
-   [pg_policies](#pg_policies)
-   [pg_resqueue_attributes](#pg_resqueue_attributes)
-   pg_resqueue_status (Deprecated. Use [gp_toolkit.gp_resqueue_status](../gp_toolkit.html).)
-   [pg_stat_activity](#pg_stat_activity)
-   [pg_stat_all_indexes](#pg_stat_all_indexes)
-   [pg_stat_all_tables](#pg_stat_all_tables)
-   [pg_stat_replication](#pg_stat_replication)
-   [pg_stat_resqueues](#pg_stat_resqueues)
-   [pg_stat_slru](#pg_stat_slru)
-   [pg_stat_wal](#pg_stat_wal)
-   [pg_stats](#pg-stats)
-   [pg_stats_ext](#pg_stats_ext)
-   session_level_memory_consumption (See [Monitoring a Greenplum System](../../admin_guide/managing/monitor.html#topic_slt_ddv_1q).)

For more information about the standard system views supported in PostgreSQL and Greenplum Database, see the following sections of the PostgreSQL documentation:

-   [System Views](https://www.postgresql.org/docs/12/views-overview.html)
-   [Statistics Collector Views](https://www.postgresql.org/docs/12/monitoring-stats.html#MONITORING-STATS-VIEWS)
-   [The Information Schema](https://www.postgresql.org/docs/12/information-schema.html)

## <a id="gp_distributed_log"></a>gp_distributed_log 

The `gp_distributed_log` view contains status information about distributed transactions and their associated local transactions. A distributed transaction is a transaction that involves modifying data on the segment instances. Greenplum's distributed transaction manager ensures that the segments stay in synch. This view allows you to see the status of distributed transactions.

|column|type|references|description|
|------|----|----------|-----------|
|`segment_id`|smallint|gp\_segment\_configuration.content|The content id of the segment. The coordinator is always -1 \(no content\).|
|`dbid`|smallint|gp\_segment\_configuration.dbid|The unique id of the segment instance.|
|`distributed_xid`|xid| |The global transaction id.|
|`distributed_id`|text| |A system assigned ID for a distributed transaction.|
|`status`|text| |The status of the distributed transaction \(Committed or Aborted\).|
|`local_transaction`|xid| |The local transaction ID.|

## <a id="gp_distributed_xacts"></a>gp_distributed_xacts 

The `gp_distributed_xacts` view contains information about Greenplum Database distributed transactions. A distributed transaction is a transaction that involves modifying data on the segment instances. Greenplum's distributed transaction manager ensures that the segments stay in synch. This view allows you to see the currently active sessions and their associated distributed transactions.

|column|type|references|description|
|------|----|----------|-----------|
|`distributed_xid`|xid| |The transaction ID used by the distributed transaction across the Greenplum Database array.|
|`distributed_id`|text| |The distributed transaction identifier. It has 2 parts — a unique timestamp and the distributed transaction number.|
|`state`|text| |The current state of this session with regards to distributed transactions.|
|`gp_session_id`|int| |The ID number of the Greenplum Database session associated with this transaction.|
|`xmin_distributed _snapshot`|xid| |The minimum distributed transaction number found among all open transactions when this transaction was started. It is used for MVCC distributed snapshot purposes.|

## <a id="gp_endpoints"></a>gp_endpoints 

The `gp_endpoints` view lists the endpoints created for all active parallel retrieve cursors declared by the current session user in the current database. When the Greenplum Database superuser accesses this view, it returns a list of all endpoints created for all parallel retrieve cursors declared by all users in the current database.

Endpoints exist only for the duration of the transaction that defines the parallel retrieve cursor, or until the cursor is closed.

|name|type|references|description|
|----|----|----------|-----------|
|gp\_segment\_id|integer| |The QE's endpoint `gp_segment_id`.|
|auth\_token|text| |The authentication token for a retrieve session.|
|cursorname|text| |The name of the parallel retrieve cursor.|
|sessionid|integer| |The identifier of the session in which the parallel retrieve cursor was created.|
|hostname|varchar\(64\)| |The name of the host from which to retrieve the data for the endpoint.|
|port|integer| |The port number from which to retrieve the data for the endpoint.|
|username|text| |The name of the session user \(not the current user\); *you must initiate the retrieve session as this user*.|
|state|text| |The state of the endpoint; the valid states are:<br/><br/>READY: The endpoint is ready to be retrieved.<br/><br/>ATTACHED: The endpoint is attached to a retrieve connection.<br/><br/>RETRIEVING: A retrieve session is retrieving data from the endpoint at this moment.<br/><br/>FINISHED: The endpoint has been fully retrieved.<br/><br/>RELEASED: Due to an error, the endpoint has been released and the connection closed.|
|endpointname|text| |The endpoint identifier; you provide this identifier to the `RETRIEVE` command.|

## <a id="gp_pgdatabase"></a>gp_pgdatabase

The `gp_pgdatabase` view displays the status of Greenplum segment instances and whether they are acting as the mirror or the primary. The Greenplum fault detection and recovery utilities use this view internally to identify failed segments.

|column|type|references|description|
|------|----|----------|-----------|
|`dbid`|smallint|gp\_segment\_configuration.dbid|System-assigned ID. The unique identifier of a segment \(or coordinator\) instance.|
|`isprimary`|boolean|gp\_segment\_configuration.role|Whether or not this instance is active. Is it currently acting as the primary segment \(as opposed to the mirror\).|
|`content`|smallint|gp\_segment\_configuration.content|The ID for the portion of data on an instance. A primary segment instance and its mirror will have the same content ID.<br/><br/>For a segment the value is from 0-*N-1*, where *N* is the number of segments in Greenplum Database.<br/><br/>For the coordinator, the value is -1.|
|`valid`|boolean|gp\_segment\_configuration.mode|Whether or not this instance is up and the mode is either *s* (synchronized) or *n* (not in sync).|
|`definedprimary`|boolean|gp\_segment\_ configuration.preferred\_role|Whether or not this instance was defined as the primary \(as opposed to the mirror\) at the time the system was initialized.|

## <a id="gp_resgroup_config"></a>gp_resgroup_config

The `gp_toolkit.gp_resgroup_config` view allows administrators to see the current CPU, memory, and concurrency limits for a resource group.

> **Note** The `gp_resgroup_config` view is valid only when resource group-based resource management is active.

|column|type|references|description|
|------|----|----------|-----------|
|`groupid`|oid|pg\_resgroup.oid|The ID of the resource group.|
|`groupname`|name|pg\_resgroup.rsgname|The name of the resource group.|
|`concurrency`|text|pg\_resgroupcapability.value for pg\_resgroupcapability.reslimittype = 1|The concurrency \(`CONCURRENCY`\) value specified for the resource group.|
|`cpu_hard_quota_limit`|text|pg\_resgroupcapability.value for pg\_resgroupcapability.reslimittype = 2|The CPU limit \(`cpu_hard_quota_limit`\) value specified for the resource group, or -1.|
|`memory_limit`|text|pg\_resgroupcapability.value for pg\_resgroupcapability.reslimittype = 3|The memory limit \(`MEMORY_LIMIT`\) value specified for the resource group.|
|`memory_shared_quota`|text|pg\_resgroupcapability.value for pg\_resgroupcapability.reslimittype = 4|The shared memory quota \(`MEMORY_SHARED_QUOTA`\) value specified for the resource group.|
|`memory_spill_ratio`|text|pg\_resgroupcapability.value for pg\_resgroupcapability.reslimittype = 5|The memory spill ratio \(`MEMORY_SPILL_RATIO`\) value specified for the resource group.|
|`memory_auditor`|text|pg\_resgroupcapability.value for pg\_resgroupcapability.reslimittype = 6|The memory auditor in use for the resource group.|
|`cpuset`|text|pg\_resgroupcapability.value for pg\_resgroupcapability.reslimittype = 7|The CPU cores reserved for the resource group, or -1.|

## <a id="gp_resgroup_status"></a>gp_resgroup_status

The `gp_toolkit.gp_resgroup_status` view allows administrators to see status and activity for a resource group. It shows how many queries are waiting to run and how many queries are currently active in the system for each resource group. The view also displays current memory and CPU usage for the resource group.

> **Note** The `gp_resgroup_status` view is valid only when resource group-based resource management is active.

|column|type|references|description|
|------|----|----------|-----------|
|`rsgname`|name|pg\_resgroup.rsgname|The name of the resource group.|
|`groupid`|oid|pg\_resgroup.oid|The ID of the resource group.|
|`num_running`|integer| |The number of transactions currently running in the resource group.|
|`num_queueing`|integer| |The number of currently queued transactions for the resource group.|
|`num_queued`|integer| |The total number of queued transactions for the resource group since the Greenplum Database cluster was last started, excluding the `num_queueing`.|
|`num_executed`|integer| |The total number of transactions run in the resource group since the Greenplum Database cluster was last started, excluding the `num_running`.|
|`total_queue_duration`|interval| |The total time any transaction was queued since the Greenplum Database cluster was last started.|
|`cpu_usage`|json| |A set of key-value pairs. For each segment instance \(the key\), the value is the real-time, per-segment instance CPU core usage by a resource group. The value is the sum of the percentages \(as a decimal value\) of CPU cores that are used by the resource group for the segment instance.|
|`memory_usage`|json| |The real-time memory usage of the resource group on each Greenplum Database segment's host.|

The `memory_usage` field is a JSON-formatted, key:value string. The string contents differ depending upon the type of resource group. For each resource group that you assign to a role \(default memory auditor `vmtracker`\), this string identifies the used and available fixed and shared memory quota allocations on each segment. The key is segment id. The values are memory values displayed in MB units. The following example shows `memory_usage` column output for a single segment for a resource group that you assign to a role:

```
"0":{"used":0, "available":76, "quota_used":-1, "quota_available":60, "shared_used":0, "shared_available":16}
```

For each resource group that you assign to an external component, the `memory_usage` JSON-formatted string identifies the memory used and the memory limit on each segment. The following example shows `memory_usage` column output for an external component resource group for a single segment:

```
"1":{"used":11, "limit_granted":15}
```

## <a id="gp_resgroup_status_per_host"></a>gp_resgroup_status_per_host

The `gp_toolkit.gp_resgroup_status_per_host` view allows administrators to see current memory and CPU usage and allocation for each resource group on a per-host basis.

Memory amounts are specified in MBs.

> **Note** The `gp_resgroup_status_per_host` view is valid only when resource group-based resource management is active.

|column|type|references|description|
|------|----|----------|-----------|
|`rsgname`|name|pg\_resgroup.rsgname|The name of the resource group.|
|`groupid`|oid|pg\_resgroup.oid|The ID of the resource group.|
|`hostname`|text|gp\_segment\_configuration.hostname|The hostname of the segment host.|
|`cpu`|numeric| |The real-time CPU core usage by the resource group on a host. The value is the sum of the percentages \(as a decimal value\) of the CPU cores that are used by the resource group on the host.|
|`cpu_usage`|json| |A set of key-value pairs. For each segment instance \(the key\), the value is the real-time, per-segment instance CPU core usage by a resource group. The value is the sum of the percentages \(as a decimal value\) of CPU cores that are used by the resource group for the segment instance.|
|`memory_usage`|json| |The real-time memory usage of the resource group on each Greenplum Database segment's host.|
|`memory_used`|integer| |The real-time memory usage of the resource group on the host. This total includes resource group fixed and shared memory. It also includes global shared memory used by the resource group.|
|`memory_available`|integer| |The unused fixed and shared memory for the resource group that is available on the host. This total does not include available resource group global shared memory.|
|`memory_quota_used`|integer| |The real-time fixed memory usage for the resource group on the host.|
|`memory_quota_available`|integer| |The fixed memory available to the resource group on the host.|
|`memory_shared_used`|integer| |The group shared memory used by the resource group on the host. If any global shared memory is used by the resource group, this amount is included in the total as well.|
|`memory_shared_available`|integer| |The amount of group shared memory available to the resource group on the host. Resource group global shared memory is not included in this total.|

The `cpu_usage` field is a JSON-formatted, key:value string that identifies, for each resource group, the per-segment instance CPU core usage. The key is the segment id. The value is the sum of the percentages \(as a decimal value\) of the CPU cores used by the segment instance's resource group on the segment host; the maximum value is 1.00. The total CPU usage of all segment instances running on a host should not exceed the `gp_resource_group_cpu_limit`. Example `cpu_usage` column output:

```
{"-1":0.01, "0":0.31, "1":0.31}
```

In the example, segment `0` and segment `1` are running on the same host; their CPU usage is the same.

The `memory_usage` field is also a JSON-formatted, key:value string. The string contents differ depending upon the type of resource group. For each resource group that you assign to a role \(default memory auditor `vmtracker`\), this string identifies the used and available fixed and shared memory quota allocations on each segment. The key is segment id. The values are memory values displayed in MB units. The following example shows `memory_usage` column output for a single segment for a resource group that you assign to a role:

```
"0":{"used":0, "available":76, "quota_used":-1, "quota_available":60, "shared_used":0, "shared_available":16}
```

For each resource group that you assign to an external component, the `memory_usage` JSON-formatted string identifies the memory used and the memory limit on each segment. The following example shows `memory_usage` column output for an external component resource group for a single segment:

```
"1":{"used":11, "limit_granted":15}
```

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

|name|type|references|description|
|----|----|----------|-----------|
|auth\_token|text| |The authentication token for the retrieve session.|
|databaseid|oid| |The identifier of the database in which the parallel retrieve cursor was created.|
|senderpid|integer| |The identifier of the process sending the query results.|
|receiverpid|integer| |The process identifier of the retrieve session that is receiving the query results.|
|state|text| |The state of the endpoint; the valid states are:<br/><br/>READY: The endpoint is ready to be retrieved.<br/><br/>ATTACHED: The endpoint is attached to a retrieve connection.<br/><br/>RETRIEVING: A retrieve session is retrieving data from the endpoint at this moment.<br/><br/>FINISHED: The endpoint has been fully retrieved.<br/><br/>RELEASED: Due to an error, the endpoint has been released and the connection closed.|
|gp\_segment\_id|integer| |The QE's endpoint `gp_segment_id`.|
|sessionid|integer| |The identifier of the session in which the parallel retrieve cursor was created.|
|username|text| |The name of the session user \(not the current user\); *you must initiate the retrieve session as this user*.|
|endpointname|text| |The endpoint identifier; you provide this identifier to the `RETRIEVE` command.|
|cursorname|text| |The name of the parallel retrieve cursor.|

## <a id="gp_session_endpoints"></a>gp_session_endpoints

The `gp_session_endpoints` view lists the endpoints created for all active parallel retrieve cursors declared by the current session user in the current session.

Endpoints exist only for the duration of the transaction that defines the parallel retrieve cursor, or until the cursor is closed.

|name|type|references|description|
|----|----|----------|-----------|
|gp\_segment\_id|integer| |The QE's endpoint `gp_segment_id`.|
|auth\_token|text| |The authentication token for a retrieve session.|
|cursorname|text| |The name of the parallel retrieve cursor.|
|sessionid|integer| |The identifier of the session in which the parallel retrieve cursor was created.|
|hostname|varchar\(64\)| |The name of the host from which to retrieve the data for the endpoint.|
|port|integer| |The port number from which to retrieve the data for the endpoint.|
|username|text| |The name of the session user \(not the current user\); *you must initiate the retrieve session as this user*.|
|state|text| |The state of the endpoint; the valid states are:<br/><br/>READY: The endpoint is ready to be retrieved.<br/><br/>ATTACHED: The endpoint is attached to a retrieve connection.<br/><br/>RETRIEVING: A retrieve session is retrieving data from the endpoint at this moment.<br/><br/>FINISHED: The endpoint has been fully retrieved.<br/><br/>RELEASED: Due to an error, the endpoint has been released and the connection closed.|
|endpointname|text| |The endpoint identifier; you provide this identifier to the `RETRIEVE` command.|

## <a id="gp_transaction_log"></a>gp_transaction_log

The `gp_transaction_log` view contains status information about transactions local to a particular segment. This view allows you to see the status of local transactions.

|column|type|references|description|
|------|----|----------|-----------|
|`segment_id`|smallint|gp\_segment\_configuration.content|The content id of the segment. The coordinator is always -1 \(no content\).|
|`dbid`|smallint|gp\_segment\_configuration.dbid|The unique id of the segment instance.|
|`transaction`|xid| |The local transaction ID.|
|`status`|text| |The status of the local transaction \(Committed or Aborted\).|

## <a id="gpexpandexpansion_progress"></a>gpexpand.expansion_progress

The `gpexpand.expansion_progress` view contains information about the status of a system expansion operation. The view provides calculations of the estimated rate of table redistribution and estimated time to completion.

Status for specific tables involved in the expansion is stored in [gpexpand.status\_detail](gp_expansion_tables.html).

|column|type|references|description|
|------|----|----------|-----------|
|`name`|text| |Name for the data field provided. Includes:<br/><br/>Bytes Left<br/><br/>Bytes Done<br/><br/>Estimated Expansion Rate<br/><br/>Estimated Time to Completion<br/><br/>Tables Expanded<br/><br/>Tables Left|
|`value`|text| |The value for the progress data. For example: `Estimated Expansion Rate - 9.75667095996092 MB/s`|

## <a id="gp_stat_activity"></a>gp_stat_activity

The `gp_stat_activity` view is a cluster-wide view that displays the [`pg_stat_activity` ](#pg_stat_activity) information from every primary segment. 

<div class="table" id="PG-STAT-ACTIVITY-VIEW">
      <p class="title"><strong>Table&nbsp;28.3.&nbsp;<code class="structname">pg_stat_activity</code> View</strong></p>
      <div class="table-contents">
        <table class="table" summary="pg_stat_activity View" border="1">
          <colgroup>
            <col>
          </colgroup>
          <thead>
            <tr>
              <th class="catalog_table_entry">
                <p class="column_definition">Column Type</p>
                <p>Description</p>
              </th>
            </tr>
          </thead>
          <tbody>
            <tr>
              <td class="catalog_table_entry">
                <p class="column_definition"><code class="structfield">datid</code> <code class="type">oid</code></p>
                <p>OID of the database this backend is connected to</p>
              </td>
            </tr>
            <tr>
              <td class="catalog_table_entry">
                <p class="column_definition"><code class="structfield">datname</code> <code class="type">name</code></p>
                <p>Name of the database this backend is connected to</p>
              </td>
            </tr>
            <tr>
              <td class="catalog_table_entry">
                <p class="column_definition"><code class="structfield">pid</code> <code class="type">integer</code></p>
                <p>Process ID of this backend</p>
              </td>
            </tr>
            <tr>
              <td class="catalog_table_entry">
                <p class="column_definition"><code class="structfield">leader_pid</code> <code class="type">integer</code></p>
                <p>Process ID of the parallel group leader, if this process is a parallel query worker. <code class="literal">NULL</code> if this process is a parallel group leader or does not participate in parallel query.</p>
              </td>
            </tr>
            <tr>
              <td class="catalog_table_entry">
                <p class="column_definition"><code class="structfield">usesysid</code> <code class="type">oid</code></p>
                <p>OID of the user logged into this backend</p>
              </td>
            </tr>
            <tr>
              <td class="catalog_table_entry">
                <p class="column_definition"><code class="structfield">usename</code> <code class="type">name</code></p>
                <p>Name of the user logged into this backend</p>
              </td>
            </tr>
            <tr>
              <td class="catalog_table_entry">
                <p class="column_definition"><code class="structfield">application_name</code> <code class="type">text</code></p>
                <p>Name of the application that is connected to this backend</p>
              </td>
            </tr>
            <tr>
              <td class="catalog_table_entry">
                <p class="column_definition"><code class="structfield">client_addr</code> <code class="type">inet</code></p>
                <p>IP address of the client connected to this backend. If this field is null, it indicates either that the client is connected via a Unix socket on the server machine or that this is an internal process such as autovacuum.</p>
              </td>
            </tr>
            <tr>
              <td class="catalog_table_entry">
                <p class="column_definition"><code class="structfield">client_hostname</code> <code class="type">text</code></p>
                <p>Host name of the connected client, as reported by a reverse DNS lookup of <code class="structfield">client_addr</code>. This field will only be non-null for IP connections, and only when <a class="xref" href="../../security-guide/topics/Auditing.html">log_hostname</a> is enabled.</p>
              </td>
            </tr>
            <tr>
              <td class="catalog_table_entry">
                <p class="column_definition"><code class="structfield">client_port</code> <code class="type">integer</code></p>
                <p>TCP port number that the client is using for communication with this backend, or <code class="literal">-1</code> if a Unix socket is used. If this field is null, it indicates that this is an internal server process.</p>
              </td>
            </tr>
            <tr>
              <td class="catalog_table_entry">
                <p class="column_definition"><code class="structfield">backend_start</code> <code class="type">timestamp with time zone</code></p>
                <p>Time when this process was started. For client backends, this is the time the client connected to the server.</p>
              </td>
            </tr>
            <tr>
              <td class="catalog_table_entry">
                <p class="column_definition"><code class="structfield">xact_start</code> <code class="type">timestamp with time zone</code></p>
                <p>Time when this process' current transaction was started, or null if no transaction is active. If the current query is the first of its transaction, this column is equal to the <code class="structfield">query_start</code> column.</p>
              </td>
            </tr>
            <tr>
              <td class="catalog_table_entry">
                <p class="column_definition"><code class="structfield">query_start</code> <code class="type">timestamp with time zone</code></p>
                <p>Time when the currently active query was started, or if <code class="structfield">state</code> is not <code class="literal">active</code>, when the last query was started</p>
              </td>
            </tr>
            <tr>
              <td class="catalog_table_entry">
                <p class="column_definition"><code class="structfield">state_change</code> <code class="type">timestamp with time zone</code></p>
                <p>Time when the <code class="structfield">state</code> was last changed</p>
              </td>
            </tr>
            <tr>
              <td class="catalog_table_entry">
                <p class="column_definition"><code class="structfield">wait_event_type</code> <code class="type">text</code></p>
                <p>The type of event for which the backend is waiting, if any; otherwise NULL. See <a class="xref" href="https://www.postgresql.org/docs/12/monitoring-stats.html#WAIT-EVENT-TABLE" title="Table&nbsp;28.4.&nbsp;Wait Event Types">Table&nbsp;28.4</a>.</p>
              </td>
            </tr>
            <tr>
              <td class="catalog_table_entry">
                <p class="column_definition"><code class="structfield">wait_event</code> <code class="type">text</code></p>
                <p>Wait event name if backend is currently waiting, otherwise NULL. See <a class="xref" href="https://www.postgresql.org/docs/12/monitoring-stats.html#WAIT-EVENT-ACTIVITY-TABLE" title="Table&nbsp;28.5.&nbsp;Wait Events of Type Activity">Table&nbsp;28.5</a> through <a class="xref" href="https://www.postgresql.org/docs/12/monitoring-stats.html#WAIT-EVENT-TIMEOUT-TABLE" title="Table&nbsp;28.13.&nbsp;Wait Events of Type Timeout">Table&nbsp;28.13</a>.</p>
              </td>
            </tr>
            <tr>
              <td class="catalog_table_entry">
                <p class="column_definition"><code class="structfield">state</code> <code class="type">text</code></p>
                <p>Current overall state of this backend. Possible values are:</p>
                <div class="itemizedlist">
                  <ul class="itemizedlist" style="list-style-type: disc;">
                    <li class="listitem">
                      <p><code class="literal">active</code>: The backend is executing a query.</p>
                    </li>
                    <li class="listitem">
                      <p><code class="literal">idle</code>: The backend is waiting for a new client command.</p>
                    </li>
                    <li class="listitem">
                      <p><code class="literal">idle in transaction</code>: The backend is in a transaction, but is not currently executing a query.</p>
                    </li>
                    <li class="listitem">
                      <p><code class="literal">idle in transaction (aborted)</code>: This state is similar to <code class="literal">idle in transaction</code>, except one of the statements in the transaction caused an error.</p>
                    </li>
                    <li class="listitem">
                      <p><code class="literal">fastpath function call</code>: The backend is executing a fast-path function.</p>
                    </li>
                    <li class="listitem">
                      <p><code class="literal">disabled</code>: This state is reported if <a class="xref" href="#pg_stat_activity">track_activities</a> is disabled in this backend.</p>
                    </li>
                  </ul>
                </div>
              </td>
            </tr>
            <tr>
              <td class="catalog_table_entry">
                <p class="column_definition"><code class="structfield">backend_xid</code> <code class="type">xid</code></p>
                <p>Top-level transaction identifier of this backend, if any.</p>
              </td>
            </tr>
            <tr>
              <td class="catalog_table_entry">
                <p class="column_definition"><code class="structfield">backend_xmin</code> <code class="type">xid</code></p>
                <p>The current backend's <code class="literal">xmin</code> horizon.</p>
              </td>
            </tr>
            <tr>
              <td class="catalog_table_entry">
                <p class="column_definition"><code class="structfield">query_id</code> <code class="type">bigint</code></p>
                <p>Identifier of this backend's most recent query. If <code class="structfield">state</code> is <code class="literal">active</code> this field shows the identifier of the currently executing query. In all other states, it shows the identifier of last query that was executed. Query identifiers are not computed by default so this field will be null unless a third-party module that computes query identifiers is configured.</p>
              </td>
            </tr>
            <tr>
              <td class="catalog_table_entry">
                <p class="column_definition"><code class="structfield">query</code> <code class="type">text</code></p>
                <p>Text of this backend's most recent query. If <code class="structfield">state</code> is <code class="literal">active</code> this field shows the currently executing query. In all other states, it shows the last query that was executed. By default the query text is truncated at 1024 bytes; this value can be changed via the parameter <a class="xref" href="#pg_stat_activity">track_activity_query_size</a>.</p>
              </td>
            </tr>
            <tr>
              <td class="catalog_table_entry">
                <p class="column_definition"><code class="structfield">backend_type</code> <code class="type">text</code></p>
                <p>Type of current backend. Possible types are <code class="literal">autovacuum launcher</code>, <code class="literal">autovacuum worker</code>, <code class="literal">logical replication launcher</code>, <code class="literal">logical replication worker</code>, <code class="literal">parallel worker</code>, <code class="literal">background writer</code>, <code class="literal">client backend</code>, <code class="literal">checkpointer</code>, <code class="literal">archiver</code>, <code class="literal">startup</code>, <code class="literal">walreceiver</code>, <code class="literal">walsender</code> and <code class="literal">walwriter</code>. In addition, background workers registered by extensions may have additional types.</p>
              </td>
            </tr>
            <tr>
              <td class="catalog_table_entry">
                <p class="column_definition"><code class="structfield">gp_segment_id</code> <code class="type">bigint</code></p>
                <p>Unique identifier of a segment (or coordinator) instance.</p>
              </td>
            </tr>
          </tbody>
        </table>
      </div>
    </div>

## <a id="gp_stat_replication"></a>gp_stat_replication

The `gp_stat_replication` view contains replication statistics of the `walsender` process that is used for Greenplum Database Write-Ahead Logging \(WAL\) replication when coordinator or segment mirroring is enabled.

|column|type|references|description|
|------|----|----------|-----------|
|`gp_segment_id`|integer| |Unique identifier of a segment \(or coordinator\) instance.|
|`pid`|integer| |Process ID of the `walsender` backend process.|
|`usesysid`|oid| |User system ID that runs the `walsender` backend process.|
|`usename`|name| |User name that runs the `walsender` backend process.|
|`application_name`|text| |Client application name.|
|`client_addr`|inet| |Client IP address.|
|`client_hostname`|text| |Client host name.|
|`client_port`|integer| |Client port number.|
|`backend_start`|timestamp| |Operation start timestamp.|
|`backend_xmin`|xid| |The current backend's `xmin` horizon.|
|`state`|text| |`walsender` state. The value can be:<br/><br/>`startup`<br/><br/>`backup`<br/><br/>`catchup`<br/><br/>`streaming`|
|`sent_location`|text| |`walsender` xlog record sent location.|
|`write_location`|text| |`walreceiver` xlog record write location.|
|`flush_location`|text| |`walreceiver` xlog record flush location.|
|`replay_location`|text| |Coordinator standby or segment mirror xlog record replay location.|
|`sync_priority`|integer| |Priority. The value is `1`.|
|`sync_state`|text| |`walsender`synchronization state. The value is `sync`.|
|`sync_error`|text| |`walsender` synchronization error. `none` if no error.|

## <a id="pg_backend_memory_contexts"></a>pg_backend_memory_contexts

The `pg_backend_memory_contexts` system view displays all of the memory contexts in use by the server process attached to the current session.

`pg_backend_memory_contexts` contains one row for each memory context.

|column|type|description|
|------|----|-----------|
|`name`|text| The name of the memory context.|
|`ident`|text| Identification information of the memory context. This field is truncated at 1024 bytes.|
|`parent`|text| The name of the parent of this memory context.|
|`level`|int4| The distance from `TopMemoryContext` in context tree.|
|`total_bytes`|int8| The total number of bytes allocated for this memory context.|
|`total_nblocks`|int8| The total number of blocks allocated for this memory context.|
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
|`name`|text| |The name of the cursor.|
|`statement`|text| |The verbatim query string submitted to declare this cursor.|
|`is_holdable`|boolean| |`true` if the cursor is holdable \(that is, it can be accessed after the transaction that declared the cursor has committed\); `false` otherwise.<br/><br/>> **Note** Greenplum Database does not support holdable parallel retrieve cursors, this value is always `false` for such cursors.|
|`is_binary`|boolean| |`true` if the cursor was declared `BINARY`; `false` otherwise.|
|`is_scrollable`|boolean| |`true` if the cursor is scrollable \(that is, it allows rows to be retrieved in a nonsequential manner\); `false` otherwise.<br/><br/>> **Note** Greenplum Database does not support scrollable cursors, this value is always `false`.|
|`creation_time`|timestamptz| |The time at which the cursor was declared.|
|`is_parallel`|boolean| |`true` if the cursor was declared `PARALLEL RETRIEVE`; `false` otherwise.|

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
|`hostname`|name| |The host name used to access a particular segment instance on a segment host.|
|`maxfiles`|bigint| |Number of primary segment instances on the host.|

## <a id="pg_partitions"></a>pg_partitions

The `pg_partitions` system view is used to show the structure of a partitioned table.

|column|type|references|description|
|------|----|----------|-----------|
|`schemaname`|name| |The name of the schema the partitioned table is in.|
|`tablename`|name| |The name of the top-level parent table.|
|`partitionschemaname`|name| |The namespace of the partition table.|
|`partitiontablename`|name| |The relation name of the partitioned table \(this is the table name to use if accessing the partition directly\).|
|`partitionname`|name| |The name of the partition \(this is the name to use if referring to the partition in an `ALTER TABLE` command\). `NULL` if the partition was not given a name at create time or generated by an `EVERY` clause.|
|`parentpartitiontablename`|name| |The relation name of the parent table one level up from this partition.|
|`parentpartitionname`|name| |The given name of the parent table one level up from this partition.|
|`partitiontype`|text| |The type of partition \(range or list\).|
|`partitionlevel`|smallint| |The level of this partition in the hierarchy.|
|`partitionrank`|bigint| |For range partitions, the rank of the partition compared to other partitions of the same level.|
|`partitionposition`|smallint| |The rule order position of this partition.|
|`partitionlistvalues`|text| |For list partitions, the list value\(s\) associated with this partition.|
|`partitionrangestart`|text| |For range partitions, the start value of this partition.|
|`partitionstartinclusive`|boolean| |`T` if the start value is included in this partition. `F` if it is excluded.|
|`partitionrangeend`|text| |For range partitions, the end value of this partition.|
|`partitionendinclusive`|boolean| |`T` if the end value is included in this partition. `F` if it is excluded.|
|`partitioneveryclause`|text| |The `EVERY` clause \(interval\) of this partition.|
|`partitionisdefault`|boolean| |`T` if this is a default partition, otherwise `F`.|
|`partitionboundary`|text| |The entire partition specification for this partition.|
|`parenttablespace`|text| |The tablespace of the parent table one level up from this partition.|
|`partitiontablespace`|text| |The tablespace of this partition.|

## <a id="pg_policies"></a>pg_policies

The `pg_policies` view provides access to information about each row-level security policy in the database.

|Column Name|Type|References|Description|
|------|----|----------|-----------|
|`schemaname`|name|pg\_namespace.nspname |The name of schema that contas the table the policy is on|
|`tablename`|name|pg\_class.relname|The name of the table the policy is on|
|`policyname`|name|pg\_policy.polname |The name of policy|
|`polpermissive`|text| |Is the policy permissive or restrictive?|
|`roles`|name[]| |The roles to which this policy applies|
|`cmd`|text| |The command type to which the policy is applied|
|`qual`|text| |The expression added to the security barrier qualifications for queries to which this policy applies|
|`with_check`|text| |The expression added to the `WITH CHECK` qualifications for queries that attempt to add rows to this table|

> **Note**  Policies stored in `pg_policy` are applied only when `pg_class.relrowsecurity` is set for their table.

## <a id="pg_resqueue_attributes"></a>pg_resqueue_attributes

> **Note** The `pg_resqueue_attributes` view is valid only when resource queue-based resource management is active.

The `pg_resqueue_attributes` view allows administrators to see the attributes set for a resource queue, such as its active statement limit, query cost limits, and priority.

|column|type|references|description|
|------|----|----------|-----------|
|`rsqname`|name|pg\_resqueue.rsqname|The name of the resource queue.|
|`resname`|text| |The name of the resource queue attribute.|
|`resetting`|text| |The current value of a resource queue attribute.|
|`restypid`|integer| |System assigned resource type id.|

## <a id="pg_stat_activity"></a>pg_stat_activity

The `pg_stat_activity` view shows one row per server process with details about the associated user session and query. The columns that report data on the current query are available unless the parameter `stats_command_string` has been turned off. Furthermore, these columns are only visible if the user examining the view is a superuser or the same as the user owning the process being reported on.

The maximum length of the query text string stored in the column `query` can be controlled with the server configuration parameter `track_activity_query_size`.

|column|type|references|description|
|------|----|----------|-----------|
|`datid`|oid|pg\_database.oid|Database OID|
|`datname`|name| |Database name|
|`pid`|integer| |Process ID of this backend|
|`sess_id`|integer| |Session ID|
|`usesysid`|oid|pg\_authid.oid|OID of the user logged into this backend|
|`usename`|name| |Name of the user logged into this backend|
|`application_name`|text| |Name of the application that is connected to this backend|
|`client_addr`|inet| |IP address of the client connected to this backend. If this field is null, it indicates either that the client is connected via a Unix socket on the server machine or that this is an internal process such as autovacuum.|
|`client_hostname`|text| |Host name of the connected client, as reported by a reverse DNS lookup of `client_addr`. This field will only be non-null for IP connections, and only when log\_hostname is enabled.|
|`client_port`|integer| |TCP port number that the client is using for communication with this backend, or -1 if a Unix socket is used|
|`backend_start`|timestamptz| |Time backend process was started|
|`xact_start`|timestamptz| |Transaction start time|
|`query_start`|timestamptz| |Time query began execution|
|`state_change`|timestampz| |Time when the `state` was last changed|
|`wait_event_type`|text| |Type of event for which the backend is waiting|
|`wait_event`|text| |Wait event name if backend is currently waiting|
|`state`|text| |Current overall state of this backend. Possible values are:<br/><br/>-   `active`: The backend is running a query.<br/><br/>-   `idle`: The backend is waiting for a new client command.<br/><br/>-   `idle in transaction`: The backend is in a transaction, but is not currently running a query.<br/><br/>-   `idle in transaction (aborted)`: This state is similar to idle in transaction, except one of the statements in the transaction caused an error.<br/><br/>-   `fastpath function call`: The backend is running a fast-path function.<br/><br/>-   `disabled`: This state is reported if `track_activities` is deactivated in this backend.|
|`query`|text| |Text of this backend's most recent query. If `state` is active this field shows the currently running query. In all other states, it shows the last query that was run.|
|`rsgid`|oid|pg\_resgroup.oid|Resource group OID or `0`.<br/><br/>See [Note](#rsg_note).|
|`rsgname`|text|pg\_resgroup.rsgname|Resource group name or `unknown`.<br/><br/>See [Note](#rsg_note).|

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

## <a id="pg_stat_replication"></a>pg_stat_replication

The `pg_stat_replication` view contains metadata of the `walsender` process that is used for Greenplum Database coordinator mirroring.

|column|type|references|description|
|------|----|----------|-----------|
|`pid`|integer| |Process ID of WAL sender backend process.|
|`usesysid`|integer| |User system ID that runs the WAL sender backend process|
|`usename`|name| |User name that runs WAL sender backend process.|
|`application_name`|oid| |Client application name.|
|`client_addr`|name| |Client IP address.|
|`client_hostname`|text| |The host name of the client machine.|
|`client_port`|integer| |Client port number.|
|`backend_start`|timestamp| |Operation start timestamp.|
|`backend_xmin`|xid| |The current backend's `xmin` horizon.|
|`state`|text| |WAL sender state. The value can be:<br/><br/>`startup`<br/><br/>`backup`<br/><br/>`catchup`<br/><br/>`streaming`|
|`sent_location`|text| |WAL sender xlog record sent location.|
|`write_location`|text| |WAL receiver xlog record write location.|
|`flush_location`|text| |WAL receiver xlog record flush location.|
|`replay_location`|text| |Standby xlog record replay location.|
|`sync_priority`|text| |Priority. the value is `1`.|
|`sync_state`|text| |WAL sender synchronization state. The value is `sync`.|

## <a id="pg_stat_resqueues"></a>pg_stat_resqueues

> **Note** The `pg_stat_resqueues` view is valid only when resource queue-based resource management is active.

The `pg_stat_resqueues` view allows administrators to view metrics about a resource queue's workload over time. To allow statistics to be collected for this view, you must enable the `stats_queue_level` server configuration parameter on the Greenplum Database coordinator instance. Enabling the collection of these metrics does incur a small performance penalty, as each statement submitted through a resource queue must be logged in the system catalog tables.

|column|type|references|description|
|------|----|----------|-----------|
|`queueid`|oid| |The OID of the resource queue.|
|`queuename`|name| |The name of the resource queue.|
|`n_queries_exec`|bigint| |Number of queries submitted for execution from this resource queue.|
|`n_queries_wait`|bigint| |Number of queries submitted to this resource queue that had to wait before they could run.|
|`elapsed_exec`|bigint| |Total elapsed execution time for statements submitted through this resource queue.|
|`elapsed_wait`|bigint| |Total elapsed time that statements submitted through this resource queue had to wait before they were run.|

## <a id="pg_stat_slru"></a>pg_stat_slru

Greenplum Database accesses certain on-disk information via SLRU (simple least-recently-used) caches. The `pg_stat_slru` view contains one row for each tracked SLRU cache, showing statistics about access to cached pages.

|column|type|references|description|
|------|----|----------|-----------|
|`name`|text| |Name of the SLRU.|
|`blks_zeroed`|bigint| |Number of blocks zeroed during initializations.|
|`blks_hit`|bigint| |Number of times disk blocks were found already in the SLRU, so that a read was not necessary (this only includes hits in the SLR, not the operating system's file system cache).|
|`blks_read`|bigint| |Number of disk blocks read for this SLRU.|
|`blks_written`|biging| |Number of disk blocks written for this SLRU.|
|`blks_exists`|biging| |Number of blocks checked for existence for this SLRU.|
|`flushes`|bigint| |Number of flushes of dirty data for this SLRU.|
|`truncates`|bigint| |Number of truncates for this SLRU.|
|`stats_reset`|timestamp with time zone| |Time at which these statistics were last reset.|

## <a id="pg_stat_wal"></a>pg_stat_wal

The `pg_stat_wal` view shows data about the WAL activity of the cluster. It contains always a single row.

|column|type|references|description|
|------|----|----------|-----------|
|`wal_records`|bigint| |Total number of WAL records generated.|
|`wal_fpw`|bigint| |Total number of WAL full page images generated.|
|`wal_bytes`|numeric| |Total amount of WAL generated in bytes.|
|`wal_buffers_full`|bigint| |Number of times WAL data was written to disk because WAL buffers became full.|
|`wal_write`|bigint| |Number of times WAL buffers were written out to disk.|
|`wal_sync`|bigint||Number of times WAL files were synced to disk.|
|`wal_write_time`|double precision| |Total amount of time spent writing WAL buffers to disk, in milliseconds (if [track_wal_io_timing](../config_params/guc-list.html#track_wal_io_timing) is enabled, otherwise zero).|
|`wal_sync_time`|double precision| |Total amount of time spent syncing WAL files to disk, in milliseconds (if `track_wal_io_timing` is enabled, otherwise zero).|
|`stats_reset`|timestamp with time zone| |Time at which these statistics were last reset.|

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

**Parent topic:** [System Catalogs](../system_catalogs/catalog_ref.html)
