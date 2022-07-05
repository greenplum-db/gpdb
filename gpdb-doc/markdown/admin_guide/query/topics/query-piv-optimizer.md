# About GPORCA 

In Greenplum Database, the default GPORCA optimizer co-exists with the Postgres Planner.

These sections describe GPORCA functionality and usage:

-   **[Overview of GPORCA](../../query/topics/query-piv-opt-overview.html)**  
GPORCA extends the planning and optimization capabilities of the Greenplum Database legacy optimizer.
-   **[Enabling and Disabling GPORCA](../../query/topics/query-piv-opt-enable.html)**  
By default, Greenplum Database uses GPORCA instead of the legacy query planner. Server configuration parameters enable or disable GPORCA.
-   **[Collecting Root Partition Statistics](../../query/topics/query-piv-opt-root-partition.html)**  
For a partitioned table, GPORCA uses statistics of the table root partition to generate query plans. These statistics are used for determining the join order, for splitting and joining aggregate nodes, and for costing the query steps. In contrast, the legacy planner uses the statistics of each leaf partition.
-   **[Considerations when Using GPORCA](../../query/topics/query-piv-opt-notes.html)**  
 To execute queries optimally with GPORCA, query criteria to consider.
-   **[GPORCA Features and Enhancements](../../query/topics/query-piv-opt-features.html)**  
GPORCA, the Greenplum next generation query optimizer, includes enhancements for specific types of queries and operations:
-   **[Changed Behavior with the GPORCA](../../query/topics/query-piv-opt-changed.html)**  
There are changes to Greenplum Database behavior with the GPORCA optimizer enabled \(the default\) as compared to the legacy planner.
-   **[GPORCA Limitations](../../query/topics/query-piv-opt-limitations.html)**  
There are limitations in Greenplum Database when using the default GPORCA optimizer. GPORCA and the legacy query optimizer currently coexist in Greenplum Database because GPORCA does not support all Greenplum Database features.
-   **[Determining the Query Optimizer that is Used](../../query/topics/query-piv-opt-fallback.html)**  
 When GPORCA is enabled \(the default\), you can determine if Greenplum Database is using GPORCA or is falling back to the legacy query optimizer.
-   **[About Uniform Multi-level Partitioned Tables](../../query/topics/query-piv-uniform-part-tbl.html)**  


**Parent topic:** [Querying Data](../../query/topics/query.html)

