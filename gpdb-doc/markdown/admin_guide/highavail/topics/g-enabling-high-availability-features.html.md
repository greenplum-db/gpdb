---
title: Enabling High Availability and Data Consistency Features 
---

The fault tolerance and the high-availability features of Greenplum Database can be configured.

> **Important** When data loss is not acceptable for a Greenplum Database cluster, Greenplum coordinator and segment mirroring is recommended. If mirroring is not enabled then Greenplum stores only one copy of the data, so the underlying storage media provides the only guarantee for data availability and correctness in the event of a hardware failure.

The VMware Greenplum on vSphere virtualized environment ensures the enforcement of anti-affinity rules required for Greenplum mirroring solutions and fully supports mirrorless deployments. Other virtualized or containerized deployment environments are generally not supported for production use unless both Greenplum coordinator and segment mirroring are enabled.

For information about the utilities that are used to enable high availability, see the *Greenplum Database Utility Guide*.

-   **[Overview of Greenplum Database High Availability](../../highavail/topics/g-overview-of-high-availability-in-greenplum-database.html)**  

-   **[Enabling Mirroring in Greenplum Database](../../highavail/topics/g-enabling-mirroring-in-greenplum-database.html)**  

-   **[How Greenplum Database Detects a Failed Segment](../../highavail/topics/g-detecting-a-failed-segment.html)**  

-   **[Understanding Segment Recovery](../../highavail/topics/g-understanding-segment-recovery.html)**  

-   **[Recovering from Segment Failures](../../highavail/topics/g-recovering-from-segment-failures.html)**  

-   **[Recovering a Failed Coordinator](../../highavail/topics/g-recovering-a-failed-master.html)**  


**Parent topic:** [Managing a Greenplum System](../../managing/partII.html)

