# CREATE RESOURCE GROUP 

Defines a new resource group.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
CREATE RESOURCE GROUP <name> WITH (<group_attribute>=<value> [, ... ])
```

where group\_attribute is:

```
CPU_HARD_QUOTA_LIMIT=<integer> | CPUSET=<coordinator_cores>;<segment_cores>
[ MEMORY_LIMIT=<integer> ]
[ CONCURRENCY=<integer> ]
[ MEMORY_SHARED_QUOTA=<integer> ]
[ MEMORY_SPILL_RATIO=<integer> ]
[ MEMORY_AUDITOR= {vmtracker | cgroup} ]
```

## <a id="section3"></a>Description 

Creates a new resource group for Greenplum Database resource management. You can create resource groups to manage resources for roles or to manage the resources of a Greenplum Database external component such as PL/Container.

A resource group that you create to manage a user role identifies concurrent transaction, memory, and CPU limits for the role when resource groups are enabled. You may assign such resource groups to one or more roles.

A resource group that you create to manage the resources of a Greenplum Database external component such as PL/Container identifies the memory and CPU limits for the component when resource groups are enabled. These resource groups use cgroups for both CPU and memory management. Assignment of resource groups to external components is component-specific. For example, you assign a PL/Container resource group when you configure a PL/Container runtime. You cannot assign a resource group that you create for external components to a role, nor can you assign a resource group that you create for roles to an external component.

You must have `SUPERUSER` privileges to create a resource group. The maximum number of resource groups allowed in your Greenplum Database cluster is 100.

Greenplum Database pre-defines two default resource groups: `admin_group` and `default_group`. These group names, as well as the group name `none`, are reserved.

To set appropriate limits for resource groups, the Greenplum Database administrator must be familiar with the queries typically run on the system, as well as the users/roles running those queries and the external components they may be using, such as PL/Containers.

After creating a resource group for a role, assign the group to one or more roles using the [ALTER ROLE](ALTER_ROLE.html) or [CREATE ROLE](CREATE_ROLE.html) commands.

After you create a resource group to manage the CPU and memory resources of an external component, configure the external component to use the resource group. For example, configure the PL/Container runtime `resource_group_id`.

## <a id="section4"></a>Parameters 

name
:   The name of the resource group.

CONCURRENCY integer
:   The maximum number of concurrent transactions, including active and idle transactions, that are permitted for this resource group. The `CONCURRENCY` value must be an integer in the range \[0 .. `max_connections`\]. The default `CONCURRENCY` value for resource groups defined for roles is 20.

:   You must set `CONCURRENCY` to zero \(0\) for resource groups that you create for external components.

:   > **Note** You cannot set the `CONCURRENCY` value for the `admin_group` to zero \(0\).

CPU\_RATE\_LIMIT integer
CPUSET <coordinator_cores>;<segment_cores>
:   Required. You must specify only one of `CPU_RATE_LIMIT` or `CPUSET` when you create a resource group.

:   `cpu_hard_quota_limit` is the percentage of CPU resources to allocate to this resource group. The minimum CPU percentage you can specify for a resource group is 1. The maximum is 100. The sum of the `cpu_hard_quota_limit` values specified for all resource groups defined in the Greenplum Database cluster must be less than or equal to 100.

:   `CPUSET` identifies the CPU cores to reserve for this resource group on the coordinator host and on segment hosts. The CPU cores that you specify must be available in the system and cannot overlap with any CPU cores that you specify for other resource groups.

:   Specify cores as a comma-separated list of single core numbers or core number intervals. Define the coordinator host cores first, followed by segment host cores, and separate the two with a semicolon. You must enclose the full core configuration in single quotes. For example, '1;1,3-4' configures core 1 for the coordinator host, and cores 1, 3, and 4 for the segment hosts.

:   > **Note** You can configure `CPUSET` for a resource group only after you have enabled resource group-based resource management for your Greenplum Database cluster.

MEMORY\_LIMIT integer
:   The total percentage of Greenplum Database memory resources to reserve for this resource group. The minimum memory percentage you can specify for a resource group is 0. The maximum is 100. The default value is 0.

:   When you specify a `MEMORY_LIMIT` of 0, Greenplum Database reserves no memory for the resource group, but uses global shared memory to fulfill all memory requests in the group. If `MEMORY_LIMIT` is 0, `MEMORY_SPILL_RATIO` must also be 0.

:   The sum of the `MEMORY_LIMIT` values specified for all resource groups defined in the Greenplum Database cluster must be less than or equal to 100.

MEMORY\_SHARED\_QUOTA integer
:   The quota of shared memory in the resource group. Resource groups with a `MEMORY_SHARED_QUOTA` threshold set aside a percentage of memory allotted to the resource group to share across transactions. This shared memory is allocated on a first-come, first-served basis as available. A transaction may use none, some, or all of this memory. The minimum memory shared quota percentage you can specify for a resource group is 0. The maximum is 100. The default `MEMORY_SHARED_QUOTA` value is 80.

MEMORY\_SPILL\_RATIO integer
:   The memory usage threshold for memory-intensive operators in a transaction. When this threshold is reached, a transaction spills to disk. You can specify an integer percentage value from 0 to 100 inclusive. The default `MEMORY_SPILL_RATIO` value is 0. When `MEMORY_SPILL_RATIO` is 0, Greenplum Database uses the [`statement_mem`](../config_params/guc-list.html) server configuration parameter value to control initial query operator memory.

MEMORY\_AUDITOR \{vmtracker \| cgroup\}
:   The memory auditor for the resource group. Greenplum Database employs virtual memory tracking for role resources and cgroup memory tracking for resources used by external components. The default `MEMORY_AUDITOR` is `vmtracker`. When you create a resource group with `vmtracker` memory auditing, Greenplum Database tracks that resource group's memory internally.

:   When you create a resource group specifying the `cgroup` `MEMORY_AUDITOR`, Greenplum Database defers the accounting of memory used by that resource group to cgroups. `CONCURRENCY` must be zero \(0\) for a resource group that you create for external components such as PL/Container. You cannot assign a resource group that you create for external components to a Greenplum Database role.

## <a id="section5"></a>Notes 

You cannot submit a `CREATE RESOURCE GROUP` command in an explicit transaction or sub-transaction.

Use the `gp_toolkit.gp_resgroup_config` system view to display the limit settings of all resource groups:

```
SELECT * FROM gp_toolkit.gp_resgroup_config;
```

## <a id="section6"></a>Examples 

Create a resource group with CPU and memory limit percentages of 35:

```
CREATE RESOURCE GROUP rgroup1 WITH (cpu_hard_quota_limit=35, MEMORY_LIMIT=35);
```

Create a resource group with a concurrent transaction limit of 20, a memory limit of 15, and a CPU limit of 25:

```
CREATE RESOURCE GROUP rgroup2 WITH (CONCURRENCY=20, 
  MEMORY_LIMIT=15, cpu_hard_quota_limit=25);
```

Create a resource group to manage PL/Container resources specifying a memory limit of 10, and a CPU limit of 10:

```
CREATE RESOURCE GROUP plc_run1 WITH (MEMORY_LIMIT=10, cpu_hard_quota_limit=10,
  CONCURRENCY=0, MEMORY_AUDITOR=cgroup);
```

Create a resource group with a memory limit percentage of 11 to which you assign CPU core 1 on the coordinator host, and cores 1 to 3 on segment hosts:

```
CREATE RESOURCE GROUP rgroup3 WITH (CPUSET='1;1-3', MEMORY_LIMIT=11);
```

## <a id="section7"></a>Compatibility 

`CREATE RESOURCE GROUP` is a Greenplum Database extension. There is no provision for resource groups or resource management in the SQL standard.

## <a id="section8"></a>See Also 

[ALTER ROLE](ALTER_ROLE.html), [CREATE ROLE](CREATE_ROLE.html), [ALTER RESOURCE GROUP](ALTER_RESOURCE_GROUP.html), [DROP RESOURCE GROUP](DROP_RESOURCE_GROUP.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

