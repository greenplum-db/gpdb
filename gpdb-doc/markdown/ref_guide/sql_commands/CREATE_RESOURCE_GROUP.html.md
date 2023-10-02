# CREATE RESOURCE GROUP 

Defines a new resource group.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
CREATE RESOURCE GROUP <name> WITH (<group_attribute>=<value> [, ... ])
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

## <a id="section3"></a>Description 

Creates a new resource group for Greenplum Database resource management. 

A resource group that you create to manage a user role identifies concurrent transaction, memory, CPU, and disk I/O limits for the role when resource groups are enabled. You may assign such resource groups to one or more roles.

You must have `SUPERUSER` privileges to create a resource group. The maximum number of resource groups allowed in your Greenplum Database cluster is 100.

Greenplum Database pre-defines three default resource groups: `admin_group`, `default_group`, and `system_group`. These group names, as well as the group name `none`, are reserved.

To set appropriate limits for resource groups, the Greenplum Database administrator must be familiar with the queries typically run on the system, as well as the users/roles running those queries.

After creating a resource group for a role, assign the group to one or more roles using the [ALTER ROLE](ALTER_ROLE.html) or [CREATE ROLE](CREATE_ROLE.html) commands.

## <a id="section4"></a>Parameters 

name
:   The name of the resource group.

CONCURRENCY integer
:   Optional. The maximum number of concurrent transactions, including active and idle transactions, that are permitted for this resource group. The `CONCURRENCY` value must be an integer in the range \[0 .. `max_connections`\]. The default `CONCURRENCY` value for resource groups defined for roles is 20.

:   You must set `CONCURRENCY` to `0` for resource groups that you create for external components.

:   > **Note** You cannot set the `CONCURRENCY` value for the `admin_group` to `0`.

CPU_MAX_PERCENT integer
:   Optional. The percentage of the maximum available CPU resources that the resource group can use. The value range is `1-100`. 

CPU_WEIGHT integer
:   Optional. The scheduling priority of the current group. The value range is `1-500`, the default is `100. 

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
:   Optional. The maximum available memory, in MB, to reserve for this resource group. This value determines the total amount of memory that all worker processes within a resource group can consume on a segment host during query execution. 

:   The minimum memory quantity you can specify for a resource group is `0`. The default value is `-1`. 

:   When you specify a `MEMORY_LIMIT` of `-1`, `MEMORY LIMIT` takes the value of the `statement_mem` server configuration parameter. 

:   > **Note** If the server configuration parameter `gp_resgroup_memory_query_fixed_mem` is set, its value overrides at the session level the value of `MEMORY_LIMIT`.

MIN_COST integer
:   Optional. The limit on the cost of the query plan generated by a query in this resource group. When the query plan cost of the query is less than this value, the query will be unassigned from the resource group to which it belongs. 

This means that low-cost queries will execute more quickly, as they are not subject to resource constraints. 

The value range is `0-500`. The default value is `0`, which means that the cost is not used to bypass the query. 

## <a id="section5"></a>Notes 

You cannot submit a `CREATE RESOURCE GROUP` command in an explicit transaction or sub-transaction.

Use the `gp_toolkit.gp_resgroup_config` system view to display the limit settings of all resource groups:

```
SELECT * FROM gp_toolkit.gp_resgroup_config;
```

## <a id="section6"></a>Examples 

Create a resource group with CPU and memory limit of 350 MB:

```
CREATE RESOURCE GROUP rgroup1 WITH (CPU_MAX_PERCENT=35, MEMORY_LIMIT=350);
```

Create a resource group with a concurrent transaction limit of 20, a memory limit of 1500 MB, a CPU limit of 25, and disk I/O limits for the `pg_default` tablespace:

```
CREATE RESOURCE GROUP rgroup2 WITH (CONCURRENCY=20, 
  MEMORY_LIMIT=1500, CPU_MAX_PERCENT=25,
  IO_LIMIT=’pg_default: wbps=1000, rbps=1000, wiops=100, riops=100’);
```

Create a resource group with a concurrent transaction limit of 20, a memory limit of 1500 MB, a CPU limit of 25, and disk I/O limits for a tablespace with oid 1663:

```
CREATE RESOURCE GROUP rgroup2 WITH (CONCURRENCY=20,
  MEMORY_LIMIT=1500, CPU_MAX_PERCENT=25,
  IO_LIMIT=’1663: wbps=1000, rbps=1000, wiops=100, riops=100’);
```

Create a resource group with a memory limit of 110 MB to which you assign CPU core 1 on the coordinator host, and cores 1 to 3 on segment hosts:

```
CREATE RESOURCE GROUP rgroup3 WITH (CPUSET='1;1-3', MEMORY_LIMIT=110);
```

## <a id="section7"></a>Compatibility 

`CREATE RESOURCE GROUP` is a Greenplum Database extension. There is no provision for resource groups or resource management in the SQL standard.

## <a id="section8"></a>See Also 

[ALTER ROLE](ALTER_ROLE.html), [CREATE ROLE](CREATE_ROLE.html), [ALTER RESOURCE GROUP](ALTER_RESOURCE_GROUP.html), [DROP RESOURCE GROUP](DROP_RESOURCE_GROUP.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

