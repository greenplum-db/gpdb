# VACUUM 

Garbage-collects and optionally analyzes a database.

## Synopsis 

``` {#sql_command_synopsis}
VACUUM [FULL] [FREEZE] [VERBOSE] [<table>]

VACUUM [FULL] [FREEZE] [VERBOSE] ANALYZE
              [<table> [(<column> [, ...] )]]
```

## Description 

`VACUUM` reclaims storage occupied by deleted tuples. In normal Greenplum Database operation, tuples that are deleted or obsoleted by an update are not physically removed from their table; they remain present on disk until a `VACUUM` is done. Therefore it is necessary to do `VACUUM` periodically, especially on frequently-updated tables.

**Note:** Starting in Greenplum 5.x, `VACUUM` is supported with persistent table system catalogs, and is required to manage free space.

With no parameter, `VACUUM` processes every table in the current database. With a parameter, `VACUUM` processes only that table.

`VACUUM ANALYZE` performs a `VACUUM` and then an `ANALYZE` for each selected table. This is a handy combination form for routine maintenance scripts. See [ANALYZE](ANALYZE.html) for more details about its processing.

`VACUUM` \(without `FULL`\) marks deleted and obsoleted data in tables and indexes for future reuse and reclaims space for re-use only if the space is at the end of the table and an exclusive table lock can be easily obtained. Unused space at the start or middle of a table remains as is. With heap tables, this form of the command can operate in parallel with normal reading and writing of the table, as an exclusive lock is not obtained.

With append-optimized tables, `VACUUM` compacts a table by first vacuuming the indexes, then compacting each segment file in turn, and finally vacuuming auxiliary relations and updating statistics. On each segment, visible rows are copied from the current segment file to a new segment file, and then the current segment file is scheduled to be dropped and the new segment file is made available. Plain `VACUUM` of an append-optimized table allows scans, inserts, deletes, and updates of the table while a segment file is compacted. However, an Access Exclusive lock is taken briefly to drop the current segment file and activate the new segment file.

`VACUUM FULL` does more extensive processing, including moving of tuples across blocks to try to compact the table to the minimum number of disk blocks. This form is much slower and requires an Access Exclusive lock on each table while it is being processed. The Access Exclusive lock guarantees that the holder is the only transaction accessing the table in any way.

**Important:** For information on the use of `VACUUM`, `VACUUM FULL`, and `VACUUM ANALYZE`, see [Notes](#section6)

**Outputs**

When `VERBOSE` is specified, `VACUUM` emits progress messages to indicate which table is currently being processed. Various statistics about the tables are printed as well.

## Parameters 

FULL
:   Selects a full vacuum, which may reclaim more space, but takes much longer and exclusively locks the table.

FREEZE
:   Specifying `FREEZE` is equivalent to performing `VACUUM` with the `vacuum_freeze_min_age` server configuration parameter set to zero. See [Server Configuration Parameters](../config_params/guc_config.html) for information about `vacuum_freeze_min_age`.

VERBOSE
:   Prints a detailed vacuum activity report for each table.

ANALYZE
:   Updates statistics used by the planner to determine the most efficient way to execute a query.

table
:   The name \(optionally schema-qualified\) of a specific table to vacuum. Defaults to all tables in the current database.

column
:   The name of a specific column to analyze. Defaults to all columns.

## Notes 

`VACUUM` cannot be executed inside a transaction block.

Vacuum active databases frequently \(at least nightly\), in order to remove expired rows. After adding or deleting a large number of rows, running the `VACUUM ANALYZE` command for the affected table might be useful. This updates the system catalogs with the results of all recent changes, and allows the Greenplum Database query optimizer to make better choices in planning queries.

**Important:** PostgreSQL has a separate optional server process called the *autovacuum daemon*, whose purpose is to automate the execution of `VACUUM` and `ANALYZE` commands. Greenplum Database enables the autovacuum daemon to perform `VACUUM` operations only on the Greenplum Database template database `template0`. Autovacuum is enabled for `template0` because connections are not allowed to `template0`. The autovacuum daemon performs `VACUUM` operations on `template0` to manage transaction IDs \(XIDs\) and help avoid transaction ID wraparound issues in `template0`.

Manual `VACUUM` operations must be performed in user-defined databases to manage transaction IDs \(XIDs\) in those databases.

`VACUUM` causes a substantial increase in I/O traffic, which can cause poor performance for other active sessions. Therefore, it is advisable to vacuum the database at low usage times.

For heap tables, expired rows are held in what is called the *free space map*. The free space map must be sized large enough to cover the dead rows of all heap tables in your database. If not sized large enough, space occupied by dead rows that overflow the free space map cannot be reclaimed by a regular `VACUUM` command.

`VACUUM` commands skip external tables.

`VACUUM FULL` reclaims all expired row space, however it requires an exclusive lock on each table being processed, is a very expensive operation, and might take a long time to complete on large, distributed Greenplum Database tables. Perform `VACUUM FULL` operations during database maintenance periods.

As an alternative to `VACUUM FULL`, you can re-create the table with a `CREATE TABLE AS` statement and drop the old table.

Size the free space map appropriately. You configure the free space map using the following server configuration parameters:

-   `max_fsm_pages`
-   `max_fsm_relations`

For append-optimized tables, `VACUUM` requires enough available disk space to accommodate the new segment file during the `VACUUM` process. If the ratio of hidden rows to total rows in a segment file is less than a threshold value \(10, by default\), the segment file is not compacted. The threshold value can be configured with the `gp_appendonly_compaction_threshold` server configuration parameter. `VACUUM FULL` ignores the threshold and rewrites the segment file regardless of the ratio. `VACUUM` can be disabled for append-optimized tables using the `gp_appendonly_compaction` server configuration parameter. See [Server Configuration Parameters](../config_params/guc_config.html) for information about the server configuration parameters.

If a concurrent serializable transaction is detected when an append-optimized table is being vacuumed, the current and subsequent segment files are not compacted. If a segment file has been compacted but a concurrent serializable transaction is detected in the transaction that drops the original segment file, the drop is skipped. This could leave one or two segment files in an "awaiting drop" state after the vacuum has completed.

For more information about concurrency control in Greenplum Database, see "Routine System Maintenance Tasks" in *Greenplum Database Administrator Guide*.

## Examples 

Vacuum all tables in the current database:

```
VACUUM;
```

Vacuum a specific table only:

```
VACUUM mytable;
```

Vacuum all tables in the current database and collect statistics for the query optimizer:

```
VACUUM ANALYZE;
```

## Compatibility 

There is no `VACUUM` statement in the SQL standard.

## See Also 

[ANALYZE](ANALYZE.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

