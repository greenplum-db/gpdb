# Enabling High Availability and Data Consistency Features 

The fault tolerance and the high-availability features of Greenplum Database can be configured.

**Important:** When data loss is not acceptable for a Greenplum Database cluster, Greenplum master and segment mirroring is recommended. If mirroring is not enabled then Greenplum stores only one copy of the data, so the underlying storage media provides the only guarantee for data availability and correctness in the event of a hardware failure.

Kubernetes enables quick recovery from both pod and host failures, and Kubernetes storage services provide a high level of availability for the underlying data. Furthermore, virtualized environments make it difficult to ensure the anti-affinity guarantees required for Greenplum mirroring solutions. For these reasons, mirrorless deployments are fully supported with Greenplum for Kubernetes. Other deployment environments are generally not supported for production use unless both Greenplum master and segment mirroring are enabled.

For information about the utilities that are used to enable high availability, see the *Greenplum Database Utility Guide*.

-   **[Overview of Greenplum Database High Availability](../../highavail/topics/g-overview-of-high-availability-in-greenplum-database.html)**  
A Greenplum Database cluster can be made highly available by providing a fault-tolerant hardware platform, by enabling Greenplum Database high-availability features, and by performing regular monitoring and maintenance procedures to ensure the health of all system components.
-   **[Enabling Mirroring in Greenplum Database](../../highavail/topics/g-enabling-mirroring-in-greenplum-database.html)**  

-   **[Detecting a Failed Segment](../../highavail/topics/g-detecting-a-failed-segment.html)**  

-   **[Recovering a Failed Segment](../../highavail/topics/g-recovering-a-failed-segment.html)**  

-   **[Recovering a Failed Master](../../highavail/topics/g-recovering-a-failed-master.html)**  


**Parent topic:** [Managing a Greenplum System](../../managing/partII.html)

