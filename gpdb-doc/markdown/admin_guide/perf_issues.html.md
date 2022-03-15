---
title: Common Causes of Performance Issues 
---

This section explains the troubleshooting processes for common performance issues and potential solutions to these issues.

**Parent topic:**[Managing Performance](partV.html)

## <a id="topic2"></a>Identifying Hardware and Segment Failures 

The performance of Greenplum Database depends on the hardware and IT infrastructure on which it runs. Greenplum Database is comprised of several servers \(hosts\) acting together as one cohesive system \(array\); as a first step in diagnosing performance problems, ensure that all Greenplum Database segments are online. Greenplum Database's performance will be as fast as the slowest host in the array. Problems with CPU utilization, memory management, I/O processing, or network load affect performance. Common hardware-related issues are:

-   **Disk Failure** – Although a single disk failure should not dramatically affect database performance if you are using RAID, disk resynchronization does consume resources on the host with failed disks. The `gpcheckperf` utility can help identify segment hosts that have disk I/O issues.
-   **Host Failure** – When a host is offline, the segments on that host are nonoperational. This means other hosts in the array must perform twice their usual workload because they are running the primary segments and multiple mirrors. If mirrors are not enabled, service is interrupted. Service is temporarily interrupted to recover failed segments. The `gpstate` utility helps identify failed segments.
-   **Network Failure** – Failure of a network interface card, a switch, or DNS server can bring down segments. If host names or IP addresses cannot be resolved within your Greenplum array, these manifest themselves as interconnect errors in Greenplum Database. The `gpcheckperf` utility helps identify segment hosts that have network issues.
-   **Disk Capacity** – Disk capacity on your segment hosts should never exceed 70 percent full. Greenplum Database needs some free space for runtime processing. To reclaim disk space that deleted rows occupy, run `VACUUM` after loads or updates.The *gp\_toolkit* administrative schema has many views for checking the size of distributed database objects.

    See the *Greenplum Database Reference Guide* for information about checking database object sizes and disk space.


## <a id="topic3"></a>Managing Workload 

A database system has a limited CPU capacity, memory, and disk I/O resources. When multiple workloads compete for access to these resources, database performance degrades. Resource management maximizes system throughput while meeting varied business requirements. Greenplum Database provides resource queues and resource groups to help you manage these system resources.

Resource queues and resource groups limit resource usage and the total number of concurrent queries running in the particular queue or group. By assigning database roles to the appropriate queue or group, administrators can control concurrent user queries and prevent system overload. For more information about resource queues and resource groups, including selecting the appropriate scheme for your Greenplum Database environment, see [Managing Resources](wlmgmt.html).

Greenplum Database administrators should run maintenance workloads such as data loads and `VACUUM ANALYZE` operations after business hours. Do not compete with database users for system resources; perform administrative tasks at low-usage times.

## <a id="topic4"></a>Avoiding Contention 

Contention arises when multiple users or workloads try to use the system in a conflicting way; for example, contention occurs when two transactions try to update a table simultaneously. A transaction that seeks a table-level or row-level lock will wait indefinitely for conflicting locks to be released. Applications should not hold transactions open for long periods of time, for example, while waiting for user input.

## <a id="topic5"></a>Maintaining Database Statistics 

Greenplum Database uses a cost-based query optimizer that relies on database statistics. Accurate statistics allow the query optimizer to better estimate the number of rows retrieved by a query to choose the most efficient query plan. Without database statistics, the query optimizer cannot estimate how many records will be returned. The optimizer does not assume it has sufficient memory to perform certain operations such as aggregations, so it takes the most conservative action and does these operations by reading and writing from disk. This is significantly slower than doing them in memory. ANALYZE collects statistics about the database that the query optimizer needs.

**Note:** When running an SQL command with GPORCA, Greenplum Database issues a warning if the command performance could be improved by collecting statistics on a column or set of columns referenced by the command. The warning is issued on the command line and information is added to the Greenplum Database log file. For information about collecting statistics on table columns, see the ANALYZE command in the *Greenplum Database Reference Guide*

### <a id="topic6"></a>Identifying Statistics Problems in Query Plans 

Before you interpret a query plan for a query using EXPLAIN or `EXPLAIN ANALYZE`, familiarize yourself with the data to help identify possible statistics problems. Check the plan for the following indicators of inaccurate statistics:

-   **Are the optimizer's estimates close to reality?** Run `EXPLAIN ANALYZE` and see if the number of rows the optimizer estimated is close to the number of rows the query operation returned.
-   **Are selective predicates applied early in the plan?** The most selective filters should be applied early in the plan so fewer rows move up the plan tree.
-   **Is the optimizer choosing the best join order?** When you have a query that joins multiple tables, make sure the optimizer chooses the most selective join order. Joins that eliminate the largest number of rows should be done earlier in the plan so fewer rows move up the plan tree.

See [Query Profiling](query/topics/query-profiling.html) for more information about reading query plans.

### <a id="topic7"></a>Tuning Statistics Collection 

The following configuration parameters control the amount of data sampled for statistics collection:

-   `default_statistics_target`

These parameters control statistics sampling at the system level. It is better to sample only increased statistics for columns used most frequently in query predicates. You can adjust statistics for a particular column using the command:

`ALTER TABLE...SET STATISTICS`

For example:

```
ALTER TABLE sales ALTER COLUMN region SET STATISTICS 50;

```

This is equivalent to changing `default_statistics_target` for a particular column. Subsequent `ANALYZE` operations will then gather more statistics data for that column and produce better query plans as a result.

## <a id="topic8"></a>Optimizing Data Distribution 

When you create a table in Greenplum Database, you must declare a distribution key that allows for even data distribution across all segments in the system. Because the segments work on a query in parallel, Greenplum Database will always be as fast as the slowest segment. If the data is unbalanced, the segments that have more data will return their results slower and therefore slow down the entire system.

## <a id="topic9"></a>Optimizing Your Database Design 

Many performance issues can be improved by database design. Examine your database design and consider the following:

-   Does the schema reflect the way the data is accessed?
-   Can larger tables be broken down into partitions?
-   Are you using the smallest data type possible to store column values?
-   Are columns used to join tables of the same datatype?
-   Are your indexes being used?

### <a id="topic10"></a>Greenplum Database Maximum Limits 

To help optimize database design, review the maximum limits that Greenplum Database supports:

|Dimension|Limit|
|---------|-----|
|Database Size|Unlimited|
|Table Size|Unlimited, 128 TB per partition per segment|
|Row Size|1.6 TB \(1600 columns \* 1 GB\)|
|Field Size|1 GB|
|Rows per Table|281474976710656 \(2^48\)|
|Columns per Table/View|1600|
|Indexes per Table|Unlimited|
|Columns per Index|32|
|Table-level Constraints per Table|Unlimited|
|Table Name Length|63 Bytes \(Limited by *name* data type\)|

Dimensions listed as unlimited are not intrinsically limited by Greenplum Database. However, they are limited in practice to available disk space and memory/swap space. Performance may degrade when these values are unusually large.

**Note:**

There is a maximum limit on the number of objects \(tables, indexes, and views, but not rows\) that may exist at one time. This limit is 4294967296 \(2^32\).

