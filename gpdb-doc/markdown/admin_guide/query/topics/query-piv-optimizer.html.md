---
title: About GPORCA 
---

In Greenplum Database, the default GPORCA optimizer co-exists with the Postgres Planner.

-   **[Overview of GPORCA](../../query/topics/query-piv-opt-overview.html)**  
GPORCA extends the planning and optimization capabilities of the Postgres Planner.
-   **[Activating and Deactivating GPORCA](../../query/topics/query-piv-opt-enable.html)**  
By default, Greenplum Database uses GPORCA instead of the Postgres Planner. Server configuration parameters activate or deactivate GPORCA.
-   **[Collecting Root Partition Statistics](../../query/topics/query-piv-opt-root-partition.html)**  
For a partitioned table, GPORCA uses statistics of the table root partition to generate query plans. These statistics are used for determining the join order, for splitting and joining aggregate nodes, and for costing the query steps. In contrast, the Postgres Planner uses the statistics of each leaf partition.
-   **[Considerations when Using GPORCA](../../query/topics/query-piv-opt-notes.html)**  
 To run queries optimally with GPORCA, consider the query criteria closely.
-   **[GPORCA Features and Enhancements](../../query/topics/query-piv-opt-features.html)**  
GPORCA, the Greenplum next generation query optimizer, includes enhancements for specific types of queries and operations:
-   **[Changed Behavior with GPORCA](../../query/topics/query-piv-opt-changed.html)**  
There are changes to Greenplum Database behavior with the GPORCA optimizer enabled \(the default\) as compared to the Postgres Planner.
-   **[GPORCA Limitations](../../query/topics/query-piv-opt-limitations.html)**  
There are limitations in Greenplum Database when using the default GPORCA optimizer. GPORCA and the Postgres Planner currently coexist in Greenplum Database because GPORCA does not support all Greenplum Database features.
-   **[Determining the Query Optimizer that is Used](../../query/topics/query-piv-opt-fallback.html)**  
 When GPORCA is enabled \(the default\), you can determine if Greenplum Database is using GPORCA or is falling back to the Postgres Planner.
-   **[About Uniform Multi-level Partitioned Tables](../../query/topics/query-piv-uniform-part-tbl.html)**  


**Parent topic:** [Querying Data](../../query/topics/query.html)

