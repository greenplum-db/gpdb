---
title: diskquota 
---

The diskquota module allows Greenplum Database administrators to limit the amount of disk space used by schemas, roles, or tablespaces in a database.

This topic includes the following sections:

-   [Installing and Registering the Module \(First Use\)](#topic_ofb_gb1_b3b)
-   [About the diskquota Module](#intro)
-   [Understanding How diskquota Monitors Disk Usage](#topic_ndp_4wy_c3b)
-   [About the diskquota Functions and Views](#functions)
-   [Configuring the diskquota Module](#config)
-   [Using the diskquota Module](#using)
-   [Known Issues and Limitations](#limits)
-   [Notes](#topic_sfb_gb1_b3b)
-   [Upgrading the Module](#upgrade)
-   [Examples](#topic_v2z_jrv_b3b)

## <a id="topic_ofb_gb1_b3b"></a>Installing and Registering the Module \(First Use\) 

The `diskquota` module is installed when you install Greenplum Database.

Before you can use the module, you must perform these steps:

1.  Create the `diskquota` database. The `diskquota` module uses this database to store the list of databases where the module is enabled.

    ```
    $ createdb diskquota;
    ```

2.  Add the `diskquota` shared library to the Greenplum Database `shared_preload_libraries` server configuration parameter and restart Greenplum Database. Be sure to retain the previous setting of the configuration parameter. For example:

    ```
    $ gpconfig -s shared_preload_libraries
    Values on all segments are consistent
    GUC              : shared_preload_libraries
    Coordinator value: auto_explain
    Segment     value: auto_explain
    $ gpconfig -c shared_preload_libraries -v 'auto_explain,diskquota-2.1'
    $ gpstop -ar
    ```

3.  Register the `diskquota` extension in each database in which you want to enforce disk usage quotas. You can register `diskquota` in up to ten databases.

    ```
    $ psql -d testdb -c "CREATE EXTENSION diskquota"
    ```

4.  If you register the `diskquota` extension in a database that already contains data, you must initialize the `diskquota` table size data by running the `diskquota.init_table_size_table()` UDF in the database. In a database with many files, this can take some time. The `diskquota` module cannot be used until the initialization is complete.

    ```
    =# SELECT diskquota.init_table_size_table();
    ```

    > **Note** You must run the `diskquota.init_table_size_table()` UDF for `diskquota` to work.


## <a id="intro"></a>About the diskquota Module 

The disk usage for a table includes the table data, indexes, toast tables, and free space map. For append-optimized tables, the calculation includes the visibility map and index, and the block directory table.

The `diskquota` module allows a Greenplum Database administrator to limit the amount of disk space used by tables in schemas or owned by roles in up to 50 databases. The administrator can also use the module to limit the amount of disk space used by schemas and roles on a per-tablespace basis, as well as to limit the disk space used per Greenplum Database segment for a tablespace.

> **Note** A role-based disk quota cannot be set for the Greenplum Database system owner \(the user that creates the Greenplum cluster\).

You can set the following quotas with the `diskquota` module:

-   A *schema disk quota* sets a limit on the disk space that can used by all tables in a database that reside in a specific schema. The disk usage of a schema is defined as the total of disk usage on all segments for all tables in the schema.
-   A *role disk quota* sets a limit on the disk space that can be used used by all tables in a database that are owned by a specific role. The disk usage for a role is defined as the total of disk usage on all segments for all tables the role owns. Although a role is a cluster-level database object, the disk usage for roles is calculated separately for each database.
-   A *schema tablespace disk quota* sets a limit on the disk space that can used by all tables in a database that reside in a specific schema and tablespace.
-   A *role tablespace disk quota* sets a limit on the disk space that can used by all tables in a database that are owned by a specific role and reside in a specific tablespace.
-   A *per-segment tablespace disk quota* sets a limit on the disk space that can be used by a Greeplum Database segment when a tablespace quota is set for a schema or role.

## <a id="topic_ndp_4wy_c3b"></a>Understanding How diskquota Monitors Disk Usage 

A single `diskquota` launcher process runs on the active Greenplum Database coordinator node. The diskquota launcher process creates and launches a diskquota worker process on the coordinator for each diskquota-enabled database. A worker process is responsible for monitoring the disk usage of tablespaces, schemas, and roles in the target database, and communicates with the Greenplum segments to obtain the sizes of active tables. The worker process also performs quota enforcement, placing tablespaces, schemas, or roles on a denylist when they reach their quota.

When a query plan for a data-adding query is generated, and the tablespace, schema, or role into which data would be loaded is on the denylist, `diskquota` cancels the query before it starts executing, and reports an error message that the quota has been exceeded.

A query that does not add data, such as a simple `SELECT` query, is always allowed to run, even when the tablespace, role, or schema is on the denylist.

Diskquota can enforce both *soft limits* and *hard limits* for disk usage:

-   By default, `diskquota` always enforces soft limits. `diskquota` checks quotas before a query runs. If quotas are not exceeded when a query is initiated, `diskquota` allows the query to run, even if it were to eventually cause a quota to be exceeded.
-   When hard limit enforcement of disk usage is enabled, `diskquota` also monitors disk usage during query execution. If a query exceeds a disk quota during execution, `diskquota` terminates the query.

    Administrators can enable enforcement of a disk usage hard limit by setting the `diskquota.hard_limit` server configuration parameter as described in [Activating/Deactivating Hard Limit Disk Usage Enforcement](#hardlimit).

There is some delay after a quota has been reached before the schema or role is added to the denylist. Other queries could add more data during the delay. The delay occurs because `diskquota` processes that calculate the disk space used by each table run periodically with a pause between executions \(two seconds by default\). The delay also occurs when disk usage falls beneath a quota, due to operations such as `DROP`, `TRUNCATE`, or `VACUUM FULL` that remove data. Administrators can change the amount of time between disk space checks by setting the `diskquota.naptime` server configuration parameter as described in [Setting the Delay Between Disk Usage Updates](#naptime).

Diskquota can operate in both *static* and *dynamic* modes:

-   When the number of databases in which the `diskquota` extension is registered is less than or equal to the maximum number of `diskquota` worker processes, `diskquota` operates in static mode; it assigns a background worker \(bgworker\) process to monitor each database, and the bgworker process exits only when the `diskquota` extension is dropped from the database.
-   When the number of databases in which the `diskquota` extension is registered is greater than the maximum number of `diskquota` worker processes, `diskquota` operates in dynamic mode. In dynamic mode, for every monitored database every `diskquota.naptime` seconds, `diskquota` creates a bgworker process to collect disk usage information for the database, and then stops the bgworker process immediately after data collection completes. In this mode, `diskquota` dynamically starts and stops bgworker processes as needed for all monitored databases.

    Administrators can change the maximum number of worker processes configured for `diskquota` by setting the `diskquota.max_workers` server configuration parameter as described in [Specifying the Maximum Number of Active diskquota Worker Processes](#maxworkers).

If a query is unable to run because the tablespace, schema, or role has been denylisted, an administrator can increase the exceeded quota to allow the query to run. The module provides views that you can use to find the tablespaces, schemas, or roles that have exceeded their limits.

## <a id="functions"></a>About the diskquota Functions and Views 

The `diskquota` module provides user-defined functions \(UDFs\) and views that you can use to manage and monitor disk space usage in your Greenplum Database deployment.

The functions and views provided by the `diskquota` module are available in the Greenplum Database schema named `diskquota`.

> **Note** You may be required to prepend the schema name \(`diskquota.`\) to any UDF or view that you access.

User-defined functions provided by the module include:

|Function Signature|Description|
|------------------|-----------|
|void init\_table\_size\_table\(\)|Sizes the existing tables in the current database.|
|void set\_role\_quota\( role\_name text, quota text \)|Sets a disk quota for a specific role in the current database.<br/><br/>> **Note** A role-based disk quota cannot be set for the Greenplum Database system owner.|
|void set\_role\_tablespace\_quota\( role\_name text, tablespace\_name text, quota text \)|Sets a disk quota for a specific role and tablespace combination in the current database.<br/><br/>> **Note** A role-based disk quota cannot be set for the Greenplum Database system owner.|
|void set\_schema\_quota\( schema\_name text, quota text \)|Sets a disk quota for a specific schema in the current database.|
|void set\_schema\_tablespace\_quota\( schema\_name text, tablespace\_name text, quota text \)|Sets a disk quota for a specific schema and tablespace combination in the current database.|
|void set\_per\_segment\_quota\( tablespace\_name text, ratio float4 \)|Sets a per-segment disk quota for a tablespace in the current database.|
|void pause\(\)|Instructs the module to continue to count disk usage for the current database, but pause and cease emitting an error when the limit is exceeded.|
|void resume\(\)|Instructs the module to resume emitting an error when the disk usage limit is exceeded in the current database.|
|status\(\) RETURNS table|Displays the `diskquota` binary and schema versions and the status of soft and hard limit disk usage enforcement in the current database.|

Views available in the `diskquota` module include:

|View Name|Description|
|---------|-----------|
|show\_fast\_database\_size\_view|Displays the disk space usage in the current database.|
|show\_fast\_role\_quota\_view|Lists active quotas for roles in the current database.|
|show\_fast\_role\_tablespace\_quota\_view|List active quotas for roles per tablespace in the current database.|
|show\_fast\_schema\_quota\_view|Lists active quotas for schemas in the current database.|
|show\_fast\_schema\_tablespace\_quota\_view|Lists active quotas for schemas per tablespace in the current database.|
|show\_segment\_ratio\_quota\_view|Displays the per-segment disk quota ratio for any per-segment tablespace quotas set in the current database.|

## <a id="config"></a>Configuring the diskquota Module 

`diskquota` exposes server configuration parameters that allow you to control certain module functionality:

-   [diskquota.naptime](#naptime) - Controls how frequently \(in seconds\) that `diskquota` recalculates the table sizes.
-   [diskquota.max\_active\_tables](#shmem) - Identifies the maximum number of relations \(including tables, indexes, etc.\) that the `diskquota` module can monitor at the same time.
-   [diskquota.hard\_limit](#hardlimit) -  Activates or deactivates  the hard limit enforcement of disk usage.
-   [diskquota.max\_workers](#maxworkers) -  Specifies the maximum number of diskquota worker processes that may be running at any one time.
-   [diskquota.max\_table\_segments](#maxtableseg) -  Specifies the maximum number of *table segments* in the cluster.

You use the `gpconfig` command to set these parameters in the same way that you would set any Greenplum Database server configuration parameter.

### <a id="naptime"></a>Setting the Delay Between Disk Usage Updates 

The `diskquota.naptime` server configuration parameter specifies how frequently \(in seconds\) `diskquota` recalculates the table sizes. The smaller the `naptime` value, the less delay in detecting changes in disk usage. This example sets the `naptime` to ten seconds and restarts Greenplum Database:

```
$ gpconfig -c diskquota.naptime -v 10
$ gpstop -ar
```

### <a id="shmem"></a>About Shared Memory and the Maximum Number of Relations 

The `diskquota` module uses shared memory to save the denylist and to save the active table list.

The denylist shared memory can hold up to one million database objects that exceed the quota limit. If the denylist shared memory fills, data may be loaded into some schemas or roles after they have reached their quota limit.

Active table shared memory holds up to one million of active tables by default. Active tables are tables that may have changed sizes since `diskquota` last recalculated the table sizes. `diskquota` hook functions are called when the storage manager on each Greenplum Database segment creates, extends, or truncates a table file. The hook functions store the identity of the file in shared memory so that its file size can be recalculated the next time the table size data is refreshed.

The `diskquota.max_active_tables` server configuration parameter identifies the maximum number of relations \(including tables, indexes, etc.\) that the `diskquota` module can monitor at the same time. The default value is `300 * 1024`. This value should be sufficient for most Greenplum Database installations. Should you change the value of this configuration parameter, you must restart the Greenplum Database server.

### <a id="hardlimit"></a>Activating/Deactivating Hard Limit Disk Usage Enforcement 

When you enable enforcement of a hard limit of disk usage, `diskquota` checks the quota during query execution. If at any point a currently running query exceeds a quota limit, `diskquota` terminates the query.

By default, hard limit disk usage enforcement is deactivated for all databases. To activate hard limit enforcement for all databases, set the `diskquota.hard_limit` server configuration parameter to `'on'`, and then reload the Greenplum Database configuration:

```
$ gpconfig -c diskquota.hard_limit -v 'on'
$ gpstop -u
```

Run the following query to view the hard limit enforcement setting:

```
SELECT * from diskquota.status();
```

### <a id="maxworkers"></a>Specifying the Maximum Number of Active diskquota Worker Processes

The `diskquota.max_workers` server configuration parameter specifies the maximum number of diskquota worker processes \(not including the `diskquota` launcher process\) that may be running at any one time. The default number of maximum worker processes is `10`, and the maximum value that you can specify is `20`.

You must set this parameter at Greenplum Database server start time.

> **Note** Setting `diskquota.max_workers` to a value that is larger than `max_worker_processes` has no effect; `diskquota` workers are taken from the pool of worker processes established by that Greenplum Database server configuration parameter setting.

### <a id="maxtableseg"></a>Specifying the Maximum Number of Table Segments (Shards)

A Greenplum table \(including a partitioned table’s child tables\) is distributed to all segments as a shard. `diskquota` counts each table shard as a *table segment*. The `diskquota.max_table_segments` server configuration parameter identifies the maximum number of *table segments* in the Greenplum Database cluster, which in turn can gate the maximum number of tables that `diskquota` can monitor.

The runtime value of `diskquota.max_table_segments` equals the maximum number of tables multiplied by \(number\_of\_segments + 1\). The default value is `10 * 1024 * 1024`.


## <a id="using"></a>Using the diskquota Module 

You can perform the following tasks with the `diskquota` module:

-   [View the diskquota Status](#status)
-   [Pause and Resume Disk Quota Exceeded Notifications](#pause_resume)
-   [Set a Schema or Role Disk Quota](#schema_or_role_quota)
-   [Set a Tablespace Disk Quota for a Schema or Role](#tablespace_quota)
-   [Set a Per-Segment Tablespace Disk Quota](#per_seg_tblsp_quota)
-   [Display Disk Quotas and Disk Usage](#quotas_usage)
-   [Temporarily Deactivate Disk Quota Monitoring](#temp_deactivate)

### <a id="status"></a>Viewing the diskquota Status 

To view the `diskquota` module and schema version numbers, and the state of soft and hard limit enforcement in the current database, invoke the `status()` command:

```
SELECT diskquota.status();
          name          | status 
------------------------+--------- 
 soft limits            | on 
 hard limits            | on 
 current binary version | 2.0.1 
 current schema version | 2.0 
```

### <a id="pause_resume"></a>Pausing and Resuming Disk Quota Exceeded Notifications 

If you do not care to be notified of disk quota exceeded events for a period of time, you can pause and resume error notification in the current database as shown below:

```
SELECT diskquota.pause();
-- perform table operations where you do not care to be notified
-- when a disk quota exceeded
SELECT diskquota.resume(); 
```

> **Note** The pause operation does not persist through a Greenplum Database cluster restart; you must invoke `diskquota.pause()` again when the cluster is back up and running.

### <a id="schema_or_role_quota"></a>Setting a Schema or Role Disk Quota 

Use the `diskquota.set_schema_quota()` and `diskquota.set_role_quota()` user-defined functions in a database to set, update, or delete disk quota limits for schemas and roles in the database. The functions take two arguments: the schema or role name, and the quota to set. You can specify the quota in units of MB, GB, TB, or PB; for example, `'2TB'`.

The following example sets a 250GB quota for the `acct` schema:

```
SELECT diskquota.set_schema_quota('acct', '250GB');
```

This example sets a 500MB disk quota for the `nickd` role:

```
SELECT diskquota.set_role_quota('nickd', '500MB');
```

To change a quota, invoke the `diskquota.set_schema_quota()` or `diskquota.set_role_quota()` function again with the new quota value.

To remove a schema or role quota, set the quota value to `'-1'` and invoke the function.

### <a id="tablespace_quota"></a>Setting a Tablespace Disk Quota 

Use the `diskquota.set_schema_tablespace_quota()` and `diskquota.set_role_tablespace_quota()` user-defined functions in a database to set, update, or delete per-tablespace disk quota limits for schemas and roles in the current database. The functions take three arguments: the schema or role name, the tablespace name, and the quota to set. You can specify the quota in units of MB, GB, TB, or PB; for example, `'2TB'`.

The following example sets a 50GB disk quota for the tablespace named `tspaced1` and the `acct` schema:

```
SELECT diskquota.set_schema_tablespace_quota('acct', 'tspaced1', '250GB');
```

This example sets a 500MB disk quota for the `tspaced2` tablespace and the `nickd` role:

```
SELECT diskquota.set_role_tablespace_quota('nickd', 'tspaced2', '500MB');
```

To change a quota, invoke the `diskquota.set_schema_tablespace_quota()` or `diskquota.set_role_tablespace_quota()` function again with the new quota value.

To remove a schema or role tablespace quota, set the quota value to `'-1'` and invoke the function.

### <a id="per_seg_tblsp_quota"></a>Setting a Per-Segment Tablespace Disk Quota 

When an administrator sets a tablespace quota for a schema or a role, they may also choose to define a per-segment disk quota for the tablespace. Setting a per-segment quota limits the amount of disk space on a single Greenplum Database segment that a single tablespace may consume, and may help prevent a segment's disk from filling due to data skew.

You can use the `diskquota.set_per_segment_quota()` function to set, update, or delete a per-segment tablespace disk quota limit. The function takes two arguments: the tablespace name and a ratio. The ratio identifies how much more of the disk quota a single segment can use than the average segment quota. A ratio that you specify must be greater than zero.

You can calculate the average segment quota as follows:

```
avg_seg_quota = tablespace_quota / number_of_segments
```

For example, if your Greenplum Database cluster has 8 segments and you set the following schema tablespace quota:

```
SELECT diskquota.set_schema_tablespace_quota( 'accts', 'tspaced1', '800GB' );
```

The average segment quota for the `tspaced1` tablespace is `800GB / 8 = 100GB`.

If you set the following per-segment tablespace quota:

```
SELECT diskquota.set_per_segment_quota( 'tspaced1', '2.0' );
```

You can calculate the the maximum allowed disk usage per segment allowed as follows:

```
max_disk_usage_per_seg = average_segment_quota * ratio
```

In this example, the maximum disk usage allowed per segment is `100GB * 2.0 = 200GB`.

`diskquota` will allow a query to run if the disk usage on all segments for all tables that are in tablespace `tblspc1` and that are goverend by a role or schema quota does not exceed `200GB`.

You can change the per-segment tablespace quota by invoking the `diskquota.set_per_segment_quota()` function again with the new quota value.

To remove a per-segment tablespace quota, set the quota value to `'-1'` and invoke the function.

To view the per-segment quota ratio set for a tablespace, display the `show_segment_ratio_quota_view` view. For example:

```
SELECT tablespace_name, per_seg_quota_ratio
  FROM diskquota.show_segment_ratio_quota_view WHERE tablespace_name in ('tspaced1');
  tablespace_name  | per_seg_quota_ratio
-------------------+---------------------
 tspaced1          |                   2
(1 rows)

```

### <a id="dbs_monitored"></a>Identifying the diskquota-Monitored Databases

Run the following SQL commands to obtain a list of the `diskquota`-monitored databases in your Greenplum Database cluster:

``` sql
\c diskquota
SELECT d.datname FROM diskquota_namespace.database_list q, pg_database d
    WHERE q.dbid = d.oid ORDER BY d.datname;
```

### <a id="quotas_usage"></a>Displaying Disk Quotas and Disk Usage 

The `diskquota` module provides four views to display active quotas and the current computed disk space used.

The `diskquota.show_fast_schema_quota_view` view lists active quotas for schemas in the current database. The `nspsize_in_bytes` column contains the calculated size for all tables that belong to the schema.

```
SELECT * FROM diskquota.show_fast_schema_quota_view;
 schema_name | schema_oid | quota_in_mb | nspsize_in_bytes
-------------+------------+-------------+------------------
 acct        |      16561 |      256000 |           131072
 analytics   |      16519 |  1073741824 |        144670720
 eng         |      16560 |     5242880 |        117833728
 public      |       2200 |         250 |          3014656
(4 rows)
```

The `diskquota.show_fast_role_quota_view` view lists the active quotas for roles in the current database. The `rolsize_in_bytes` column contains the calculated size for all tables that are owned by the role.

```
SELECT * FROM diskquota.show_fast_role_quota_view;
 role_name | role_oid | quota_in_mb | rolsize_in_bytes
-----------+----------+-------------+------------------
 mdach     |    16558 |         500 |           131072
 adam      |    16557 |         300 |        117833728
 nickd     |    16577 |         500 |        144670720
(3 rows)
```

You can view the per-tablespace disk quotas for schemas and roles with the `diskquota.show_fast_schema_tablespace_quota_view` and `diskquota.show_fast_role_tablespace_quota_view` views. For example:

```
SELECT schema_name, tablespace_name, quota_in_mb, nspsize_tablespace_in_bytes
   FROM diskquota.show_fast_schema_tablespace_quota_view
   WHERE schema_name = 'acct' and tablespace_name ='tblspc1';
 schema_name | tablespace_name | quota_in_mb | nspsize_tablespace_in_bytes
-------------+-----------------+-------------+-----------------------------
 acct        | tspaced1        |      250000 |                      131072
(1 row)

```

### <a id="temp_deactivate"></a>About Temporarily Deactivating diskquota 

You can temporarily deactivate the `diskquota` module by removing the shared library from `shared_preload_libraries`. For example::

```
$ gpconfig -s shared_preload_libraries
Values on all segments are consistent
GUC              : shared_preload_libraries
Coordinator value: auto_explain,diskquota-2.0
Segment     value: auto_explain,diskquota-2.0
$ gpconfig -c shared_preload_libraries -v 'auto_explain'
$ gpstop -ar
```

> **Note** When you deactivate the `diskquota` module in this manner, disk quota monitoring ceases. To re-initiate disk quota monitoring in this scenario, you must:

1.  Re-add the library to `shared_preload_libraries`.
2.  Restart Greenplum Database.
3.  Re-size the existing tables in the database by running: `SELECT diskquota.init_table_size_table();`
4.  Restart Greenplum Database again.

## <a id="limits"></a>Known Issues and Limitations 

The `diskquota` module has the following limitations and known issues:

-   `diskquota` does not automatically work on a segment when the segment is replaced by a mirror. You must manually restart Greenplum Database in this circumstance.
-   `diskquota` cannot enforce a hard limit on `ALTER TABLE ADD COLUMN DEFAULT` operations.
-   If Greenplum Database restarts due to a crash, you must run `SELECT diskquota.init_table_size_table();` to ensure that the disk usage statistics are accurate.
-   To avoid the chance of deadlock, you must first pause the `diskquota` extension before you drop the extension in any database:

    ```
    SELECT diskquota.pause();
    DROP EXTENSION diskquota;
    ```

-   `diskquota` may record an incorrect table size after `ALTER TABLESPACE`, `TRUNCATE`, or other operations that modify the `relfilenode` of the table.

    Cause: `diskquota` does not acquire any locks on a relation when computing the table size. If another session is updating the table's tablespace while `diskquota` is calculating the table size, an error can occur.

    In most cases, you can ignore the difference; `diskquota` will update the size when new data is next ingested. To immediately ensure that the disk usage statistics are accurate, invoke:

    ```
    SELECT diskquota.init_table_size_table();
    ```

    And then restart Greenplum Database.

-   In rare cases, a `VACUUM FULL` operation may exceed a quota limit. To remedy the situation, pause `diskquota` before the operation and then resume `diskquota` after:

    ```
    SELECT diskquota.pause();
    -- perform the VACUUM FULL
    SELECT diskquota.resume();
    ```

    If you do not want to pause/resume `diskquota`, you may choose to temporarily set a higher quota for the operation and then set it back when the `VACUUM FULL` completes. Consider the following:

    -   If you `VACUUM FULL` only a single table, set the quota to be no smaller than the size of that table.
    -   If you `VACUUM FULL` all tables, set the quota to be no smaller than the size of the largest table in the database.
-   The size of uncommitted tables are not counted in quota views. Even though the `diskquota.show_fast_role_quota_view` view may display a smaller used quota than the quota limit, a new query may trigger a quota exceeded condition in the following circumstance:

    -   Hard limit enforcement of disk usage is deactivated.
    -   A long-running query in a session has consumed the full disk quota.
    `diskquota` does update the denylist in this scenario, but the `diskquota.show_fast_role_quota_view` may not represent the actual used quota because the long-running query is not yet committed. If you execute a new query while the original is still running, the new query will trigger a quota exceeded error.
-   When `diskquota` is operating in *static mode*, it may fail to monitor some databases when `diskquota.max_workers` is greater than the available number of bgworker processes. In *dynamic mode*, `diskquota` works correctly when there is at least one available bgworker process.


## <a id="topic_sfb_gb1_b3b"></a>Notes 

The `diskquota` module can detect a newly created table inside of an uncommitted transaction. The size of the new table is included in the disk usage calculated for the corresponding schema or role. Hard limit enforcement of disk usage must enabled for a quota-exceeding operation to trigger a `quota exceeded` error in this scenario.

Deleting rows or running `VACUUM` on a table does not release disk space, so these operations cannot alone remove a schema or role from the `diskquota` denylist. The disk space used by a table can be reduced by running `VACUUM FULL` or `TRUNCATE TABLE`.

The `diskquota` module supports high availability features provided by the background worker framework. The `diskquota` launcher process only runs on the active coordinator node. The postmaster on the standby coordinator does not start the `diskquota` launcher process when it is in standby mode. When the coordinator is down and the administrator runs the [gpactivatestandby](../../utility_guide/ref/gpactivatestandby.html) command, the standby coordinator changes its role to coordinator and the `diskquota` launcher process is forked automatically. Using the `diskquota`-enabled database list in the `diskquota` database, the `diskquota` launcher creates the `diskquota` worker processes that manage disk quotas for each database.

When you expand the Greenplum Database cluster, each table consumes more table segments, which may then reduce the maximum number of tables that `diskquota` can support. If you encounter the following warning, try increasing the `diskquota.max_table_segments` value, and then restart Greenplum Database:

```
[diskquota] the number of tables exceeds the limit, please increase the GUC value for diskquota.max_table_segments.
```


## <a id="upgrade"></a>Upgrading the Module

The `diskquota` 2.1 module is installed when you install or upgrade Greenplum Database. Versions 1.x and 2.0.x of the module will continue to work after you upgrade Greenplum.

> **Note**
> `diskquota` will be paused during the upgrade procedure and will be automatically resumed when the upgrade completes.

*If you are upgrading from `diskquota` version 2.0.x*, perform the procedure in [Upgrading from Version 2.0.x](#upgrade_20to21).

*If you are upgrading from `diskquota` version 1.x*, there are two steps in the upgrade procedure:

1. You must first [Upgrade the Module from Version 1.x to Version 2.0.x](#upgrade_1to2).
1. And then you must [Upgrade the Module from Version 2.0.x](#upgrade_20to21) to version 2.1.


### <a id="upgrade_20to21"></a>Upgrading from Version 2.0.x

If you are using version 2.0.x of the module and you want to upgrade to `diskquota` version 2.1, you must perform the following procedure:

1.  Replace the `diskquota-2.0` shared library in the Greenplum Database `shared_preload_libraries` server configuration parameter setting and restart Greenplum Database. Be sure to retain the other libraries. For example:

    ```
    $ gpconfig -s shared_preload_libraries
    Values on all segments are consistent
    GUC              : shared_preload_libraries
    Coordinator value: auto_explain,diskquota-2.0
    Segment     value: auto_explain,diskquota-2.0
    $ gpconfig -c shared_preload_libraries -v 'auto_explain,diskquota-2.1'
    $ gpstop -ar
    ```

2.  Update the `diskquota` extension in every database in which you registered the module:

    ```
    $ psql -d testdb -c "ALTER EXTENSION diskquota UPDATE TO '2.1'";
    ```

4.  Restart Greenplum Database:

    ```
    $ gpstop -ar
    ```

After upgrade, your existing disk quota rules continue to be enforced, and you can define new tablespace or per-segment rules. You can also utilize the new pause/resume disk quota enforcement functions.

## <a id="upgrade_1to2"></a>Upgrading From Version 1.x to Version 2.0.x

If you are using version 1.x of the module and you want to upgrade to `diskquota` version 2.x, you must first perform the following procedure to upgrade to version 2.0.x :

1.  Replace the `diskquota` shared library in the Greenplum Database `shared_preload_libraries` server configuration parameter setting and restart Greenplum Database. Be sure to retain the other libraries. For example:

    ```
    $ gpconfig -s shared_preload_libraries
    Values on all segments are consistent
    GUC              : shared_preload_libraries
    Coordinator value: auto_explain,diskquota
    Segment     value: auto_explain,diskquota
    $ gpconfig -c shared_preload_libraries -v 'auto_explain,diskquota-2.0'
    $ gpstop -ar
    ```

2.  Update the `diskquota` extension in every database in which you registered the module:

    ```
    $ psql -d testdb -c "ALTER EXTENSION diskquota UPDATE TO '2.0'";
    ```

3.  Re-initialize `diskquota` table size data:

    ```
    =# SELECT diskquota.init_table_size_table();
    ```

4.  Restart Greenplum Database:

    ```
    $ gpstop -ar
    ```

After upgrade, your existing disk quota rules continue to be enforced, and you can define new tablespace or per-segment rules. You can also utilize the new pause/resume disk quota enforcement functions.

## <a id="topic_v2z_jrv_b3b"></a>Examples 

### <a id="schemaquota"></a>Setting a Schema Quota 

This example demonstrates how to configure a schema quota and then observe `diskquota` soft limit behavior as data is added to the schema. The example assumes that the `diskquota` processes are configured and running.

1.  Create a database named `testdb` and connect to it.

    ```
    $ createdb testdb
    $ psql -d testdb
    ```

2.  Create the diskquota extension in the database.

    ```
    CREATE EXTENSION diskquota;
    ```

3.  Create a schema named `s1`:

    ```
    CREATE SCHEMA s1;
    ```

4.  Set a 1MB disk quota for the `s1` schema.

    ```
    SELECT diskquota.set_schema_quota('s1', '1MB');
    ```

5.  Run the following commands to create a table in the `s1` schema and insert a small amount of data into it. The schema has no data yet, so it is not on the denylist.

    ```
    SET search_path TO s1;
    CREATE TABLE a(i int);
    INSERT INTO a SELECT generate_series(1,100);
    ```

6.  Insert a large amount of data, enough to exceed the 1MB quota that was set for the schema. Before the `INSERT` command, the `s1` schema is still not on the denylist, so this command should be allowed to run with only soft limit disk usage enforcement in effect, even though the operation will exceed the limit set for the schema.

    ```
    INSERT INTO a SELECT generate_series(1,10000000);
    ```

7.  Attempt to insert a small amount of data. Because the previous command exceeded the schema's disk quota soft limit, the schema should be denylisted and any data loading command should be cancelled.

    ```
    INSERT INTO a SELECT generate_series(1,100);
    ERROR:  schema's disk space quota exceeded with name: s1
    ```

8.  Remove the quota from the `s1` schema by setting it to `-1` and again inserts a small amount of data. A 5-second sleep before the `INSERT` command ensures that the `diskquota` table size data is updated before the command is run.

    ```
    SELECT diskquota.set_schema_quota('s1', '-1');
    -- Wait for 5 seconds to ensure that the denylist is updated
    SELECT pg_sleep(5);
    INSERT INTO a SELECT generate_series(1,100);
    ```


### <a id="enablehardlimit"></a>Enabling Hard Limit Disk Usage Enforcement and Exceeding Quota 

In this example, we enable hard limit enforcement of disk usage, and re-run commands from the previous example.

1.  Enable hard limit disk usage enforcement:

    ```
    $ gpconfig -c diskquota.hard_limit -v 'on'
    $ gpstop -u
    ```

2.  Run the following query to view the hard limit enforcement setting:

    ```
    SELECT * from diskquota.status();
    ```

3.  Re-set a 1MB disk quota for the `s1` schema.

    ```
    SELECT diskquota.set_schema_quota('s1', '1MB');
    ```

4.  Insert a large amount of data, enough to exceed the 1MB quota that was set for the schema. Before the `INSERT` command, the `s1` schema is still not on the denylist, so this command should be allowed to start. When the operation exceeds the schema quota, `diskquota` will terminate the query.

    ```
    INSERT INTO a SELECT generate_series(1,10000000);
    [hardlimit] schema's disk space quota exceeded
    ```

5.  Remove the quota from the `s1` schema:

    ```
    SELECT diskquota.set_schema_quota('s1', '-1');
    ```


### <a id="persegtablespace"></a>Setting a Per-Segment Tablespace Quota 

This example demonstrates how to configure tablespace and per-segment tablespace quotas. In addition to using the `testdb` database and the `s1` schema that you created in the previous example, this example assumes the following:

-   Hard limit enforcement of disk usage is enabled \(as in the previous example\).
-   The Greenplum Database cluster has 8 primary segments.
-   A tablespace named `tbsp1` has been created in the cluster.

Procedure:

1.  Set a disk quota of `1 MB` for the tablespace named `tbsp1` and the schema named `s1`:

    ```
    SELECT diskquota.set_schema_tablespace_quota('s1', 'tbsp1', '1MB');
    ```

2.  Set a per-segment ratio of `2` for the `tbsp1` tablespace:

    ```
    SELECT diskquota.set_per_segment_quota('tbsp1', 2);
    ```

    With this ratio setting, the average segment quota is `1MB / 8 = 125KB`, and the max per-segment disk usage for the tablespace is `125KB * 2 = 250KB`.

3.  Create a new table named `b` and insert some data:

    ```
    CREATE TABLE b(i int);
    INSERT INTO b SELECT generate_series(1,100);
    ```

4.  Insert a large amount of data into the table, enough to exceed the 250KB per-segment quota that was set for the tablespace. When the operation exceeds the per-segment tablespace quota, `diskquota` will terminate the query.

    ```
    INSERT INTO b SELECT generate_series(1,10000000);
    ERROR:  tablespace: tbsp1, schema: s1 diskquota exceeded per segment quota
    ```


