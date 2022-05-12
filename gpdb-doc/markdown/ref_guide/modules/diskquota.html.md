---
title: diskquota 
---

The `diskquota` module allows Greenplum Database administrators to limit the amount of disk space used by schemas or roles in a database.

## <a id="topic_ofb_gb1_b3b"></a>Installing and Registering the Module 

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
    GUC          : shared_preload_libraries
    Master  value: auto_explain
    Segment value: auto_explain
    $ gpconfig -c shared_preload_libraries -v 'auto_explain,diskquota'
    $ gpstop -ar
    ```

3.  Register the `diskquota` extension in databases where you want to enforce disk usage quotas. `diskquota` can be registered in up to ten databases.

    ```
    $ psql -d testdb -c "CREATE EXTENSION diskquota"
    ```

4.  If you register the `diskquota` extension in a database that already contains data, you must initialize the `diskquota` table size data by running the `diskquota.init_table_size_table()` UDF in the database. In a database with many files, this can take a long time. The `diskquota` module cannot be used until the initialization is complete.

    ```
    =# SELECT diskquota.init_table_size_table();
    ```


## <a id="topic_ndp_4wy_c3b"></a>About the diskquota Module 

A Greenplum Database superuser can set disk usage quotas for schemas and roles. A schema quota sets a limit on disk space used by all tables that belong to a schema. A role quota sets a limit on disk space used by all tables that are owned by a role.

Diskquota processes running on the master and segment hosts check disk usage periodically and place schemas or roles on a denylist when they reach their quota.

When a query plan has been generated for a query that would add data, and the schema or role is on the denylist, the query is cancelled before it can start. An error message reports the quota that has been exceeded. A query that does not add data, such as a simple `SELECT` query, is allowed to run even when the role or schema is on the denylist.

Diskquota enforces *soft limits* for disk usage. Quotas are only checked before a query runs. If the quota is not exceeded when the query is about to run, the query is allowed to run, even if it causes the quota to be exceeded.

There is some delay after a quota has been reached before the schema or role is added to the denylist. Other queries could add more data during the delay. The delay occurs because `diskquota` processes that calculate the disk space used by each table run periodically with a pause between executions \(two seconds by default\). The delay also occurs when disk usage falls beneath a quota, due to operations such as `DROP`, `TRUNCATE`, or `VACUUM FULL` that remove data. Administrators can change the amount of time between disk space checks by setting the `diskquota.naptime` server configuration parameter.

If a query is unable to run because the schema or role has been denylisted, an administrator can increase the exceeded quota to allow the query to run. The `show_fast_schema_quota_view` and `show_fast_role_quota_view` views can be used to find the schemas or roles that have exceeded their limits.

## <a id="topic_qfb_gb1_b3b"></a>Using the diskquota Module 

### <a id="setq"></a>Setting Disk Quotas 

Use the `diskquota.set_schema_quota()` and `diskquota.set_role_quota()` user-defined functions in a database to set, update, or delete disk quota limits for schemas and roles in the database. The functions take two arguments: the schema or role name, and the quota to set. The quota can be specified in units of MB, GB, TB, or PB, for example '2TB'.

The following example sets a 250GB quota for the `acct` schema:

```
$ SELECT diskquota.set_schema_quota('acct', '250GB');
```

This example sets a 500MB quota for the `nickd` role:

```
$ SELECT diskquota.set_role_quota('nickd', '500MB');
```

To change a quota, call the `diskquota.set_schema_quota()` or `diskquota.set_role_quota()` function again with the new quota value.

To remove a quota, set the quota value to `'-1'`.

### <a id="dispquot"></a>Displaying Disk Quotas and Disk Usage 

The `diskquota` module provides two views to display active quotas and the current computed disk space used.

The `diskquota.show_fast_schema_quota_view` view lists active quotas for schemas in the current database. The `nspsize_in_bytes` column contains the calculated size for all tables that belong to the schema.

```
=# SELECT * FROM diskquota.show_fast_schema_quota_view;
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
=# SELECT * FROM diskquota.show_fast_role_quota_view;
 role_name | role_oid | quota_in_mb | rolsize_in_bytes
-----------+----------+-------------+------------------
 mdach     |    16558 |         500 |           131072
 adam      |    16557 |         300 |        117833728
 nickd     |    16577 |         500 |        144670720
(3 rows)
```

### <a id="setdel"></a>Setting the Delay Between Disk Usage Updates 

The `diskquota.naptime` server configuration parameter specifies how frequently \(in seconds\) the table sizes are recalculated. The smaller the `naptime` value, the less delay in detecting changes in disk usage. This example sets the `naptime` to ten seconds.

```
$ gpconfig -c diskquota.naptime -v 10
$ gpstop -ar
```

### <a id="aboutquot"></a>About diskquota Shared Memory Usage 

The `diskquota` module uses shared memory to save the denylist and to save the active table list.

The denylist shared memory can hold up to 1MiB of database objects that exceed the quota limit. If the denylist shared memory fills, data may be loaded into some schemas or roles after they have reached their quota limit.

Active table shared memory holds up to 1MiB of active tables by default. Active tables are tables that may have changed sizes since `diskquota` last recalculated the table sizes. `diskquota` hook functions are called when the storage manager on each Greenplum Database segment creates, extends, or truncates a table file. The hook functions store the identity of the file in shared memory so that its file size can be recalculated the next time the table size data is refreshed.

If the shared memory for active tables fills, `diskquota` may fail to detect a change in disk usage.

## <a id="topic_sfb_gb1_b3b"></a>Notes 

The `diskquota.max_active_tables` server configuration parameter identifies the maximum number of relations \(including tables, indexes, etc.\) that the `diskquota` module can monitor at the same time. The default value is `1 * 1024 * 1024`. This value should be sufficient for most Greenplum Database installations. Should you change the value of this configuration parameter, you must restart the Greenplum Database server.

The `diskquota` module can be enabled in up to ten databases. One diskquota worker process is created on the Greenplum Database master host for each diskquota-enabled database.

The disk usage for a role is defined as the total of disk usage on all segments for all tables the role owns. Although a role is a cluster-level database object, the disk usage for roles is calculated separately for each database.

The disk usage of a schema is defined as the total of disk usage on all segments for all tables in the schema.

The disk usage for a table includes the table data, indexes, toast tables, and free space map. For append-optimized tables, the calculation includes the visibility map and index, and the block directory table.

The `diskquota` module cannot detect a newly created table inside of an uncommited transaction. The size of the new table is not included in the disk usage calculated for the corresponding schema or role until after the transaction has committed. Similarly, a table created using the `CREATE TABLE AS` command is not included in disk usage statistics until the command has completed.

Deleting rows or running `VACUUM` on a table does not release disk space, so these operations cannot alone remove a schema or role from the `diskquota` denylist. The disk space used by a table can be reduced by running `VACUUM FULL` or `TRUNCATE TABLE`.

The `diskquota` module supports high availability features provided by the background worker framework. The `diskquota` launcher process only runs on the active master node. The postmaster on the standby master does not start the `diskquota` launcher process when it is in standby mode. When the master is down and the administrator runs the [gpactivatestandby](../../utility_guide/ref/gpactivatestandby.html) command, the standby master changes its role to master and the `diskquota` launcher process is forked automatically. Using the `diskquota`-enabled database list in the `diskquota` database, the `diskquota` launcher creates the `diskquota` worker processes that manage disk quotas for each database.

## <a id="topic_v2z_jrv_b3b"></a>Examples 

This example demonstrates how to set up a schema quota and then observe diskquota behavior as data is added to the schema.

1.  Create a database named `test` and log in to it.

    ```
    $ createdb test
    $ psql -d test
    ```

2.  Create the diskquota extension in the database.

    ```
    =# CREATE EXTENSION diskquota;
    CREATE EXTENSION
    ```

3.  Create the `s1` schema.

    ```
    =# CREATE SCHEMA s1;
    CREATE SCHEMA
    ```

4.  Set a 1MB disk quota for the `s1` schema.

    ```
    =# SELECT diskquota.set_schema_quota('s1', '1MB');
     set_schema_quota
    ------------------
    
    (1 row)
    ```

5.  The following commands create a table in the `s1` schema and insert a small amount of data into it. The schema has no data yet, so it is not on the denylist.

    ```
    =# **SET search\_path TO s1;
    SET
    **=# CREATE TABLE a(i int);
    CREATE TABLE
    =# INSERT INTO a SELECT generate_series(1,100);
    INSERT 0 100
    
    ```

6.  This command inserts a large amount of data, enough to exceed the 1MB quota that was set for the schema. Before the `INSERT` command, the `s1` schema is still not on the denylist, so this command should be allowed to run, even though it will exceed the limit set for the schema.

    ```
    =# INSERT INTO a SELECT generate_series(1,10000000);
    INSERT 0 10000000
    
    ```

7.  This command attempts to insert a small amount of data. Because the previous command exceeded the schema's disk quota limit, the schema should be denylisted and any data loading command should be cancelled.

    ```
    =# INSERT INTO a SELECT generate_series(1,100);
    ERROR:  schema's disk space quota exceeded with name:s1
    
    ```

8.  This command removes the quota from the `s1` schema by setting it to `-1` and again inserts a small amount of data. A 5-second sleep before the `INSERT` command ensures that the `diskquota` table size data is updated before the command is run.

    ```
    =# SELECT diskquota.set_schema_quota('s1', '-1');
     set_schema_quota
    ------------------
    
    (1 row)
    # Wait for 5 seconds to ensure the denylist is updated
    #= SELECT pg_sleep(5);
    #= INSERT INTO a SELECT generate_series(1,100);
    INSERT 0 100
    
    ```


