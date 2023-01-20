---
title: Memory and Resource Management with Resource Queues 
---

Avoid memory errors and manage Greenplum Database resources.

> **Note** Resource groups are a newer resource management scheme that enforces memory, CPU, and concurrent transaction limits in Greenplum Database. The [Managing Resources](../admin_guide/wlmgmt.html) topic provides a comparison of the resource queue and the resource group management schemes. Refer to [Using Resource Groups](../admin_guide/workload_mgmt_resgroups.html) for configuration and usage information for this resource management scheme.

Memory management has a significant impact on performance in a Greenplum Database cluster. The default settings are suitable for most environments. Do not change the default settings until you understand the memory characteristics and usage on your system.

-   [Resolving Out of Memory Errors](#section_jqw_qbl_zt)
-   [Low Memory Queries](#section113x)
-   [Configuring Memory for Greenplum Database](#section_r52_rbl_zt)
-   [Configuring Resource Queues](#configuring_rq)

## <a id="section_jqw_qbl_zt"></a>Resolving Out of Memory Errors 

An out of memory error message identifies the Greenplum segment, host, and process that experienced the out of memory error. For example:

```
Out of memory (seg27 host.example.com pid=47093)
VM Protect failed to allocate 4096 bytes, 0 MB available
```

Some common causes of out-of-memory conditions in Greenplum Database are:

-   Insufficient system memory \(RAM\) available on the cluster
-   Improperly configured memory parameters
-   Data skew at the segment level
-   Operational skew at the query level

Following are possible solutions to out of memory conditions:

-   Tune the query to require less memory
-   Reduce query concurrency using a resource queue
-   Validate the `gp_vmem_protect_limit` configuration parameter at the database level. See calculations for the maximum safe setting in [Configuring Memory for Greenplum Database](#section_r52_rbl_zt).
-   Set the memory quota on a resource queue to limit the memory used by queries run within the resource queue
-   Use a session setting to reduce the `statement_mem` used by specific queries
-   Decrease `statement_mem` at the database level
-   Decrease the number of segments per host in the Greenplum Database cluster. This solution requires a re-initializing Greenplum Database and reloading your data.
-   Increase memory on the host, if possible. \(Additional hardware may be required.\)

Adding segment hosts to the cluster will not in itself alleviate out of memory problems. The memory used by each query is determined by the `statement_mem` parameter and it is set when the query is invoked. However, if adding more hosts allows decreasing the number of segments per host, then the amount of memory allocated in `gp_vmem_protect_limit` can be raised.

## <a id="section113x"></a>Low Memory Queries 

A low `statement_mem` setting \(for example, in the 1-3MB range\) has been shown to increase the performance of queries with low memory requirements. Use the `statement_mem` server configuration parameter to override the setting on a per-query basis. For example:

```
SET statement_mem='2MB';
```

## <a id="section_r52_rbl_zt"></a>Configuring Memory for Greenplum Database 

Most out of memory conditions can be avoided if memory is thoughtfully managed.

It is not always possible to increase system memory, but you can prevent out-of-memory conditions by configuring memory use correctly and setting up resource queues to manage expected workloads.

It is important to include memory requirements for mirror segments that become primary segments during a failure to ensure that the cluster can continue when primary segments or segment hosts fail.

The following are recommended operating system and Greenplum Database memory settings:

-   Do not configure the OS to use huge pages.
-   **vm.overcommit_memory**

    This is a Linux kernel parameter, set in `/etc/sysctl.conf` and it should always be set to 2. It determines the method the OS uses for determining how much memory can be allocated to processes and 2 is the only safe setting for Greenplum Database. Please review the sysctl parameters in the [Greenplum Database Installation Guide](../install_guide/prep_os.html#topic3__sysctl_file).

-   **vm.overcommit_ratio**

    This is a Linux kernel parameter, set in [`/etc/sysctl.conf`](../install_guide/prep_os.html#topic3__sysctl_file). It is the percentage of RAM that is used for application processes. The remainder is reserved for the operating system. The default on Red Hat is 50.

    Setting `vm.overcommit_ratio` too high may result in not enough memory being reserved for the operating system, which can result in segment host failure or database failure. Setting the value too low reduces the amount of concurrency and query complexity that can be run by reducing the amount of memory available to Greenplum Database. When increasing the setting it is important to remember to always reserve some memory for operating system activities.

    See [Greenplum Database Memory Overview](../admin_guide/wlmgmt_intro.html) for instructions to calculate a value for `vm.overcommit_ratio`.

-   **gp_vmem_protect_limit**

    Use `gp_vmem_protect_limit` to set the maximum memory that the instance can allocate for *all* work being done in each segment database. Never set this value larger than the physical RAM on the system. If `gp_vmem_protect_limit` is too high, it is possible for memory to become exhausted on the system and normal operations may fail, causing segment failures. If `gp_vmem_protect_limit` is set to a safe lower value, true memory exhaustion on the system is prevented; queries may fail for hitting the limit, but system disruption and segment failures are avoided, which is the desired behavior.

    See [Resource Queue Segment Memory Configuration](sysconfig.html#segment_mem_config) for instructions to calculate a safe value for `gp_vmem_protect_limit`.

-   **runaway_detector_activation_percent**

    Runaway Query Termination, introduced in Greenplum Database 4.3.4, prevents out of memory conditions. The `runaway_detector_activation_percent` system parameter controls the percentage of `gp_vmem_protect_limit` memory utilized that triggers termination of queries. It is set on by default at 90%. If the percentage of `gp_vmem_protect_limit` memory that is utilized for a segment exceeds the specified value, Greenplum Database terminates queries based on memory usage, beginning with the query consuming the largest amount of memory. Queries are terminated until the utilized percentage of `gp_vmem_protect_limit` is below the specified percentage.

-   **statement_mem**

    Use `statement_mem` to allocate memory used for a query per segment database. If additional memory is required it will spill to disk. Set the optimal value for `statement_mem` as follows:

    ```
    (vmprotect * .9) / max_expected_concurrent_queries
    ```

    The default value of `statement_mem` is 125MB. For example, on a system that is configured with 8 segments per host, a query uses 1GB of memory on each segment server \(8 segments ⨉ 125MB\) with the default `statement_mem` setting. Set `statement_mem` at the session level for specific queries that require additional memory to complete. This setting works well to manage query memory on clusters with low concurrency. For clusters with high concurrency also use resource queues to provide additional control on what and how much is running on the system.

-   **gp_workfile_limit_files_per_query**

    Set `gp_workfile_limit_files_per_query` to limit the maximum number of temporary spill files \(workfiles\) allowed per query. Spill files are created when a query requires more memory than it is allocated. When the limit is exceeded the query is terminated. The default is zero, which allows an unlimited number of spill files and may fill up the file system.

-   **gp_workfile_compression**

    If there are numerous spill files then set `gp_workfile_compression` to compress the spill files. Compressing spill files may help to avoid overloading the disk subsystem with IO operations.


## <a id="configuring_rq"></a>Configuring Resource Queues 

Greenplum Database resource queues provide a powerful mechanism for managing the workload of the cluster. Queues can be used to limit both the numbers of active queries and the amount of memory that can be used by queries in the queue. When a query is submitted to Greenplum Database, it is added to a resource queue, which determines if the query should be accepted and when the resources are available to run it.

-   Associate all roles with an administrator-defined resource queue.

    Each login user \(role\) is associated with a single resource queue; any query the user submits is handled by the associated resource queue. If a queue is not explicitly assigned the user's queries are handed by the default queue, `pg_default`.

-   Do not run queries with the gpadmin role or other superuser roles.

    Superusers are exempt from resource queue limits, therefore superuser queries always run regardless of the limits set on their assigned queue.

-   Use the `ACTIVE_STATEMENTS` resource queue parameter to limit the number of active queries that members of a particular queue can run concurrently.

-   Use the `MEMORY_LIMIT` parameter to control the total amount of memory that queries running through the queue can utilize. By combining the `ACTIVE_STATEMENTS` and `MEMORY_LIMIT` attributes an administrator can fully control the activity emitted from a given resource queue.

    The allocation works as follows: Suppose a resource queue, `sample_queue`, has `ACTIVE_STATEMENTS` set to 10 and `MEMORY_LIMIT` set to 2000MB. This limits the queue to approximately 2 gigabytes of memory per segment. For a cluster with 8 segments per server, the total usage per server is 16 GB for `sample_queue` \(2GB \* 8 segments/server\). If a segment server has 64GB of RAM, there could be no more than four of this type of resource queue on the system before there is a chance of running out of memory \(4 queues \* 16GB per queue\).

    Note that by using `STATEMENT_MEM`, individual queries running in the queue can allocate more than their "share" of memory, thus reducing the memory available for other queries in the queue.

-   Resource queue priorities can be used to align workloads with desired outcomes. Queues with `MAX` priority throttle activity in all other queues until the `MAX` queue completes running all queries.

-   Alter resource queues dynamically to match the real requirements of the queue for the workload and time of day. You can script an operational flow that changes based on the time of day and type of usage of the system and add `crontab` entries to run the scripts.

-   Use gptoolkit to view resource queue usage and to understand how the queues are working.


**Parent topic:** [Greenplum Database Best Practices](intro.html)

