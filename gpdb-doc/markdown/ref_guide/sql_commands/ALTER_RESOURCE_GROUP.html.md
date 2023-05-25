# ALTER RESOURCE GROUP 

Changes the limits of a resource group.

## <a id="synopsis"></a>Synopsis 

``` {#sql_command_synopsis}
ALTER RESOURCE GROUP <name> SET <group_attribute> <value>
```

where group_attribute is one of:

```
CPU_HARD_QUOTA_LIMIT=<integer> | CPUSET=<coordinator_cores>;<segment_cores>
[ MEMORY_LIMIT=<integer> ]
[ CPU_SOFT_PRIORITY=<integer> ]
[ CONCURRENCY=<integer> ]
[ MIN_COST=<integer> ]
```

## <a id="description"></a>Description 

`ALTER RESOURCE GROUP` changes the limits of a resource group. Only a superuser can alter a resource group.

You can set or reset the concurrency limit of a resource group that you create for roles to control the maximum number of active concurrent statements in that group. You can also reset the memory or CPU resources of a resource group to control the amount of memory or CPU resources that all queries submitted through the group can consume on each segment host.

When you alter the CPU resource management mode or limit of a resource group, the new mode or limit is immediately applied.

When you alter a memory limit of a resource group that you create for roles, the new resource limit is immediately applied if current resource usage is less than or equal to the new value and there are no running transactions in the resource group. If the current resource usage exceeds the new memory limit value, or if there are running transactions in other resource groups that hold some of the resource, then Greenplum Database defers assigning the new limit until resource usage falls within the range of the new value.

When you increase the memory limit of a resource group that you create for external components, the new resource limit is phased in as system memory resources become available. If you decrease the memory limit of a resource group that you create for external components, the behavior is component-specific. For example, if you decrease the memory limit of a resource group that you create for a PL/Container runtime, queries in a running container may fail with an out of memory error.

You can alter one limit type in a single `ALTER RESOURCE GROUP` call.

## <a id="parameters"></a>Parameters 

name
:   The name of the resource group to alter.

CONCURRENCY integer
:   The maximum number of concurrent transactions, including active and idle transactions, that are permitted for resource groups that you assign to roles. Any transactions submitted after the `CONCURRENCY` value limit is reached are queued. When a running transaction completes, the earliest queued transaction is run.

:   The `CONCURRENCY` value must be an integer in the range \[0 .. `max_connections`\]. The default `CONCURRENCY` value for a resource group that you create for roles is 20.

:   > **Note** You cannot set the `CONCURRENCY` value for the `admin_group` to zero \(0\).

:   If you alter the `cpu_hard_quota_limit` of a resource group in which you previously configured a `CPUSET`, `CPUSET` is deactivated, the reserved CPU cores are returned to Greenplum Database, and `CPUSET` is set to -1.

CPU_HARD_QUOTA_LIMIT integer
:   The percentage of CPU resources to allocate to this resource group. The minimum CPU percentage you can specify for a resource group is 1. The maximum is 100. The sum of the `CPU_HARD_QUOTA_LIMIT` values specified for all resource groups defined in the Greenplum Database cluster must be less than or equal to 100.

CPUSET <coordinator_cores>;<segment_cores>
:   The CPU cores to reserve for this resource group on the coordinator host and segment hosts. The CPU cores that you specify in must be available in the system and cannot overlap with any CPU cores that you specify for other resource groups.

:   Specify cores as a comma-separated list of single core numbers or core intervals. Define the coordinator host cores first, followed by segment host cores, and separate the two with a semicolon. You must enclose the full core configuration in single quotes. For example, '1;1,3-4' configures core 1 for the coordinator host, and cores 1, 3, and 4 for the segment hosts.

:   If you alter the `CPUSET` value of a resource group for which you previously configured a `cpu_hard_quota_limit`, `cpu_hard_quota_limit` is deactivated, the reserved CPU resources are returned to Greenplum Database, and `cpu_hard_quota_limit` is set to -1.

:   You can alter `CPUSET` for a resource group only after you have enabled resource group-based resource management for your Greenplum Database cluster.

MEMORY_LIMIT integer
:   The percentage of Greenplum Database memory resources to reserve for this resource group. The minimum memory percentage for a resource group is 0. The maximum is 100. The default value is 0.

:   When `MEMORY_LIMIT` is 0, Greenplum Database reserves no memory for the resource group, but uses global shared memory to fulfill all memory requests in the group.

:   The sum of the `MEMORY_LIMIT`s of all resource groups defined in the Greenplum Database cluster must not exceed 100. If this sum is less than 100, Greenplum Database allocates any unreserved memory to a resource group global shared memory pool.

## <a id="notes"></a>Notes 

Use [CREATE ROLE](CREATE_ROLE.html) or [ALTER ROLE](ALTER_ROLE.html) to assign a specific resource group to a role \(user\).

You cannot submit an `ALTER RESOURCE GROUP` command in an explicit transaction or sub-transaction.

## <a id="examples"></a>Examples 

Change the active transaction limit for a resource group:

```
ALTER RESOURCE GROUP rgroup1 SET CONCURRENCY 13;
```

Update the CPU limit for a resource group:

```
ALTER RESOURCE GROUP rgroup2 SET cpu_hard_quota_limit 45;
```

Update the memory limit for a resource group:

```
ALTER RESOURCE GROUP rgroup3 SET MEMORY_LIMIT 30;
```

Reserve CPU core 1 for a resource group on the coordinator host and all segment hosts:

```
ALTER RESOURCE GROUP rgroup5 SET CPUSET '1;1';
```

## <a id="compatibility"></a>Compatibility 

The `ALTER RESOURCE GROUP` statement is a Greenplum Database extension. This command does not exist in standard PostgreSQL.

## <a id="see_also"></a>See Also 

[CREATE RESOURCE GROUP](CREATE_RESOURCE_GROUP.html), [DROP RESOURCE GROUP](DROP_RESOURCE_GROUP.html), [CREATE ROLE](CREATE_ROLE.html), [ALTER ROLE](ALTER_ROLE.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

