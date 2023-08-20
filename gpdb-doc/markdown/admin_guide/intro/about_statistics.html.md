---
title: About Database Statistics in Greenplum Database 
---

Statistics are metadata that describe the data stored in the database. The Postgres Planner and Greenplum Query Optimizer need up-to-date statistics to choose the best execution plan for a query. For example, if a query joins two tables and one of them must be broadcast to all segments, the planner or optimizer can choose the smaller of the two tables to minimize network traffic.

Calculating statistics consumes time and resources, so Greenplum Database produces estimates by calculating statistics on samples of large tables. In most cases, the default settings provide the information needed to generate correct execution plans for queries. If the statistics produced are not producing optimal query execution plans, the administrator can tune configuration parameters to produce more accurate statistics by increasing the sample size or the granularity of statistics saved in the system catalog. Producing more accurate statistics has CPU and storage costs and may not produce better plans, so it is important to view explain plans and test query performance to ensure that the additional statistics-related costs result in better query performance.

Statistics used by the planner and optimizer are calculated and saved in the system catalog by the `ANALYZE` command. You can initiate an analyze operation in one of three ways:

-   Running the [ANALYZE](../../ref_guide/sql_commands/ANALYZE.html) command directly.
-   Running the [analyzedb](../../utility_guide/ref/analyzedb.html) management utility outside of the database, at the system command line.
-   Triggering an automatic analyze operation when DML operations are performed on tables that have no statistics or when a DML operation modifies a number of rows greater than a specified threshold.

(The `VACUUM ANALYZE` command is another way to initiate an analyze operation, but its use is discouraged because vacuum and analyze are different operations with different purposes.)

Refer to [About Single-Column and Extended Statistics](extended_statistics.html) for more information about the statistics collected by Greenplum Database.

**Parent topic:** [Greenplum Database Concepts](../intro/partI.html)

## <a id="topic_oq3_qxj_3s"></a>About System Statistics

### <a id="tabsize"></a>Table Size

The query planner seeks to minimize the disk I/O and network traffic required to run a query, using estimates of the number of rows that must be processed and the number of disk pages the query must access. The data from which these estimates are derived are the `pg_class` system table columns `reltuples` and `relpages`, which contain the number of rows and pages at the time a `VACUUM` or `ANALYZE` command was last run. As rows are added or deleted, the numbers become less accurate. However, an accurate count of disk pages is always available from the operating system, so as long as the ratio of `reltuples` to `relpages` does not change significantly, the optimizer can produce an estimate of the number of rows that is sufficiently accurate to choose the correct query execution plan.

When the `reltuples` column differs significantly from the row count returned by `SELECT COUNT(*)`, an analyze should be performed to update the statistics.

Further details about Greenplum Database's use of statistics can be found in [How the Planner Uses Statistics](https://www.postgresql.org/docs/12/planner-stats-details.html) in the PostgreSQL documentation.

### <a id="pgstattab"></a>Statistics-Related System Tables and Views

The [pg_statistic](../../ref_guide/system_catalogs/pg_statistic.html) system table holds the results of the last `ANALYZE` operation on each database table. The `pg_statistic` table has a row for each column of every table.

The [pg_stats](../../ref_guide/system_catalogs/catalog_ref-views.html#pg_stats) system view presents the contents of `pg_statistic` in a friendlier format. `pg_stats` is readable by all, whereas `pg_statistic` is readable only by a superuser. This prevents unprivileged users from learning something about the contents of other users' tables from the statistics. Greenplum Database restricts the `pg_stats` view to show only rows about tables that the current user can read.

Newly-created tables and indexes have no statistics. You can check for tables with missing statistics using the `gp_stats_missing` view, which is located in the `gp_toolkit` schema.

### <a id="section_wsy_1rv_mt"></a>Sampling 

When calculating statistics for large tables, Greenplum Database creates a smaller table by sampling the base table. If the table is partitioned, samples are taken from all partitions.

### <a id="section_u5p_brv_mt"></a>Updating Statistics 

Running [ANALYZE](../../ref_guide/sql_commands/ANALYZE.html) with no arguments updates statistics for all tables in the database. This could take a very long time, so it is better to analyze tables selectively after data has changed. You can also analyze a subset of the columns in a table, for example columns used in `JOIN`s, `WHERE` clauses, `SORT` clauses, `GROUP BY` clauses, or `HAVING` clauses.

Refer to [Updating Statistics with ANALYZE](../../best_practices/analyze.html) for more information about selective analyzing and configuring statistics.

## <a id="section_cv2_crv_mt"></a>Analyzing Partitioned Tables 

When the `ANALYZE` command is run on a partitioned table, it analyzes each child leaf partition table, one at a time. You can run `ANALYZE` on just new or changed partition tables to avoid analyzing partitions that have not changed.

The `analyzedb` command-line utility skips unchanged partitions automatically. It also runs concurrent sessions so it can analyze several partitions at the same time. It runs five sessions by default, but the number of sessions can be set from 1 to 10 with the `-p` command-line option. Each time `analyzedb` runs, it saves state information for append-optimized tables and partitions in the `db_analyze` directory in the coordinator data directory. The next time it runs, `analyzedb` compares the current state of each table with the saved state and skips analyzing a table or partition if it is unchanged. Heap tables are always analyzed.

When the Greenplum Query Optimizer (GPORCA) is enabled \(the default\), you also need to run `ANALYZE` or `ANALYZE ROOTPARTITION` on the root partition of a partitioned table \(not a leaf partition\) to refresh the root partition statistics. GPORCA requires statistics at the root level for partitioned tables. The Postgres Planner does not use these statistics.

The time to analyze a partitioned table is similar to the time to analyze a non-partitioned table with the same data. When all the leaf partitions have statistics, performing `ANALYZE ROOTPARTITION` to generate root partition statistics should be quick \(a few seconds depending on the number of partitions and table columns\). If some of the leaf partitions do not have statistics, then all the table data is sampled to generate root partition statistics. Sampling table data takes longer and results in lower quality root partition statistics.

The Greenplum Database server configuration parameter [optimizer\_analyze\_root\_partition](../../ref_guide/config_params/guc-list.html#optimizer_analyze_root_partition) affects when statistics are collected on the root partition of a partitioned table. If the parameter is `on` \(the default\), the `ROOTPARTITION` keyword is not required to collect statistics on the root partition when you run `ANALYZE`. Root partition statistics are collected when you run `ANALYZE` on the root partition, or when you run `ANALYZE` on a child leaf partition of the partitioned table and the other child leaf partitions have statistics. If the parameter is `off`, you must run `ANALYZE ROOTPARTITION` to collect root partition statistics.

If you do not intend to run queries on partitioned tables with GPORCA \(setting the server configuration parameter [optimizer](../../ref_guide/config_params/guc-list.html#optimizer) to `off`\), you can also set the server configuration parameter `optimizer_analyze_root_partition` to `off` to limit when `ANALYZE` updates the root partition statistics.

