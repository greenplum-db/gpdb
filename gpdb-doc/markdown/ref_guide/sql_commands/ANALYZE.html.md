# ANALYZE 

Collects statistics about a database.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
ANALYZE [VERBOSE] [SKIP_LOCKED] [<table> [ (<column> [, ...] ) ]]

ANALYZE [VERBOSE] [SKIP_LOCKED] {<root_partition_table_name>|<leaf_partition_table_name>} [ (<column> [, ...] )] 

ANALYZE [VERBOSE] [SKIP_LOCKED] ROOTPARTITION {ALL | <root_partition_table_name> [ (<column> [, ...] )]}
```

## <a id="section3"></a>Description 

`ANALYZE` collects statistics about the contents of tables in the database, and stores the results in the system table *pg\_statistic*. Subsequently, Greenplum Database uses these statistics to help determine the most efficient execution plans for queries. For information about the table statistics that are collected, see [Notes](#section5).

With no parameter, `ANALYZE` collects statistics for every table in the current database. You can specify a table name to collect statistics for a single table. You can specify a set of column names in a specific table, in which case the statistics only for those columns from that table are collected.

`ANALYZE` does not collect statistics on external tables.

For partitioned tables, `ANALYZE` collects additional statistics, HyperLogLog \(HLL\) statistics, on the leaf child partitions. HLL statistics are used are used to derive number of distinct values \(NDV\) for queries against partitioned tables.

-   When aggregating NDV estimates across multiple leaf child partitions, HLL statistics generate a more accurate NDV estimates than the standard table statistics.
-   When updating HLL statistics, `ANALYZE` operations are required only on leaf child partitions that have changed. For example, `ANALYZE` is required if the leaf child partition data has changed, or if the leaf child partition has been exchanged with another table. For more information about updating partitioned table statistics, see [Notes](#section5).

> **Important** If you intend to run queries on partitioned tables with GPORCA enabled \(the default\), then you must collect statistics on the root partition of the partitioned table with the `ANALYZE` or `ANALYZE ROOTPARTITION` command. For information about collecting statistics on partitioned tables and when the `ROOTPARTITION` keyword is required, see [Notes](#section5). For information about GPORCA, see [Overview of GPORCA](../../admin_guide/query/topics/query-piv-opt-overview.html) in the *Greenplum Database Administrator Guide*.

> **Note** You can also use the Greenplum Database utility `analyzedb` to update table statistics. The `analyzedb` utility can update statistics for multiple tables concurrently. The utility can also check table statistics and update statistics only if the statistics are not current or do not exist. For information about the utility, see the *Greenplum Database Utility Guide*.

## <a id="section4"></a>Parameters 

\{ root\_partition\_table\_name \| leaf\_partition\_table\_name \} \[ \(column \[, ...\] \) \]
:   Collect statistics for partitioned tables including HLL statistics. HLL statistics are collected only on leaf child partitions.

:   `ANALYZE root\_partition\_table\_name`, collects statistics on all leaf child partitions and the root partition.

:   `ANALYZE leaf\_partition\_table\_name`, collects statistics on the leaf child partition.

:   By default, if you specify a leaf child partition, and all other leaf child partitions have statistics, `ANALYZE` updates the root partition statistics. If not all leaf child partitions have statistics, `ANALYZE` logs information about the leaf child partitions that do not have statistics. For information about when root partition statistics are collected, see [Notes](#section5).

ROOTPARTITION \[ALL\]
:   Collect statistics only on the root partition of partitioned tables based on the data in the partitioned table. If possible, `ANALYZE` uses leaf child partition statistics to generate root partition statistics. Otherwise, `ANALYZE` collects the statistics by sampling leaf child partition data. Statistics are not collected on the leaf child partitions, the data is only sampled. HLL statistics are not collected.

:   For information about when the `ROOTPARTITION` keyword is required, see [Notes](#section5).

:   When you specify `ROOTPARTITION`, you must specify either `ALL` or the name of a partitioned table.

:   If you specify `ALL` with `ROOTPARTITION`, Greenplum Database collects statistics for the root partition of all partitioned tables in the database. If there are no partitioned tables in the database, a message stating that there are no partitioned tables is returned. For tables that are not partitioned tables, statistics are not collected.

:   If you specify a table name with `ROOTPARTITION` and the table is not a partitioned table, no statistics are collected for the table and a warning message is returned.

:   The `ROOTPARTITION` clause is not valid with `VACUUM ANALYZE`. The command `VACUUM ANALYZE ROOTPARTITION` returns an error.

:   If all the leaf partitions have statistics, performing `ANALYZE ROOTPARTITION` to generate root partition statistics should be quick \(a few seconds depending on the number of partitions and table columns\). If some of the leaf partitions do not have statistics, then all the table data is sampled to generate root partition statistics. Sampling table data takes longer and results in lower quality root partition statistics.

:   For the partitioned table *sales\_curr\_yr*, this example command collects statistics only on the root partition of the partitioned table. `ANALYZE ROOTPARTITION sales_curr_yr;`

:   This example `ANALYZE` command collects statistics on the root partition of all the partitioned tables in the database.

    ```
    ANALYZE ROOTPARTITION ALL;
    ```

VERBOSE
:   Enables display of progress messages. When specified, `ANALYZE` emits this information

    -   The table that is being processed.
    -   The query that is run to generate the sample table.
    -   The column for which statistics is being computed.
    -   The queries that are issued to collect the different statistics for a single column.
    -   The statistics that are collected.

SKIP_LOCKED
:   Specifies that `ANALYZE` should not wait for any conflicting locks to be released when beginning work on a relation: if it cannot lock a relation immediately without waiting, it skips the relation. Note that even with this option, `ANALYZE` may still block when opening the relation's indexes or when acquiring sample rows from partitions, table inheritance children, and some types of foreign tables. Also, while `ANALYZE` ordinarily processes all partitions of specified partitioned tables, this option will cause `ANALYZE` to skip all partitions if there is a conflicting lock on the partitioned table.

table
:   The name \(possibly schema-qualified\) of a specific table to analyze. If omitted, all regular tables \(but not foreign tables\) in the current database are analyzed.

column
:   The name of a specific column to analyze. Defaults to all columns.

## <a id="section5"></a>Notes 

Foreign tables are analyzed only when explicitly selected. Not all foreign data wrappers support `ANALYZE`. If the table's wrapper does not support `ANALYZE`, the command prints a warning and does nothing.

It is a good idea to run `ANALYZE` periodically, or just after making major changes in the contents of a table. Accurate statistics helps Greenplum Database choose the most appropriate query plan, and thereby improve the speed of query processing. A common strategy for read-mostly databases is to run [VACUUM](VACUUM.html) and `ANALYZE` once a day during a low-usage time of day. \(This will not be sufficient if there is heavy update activity.\) You can check for tables with missing statistics using the `gp_stats_missing` view, which is in the `gp_toolkit` schema:

```
SELECT * from gp_toolkit.gp_stats_missing;
```

`ANALYZE` requires `SHARE UPDATE EXCLUSIVE` lock on the target table. This lock conflicts with these locks: `SHARE UPDATE EXCLUSIVE`, `SHARE`, `SHARE ROW EXCLUSIVE`, `EXCLUSIVE`, `ACCESS EXCLUSIVE`.

If you run `ANALYZE` on a table that does not contain data, statistics are not collected for the table. For example, if you perform a `TRUNCATE` operation on a table that has statistics, and then run `ANALYZE` on the table, the statistics do not change.

For a partitioned table, specifying which portion of the table to analyze, the root partition or subpartitions \(leaf child partition tables\) can be useful if the partitioned table has a large number of partitions that have been analyzed and only a few leaf child partitions have changed.

> **Note** When you create a partitioned table with the `CREATE TABLE` command, Greenplum Database creates the table that you specify \(the root partition or parent table\), and also creates a hierarchy of tables based on the partition hierarchy that you specified \(the child tables\).

-   When you run `ANALYZE` on the root partitioned table, statistics are collected for all the leaf child partitions. Leaf child partitions are the lowest-level tables in the hierarchy of child tables created by Greenplum Database for use by the partitioned table.
-   When you run `ANALYZE` on a leaf child partition, statistics are collected only for that leaf child partition and the root partition. If data in the leaf partition has changed \(for example, you made significant updates to the leaf child partition data or you exchanged the leaf child partition\), then you can run ANALYZE on the leaf child partition to collect table statistics. By default, if all other leaf child partitions have statistics, the command updates the root partition statistics.

    For example, if you collected statistics on a partitioned table with a large number partitions and then updated data in only a few leaf child partitions, you can run `ANALYZE` only on those partitions to update statistics on the partitions and the statistics on the root partition.

-   When you run `ANALYZE` on a child table that is not a leaf child partition, statistics are not collected.

    For example, you can create a partitioned table with partitions for the years 2006 to 2016 and subpartitions for each month in each year. If you run `ANALYZE` on the child table for the year 2013 no statistics are collected. If you run `ANALYZE` on the leaf child partition for March of 2013, statistics are collected only for that leaf child partition.


For a partitioned table that contains a leaf child partition that has been exchanged to use an external table, `ANALYZE` does not collect statistics for the external table partition:

-   If `ANALYZE` is run on an external table partition, the partition is not analyzed.
-   If `ANALYZE` or `ANALYZE ROOTPARTITION` is run on the root partition, external table partitions are not sampled and root table statistics do not include external table partition.
-   If the `VERBOSE` clause is specified, an informational message is displayed: `skipping external table`.

The Greenplum Database server configuration parameter [optimizer\_analyze\_root\_partition](../config_params/guc-list.html) affects when statistics are collected on the root partition of a partitioned table. If the parameter is `on` \(the default\), the `ROOTPARTITION` keyword is not required to collect statistics on the root partition when you run `ANALYZE`. Root partition statistics are collected when you run `ANALYZE` on the root partition, or when you run `ANALYZE` on a child leaf partition of the partitioned table and the other child leaf partitions have statistics. If the parameter is `off`, you must run `ANALZYE ROOTPARTITION` to collect root partition statistics.

The statistics collected by `ANALYZE` usually include a list of some of the most common values in each column and a histogram showing the approximate data distribution in each column. One or both of these may be omitted if `ANALYZE` deems them uninteresting \(for example, in a unique-key column, there are no common values\) or if the column data type does not support the appropriate operators.

For large tables, `ANALYZE` takes a random sample of the table contents, rather than examining every row. This allows even very large tables to be analyzed in a small amount of time. Note, however, that the statistics are only approximate, and will change slightly each time `ANALYZE` is run, even if the actual table contents did not change. This may result in small changes in the planner's estimated costs shown by `EXPLAIN`. In rare situations, this non-determinism will cause the query optimizer to choose a different query plan between runs of `ANALYZE`. To avoid this, raise the amount of statistics collected by `ANALYZE` by adjusting the default\_statistics\_target configuration parameter, or on a column-by-column basis by setting the per-column statistics target with `ALTER TABLE ... ALTER COLUMN ... SET (n_distinct ...)` \(see `ALTER TABLE`\). The target value sets the maximum number of entries in the most-common-value list and the maximum number of bins in the histogram. The default target value is 100, but this can be adjusted up or down to trade off accuracy of planner estimates against the time taken for `ANALYZE` and the amount of space occupied in `pg_statistic`. In particular, setting the statistics target to zero deactivates collection of statistics for that column. It may be useful to do that for columns that are never used as part of the `WHERE`, `GROUP BY`, or `ORDER BY` clauses of queries, since the planner will have no use for statistics on such columns.

The largest statistics target among the columns being analyzed determines the number of table rows sampled to prepare the statistics. Increasing the target causes a proportional increase in the time and space needed to do `ANALYZE`.

One of the values estimated by `ANALYZE` is the number of distinct values that appear in each column. Because only a subset of the rows are examined, this estimate can sometimes be quite inaccurate, even with the largest possible statistics target. If this inaccuracy leads to bad query plans, a more accurate value can be determined manually and then installed with `ALTER TABLE ... ALTER COLUMN ... SET STATISTICS DISTINCT` \(see [ALTER TABLE](ALTER_TABLE.html)\).

When Greenplum Database performs an `ANALYZE` operation to collect statistics for a table and detects that all the sampled table data pages are empty \(do not contain valid data\), Greenplum Database displays a message that a `VACUUM FULL` operation should be performed. If the sampled pages are empty, the table statistics will be inaccurate. Pages become empty after a large number of changes to the table, for example deleting a large number of rows. A `VACUUM FULL` operation removes the empty pages and allows an `ANALYZE` operation to collect accurate statistics.

If there are no statistics for the table, the server configuration parameter [gp\_enable\_relsize\_collection](../config_params/guc-list.html) controls whether the Postgres Planner uses a default statistics file or estimates the size of a table using the `pg_relation_size` function. By default, the Postgres Planner uses the default statistics file to estimate the number of rows if statistics are not available.

## <a id="section6"></a>Examples 

Collect statistics for the table `mytable`:

```
ANALYZE mytable;
```

## <a id="section7"></a>Compatibility 

There is no `ANALYZE` statement in the SQL standard.

## <a id="section8"></a>See Also 

[ALTER TABLE](ALTER_TABLE.html), [EXPLAIN](EXPLAIN.html), [VACUUM](VACUUM.html), [analyzedb](../../utility_guide/ref/analyzedb.html).

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

