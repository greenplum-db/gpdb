---
title: Memory and Resource Management with Resource Groups 
---

Managing Greenplum Database resources with resource groups.

Memory, CPU, and concurrent transaction management have a significant impact on performance in a Greenplum Database cluster. Resource groups are a newer resource management scheme that enforce memory, CPU, and concurrent transaction limits in Greenplum Database.

-   [Configuring Memory for Greenplum Database](#section_r52_rbl_zt)
-   [Memory Considerations when using Resource Groups](#toolowmem)
-   [Configuring Resource Groups](#configuring_rg)
-   [Low Memory Queries](#section113x)
-   [Administrative Utilities and admin_group Concurrency](#section177x)

## <a id="section_r52_rbl_zt"></a>Configuring Memory for Greenplum Database 

While it is not always possible to increase system memory, you can avoid many out-of-memory conditions by configuring resource groups to manage expected workloads.

The following operating system and Greenplum Database memory settings are significant when you manage Greenplum Database resources with resource groups:

-   **vm.overcommit_memory**

    This Linux kernel parameter, set in [`/etc/sysctl.conf`](../install_guide/prep_os.html#topic3), identifies the method that the operating system uses to determine how much memory can be allocated to processes. `vm.overcommit_memory` must always be set to 2 for Greenplum Database systems.

-   **vm.overcommit_ratio**

    This Linux kernel parameter, set in [`/etc/sysctl.conf`](../install_guide/prep_os.html#topic3), identifies the percentage of RAM that is used for application processes; the remainder is reserved for the operating system. Tune the setting as necessary. If your memory utilization is too low, increase the value; if your memory or swap usage is too high, decrease the setting.

-   **gp_resource_group_memory_limit**

    The percentage of system memory to allocate to Greenplum Database. The default value is .7 \(70%\).

-   **gp_workfile_limit_files_per_query**

    Set `gp_workfile_limit_files_per_query` to limit the maximum number of temporary spill files \(workfiles\) allowed per query. Spill files are created when a query requires more memory than it is allocated. When the limit is exceeded the query is terminated. The default is zero, which allows an unlimited number of spill files and may fill up the file system.

-   **gp_workfile_compression**

    If there are numerous spill files then set `gp_workfile_compression` to compress the spill files. Compressing spill files may help to avoid overloading the disk subsystem with IO operations.

-   **memory_spill_ratio**

    Set `memory_spill_ratio` to increase or decrease the amount of query operator memory Greenplum Database allots to a query. When `memory_spill_ratio` is larger than 0, it represents the percentage of resource group memory to allot to query operators. If concurrency is high, this memory amount may be small even when `memory_spill_ratio` is set to the max value of 100. When you set `memory_spill_ratio` to 0, Greenplum Database uses the `statement_mem` setting to determine the initial amount of query operator memory to allot.

-   **statement_mem**

    When `memory_spill_ratio` is 0, Greenplum Database uses the `statement_mem` setting to determine the amount of memory to allocate to a query.


Other considerations:

-   Do not configure the operating system to use huge pages. See the [Recommended OS Parameters Settings](../install_guide/prep_os.html#topic3/huge_pages) in the *Greenplum Installation Guide*.
-   When you configure resource group memory, consider memory requirements for mirror segments that become primary segments during a failure to ensure that database operations can continue when primary segments or segment hosts fail.

## <a id="toolowmem"></a>Memory Considerations when using Resource Groups 

Available memory for resource groups may be limited on systems that use low or no swap space, and that use the default `vm.overcommit_ratio` and `gp_resource_group_memory_limit` settings. To ensure that Greenplum Database has a reasonable per-segment-host memory limit, you may be required to increase one or more of the following configuration settings:

1.  The swap size on the system.
2.  The system's `vm.overcommit_ratio` setting.
3.  The resource group `gp_resource_group_memory_limit` setting.

## <a id="configuring_rg"></a>Configuring Resource Groups 

Greenplum Database resource groups provide a powerful mechanism for managing the workload of the cluster. Consider these general guidelines when you configure resource groups for your system:

-   A transaction submitted by any Greenplum Database role with `SUPERUSER` privileges runs under the default resource group named `admin_group`. Keep this in mind when scheduling and running Greenplum administration utilities.
-   Ensure that you assign each non-admin role a resource group. If you do not assign a resource group to a role, queries submitted by the role are handled by the default resource group named `default_group`.
-   Use the `CONCURRENCY` resource group parameter to limit the number of active queries that members of a particular resource group can run concurrently.
-   Use the `MEMORY_LIMIT` and `MEMORY_SPILL_RATIO` parameters to control the maximum amount of memory that queries running in the resource group can consume.
-   Greenplum Database assigns unreserved memory \(100 - \(sum of all resource group `MEMORY_LIMIT`s\) to a global shared memory pool. This memory is available to all queries on a first-come, first-served basis.
-   Alter resource groups dynamically to match the real requirements of the group for the workload and the time of day.
-   Use the `gp_toolkit` views to examine resource group resource usage and to monitor how the groups are working.
-   Consider using VMware Greenplum Command Center to create and manage resource groups, and to define the criteria under which Command Center dynamically assigns a transaction to a resource group.

## <a id="section113x"></a>Low Memory Queries 

A low `statement_mem` setting \(for example, in the 10MB range\) has been shown to increase the performance of queries with low memory requirements. Use the `memory_spill_ratio` and `statement_mem` server configuration parameters to override the setting on a per-query basis. For example:

```
SET memory_spill_ratio=0;
SET statement_mem='10 MB';
```

## <a id="section177x"></a>Administrative Utilities and admin_group Concurrency 

The default resource group for database transactions initiated by Greenplum Database `SUPERUSER`s is the group named `admin_group`. The default `CONCURRENCY` value for the `admin_group` resource group is 10.

Certain Greenplum Database administrative utilities may use more than one `CONCURRENCY` slot at runtime, such as `gpbackup` that you invoke with the `--jobs` option. If the utility\(s\) you run require more concurrent transactions than that configured for `admin_group`, consider temporarily increasing the group's `MEMORY_LIMIT` and `CONCURRENCY` values to meet the utility's requirement, making sure to return these parameters back to their original settings when the utility completes.

> **Note** Memory allocation changes that you initiate with `ALTER RESOURCE GROUP` may not take affect immediately due to resource consumption by currently running queries. Be sure to alter resource group parameters in advance of your maintenance window.

**Parent topic:** [Greenplum Database Best Practices](intro.html)

