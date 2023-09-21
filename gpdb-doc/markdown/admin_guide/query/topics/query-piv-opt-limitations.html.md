---
title: GPORCA Limitations 
---

There are limitations in Greenplum Database when using the default GPORCA optimizer. GPORCA and the Postgres-based planner currently coexist in Greenplum Database because GPORCA does not support all Greenplum Database features.

This section describes the limitations.

-   [Unsupported SQL Query Features](#topic_kgn_vxl_vp)
-   [Performance Regressions](#topic_u4t_vxl_vp)

**Parent topic:** [About GPORCA](../../query/topics/query-piv-optimizer.html)

## <a id="topic_kgn_vxl_vp"></a>Unsupported SQL Query Features 

Certain query features are not supported with the default GPORCA optimizer. When an unsupported query is run, Greenplum logs this notice along with the query text:

```
Falling back to Postgres-based planner because GPORCA does not support the following feature: UTILITY command
```

These features are unsupported when GPORCA is enabled \(the default\):

-   Prepared statements that have parameterized values.
-   SP-GiST indexing method. GPORCA supports only B-tree, bitmap, GIN, and GiST indexes. GPORCA ignores indexes created with unsupported methods.
-   External parameters
-   Multi-level partitioned tables.
-   Non-uniform partitioned tables.
-   SortMergeJoin \(SMJ\).
-   Ordered aggregations.
-   Multi-argument `DISTINCT` qualified aggregates, for example `SELECT corr(DISTINCT a, b) FROM tbl1;`
-   Multiple grouping sets specified using a duplicate alias in a null-producing grouping set spec. Such queries fall back to the Postgres-based planner unless you directly coerce the alias to a separate variable as shown in the example below:

    ``` sql
    CREATE TEMP TABLE tempt AS SELECT i AS ai1, i AS ai2 FROM generate_series(1, 3)i;
    SELECT ai1, ai2 FROM tempt GROUP BY ai2, ROLLUP(ai1) ORDER BY ai1, ai2;
    ```
-   These scalar operators:
    -   ROW
    -   ROWCOMPARE
    -   FIELDSELECT
-   Aggregate functions that take set operators as input arguments.
-   Multiple Distinct Qualified Aggregates, such as `SELECT count(DISTINCT a), sum(DISTINCT b) FROM foo`, are not supported by default. They can be enabled with the `optimizer_enable_multiple_distinct_aggs` [Configuration Parameter](../../../ref_guide/config_params/guc-list.html).
-   percentile\_\* window functions \(ordered-set aggregate functions\).
-   Inverse distribution functions.
-   Queries that run functions that are defined with the `ON COORDINATOR` or `ON ALL SEGMENTS` attribute.
-   Queries that contain UNICODE characters in metadata names, such as table names, and the characters are not compatible with the host system locale.
-   `SELECT`, `UPDATE`, and `DELETE` commands where a table name is qualified by the `ONLY` keyword.
-   Per-column collation. GPORCA supports collation only when all columns in the query use the same collation. If columns in the query use different collations, then Greenplum uses the Postgres-based planner.
-   DML and `COPY ... FROM` operations on foreign tables.
-   Unsupported index-related features include:

    - Index scan on AO tables
    - Partial dynamic index scan
    - Partial indexes
    - Forward and backward dynamic index and dynamic index-only scans on partitioned tables
    - Indexed expressions (an index defined as an expression based on one or more columns of the table)
    - Combined indexes

## <a id="topic_u4t_vxl_vp"></a>Performance Regressions 

The following features are known performance regressions that occur with GPORCA enabled:

-   Short running queries - For GPORCA, short running queries might encounter additional overhead due to GPORCA enhancements for determining an optimal query execution plan.
-   ANALYZE - For GPORCA, the ANALYZE command generates root partition statistics for partitioned tables. For the Postgres-based planner, these statistics are not generated.
-   DML operations - For GPORCA, DML enhancements including the support of updates on partition and distribution keys might require additional overhead.

Also, enhanced functionality of the features from previous versions could result in additional time required when GPORCA runs SQL statements with the features.

