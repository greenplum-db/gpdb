---
title: ALTER RESOURCE GROUP 
---

Changes the limits of a resource group.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
ALTER RESOURCE GROUP <name> SET <group_attribute> <value>
```

where group\_attribute is one of:

```
CONCURRENCY <integer>
CPU_RATE_LIMIT <integer> 
CPUSET <tuple> 
MEMORY_LIMIT <integer>
MEMORY_SHARED_QUOTA <integer>
MEMORY_SPILL_RATIO <integer>
```

## <a id="section3"></a>Description 

`ALTER RESOURCE GROUP` changes the limits of a resource group. Only a superuser can alter a resource group.

You can set or reset the concurrency limit of a resource group that you create for roles to control the maximum number of active concurrent statements in that group. You can also reset the memory or CPU resources of a resource group to control the amount of memory or CPU resources that all queries submitted through the group can consume on each segment host.

When you alter the CPU resource management mode or limit of a resource group, the new mode or limit is immediately applied.

When you alter a memory limit of a resource group that you create for roles, the new resource limit is immediately applied if current resource usage is less than or equal to the new value and there are no running transactions in the resource group. If the current resource usage exceeds the new memory limit value, or if there are running transactions in other resource groups that hold some of the resource, then Greenplum Database defers assigning the new limit until resource usage falls within the range of the new value.

When you increase the memory limit of a resource group that you create for external components, the new resource limit is phased in as system memory resources become available. If you decrease the memory limit of a resource group that you create for external components, the behavior is component-specific. For example, if you decrease the memory limit of a resource group that you create for a PL/Container runtime, queries in a running container may fail with an out of memory error.

You can alter one limit type in a single `ALTER RESOURCE GROUP` call.

## <a id="section4"></a>Parameters 

name
:   The name of the resource group to alter.

CONCURRENCY integer
:   The maximum number of concurrent transactions, including active and idle transactions, that are permitted for resource groups that you assign to roles. Any transactions submitted after the `CONCURRENCY` value limit is reached are queued. When a running transaction completes, the earliest queued transaction is run.

:   The `CONCURRENCY` value must be an integer in the range \[0 .. `max_connections`\]. The default `CONCURRENCY` value for a resource group that you create for roles is 20.

:   **Note:** You cannot set the `CONCURRENCY` value for the `admin_group` to zero \(0\).

CPU\_RATE\_LIMIT integer
:   The percentage of CPU resources to allocate to this resource group. The minimum CPU percentage for a resource group is 1. The maximum is 100. The sum of the `CPU_RATE_LIMIT`s of all resource groups defined in the Greenplum Database cluster must not exceed 100.

:   If you alter the `CPU_RATE_LIMIT` of a resource group in which you previously configured a `CPUSET`, `CPUSET` is deactivated, the reserved CPU cores are returned to Greenplum Database, and `CPUSET` is set to -1.

CPUSET tuple
:   The CPU cores to reserve for this resource group. The CPU cores that you specify in tuple must be available in the system and cannot overlap with any CPU cores that you specify for other resource groups.

:   tuple is a comma-separated list of single core numbers or core intervals. You must enclose tuple in single quotes, for example, '1,3-4'.

:   If you alter the `CPUSET` value of a resource group for which you previously configured a `CPU_RATE_LIMIT`, `CPU_RATE_LIMIT` is deactivated, the reserved CPU resources are returned to Greenplum Database, and `CPU_RATE_LIMIT` is set to -1.

:   You can alter `CPUSET` for a resource group only after you have enabled resource group-based resource management for your Greenplum Database cluster.

MEMORY\_LIMIT integer
:   The percentage of Greenplum Database memory resources to reserve for this resource group. The minimum memory percentage for a resource group is 0. The maximum is 100. The default value is 0.

:   When `MEMORY_LIMIT` is 0, Greenplum Database reserves no memory for the resource group, but uses global shared memory to fulfill all memory requests in the group. If `MEMORY_LIMIT` is 0, `MEMORY_SPILL_RATIO` must also be 0.

:   The sum of the `MEMORY_LIMIT`s of all resource groups defined in the Greenplum Database cluster must not exceed 100. If this sum is less than 100, Greenplum Database allocates any unreserved memory to a resource group global shared memory pool.

MEMORY\_SHARED\_QUOTA integer
:   The percentage of memory resources to share among transactions in the resource group. The minimum memory shared quota percentage for a resource group is 0. The maximum is 100. The default `MEMORY_SHARED_QUOTA` value is 80.

MEMORY\_SPILL\_RATIO integer
:   The memory usage threshold for memory-intensive operators in a transaction. You can specify an integer percentage value from 0 to 100 inclusive. The default `MEMORY_SPILL_RATIO` value is 0. When `MEMORY_SPILL_RATIO` is 0, Greenplum Database uses the [`statement_mem`](../config_params/guc-list.html) server configuration parameter value to control initial query operator memory.

## <a id="section5"></a>Notes 

Use [CREATE ROLE](CREATE_ROLE.html) or [ALTER ROLE](ALTER_ROLE.html) to assign a specific resource group to a role \(user\).

You cannot submit an `ALTER RESOURCE GROUP` command in an explicit transaction or sub-transaction.

## <a id="section6"></a>Examples 

Change the active transaction limit for a resource group:

```
ALTER RESOURCE GROUP rgroup1 SET CONCURRENCY 13;
```

Update the CPU limit for a resource group:

```
ALTER RESOURCE GROUP rgroup2 SET CPU_RATE_LIMIT 45;
```

Update the memory limit for a resource group:

```
ALTER RESOURCE GROUP rgroup3 SET MEMORY_LIMIT 30;
```

Update the memory spill ratio for a resource group:

```
ALTER RESOURCE GROUP rgroup4 SET MEMORY_SPILL_RATIO 25;
```

Reserve CPU core 1 for a resource group:

```
ALTER RESOURCE GROUP rgroup5 SET CPUSET '1';
```

## <a id="section7"></a>Compatibility 

The `ALTER RESOURCE GROUP` statement is a Greenplum Database extension. This command does not exist in standard PostgreSQL.

## <a id="section8"></a>See Also 

[CREATE RESOURCE GROUP](CREATE_RESOURCE_GROUP.html), [DROP RESOURCE GROUP](DROP_RESOURCE_GROUP.html), [CREATE ROLE](CREATE_ROLE.html), [ALTER ROLE](ALTER_ROLE.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

