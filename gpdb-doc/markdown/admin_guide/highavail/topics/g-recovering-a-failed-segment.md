# Recovering a Failed Segment 

If the master cannot connect to a segment instance, it marks that segment as down in the Greenplum Database system catalog. The segment instance remains offline until an administrator takes steps to bring the segment back online. The process for recovering a failed segment instance or host depends on the failure cause and whether or not mirroring is enabled. A segment instance can be unavailable for many reasons:

-   A segment host is unavailable; for example, due to network or hardware failures.
-   A segment instance is not running; for example, there is no `postgres` database listener process.
-   The data directory of the segment instance is corrupt or missing; for example, data is not accessible, the file system is corrupt, or there is a disk failure.

[Figure 1](#ki155628) shows the high-level steps for each of the preceding failure scenarios.

![](../../graphics/recovermatrix.png "Segment Failure Troubleshooting Matrix")

![](../../graphics/recovermatrix.png "Segment Failure Troubleshooting Matrix")

-   **[Recovering From Segment Failures](../../highavail/topics/g-recovering-from-segment-failures.html)**  


**Parent topic:** [Enabling High Availability and Data Consistency Features](../../highavail/topics/g-enabling-high-availability-features.html)

