# ALTER RESOURCE GROUP 

Changes the limits of a resource group.

## <a id="synopsis"></a>Synopsis 

``` {#sql_command_synopsis}
ALTER RESOURCE GROUP <name> SET <group_attribute> <value>
```

where group_attribute is one of:

```
[ CONCURRENCY=<integer> ]
CPU_MAX_PERCENT=<integer> | CPUSET=<coordinator_cores>;<segment_cores>
[ CPU_WEIGHT=<integer> ]
[ MEMORY_LIMIT=<integer> ]
[ MIN_COST=<integer> ]
[ IO_LIMIT=' <tablespace_io_limit_spec> [; ...] ' ]
```

Where `<tablespace_io_limit_spec>` is:

```
<tablespace_name> | <oid> : <io_limit_option_spec> [, ...]
```

Where `<io_limit_option_spec>` is:

```
wbps=<io_limit_option_value>
| rbps=<io_limit_option_value>
| wiops=<io_limit_option_value>
| riops=<io_limit_option_value>
```

Where `<io_limit_option_vlaue>` is:

```
<integer> | max
```

## <a id="description"></a>Description 

`ALTER RESOURCE GROUP` changes the limits of a resource group. Only a superuser can alter a resource group.

You can set or reset the concurrency limit of a resource group that you create for roles to control the maximum number of active concurrent statements in that group. You can also reset the memory or CPU resources of a resource group to control the amount of memory or CPU resources that all queries submitted through the group can consume on each segment host.

When you alter the CPU resource management mode or limit of a resource group, the new mode or limit is immediately applied.

When you alter a memory limit of a resource group that you create for roles, the new resource limit is immediately applied if current resource usage is less than or equal to the new value and there are no running transactions in the resource group. If the current resource usage exceeds the new memory limit value, or if there are running transactions in other resource groups that hold some of the resource, then Greenplum Database defers assigning the new limit until resource usage falls within the range of the new value.

You can alter one limit type in a single `ALTER RESOURCE GROUP` call.

## <a id="parameters"></a>Parameters 

name
:   The name of the resource group to alter.

CONCURRENCY integer
:   The maximum number of concurrent transactions, including active and idle transactions, that are permitted for this resource group. The `CONCURRENCY` value must be an integer in the range \[0 .. `max_connections`\]. The default `CONCURRENCY` value for resource groups defined for roles is 20.

:   You must set `CONCURRENCY` to `0` for resource groups that you create for external components.

:   > **Note** You cannot set the `CONCURRENCY` value for the `admin_group` to `0`.

CPU_MAX_PERCENT integer
:   The percentage of the maximum available CPU resources that the resource group can use. The value range is `1-100`. 

CPU_WEIGHT integer
:   The scheduling priority of the current group. The value range is `1-500`, the default is `100. 

CPUSET <coordinator_cores>;<segment_cores>

:   `CPUSET` identifies the CPU cores to reserve for this resource group on the coordinator host and on segment hosts. The CPU cores that you specify must be available in the system and cannot overlap with any CPU cores that you specify for other resource groups.

:   > **Note** You must specify either `CPU_MAX_PERCENT` or `CPUSET` when you create a resource group, but not both.

:   Specify cores as a comma-separated list of single core numbers or core number intervals. Define the coordinator host cores first, followed by segment host cores, and separate the two with a semicolon. You must enclose the full core configuration in single quotes. For example, '1;1,3-4' configures core 1 for the coordinator host, and cores 1, 3, and 4 for the segment hosts.

:   > **Note** You can configure `CPUSET` for a resource group only after you have enabled resource group-based resource management for your Greenplum Database cluster.

IO_LIMIT='<tablespace_io_limit_spec> [; ...]'
:   Optional. The maximum read/write sequential disk I/O throughput, and the maximum read/write I/O operations per second for the queries assigned to a specific resource group. 

Where `<tablespace_io_limit_spec>` is:

```
<tablespace_name> | <oid> : <io_limit_option_spec> [, ...]
```

Where `<io_limit_option_spec>` is:

```
wbps=<io_limit_option_value>
| rbps=<io_limit_option_value>
| wiops=<io_limit_option_value>
| riops=<io_limit_option_value>
```

Where `<io_limit_option_vlaue>` is:

```
<integer> | max 
```

When you use this parameter, you may speficy:
- The tablespace name or the tablespace object ID (OID) you set the limits for. Use `*` to set limits for all tablespaces.
- The values for `rbps` and `wbps` to limit the maximum read and write sequential disk I/O throughput in the resource group, in MB/S. The default value is `max`, which means there is no limit.
- The values for `riops` and `wiops` to limit the maximum read and write I/O operations per second in the resource group. The default value is `max`, which means there is no limit.

If the parameter `IO_LIMIT` is not set, the default value for `rbps`, `wpbs`, `riops`, and `wiops`s is set to `max`, which means that there are no disk I/O limits. In this scenario, the `gp_toolkit.gp_resgroup_config` system view displays its value as `-1`.

> **Note** The parameter `IO_LIMIT` is only available when you use Linux Control Groups v2. See [Configuring and Using Resource Groups](../../admin_guide/workload_mgmt_resgroups.html#topic71717999) for more information.

MEMORY_LIMIT integer
:   The maximum available memory, in MB, to reserve for this resource group. This value determines the total amount of memory that all worker processes within a resource group can consume on a segment host during query execution. 

:   The minimum memory quantity you can specify for a resource group is `0`. The default value is `-1`. 

:   When you specify a `MEMORY_LIMIT` of `-1`, `MEMORY LIMIT` takes the value of the `statement_mem` server configuration parameter. 

:   > **Note** If the server configuration parameter `gp_resgroup_memory_query_fixed_mem` is set, its value overrides at the session level the value of `MEMORY_LIMIT`.

MIN_COST integer
:   The limit on the cost of the query plan generated by a query in this resource group. When the query plan cost of the query is less than this value, the query will be unassigned from the resource group to which it belongs. 

This means that low-cost queries will execute more quickly, as they are not subject to resource constraints. 

The value range is `0-500`. The default value is `0`, which means that the cost is not used to bypass the query. 

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
ALTER RESOURCE GROUP rgroup2 SET CPU_MAX_PERCENT 45;
```

Update the memory limit for a resource group:

```
ALTER RESOURCE GROUP rgroup3 SET MEMORY_LIMIT 300;
```

Reserve CPU core 1 for a resource group on the coordinator host and all segment hosts:

```
ALTER RESOURCE GROUP rgroup5 SET CPUSET '1;1';
```

Set disk I/O limits for tablespaces `tablespace1` and a tablespace with oid 1663:

```
ALTER RESOURCE GROUP admin_group SET IO_LIMIT 'tablespace1:wbps=2000,wiops=2000;1663:rbps=2024,riops=2024';
```

## <a id="compatibility"></a>Compatibility 

The `ALTER RESOURCE GROUP` statement is a Greenplum Database extension. This command does not exist in standard PostgreSQL.

## <a id="see_also"></a>See Also 

[CREATE RESOURCE GROUP](CREATE_RESOURCE_GROUP.html), [DROP RESOURCE GROUP](DROP_RESOURCE_GROUP.html), [CREATE ROLE](CREATE_ROLE.html), [ALTER ROLE](ALTER_ROLE.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

