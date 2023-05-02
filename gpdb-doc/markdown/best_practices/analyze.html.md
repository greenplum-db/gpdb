---
title: Updating Statistics with ANALYZE 
---

The most important prerequisite for good query performance is to begin with accurate statistics for the tables. Updating statistics with the `ANALYZE` statement enables the query planner to generate optimal query plans. When a table is analyzed, information about the data is stored in the system catalog tables. If the stored information is out of date, the planner can generate inefficient plans.

## <a id="genstat"></a>Generating Statistics Selectively 

Running [ANALYZE](../ref_guide/sql_commands/ANALYZE.html) with no arguments updates statistics for all tables in the database. This can be a very long-running process and it is not recommended. You should `ANALYZE` tables selectively when data has changed or use the [analyzedb](../utility_guide/ref/analyzedb.html) utility.

Running `ANALYZE` on a large table can take a long time. If it is not feasible to run `ANALYZE` on all columns of a very large table, you can generate statistics for selected columns only using `ANALYZE table(column, ...)`. Be sure to include columns used in joins, `WHERE` clauses, `SORT` clauses, `GROUP BY` clauses, or `HAVING` clauses.

For a partitioned table, you can run `ANALYZE` on just partitions that have changed, for example, if you add a new partition. Note that for partitioned tables, you can run `ANALYZE` on the parent \(main\) table, or on the leaf nodes—the partition files where data and statistics are actually stored. The intermediate files for sub-partitioned tables store no data or statistics, so running `ANALYZE` on them does not work. You can find the names of the leaf partition tables using the `pg_partition_tree()` function:

```
SELECT * FROM pg_partition_tree( 'parent_table' );
```

## <a id="impstat"></a>Improving Statistics Quality 

There is a trade-off between the amount of time it takes to generate statistics and the quality, or accuracy, of the statistics.

To allow large tables to be analyzed in a reasonable amount of time, `ANALYZE` takes a random sample of the table contents, rather than examining every row. To increase the number of sample values for all table columns adjust the `default_statistics_target` configuration parameter. The target value ranges from 1 to 1000; the default target value is 100. The `default_statistics_target` variable applies to all columns by default, and specifies the number of values that are stored in the list of common values. A larger target may improve the quality of the query planner’s estimates, especially for columns with irregular data patterns. `default_statistics_target` can be set at the coordinator/session level and requires a reload.

## <a id="whenrun"></a>When to Run ANALYZE 

Run `ANALYZE`:

-   after loading data,
-   after `CREATE INDEX` operations,
-   and after `INSERT`, `UPDATE`, and `DELETE` operations that significantly change the underlying data.

`ANALYZE` requires only a read lock on the table, so it may be run in parallel with other database activity, but do not run `ANALYZE` while performing loads, `INSERT`, `UPDATE`, `DELETE`, and `CREATE INDEX` operations.

Analyzing a severely bloated table can generate poor statistics if the sample contains empty pages, so it is good practice to vacuum a bloated table before analyzing it.

## <a id="statcfg"></a>Configuring Statistics

### <a id="stattarg"></a>Configuring the Statistics Target

The statistics target is the size of the `most_common_vals`, `most_common_freqs`, and `histogram_bounds` arrays for an individual column. This size is governed by the [default_statistics_target](../ref_guide/config_params/guc-list.html#default_statistics_target) server configuration parameter, and the default is 100.

You can also set the statistics target for individual columns using the `ALTER TABLE` command. For example, some queries can be improved by increasing the target for certain columns, especially columns that have irregular distributions. You can set the target to zero for columns that never contribute to query optimization. When the target is 0, `ANALYZE` ignores the column.

The statistics target for a column can be set in the range 0 to 1000; set it to -1 to revert to using the system default statistics target.

In general, larger target values increase the `ANALYZE` time, but may improve the quality of the Postgres Planner estimates.

Setting the statistics target on a parent partition table affects the child partitions. If you set statistics to 0 on some columns on the parent table, the statistics for the same columns are set to 0 for all children partitions. However, if you later add or exchange another child partition, the new child partition will use either the default statistics target or, in the case of an exchange, the previous statistics target. Therefore, if you add or exchange child partitions, you should set the statistics targets on the new child table.

### <a id="conauto"></a>Configuring Automatic Statistics Collection 

Automatic statistics collection is governed by these server configuration parameters:

|Configuration Parameter|Description|
|--------------------|-----------|
| [gp_autostats_allow_nonowner](../ref_guide/config_params/guc-list.html#gp_autostats_allow_nonowner) | Determines whether or not to allow Greenplum Database to trigger automatic statistics collection when a table is modified by a non-owner. |
| [gp_autostats_mode](../ref_guide/config_params/guc-list.html#gp_autostats_mode) | Specifies the mode for triggering automatic statistics collection. |
| [gp_autostats_mode_in_functions](../ref_guide/config_params/guc-list.html#gp_autostats_mode_in_functions) | Specifies the mode for triggering automatic statistics collection with `ANALYZE` for statements in procedural language functions. |
| [gp_autostats_on_change_threshold](../ref_guide/config_params/guc-list.html#gp_autostats_on_change_threshold) | Specifies the threshold for automatic statistics collection when `gp_autostats_mode` is set to `on_change`. |
| [log_autostats](../ref_guide/config_params/guc-list.html#log_autostats)  | Logs information about `ANALYZE` operations that Greenplum Database automatically initiated. |

The `gp_autostats_mode` configuration parameter, together with the `gp_autostats_on_change_threshold` parameter, determines when an automatic analyze operation is triggered. When automatic statistics collection is triggered, the planner adds an `ANALYZE` step to the query. Possible `gp_autostats_mode`s are:

-   `none` - Deactivate automatic statistics collection. *This is the default.*
-   `on_no_stats` - Trigger an analyze operation for a table with no existing statistics when any of the commands `CREATE TABLE AS SELECT`, `INSERT`, or `COPY` are run on the table by the table owner.
-   `on_change` - Trigger an analyze operation when any of the commands `CREATE TABLE AS SELECT`, `UPDATE`, `DELETE`, `INSERT`, or `COPY` are run on the table by the table owner, and the number of rows affected exceeds the threshold defined by the `gp_autostats_on_change_threshold` configuration parameter, which has a default value of 2147483647.

The automatic statistics collection mode is set separately for commands that occur within a procedural language function, and is governed by the `gp_autostats_mode_in_functions` server configuration parameter which is set to `none` by default.

With the `on_change` mode, `ANALYZE` is triggered only if the number of rows affected exceeds the threshold defined by the `gp_autostats_on_change_threshold` configuration parameter. The default value for this parameter is a very high value, 2147483647, which effectively deactivates automatic statistics collection; you must set the threshold to a lower number to enable it. The `on_change` mode could trigger large, unexpected analyze operations that could disrupt the system, so it is not recommended to set it globally. It could be useful in a session, for example to automatically analyze a table following a load.

Setting the `gp_autostats_allow_nonowner` server configuration parameter to `true` also instructs Greenplum Database to trigger automatic statistics collection on a table when:

-   `gp_autostats_mode=on_change` and the table is modified by a non-owner.
-   `gp_autostats_mode=on_no_stats` and the first user to `INSERT` or `COPY` into the table is a non-owner.

Setting `gp_autostats_mode` to `none` deactivates automatics statistics collection.

For partitioned tables, automatic statistics collection is not triggered if data is inserted from the top-level parent table of a partitioned table. But automatic statistics collection *is* triggered if data is inserted directly in a leaf table \(where the data is stored\) of the partitioned table.

Finally, you can use the `log_autostats` configuration parameter to enable the logging of automatic statistics collection operations.

**Parent topic:** [System Monitoring and Maintenance](maintenance.html)

