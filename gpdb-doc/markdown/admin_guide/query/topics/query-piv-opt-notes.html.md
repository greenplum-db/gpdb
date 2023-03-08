---
title: Considerations when Using GPORCA 
---

To run queries optimally with GPORCA, consider the query criteria closely.

Ensure the following criteria are met:

-   The table does not contain multi-column partition keys.
-   The multi-level partitioned table is a uniform multi-level partitioned table. See [About Uniform Multi-level Partitioned Tables](query-piv-uniform-part-tbl.html).
-   The server configuration parameter `optimizer_enable_master_only_queries` is set to `on` when running against coordinator only tables such as the system table *pg\_attribute*. For information about the parameter, see the *Greenplum Database Reference Guide*.

    > **Note** Enabling this parameter decreases performance of short running catalog queries. To avoid this issue, set this parameter only for a session or a query.

-   Statistics have been collected on the root partition of a partitioned table.

If the partitioned table contains more than 20,000 partitions, consider a redesign of the table schema.

These server configuration parameters affect GPORCA query processing.

-   `optimizer_cte_inlining_bound` controls the amount of inlining performed for common table expression \(CTE\) queries \(queries that contain a `WHERE` clause\).
-   `optimizer_force_comprehensive_join_implementation` affects GPORCA's consideration of nested loop join and hash join alternatives. When the value is `false` \(the default\), GPORCA does not consider nested loop join alternatives when a hash join is available.
-   `optimizer_force_multistage_agg` forces GPORCA to choose a multi-stage aggregate plan for a scalar distinct qualified aggregate. When the value is `off` \(the default\), GPORCA chooses between a one-stage and two-stage aggregate plan based on cost.
-   `optimizer_force_three_stage_scalar_dqa` forces GPORCA to choose a plan with multistage aggregates when such a plan alternative is generated.
-   `optimizer_join_order` sets the query optimization level for join ordering by specifying which types of join ordering alternatives to evaluate.
-   `optimizer_join_order_threshold` specifies the maximum number of join children for which GPORCA uses the dynamic programming-based join ordering algorithm.
-   `optimizer_nestloop_factor` controls nested loop join cost factor to apply to during query optimization.
-   `optimizer_parallel_union` controls the amount of parallelization that occurs for queries that contain a `UNION` or `UNION ALL` clause. When the value is `on`, GPORCA can generate a query plan of the child operations of a `UNION` or `UNION ALL` operation run in parallel on segment instances.
-   `optimizer_sort_factor` controls the cost factor that GPORCA applies to sorting operations during query optimization. The cost factor can be adjusted for queries when data skew is present.
-   `gp_enable_relsize_collection` controls how GPORCA \(and the Postgres Planner\) handle a table without statistics. By default, GPORCA uses a default value to estimate the number of rows if statistics are not available. When this value is `on`, GPORCA uses the estimated size of a table if there are no statistics for the table.

    This parameter is ignored for a root partition of a partitioned table. If the root partition does not have statistics, GPORCA always uses the default value. You can use `ANALZYE ROOTPARTITION` to collect statistics on the root partition. See [ANALYZE](../../../ref_guide/sql_commands/ANALYZE.html).


These server configuration parameters control the display and logging of information.

-   `optimizer_print_missing_stats` controls the display of column information about columns with missing statistics for a query \(default is `true`\)
-   `optimizer_print_optimization_stats` controls the logging of GPORCA query optimization metrics for a query \(default is `off`\)

For information about the parameters, see the *Greenplum Database Reference Guide*.

GPORCA generates minidumps to describe the optimization context for a given query. The minidump files are used by VMware support to analyze Greenplum Database issues. The information in the file is not in a format that can be easily used for debugging or troubleshooting. The minidump file is located under the coordinator data directory and uses the following naming format:

`Minidump_date_time.mdp`

For information about the minidump file, see the server configuration parameter `optimizer_minidump` in the *Greenplum Database Reference Guide*.

When the `EXPLAIN ANALYZE` command uses GPORCA, the `EXPLAIN` plan shows only the number of partitions that are being eliminated. The scanned partitions are not shown. To show the name of the scanned partitions in the segment logs set the server configuration parameter `gp_log_dynamic_partition_pruning` to `on`. This example `SET` command enables the parameter.

```
SET gp_log_dynamic_partition_pruning = on;
```

**Parent topic:** [About GPORCA](../../query/topics/query-piv-optimizer.html)

