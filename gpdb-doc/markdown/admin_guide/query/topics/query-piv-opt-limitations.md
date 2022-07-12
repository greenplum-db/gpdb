# GPORCA Limitations 

There are limitations in Greenplum Database when using the default GPORCA optimizer. GPORCA and the legacy query optimizer currently coexist in Greenplum Database because GPORCA does not support all Greenplum Database features.

This section describes the limitations.

-   [Unsupported SQL Query Features](#topic_kgn_vxl_vp)
-   [Performance Regressions](#topic_u4t_vxl_vp)

**Parent topic:** [About GPORCA](../../query/topics/query-piv-optimizer.html)

## Unsupported SQL Query Features 

Certain query features are not supported with the default GPORCA optimizer. When an unsupported query is executed, Greenplum logs this notice along with the query text:

```
Feature not supported by the Tanzu Greenplum Query Optimizer: UTILITY command
```

These features are unsupported when GPORCA is enabled \(the default\):

-   Prepared statements that have parameterized values.
-   Indexed expressions \(an index defined as expression based on one or more columns of the table\)
-   GIN indexing method. GPORCA supports only B-tree, bitmap, and GiST indexes. GPORCA ignores indexes created with unsupported methods.
-   External parameters
-   These types of partitioned tables:
    -   Non-uniform partitioned tables.
    -   Partitioned tables that have been altered to use an external table as a leaf child partition.
-   SortMergeJoin \(SMJ\).
-   Ordered aggregations.
-   These analytics extensions:
    -   CUBE
    -   Multiple grouping sets
-   These scalar operators:
    -   ROW
    -   ROWCOMPARE
    -   FIELDSELECT
-   Aggregate functions that take set operators as input arguments.
-   percentile\_\* window functions \(Greenplum Database does not support ordered-set aggregate functions\).
-   Inverse distribution functions.
-   Queries that contain UNICODE characters in metadata names, such as table names, and the characters are not compatible with the host system locale.
-   `SELECT`, `UPDATE`, and `DELETE` commands where a table name is qualified by the `ONLY` keyword.

## Performance Regressions 

The following features are known performance regressions that occur with GPORCA enabled:

-   Short running queries - For GPORCA, short running queries might encounter additional overhead due to GPORCA enhancements for determining an optimal query execution plan.
-   ANALYZE - For GPORCA, the ANALYZE command generates root partition statistics for partitioned tables. For the legacy optimizer, these statistics are not generated.
-   DML operations - For GPORCA, DML enhancements including the support of updates on partition and distribution keys might require additional overhead.

Also, enhanced functionality of the features from previous versions could result in additional time required when GPORCA executes SQL statements with the features.

