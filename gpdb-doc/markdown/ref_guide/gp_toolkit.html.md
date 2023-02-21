# The gp\_toolkit Administrative Schema 

Greenplum Database provides an administrative schema called `gp_toolkit` that you can use to query the system catalogs, log files, and operating environment for system status information. The `gp_toolkit` schema contains a number of views that you can access using SQL commands. The `gp_toolkit` schema is accessible to all database users, although some objects may require superuser permissions. For convenience, you may want to add the `gp_toolkit` schema to your schema search path. For example:

```
=> ALTER ROLE myrole SET search_path TO myschema,gp_toolkit;
```

This documentation describes the most useful views in `gp_toolkit`. You may notice other objects \(views, functions, and external tables\) within the `gp_toolkit` schema that are not described in this documentation \(these are supporting objects to the views described in this section\).

> **Caution** Do not change database objects in the gp\_toolkit schema. Do not create database objects in the schema. Changes to objects in the schema might affect the accuracy of administrative information returned by schema objects. Any changes made in the gp\_toolkit schema are lost when the database is backed up and then restored with the `gpbackup` and `gprestore` utilities.

These are the categories for views in the `gp_toolkit` schema.

-   **[Checking for Tables that Need Routine Maintenance](gp_toolkit.html)**  

-   **[Checking for Locks](gp_toolkit.html)**  

-   **[Checking Append-Optimized Tables](gp_toolkit.html)**  

-   **[Viewing Greenplum Database Server Log Files](gp_toolkit.html)**  

-   **[Checking Server Configuration Files](gp_toolkit.html)**  

-   **[Checking for Failed Segments](gp_toolkit.html)**  

-   **[Checking Resource Group Activity and Status](gp_toolkit.html)**  

-   **[Checking Resource Queue Activity and Status](gp_toolkit.html)**  

-   **[Checking Query Disk Spill Space Usage](gp_toolkit.html)**  

-   **[Viewing Users and Groups \(Roles\)](gp_toolkit.html)**  

-   **[Checking Database Object Sizes and Disk Space](gp_toolkit.html)**  

-   **[Checking for Uneven Data Distribution](gp_toolkit.html)**  


**Parent topic:** [Greenplum Database Reference Guide](ref_guide.html)

## <a id="topic2"></a>Checking for Tables that Need Routine Maintenance 

The following views can help identify tables that need routine table maintenance \(`VACUUM` and/or `ANALYZE`\).

-   [gp\_bloat\_diag](#topic3)
-   [gp\_stats\_missing](#topic4)

The `VACUUM` or `VACUUM FULL` command reclaims disk space occupied by deleted or obsolete rows. Because of the MVCC transaction concurrency model used in Greenplum Database, data rows that are deleted or updated still occupy physical space on disk even though they are not visible to any new transactions. Expired rows increase table size on disk and eventually slow down scans of the table.

The `ANALYZE` command collects column-level statistics needed by the query optimizer. Greenplum Database uses a cost-based query optimizer that relies on database statistics. Accurate statistics allow the query optimizer to better estimate selectivity and the number of rows retrieved by a query operation in order to choose the most efficient query plan.

**Parent topic:** [The gp\_toolkit Administrative Schema](gp_toolkit.html)

### <a id="topic3"></a>gp\_bloat\_diag 

This view shows regular heap-storage tables that have bloat \(the actual number of pages on disk exceeds the expected number of pages given the table statistics\). Tables that are bloated require a `VACUUM` or a `VACUUM FULL` in order to reclaim disk space occupied by deleted or obsolete rows. This view is accessible to all users, however non-superusers will only be able to see the tables that they have permission to access.

> **Note** For diagnostic functions that return append-optimized table information, see [Checking Append-Optimized Tables](#topic8).

|Column|Description|
|------|-----------|
|bdirelid|Table object id.|
|bdinspname|Schema name.|
|bdirelname|Table name.|
|bdirelpages|Actual number of pages on disk.|
|bdiexppages|Expected number of pages given the table data.|
|bdidiag|Bloat diagnostic message.|

### <a id="topic4"></a>gp\_stats\_missing 

This view shows tables that do not have statistics and therefore may require an `ANALYZE` be run on the table.

|Column|Description|
|------|-----------|
|smischema|Schema name.|
|smitable|Table name.|
|smisize|Does this table have statistics? False if the table does not have row count and row sizing statistics recorded in the system catalog, which may indicate that the table needs to be analyzed. This will also be false if the table does not contain any rows. For example, the parent tables of partitioned tables are always empty and will always return a false result.|
|smicols|Number of columns in the table.|
|smirecs|The total number of columns in the table that have statistics recorded.|

## <a id="topic5"></a>Checking for Locks 

When a transaction accesses a relation \(such as a table\), it acquires a lock. Depending on the type of lock acquired, subsequent transactions may have to wait before they can access the same relation. For more information on the types of locks, see "Managing Data" in the *Greenplum Database Administrator Guide*. Greenplum Database resource queues \(used for resource management\) also use locks to control the admission of queries into the system.

The `gp_locks_*` family of views can help diagnose queries and sessions that are waiting to access an object due to a lock.

-   [gp\_locks\_on\_relation](#topic6)
-   [gp\_locks\_on\_resqueue](#topic7)

**Parent topic:** [The gp\_toolkit Administrative Schema](gp_toolkit.html)

### <a id="topic6"></a>gp\_locks\_on\_relation 

This view shows any locks currently being held on a relation, and the associated session information about the query associated with the lock. For more information on the types of locks, see "Managing Data" in the *Greenplum Database Administrator Guide*. This view is accessible to all users, however non-superusers will only be able to see the locks for relations that they have permission to access.

|Column|Description|
|------|-----------|
|lorlocktype|Type of the lockable object: `relation`, `extend`, `page`, `tuple`, `transactionid`, `object`, `userlock`, `resource queue`, or `advisory`|
|lordatabase|Object ID of the database in which the object exists, zero if the object is a shared object.|
|lorrelname|The name of the relation.|
|lorrelation|The object ID of the relation.|
|lortransaction|The transaction ID that is affected by the lock.|
|lorpid|Process ID of the server process holding or awaiting this lock. NULL if the lock is held by a prepared transaction.|
|lormode|Name of the lock mode held or desired by this process.|
|lorgranted|Displays whether the lock is granted \(true\) or not granted \(false\).|
|lorcurrentquery|The current query in the session.|

### <a id="topic7"></a>gp\_locks\_on\_resqueue 

> **Note** The `gp_locks_on_resqueue` view is valid only when resource queue-based resource management is active.

This view shows any locks currently being held on a resource queue, and the associated session information about the query associated with the lock. This view is accessible to all users, however non-superusers will only be able to see the locks associated with their own sessions.

|Column|Description|
|------|-----------|
|lorusename|Name of the user running the session.|
|lorrsqname|The resource queue name.|
|lorlocktype|Type of the lockable object: `resource queue`|
|lorobjid|The ID of the locked transaction.|
|lortransaction|The ID of the transaction that is affected by the lock.|
|lorpid|The process ID of the transaction that is affected by the lock.|
|lormode|The name of the lock mode held or desired by this process.|
|lorgranted|Displays whether the lock is granted \(true\) or not granted \(false\).|
|lorwaiting|Displays whether or not the session is waiting.|

## <a id="topic8"></a>Checking Append-Optimized Tables 

The `gp_toolkit` schema includes a set of diagnostic functions you can use to investigate the state of append-optimized tables.

When an append-optimized table \(or column-oriented append-optimized table\) is created, another table is implicitly created, containing metadata about the current state of the table. The metadata includes information such as the number of records in each of the table's segments.

Append-optimized tables may have non-visible rows—rows that have been updated or deleted, but remain in storage until the table is compacted using `VACUUM`. The hidden rows are tracked using an auxiliary visibility map table, or visimap.

The following functions let you access the metadata for append-optimized and column-oriented tables and view non-visible rows.

For most of the functions, the input argument is `regclass`, either the table `name` or the `oid` of a table.

**Parent topic:** [The gp\_toolkit Administrative Schema](gp_toolkit.html)

### <a id="topic_ylp_gxw_yq"></a>\_\_gp\_aovisimap\_compaction\_info\(oid\) 

This function displays compaction information for an append-optimized table. The information is for the on-disk data files on Greenplum Database segments that store the table data. You can use the information to determine the data files that will be compacted by a `VACUUM` operation on an append-optimized table.

> **Note** Until a VACUUM operation deletes the row from the data file, deleted or updated data rows occupy physical space on disk even though they are hidden to new transactions. The configuration parameter [gp\_appendonly\_compaction](config_params/guc-list.html) controls the functionality of the `VACUUM` command.

This table describes the \_\_gp\_aovisimap\_compaction\_info function output table.

|Column|Description|
|------|-----------|
|content|Greenplum Database segment ID.|
|datafile|ID of the data file on the segment.|
|compaction\_possible|The value is either `t` or `f`. The value `t` indicates that the data in data file be compacted when a VACUUM operation is performed.<br/><br/>The server configuration parameter `gp_appendonly_compaction_threshold` affects this value.|
|hidden\_tupcount|In the data file, the number of hidden \(deleted or updated\) rows.|
|total\_tupcount|In the data file, the total number of rows.|
|percent\_hidden|In the data file, the ratio \(as a percentage\) of hidden \(deleted or updated\) rows to total rows.|

### <a id="topic9"></a>\_\_gp\_aoseg\(regclass\) 

This function returns metadata information contained in the append-optimized table's on-disk segment file.

The input argument is the name or the oid of an append-optimized table.

|Column|Description|
|------|-----------|
|segno|The file segment number.|
|eof|The effective end of file for this file segment.|
|tupcount|The total number of tuples in the segment, including invisible tuples.|
|varblockcount|The total number of varblocks in the file segment.|
|eof\_uncompressed|The end of file if the file segment were uncompressed.|
|modcount|The number of data modification operations.|
|state|The state of the file segment. Indicates if the segment is active or ready to be dropped after compaction.|

### <a id="topic10"></a>\_\_gp\_aoseg\_history\(regclass\) 

This function returns metadata information contained in the append-optimized table's on-disk segment file. It displays all different versions \(heap tuples\) of the aoseg meta information. The data is complex, but users with a deep understanding of the system may find it useful for debugging.

The input argument is the name or the oid of an append-optimized table.

|Column|Description|
|------|-----------|
|gp\_tid|The id of the tuple.|
|gp\_xmin|The id of the earliest transaction.|
|gp\_xmin\_status|Status of the gp\_xmin transaction.|
|gp\_xmin\_commit\_|The commit distribution id of the gp\_xmin transaction.|
|gp\_xmax|The id of the latest transaction.|
|gp\_xmax\_status|The status of the latest transaction.|
|gp\_xmax\_commit\_|The commit distribution id of the gp\_xmax transaction.|
|gp\_command\_id|The id of the query command.|
|gp\_infomask|A bitmap containing state information.|
|gp\_update\_tid|The ID of the newer tuple if the row is updated.|
|gp\_visibility|The tuple visibility status.|
|segno|The number of the segment in the segment file.|
|tupcount|The number of tuples, including hidden tuples.|
|eof|The effective end of file for the segment.|
|eof\_uncompressed|The end of file for the segment if data were uncompressed.|
|modcount|A count of data modifications.|
|state|The status of the segment.|

### <a id="topic11"></a>\_\_gp\_aocsseg\(regclass\) 

This function returns metadata information contained in a column-oriented append-optimized table's on-disk segment file, excluding non-visible rows. Each row describes a segment for a column in the table.

The input argument is the name or the oid of a column-oriented append-optimized table.

|Column|Description|
|------|-----------|
|gp\_tid|The table id.|
|segno|The segment number.|
|column\_num|The column number.|
|physical\_segno|The number of the segment in the segment file.|
|tupcount|The number of rows in the segment, excluding hidden tuples.|
|eof|The effective end of file for the segment.|
|eof\_uncompressed|The end of file for the segment if the data were uncompressed.|
|modcount|A count of data modification operations for the segment.|
|state|The status of the segment.|

### <a id="topic12"></a>\_\_gp\_aocsseg\_history\(regclass\) 

This function returns metadata information contained in a column-oriented append-optimized table's on-disk segment file. Each row describes a segment for a column in the table. The data is complex, but users with a deep understanding of the system may find it useful for debugging.

The input argument is the name or the oid of a column-oriented append-optimized table.

|Column|Description|
|------|-----------|
|gp\_tid|The oid of the tuple.|
|gp\_xmin|The earliest transaction.|
|gp\_xmin\_status|The status of the gp\_xmin transaction.|
|gp\_xmin\_|Text representation of gp\_xmin.|
|gp\_xmax|The latest transaction.|
|gp\_xmax\_status|The status of the gp\_xmax transaction.|
|gp\_xmax\_|Text representation of gp\_max.|
|gp\_command\_id|ID of the command operating on the tuple.|
|gp\_infomask|A bitmap containing state information.|
|gp\_update\_tid|The ID of the newer tuple if the row is updated.|
|gp\_visibility|The tuple visibility status.|
|segno|The segment number in the segment file.|
|column\_num|The column number.|
|physical\_segno|The segment containing data for the column.|
|tupcount|The total number of tuples in the segment.|
|eof|The effective end of file for the segment.|
|eof\_uncompressed|The end of file for the segment if the data were uncompressed.|
|modcount|A count of the data modification operations.|
|state|The state of the segment.|

### <a id="topic13"></a>\_\_gp\_aovisimap\(regclass\) 

This function returns the tuple ID, the segment file, and the row number of each non-visible tuple according to the visibility map.

The input argument is the name or the oid of an append-optimized table.

|Column|Description|
|------|-----------|
|tid|The tuple id.|
|segno|The number of the segment file.|
|row\_num|The row number of a row that has been deleted or updated.|

### <a id="topic14"></a>\_\_gp\_aovisimap\_hidden\_info\(regclass\) 

This function returns the numbers of hidden and visible tuples in the segment files for an append-optimized table.

The input argument is the name or the oid of an append-optimized table.

|Column|Description|
|------|-----------|
|segno|The number of the segment file.|
|hidden\_tupcount|The number of hidden tuples in the segment file.|
|total\_tupcount|The total number of tuples in the segment file.|

### <a id="topic15"></a>\_\_gp\_aovisimap\_entry\(regclass\) 

This function returns information about each visibility map entry for the table.

The input argument is the name or the oid of an append-optimized table.

|Column|Description|
|------|-----------|
|segno|Segment number of the visibility map entry.|
|first\_row\_num|The first row number of the entry.|
|hidden\_tupcount|The number of hidden tuples in the entry.|
|bitmap|A text representation of the visibility bitmap.|

### <a id="topic_aoblkdir"></a>\_\_gp\_aoblkdir\(regclass\)

For a given AO/AOCO table that had or has an index, this function returns a row for each block directory entry recorded in the block directory relation; it flattens the `minipage` column of block directory relations and returns a row for each `minipage` entry.

The input argument is the name or the oid of an append-optimized table.

You must execute this function in utility mode against every segment, or with `gp_dist_random()` as shown here:

``` sql
SELECT (gp_toolkit.__gp_aoblkdir('<table_name>')).*
    FROM gp_dist_random('gp_id');
```


|Column|Description|
|------|-----------|
|tupleid|The tuple id of the block directory row containing this block directory entry.|
|segno|The physical segment file number.|
|columngroup\_no|The `attnum` of the column described by this `minipage` entry (always `0` for row-oriented tables).|
|entry\_no|The entry serial number inside this `minipage` containing this block directory entry.|
|first\_row\_no|The first row number of the rows covered by this block directory entry.|
|file\_offset|The starting file offset of the rows covered by this block directory entry.|
|row\_count|The count of rows covered by this block directory entry.|

## <a id="topic16"></a>Viewing Greenplum Database Server Log Files 

Each component of a Greenplum Database system \(coordinator, standby coordinator, primary segments, and mirror segments\) keeps its own server log files. The `gp_log_*` family of views allows you to issue SQL queries against the server log files to find particular entries of interest. The use of these views require superuser permissions.

-   [gp\_log\_command\_timings](#topic17)
-   [gp\_log\_database](#topic18)
-   [gp\_log\_master\_concise](#topic19)
-   [gp\_log\_system](#topic20)

**Parent topic:** [The gp\_toolkit Administrative Schema](gp_toolkit.html)

### <a id="topic17"></a>gp\_log\_command\_timings 

This view uses an external table to read the log files on the coordinator and report the run time of SQL commands in a database session. The use of this view requires superuser permissions.

|Column|Description|
|------|-----------|
|logsession|The session identifier \(prefixed with "con"\).|
|logcmdcount|The command number within a session \(prefixed with "cmd"\).|
|logdatabase|The name of the database.|
|loguser|The name of the database user.|
|logpid|The process id \(prefixed with "p"\).|
|logtimemin|The time of the first log message for this command.|
|logtimemax|The time of the last log message for this command.|
|logduration|Statement duration from start to end time.|

### <a id="topic18"></a>gp\_log\_database 

This view uses an external table to read the server log files of the entire Greenplum system \(coordinator, segments, and mirrors\) and lists log entries associated with the current database. Associated log entries can be identified by the session id \(logsession\) and command id \(logcmdcount\). The use of this view requires superuser permissions.

|Column|Description|
|------|-----------|
|logtime|The timestamp of the log message.|
|loguser|The name of the database user.|
|logdatabase|The name of the database.|
|logpid|The associated process id \(prefixed with "p"\).|
|logthread|The associated thread count \(prefixed with "th"\).|
|loghost|The segment or coordinator host name.|
|logport|The segment or coordinator port.|
|logsessiontime|Time session connection was opened.|
|logtransaction|Global transaction id.|
|logsession|The session identifier \(prefixed with "con"\).|
|logcmdcount|The command number within a session \(prefixed with "cmd"\).|
|logsegment|The segment content identifier \(prefixed with "seg" for primary or "mir" for mirror. The coordinator always has a content id of -1\).|
|logslice|The slice id \(portion of the query plan being run\).|
|logdistxact|Distributed transaction id.|
|loglocalxact|Local transaction id.|
|logsubxact|Subtransaction id.|
|logseverity|LOG, ERROR, FATAL, PANIC, DEBUG1 or DEBUG2.|
|logstate|SQL state code associated with the log message.|
|logmessage|Log or error message text.|
|logdetail|Detail message text associated with an error message.|
|loghint|Hint message text associated with an error message.|
|logquery|The internally-generated query text.|
|logquerypos|The cursor index into the internally-generated query text.|
|logcontext|The context in which this message gets generated.|
|logdebug|Query string with full detail for debugging.|
|logcursorpos|The cursor index into the query string.|
|logfunction|The function in which this message is generated.|
|logfile|The log file in which this message is generated.|
|logline|The line in the log file in which this message is generated.|
|logstack|Full text of the stack trace associated with this message.|

### <a id="topic19"></a>gp\_log\_master\_concise 

This view uses an external table to read a subset of the log fields from the coordinator log file. The use of this view requires superuser permissions.

|Column|Description|
|------|-----------|
|logtime|The timestamp of the log message.|
|logdatabase|The name of the database.|
|logsession|The session identifier \(prefixed with "con"\).|
|logcmdcount|The command number within a session \(prefixed with "cmd"\).|
|logseverity|The log severity level.|
|logmessage|Log or error message text.|

### <a id="topic20"></a>gp\_log\_system 

This view uses an external table to read the server log files of the entire Greenplum system \(coordinator, segments, and mirrors\) and lists all log entries. Associated log entries can be identified by the session id \(logsession\) and command id \(logcmdcount\). The use of this view requires superuser permissions.

|Column|Description|
|------|-----------|
|logtime|The timestamp of the log message.|
|loguser|The name of the database user.|
|logdatabase|The name of the database.|
|logpid|The associated process id \(prefixed with "p"\).|
|logthread|The associated thread count \(prefixed with "th"\).|
|loghost|The segment or coordinator host name.|
|logport|The segment or coordinator port.|
|logsessiontime|Time session connection was opened.|
|logtransaction|Global transaction id.|
|logsession|The session identifier \(prefixed with "con"\).|
|logcmdcount|The command number within a session \(prefixed with "cmd"\).|
|logsegment|The segment content identifier \(prefixed with "seg" for primary or "mir" for mirror. The coordinator always has a content id of -1\).|
|logslice|The slice id \(portion of the query plan being run\).|
|logdistxact|Distributed transaction id.|
|loglocalxact|Local transaction id.|
|logsubxact|Subtransaction id.|
|logseverity|LOG, ERROR, FATAL, PANIC, DEBUG1 or DEBUG2.|
|logstate|SQL state code associated with the log message.|
|logmessage|Log or error message text.|
|logdetail|Detail message text associated with an error message.|
|loghint|Hint message text associated with an error message.|
|logquery|The internally-generated query text.|
|logquerypos|The cursor index into the internally-generated query text.|
|logcontext|The context in which this message gets generated.|
|logdebug|Query string with full detail for debugging.|
|logcursorpos|The cursor index into the query string.|
|logfunction|The function in which this message is generated.|
|logfile|The log file in which this message is generated.|
|logline|The line in the log file in which this message is generated.|
|logstack|Full text of the stack trace associated with this message.|

## <a id="topic21"></a>Checking Server Configuration Files 

Each component of a Greenplum Database system \(coordinator, standby coordinator, primary segments, and mirror segments\) has its own server configuration file \(`postgresql.conf`\). The following `gp_toolkit` objects can be used to check parameter settings across all primary `postgresql.conf` files in the system:

-   [gp\_param\_setting\('parameter\_name'\)](#topic22)
-   [gp\_param\_settings\_seg\_value\_diffs](#topic23)

**Parent topic:** [The gp\_toolkit Administrative Schema](gp_toolkit.html)

### <a id="topic22"></a>gp\_param\_setting\('parameter\_name'\) 

This function takes the name of a server configuration parameter and returns the `postgresql.conf` value for the coordinator and each active segment. This function is accessible to all users.

|Column|Description|
|------|-----------|
|paramsegment|The segment content id \(only active segments are shown\). The coordinator content id is always -1.|
|paramname|The name of the parameter.|
|paramvalue|The value of the parameter.|

#### <a id="ie192970"></a>Example: 

```
SELECT * FROM gp_param_setting('max_connections');
```

### <a id="topic23"></a>gp\_param\_settings\_seg\_value\_diffs 

Server configuration parameters that are classified as *local* parameters \(meaning each segment gets the parameter value from its own `postgresql.conf` file\), should be set identically on all segments. This view shows local parameter settings that are inconsistent. Parameters that are supposed to have different values \(such as `port`\) are not included. This view is accessible to all users.

|Column|Description|
|------|-----------|
|psdname|The name of the parameter.|
|psdvalue|The value of the parameter.|
|psdcount|The number of segments that have this value.|

## <a id="topic24"></a>Checking for Failed Segments 

The [gp\_pgdatabase\_invalid](#topic25) view can be used to check for down segments.

**Parent topic:** [The gp\_toolkit Administrative Schema](gp_toolkit.html)

### <a id="topic25"></a>gp\_pgdatabase\_invalid 

This view shows information about segments that are marked as down in the system catalog. This view is accessible to all users.

|Column|Description|
|------|-----------|
|pgdbidbid|The segment dbid. Every segment has a unique dbid.|
|pgdbiisprimary|Is the segment currently acting as the primary \(active\) segment? \(t or f\)|
|pgdbicontent|The content id of this segment. A primary and mirror will have the same content id.|
|pgdbivalid|Is this segment up and valid? \(t or f\)|
|pgdbidefinedprimary|Was this segment assigned the role of primary at system initialization time? \(t or f\)|

## <a id="topic26x"></a>Checking Resource Group Activity and Status 

> **Note** The resource group activity and status views described in this section are valid only when resource group-based resource management is active.

Resource groups manage transactions to avoid exhausting system CPU and memory resources. Every database user is assigned a resource group. Greenplum Database evaluates every transaction submitted by a user against the limits configured for the user's resource group before running the transaction.

You can use the `gp_resgroup_config` view to check the configuration of each resource group. You can use the `gp_resgroup_status*` views to display the current transaction status and resource usage of each resource group.

-   [gp\_resgroup\_config](#topic27x)
-   [gp\_resgroup\_status](#topic31x)
-   [gp\_resgroup\_status\_per\_host](#perhost)
-   [gp\_resgroup\_status\_per\_segment](#perseg)

**Parent topic:** [The gp\_toolkit Administrative Schema](gp_toolkit.html)

### <a id="topic27x"></a>gp\_resgroup\_config 

The `gp_resgroup_config` view allows administrators to see the current CPU, memory, and concurrency limits for a resource group.

This view is accessible to all users.

|Column|Description|
|------|-----------|
|groupid|The ID of the resource group.|
|groupname|The name of the resource group.|
|concurrency|The concurrency \(`CONCURRENCY`\) value specified for the resource group.|
|cpu\_rate\_limit|The CPU limit \(`cpu_hard_quota_limit`\) value specified for the resource group, or -1.|
|memory\_limit|The memory limit \(`MEMORY_LIMIT`\) value specified for the resource group.|
|memory\_shared\_quota|The shared memory quota \(`MEMORY_SHARED_QUOTA`\) value specified for the resource group.|
|memory\_spill\_ratio|The memory spill ratio \(`MEMORY_SPILL_RATIO`\) value specified for the resource group.|
|memory\_auditor|The memory auditor for the resource group.|
|cpuset|The CPU cores reserved for the resource group on the master host and segment hosts, or -1.|

### <a id="topic31x"></a>gp\_resgroup\_status 

The `gp_resgroup_status` view allows administrators to see status and activity for a resource group. It shows how many queries are waiting to run and how many queries are currently active in the system for each resource group. The view also displays current memory and CPU usage for the resource group.

> **Note** Resource groups use the Linux control groups \(cgroups\) configured on the host systems. The cgroups are used to manage host system resources. When resource groups use cgroups that are as part of a nested set of cgroups, resource group limits are relative to the parent cgroup allotment. For information about nested cgroups and Greenplum Database resource group limits, see [Using Resource Groups](../admin_guide/workload_mgmt_resgroups.html#topic8339intro).

This view is accessible to all users.

|Column|Description|
|------|-----------|
|rsgname|The name of the resource group.|
|groupid|The ID of the resource group.|
|num\_running|The number of transactions currently running in the resource group.|
|num\_queueing|The number of currently queued transactions for the resource group.|
|num\_queued|The total number of queued transactions for the resource group since the Greenplum Database cluster was last started, excluding the num\_queueing.|
|num\_executed|The total number of transactions run in the resource group since the Greenplum Database cluster was last started, excluding the num\_running.|
|total\_queue\_duration|The total time any transaction was queued since the Greenplum Database cluster was last started.|
|cpu\_usage|A set of key-value pairs. For each segment instance \(the key\), the value is the real-time, per-segment instance CPU core usage by a resource group. The value is the sum of the percentages \(as a decimal value\) of CPU cores that are used by the resource group for the segment instance.|
|memory\_usage|The real-time memory usage of the resource group on each Greenplum Database segment's host.|

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

> **Note** See the `gp_resgroup_status_per_host` and `gp_resgroup_status_per_segment` views, described below, for more user-friendly display of CPU and memory usage.

### <a id="perhost"></a>gp\_resgroup\_status\_per\_host 

The [gp\_resgroup\_status\_per\_host](system_catalogs/gp_resgroup_status_per_host.html) view displays the real-time CPU and memory usage \(MBs\) for each resource group on a per-host basis. The view also displays available and granted group fixed and shared memory for each resource group on a host.

|Column|Description|
|------|-----------|
|`rsgname`|The name of the resource group.|
|`groupid`|The ID of the resource group.|
|`hostname`|The hostname of the segment host.|
|`cpu`|The real-time CPU core usage by the resource group on a host. The value is the sum of the percentages \(as a decimal value\) of the CPU cores that are used by the resource group on the host.|
|`memory_used`|The real-time memory usage of the resource group on the host. This total includes resource group fixed and shared memory. It also includes global shared memory used by the resource group.|
|`memory_available`|The unused fixed and shared memory for the resource group that is available on the host. This total does not include available resource group global shared memory.|
|`memory_quota_used`|The real-time fixed memory usage for the resource group on the host.|
|`memory_quota_available`|The fixed memory available to the resource group on the host.|
|`memory_shared_used`|The group shared memory used by the resource group on the host. If any global shared memory is used by the resource group, this amount is included in the total as well.|
|`memory_shared_available`|The amount of group shared memory available to the resource group on the host. Resource group global shared memory is not included in this total.|

Sample output for the `gp_resgroup_status_per_host` view:

```
 rsgname       | groupid | hostname   | cpu  | memory_used | memory_available | memory_quota_used | memory_quota_available | memory_shared_used | memory_shared_available 
---------------+---------+------------+------+-------------+------------------+-------------------+------------------------+---------------------+---------------------
 admin_group   | 6438    | my-desktop | 0.84 | 1           | 271              | 68                | 68                     | 0                  | 136                     
 default_group | 6437    | my-desktop | 0.00 | 0           | 816              | 0                 | 400                    | 0                  | 416                     
(2 rows)
```

### <a id="perseg"></a>gp\_resgroup\_status\_per\_segment 

The [gp\_resgroup\_status\_per\_segment](system_catalogs/gp_resgroup_status_per_segment.html) view displays the real-time CPU and memory usage \(MBs\) for each resource group on a per-segment-instance and per-host basis. The view also displays available and granted group fixed and shared memory for each resource group and segment instance combination on the host.

|Column|Description|
|------|-----------|
|`rsgname`|The name of the resource group.|
|`groupid`|The ID of the resource group.|
|`hostname`|The hostname of the segment host.|
|`segment_id`|The content ID for a segment instance on the segment host.|
|`cpu`|The real-time, per-segment instance CPU core usage by the resource group on the host. The value is the sum of the percentages \(as a decimal value\) of the CPU cores that are used by the resource group for the segment instance.|
|`memory_used`|The real-time memory usage of the resource group for the segment instance on the host. This total includes resource group fixed and shared memory. It also includes global shared memory used by the resource group.|
|`memory_available`|The unused fixed and shared memory for the resource group for the segment instance on the host.|
|`memory_quota_used`|The real-time fixed memory usage for the resource group for the segment instance on the host.|
|`memory_quota_available`|The fixed memory available to the resource group for the segment instance on the host.|
|`memory_shared_used`|The group shared memory used by the resource group for the segment instance on the host.|
|`memory_shared_available`|The amount of group shared memory available for the segment instance on the host. Resource group global shared memory is not included in this total.|

Query output for this view is similar to that of the `gp_resgroup_status_per_host` view, and breaks out the CPU and memory \(used and available\) for each segment instance on each host.

## <a id="topic26"></a>Checking Resource Queue Activity and Status 

> **Note** The resource queue activity and status views described in this section are valid only when resource queue-based resource management is active.

The purpose of resource queues is to limit the number of active queries in the system at any given time in order to avoid exhausting system resources such as memory, CPU, and disk I/O. All database users are assigned to a resource queue, and every statement submitted by a user is first evaluated against the resource queue limits before it can run. The `gp_resq_*` family of views can be used to check the status of statements currently submitted to the system through their respective resource queue. Note that statements issued by superusers are exempt from resource queuing.

-   [gp\_resq\_activity](#topic27)
-   [gp\_resq\_activity\_by\_queue](#topic28)
-   [gp\_resq\_priority\_statement](#topic29)
-   [gp\_resq\_role](#topic30)
-   [gp\_resqueue\_status](#topic31)

**Parent topic:** [The gp\_toolkit Administrative Schema](gp_toolkit.html)

### <a id="topic27"></a>gp\_resq\_activity 

For the resource queues that have active workload, this view shows one row for each active statement submitted through a resource queue. This view is accessible to all users.

|Column|Description|
|------|-----------|
|resqprocpid|Process ID assigned to this statement \(on the coordinator\).|
|resqrole|User name.|
|resqoid|Resource queue object id.|
|resqname|Resource queue name.|
|resqstart|Time statement was issued to the system.|
|resqstatus|Status of statement: running, waiting or cancelled.|

### <a id="topic28"></a>gp\_resq\_activity\_by\_queue 

For the resource queues that have active workload, this view shows a summary of queue activity. This view is accessible to all users.

|Column|Description|
|------|-----------|
|resqoid|Resource queue object id.|
|resqname|Resource queue name.|
|resqlast|Time of the last statement issued to the queue.|
|resqstatus|Status of last statement: running, waiting or cancelled.|
|resqtotal|Total statements in this queue.|

### <a id="topic29"></a>gp\_resq\_priority\_statement 

This view shows the resource queue priority, session ID, and other information for all statements currently running in the Greenplum Database system. This view is accessible to all users.

|Column|Description|
|------|-----------|
|rqpdatname|The database name that the session is connected to.|
|rqpusename|The user who issued the statement.|
|rqpsession|The session ID.|
|rqpcommand|The number of the statement within this session \(the command id and session id uniquely identify a statement\).|
|rqppriority|The resource queue priority for this statement \(MAX, HIGH, MEDIUM, LOW\).|
|rqpweight|An integer value associated with the priority of this statement.|
|rqpquery|The query text of the statement.|

### <a id="topic30"></a>gp\_resq\_role 

This view shows the resource queues associated with a role. This view is accessible to all users.

|Column|Description|
|------|-----------|
|rrrolname|Role \(user\) name.|
|rrrsqname|The resource queue name assigned to this role. If a role has not been explicitly assigned to a resource queue, it will be in the default resource queue \(*pg\_default*\).|

### <a id="topic31"></a>gp\_resqueue\_status 

This view allows administrators to see status and activity for a resource queue. It shows how many queries are waiting to run and how many queries are currently active in the system from a particular resource queue.

|Column|Description|
|------|-----------|
|`queueid`|The ID of the resource queue.|
|`rsqname`|The name of the resource queue.|
|`rsqcountlimit`|The active query threshold of the resource queue. A value of -1 means no limit.|
|`rsqcountvalue`|The number of active query slots currently being used in the resource queue.|
|`rsqcostlimit`|The query cost threshold of the resource queue. A value of -1 means no limit.|
|`rsqcostvalue`|The total cost of all statements currently in the resource queue.|
|`rsqmemorylimit`|The memory limit for the resource queue.|
|`rsqmemoryvalue`|The total memory used by all statements currently in the resource queue.|
|`rsqwaiters`|The number of statements currently waiting in the resource queue.|
|`rsqholders`|The number of statements currently running on the system from this resource queue.|

## <a id="topic32"></a>Checking Query Disk Spill Space Usage 

The *gp\_workfile\_\** views show information about all the queries that are currently using disk spill space. Greenplum Database creates work files on disk if it does not have sufficient memory to run the query in memory. This information can be used for troubleshooting and tuning queries. The information in the views can also be used to specify the values for the Greenplum Database configuration parameters `gp_workfile_limit_per_query` and `gp_workfile_limit_per_segment`.

-   [gp\_workfile\_entries](#topic33)
-   [gp\_workfile\_usage\_per\_query](#topic34)
-   [gp\_workfile\_usage\_per\_segment](#topic35)

**Parent topic:** [The gp\_toolkit Administrative Schema](gp_toolkit.html)

### <a id="topic33"></a>gp\_workfile\_entries 

This view contains one row for each operator using disk space for workfiles on a segment at the current time. The view is accessible to all users, however non-superusers only to see information for the databases that they have permission to access.

|Column|Type|References|Description|
|------|----|----------|-----------|
|`datname`|name| |Greenplum database name.|
|`pid`|integer| |Process ID of the server process.|
|`sess_id`|integer| |Session ID.|
|`command_cnt`|integer| |Command ID of the query.|
|`usename`|name| |Role name.|
|`query`|text| |Current query that the process is running.|
|`segid`|integer| |Segment ID.|
|`slice`|integer| |The query plan slice. The portion of the query plan that is being run.|
|`optype`|text| |The query operator type that created the work file.|
|`size`|bigint| |The size of the work file in bytes.|
|`numfiles`|integer| |The number of files created.|
|`prefix`|text| |Prefix used when naming a related set of workfiles.|

### <a id="topic34"></a>gp\_workfile\_usage\_per\_query 

This view contains one row for each query using disk space for workfiles on a segment at the current time. The view is accessible to all users, however non-superusers only to see information for the databases that they have permission to access.

|Column|Type|References|Description|
|------|----|----------|-----------|
|`datname`|name| |Greenplum database name.|
|`pid`|integer| |Process ID of the server process.|
|`sess_id`|integer| |Session ID.|
|`command_cnt`|integer| |Command ID of the query.|
|`usename`|name| |Role name.|
|`query`|text| |Current query that the process is running.|
|`segid`|integer| |Segment ID.|
|`size`|numeric| |The size of the work file in bytes.|
|`numfiles`|bigint| |The number of files created.|

### <a id="topic35"></a>gp\_workfile\_usage\_per\_segment 

This view contains one row for each segment. Each row displays the total amount of disk space used for workfiles on the segment at the current time. The view is accessible to all users, however non-superusers only to see information for the databases that they have permission to access.

|Column|Type|References|Description|
|------|----|----------|-----------|
|`segid`|smallint| |Segment ID.|
|`size`|numeric| |The total size of the work files on a segment.|
|`numfiles`|bigint| |The number of files created.|

## <a id="topic36"></a>Viewing Users and Groups \(Roles\) 

It is frequently convenient to group users \(roles\) together to ease management of object privileges: that way, privileges can be granted to, or revoked from, a group as a whole. In Greenplum Database this is done by creating a role that represents the group, and then granting membership in the group role to individual user roles.

The [gp\_roles\_assigned](#topic37) view can be used to see all of the roles in the system, and their assigned members \(if the role is also a group role\).

**Parent topic:** [The gp\_toolkit Administrative Schema](gp_toolkit.html)

### <a id="topic37"></a>gp\_roles\_assigned 

This view shows all of the roles in the system, and their assigned members \(if the role is also a group role\). This view is accessible to all users.

|Column|Description|
|------|-----------|
|raroleid|The role object ID. If this role has members \(users\), it is considered a *group* role.|
|rarolename|The role \(user or group\) name.|
|ramemberid|The role object ID of the role that is a member of this role.|
|ramembername|Name of the role that is a member of this role.|

## <a id="topic38"></a>Checking Database Object Sizes and Disk Space 

The `gp_size_*` family of views can be used to determine the disk space usage for a distributed Greenplum Database, schema, table, or index. The following views calculate the total size of an object across all primary segments \(mirrors are not included in the size calculations\).

-   [gp\_size\_of\_all\_table\_indexes](#topic39)
-   [gp\_size\_of\_database](#topic40)
-   [gp\_size\_of\_index](#topic41)
-   [gp\_size\_of\_partition\_and\_indexes\_disk](#topic42)
-   [gp\_size\_of\_schema\_disk](#topic43)
-   [gp\_size\_of\_table\_and\_indexes\_disk](#topic44)
-   [gp\_size\_of\_table\_and\_indexes\_licensing](#topic45)
-   [gp\_size\_of\_table\_disk](#topic46)
-   [gp\_size\_of\_table\_uncompressed](#topic47)
-   [gp\_disk\_free](#topic48)

The table and index sizing views list the relation by object ID \(not by name\). To check the size of a table or index by name, you must look up the relation name \(`relname`\) in the `pg_class` table. For example:

```
SELECT relname as name, sotdsize as size, sotdtoastsize as 
toast, sotdadditionalsize as other 
FROM gp_size_of_table_disk as sotd, pg_class 
WHERE sotd.sotdoid=pg_class.oid ORDER BY relname;
```

**Parent topic:** [The gp\_toolkit Administrative Schema](gp_toolkit.html)

### <a id="topic39"></a>gp\_size\_of\_all\_table\_indexes 

This view shows the total size of all indexes for a table. This view is accessible to all users, however non-superusers will only be able to see relations that they have permission to access.

|Column|Description|
|------|-----------|
|soatioid|The object ID of the table|
|soatisize|The total size of all table indexes in bytes|
|soatischemaname|The schema name|
|soatitablename|The table name|

### <a id="topic40"></a>gp\_size\_of\_database 

This view shows the total size of a database. This view is accessible to all users, however non-superusers will only be able to see databases that they have permission to access.

|Column|Description|
|------|-----------|
|sodddatname|The name of the database|
|sodddatsize|The size of the database in bytes|

### <a id="topic41"></a>gp\_size\_of\_index 

This view shows the total size of an index. This view is accessible to all users, however non-superusers will only be able to see relations that they have permission to access.

|Column|Description|
|------|-----------|
|soioid|The object ID of the index|
|soitableoid|The object ID of the table to which the index belongs|
|soisize|The size of the index in bytes|
|soiindexschemaname|The name of the index schema|
|soiindexname|The name of the index|
|soitableschemaname|The name of the table schema|
|soitablename|The name of the table|

### <a id="topic42"></a>gp\_size\_of\_partition\_and\_indexes\_disk 

This view shows the size on disk of partitioned child tables and their indexes. This view is accessible to all users, however non-superusers will only be able to see relations that they have permission to access.

|Column|Description|
|------|-----------|
|sopaidparentoid|The object ID of the parent table|
|sopaidpartitionoid|The object ID of the partition table|
|sopaidpartitiontablesize|The partition table size in bytes|
|sopaidpartitionindexessize|The total size of all indexes on this partition|
|Sopaidparentschemaname|The name of the parent schema|
|Sopaidparenttablename|The name of the parent table|
|Sopaidpartitionschemaname|The name of the partition schema|
|sopaidpartitiontablename|The name of the partition table|

### <a id="topic43"></a>gp\_size\_of\_schema\_disk 

This view shows schema sizes for the public schema and the user-created schemas in the current database. This view is accessible to all users, however non-superusers will be able to see only the schemas that they have permission to access.

|Column|Description|
|------|-----------|
|sosdnsp|The name of the schema|
|sosdschematablesize|The total size of tables in the schema in bytes|
|sosdschemaidxsize|The total size of indexes in the schema in bytes|

### <a id="topic44"></a>gp\_size\_of\_table\_and\_indexes\_disk 

This view shows the size on disk of tables and their indexes. This view is accessible to all users, however non-superusers will only be able to see relations that they have permission to access.

|Column|Description|
|------|-----------|
|sotaidoid|The object ID of the parent table|
|sotaidtablesize|The disk size of the table|
|sotaididxsize|The total size of all indexes on the table|
|sotaidschemaname|The name of the schema|
|sotaidtablename|The name of the table|

### <a id="topic45"></a>gp\_size\_of\_table\_and\_indexes\_licensing 

This view shows the total size of tables and their indexes for licensing purposes. The use of this view requires superuser permissions.

|Column|Description|
|------|-----------|
|sotailoid|The object ID of the table|
|sotailtablesizedisk|The total disk size of the table|
|sotailtablesizeuncompressed|If the table is a compressed append-optimized table, shows the uncompressed table size in bytes.|
|sotailindexessize|The total size of all indexes in the table|
|sotailschemaname|The schema name|
|sotailtablename|The table name|

### <a id="topic46"></a>gp\_size\_of\_table\_disk 

This view shows the size of a table on disk. This view is accessible to all users, however non-superusers will only be able to see tables that they have permission to access

|Column|Description|
|------|-----------|
|sotdoid|The object ID of the table|
|sotdsize|The size of the table in bytes. The size is only the main table size. The size does not include auxiliary objects such as oversized \(toast\) attributes, or additional storage objects for AO tables.|
|sotdtoastsize|The size of the TOAST table \(oversized attribute storage\), if there is one.|
|sotdadditionalsize|Reflects the segment and block directory table sizes for append-optimized \(AO\) tables.|
|sotdschemaname|The schema name|
|sotdtablename|The table name|

### <a id="topic47"></a>gp\_size\_of\_table\_uncompressed 

This view shows the uncompressed table size for append-optimized \(AO\) tables. Otherwise, the table size on disk is shown. The use of this view requires superuser permissions.

|Column|Description|
|------|-----------|
|sotuoid|The object ID of the table|
|sotusize|The uncomressed size of the table in bytes if it is a compressed AO table. Otherwise, the table size on disk.|
|sotuschemaname|The schema name|
|sotutablename|The table name|

### <a id="topic48"></a>gp\_disk\_free 

This external table runs the `df` \(disk free\) command on the active segment hosts and reports back the results. Inactive mirrors are not included in the calculation. The use of this external table requires superuser permissions.

|Column|Description|
|------|-----------|
|dfsegment|The content id of the segment \(only active segments are shown\)|
|dfhostname|The hostname of the segment host|
|dfdevice|The device name|
|dfspace|Free disk space in the segment file system in kilobytes|

## <a id="topic49"></a>Checking for Uneven Data Distribution 

All tables in Greenplum Database are distributed, meaning their data is divided across all of the segments in the system. If the data is not distributed evenly, then query processing performance may decrease. The following views can help diagnose if a table has uneven data distribution:

-   [gp\_skew\_coefficients](#topic50)
-   [gp\_skew\_idle\_fractions](#topic51)

**Parent topic:** [The gp\_toolkit Administrative Schema](gp_toolkit.html)

### <a id="topic50"></a>gp\_skew\_coefficients 

This view shows data distribution skew by calculating the coefficient of variation \(CV\) for the data stored on each segment. This view is accessible to all users, however non-superusers will only be able to see tables that they have permission to access

|Column|Description|
|------|-----------|
|skcoid|The object id of the table.|
|skcnamespace|The namespace where the table is defined.|
|skcrelname|The table name.|
|skccoeff|The coefficient of variation \(CV\) is calculated as the standard deviation divided by the average. It takes into account both the average and variability around the average of a data series. The lower the value, the better. Higher values indicate greater data skew.|

### <a id="topic51"></a>gp\_skew\_idle\_fractions 

This view shows data distribution skew by calculating the percentage of the system that is idle during a table scan, which is an indicator of processing data skew. This view is accessible to all users, however non-superusers will only be able to see tables that they have permission to access

|Column|Description|
|------|-----------|
|sifoid|The object id of the table.|
|sifnamespace|The namespace where the table is defined.|
|sifrelname|The table name.|
|siffraction|The percentage of the system that is idle during a table scan, which is an indicator of uneven data distribution or query processing skew. For example, a value of 0.1 indicates 10% skew, a value of 0.5 indicates 50% skew, and so on. Tables that have more than 10% skew should have their distribution policies evaluated.|

