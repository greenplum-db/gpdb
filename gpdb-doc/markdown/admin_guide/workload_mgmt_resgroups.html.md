---
title: Using Resource Groups 
---

You use resource groups to manage and protect the resource allocation of CPU, memory, concurrent transaction limits, and disk I/O in Greenplum Database. Once you define a resource group, you assign the group to one or more Greenplum Database roles in order to control the resources used by them. 

When you assign a resource group to a role, the resource limits that you define for the group apply to all of the roles to which you assign the group. For example, the memory limit for a resource group identifies the maximum memory usage for all running transactions submitted by Greenplum Database users in all roles to which you assign the group.

Greenplum Database uses Linux-based control groups for CPU resource management, and Runaway Detector for statistics, tracking and management of memory. 

When using resource groups to control resources like CPU cores, review the Hyperthreading note in [Hardware and Network](../install_guide/platform-requirements-overview.html#hardware-requirements).

**Parent topic:** [Managing Resources](wlmgmt.html)

## <a id="topic8339intro"></a>Understanding Role and Component Resource Groups 

The most common application for resource groups is to manage the number of active queries that different roles may run concurrently in your Greenplum Database cluster. You can also manage the amount of CPU, memory resources, and disk I/O that Greenplum allocates to each query.

When a user runs a query, Greenplum Database evaluates the query against a set of limits defined for the resource group. Greenplum Database runs the query immediately if the group's resource limits have not yet been reached and the query does not cause the group to exceed the concurrent transaction limit. If these conditions are not met, Greenplum Database queues the query. For example, if the maximum number of concurrent transactions for the resource group has already been reached, a subsequent query is queued and must wait until other queries complete before it runs. Greenplum Database may also run a pending query when the resource group's concurrency and memory limits are altered to large enough values.

Within a resource group for roles, transactions are evaluated on a first in, first out basis. Greenplum Database periodically assesses the active workload of the system, reallocating resources and starting/queuing jobs as necessary.

## <a id="topic8339introattrlim"></a>Resource Group Attributes and Limits 

When you create a resource group, you provide a set of limits that determine the amount of CPU and memory resources available to the group. The following table lists the available limits for resource groups:

|Limit Type|Description|Value Range|Default|
|----------|-----------|-----| ------| 
|CONCURRENCY|The maximum number of concurrent transactions, including active and idle transactions, that are permitted in the resource group.| [0 - [max_connections](../ref_guide/config_params/guc-list.html#max_connections)] | 20 |
|CPU_MAX_PERCENT|The maximum percentage of CPU resources the group can use.| [1 - 100] | -1 (not set)|
|CPU_WEIGHT|The scheduling priority of the resource group.| [1 -  500] | 100 |
|CPUSET|The specific CPU logical core (or logical thread in hyperthreading) reserved for this resource group.| It depends on system core configuration | -1 |
|IO_LIMIT| The limit for the maximum read/write disk I/O throughput, and maximum read/write I/O operations per second. Set the value on a per-tablespace basis.| [2 - 4294967295 or `max`] | -1 |
|MEMORY_LIMIT|The memory limit value specified for the resource group.| Integer (MB) | -1 (not set, use `statement_mem` as the memory limit for a single query) | 
|MIN_COST| The minimum cost of a query plan to be included in the resource group.| Integer | 0 |

> **Note** Resource limits are not enforced on `SET`, `RESET`, and `SHOW` commands.

### <a id="topic8339717179"></a>Transaction Concurrency Limit 

The `CONCURRENCY` limit controls the maximum number of concurrent transactions permitted for a resource group. 

Each resource group is logically divided into a fixed number of slots equal to the `CONCURRENCY` limit. Greenplum Database allocates these slots an equal, fixed percentage of memory resources.

The default `CONCURRENCY` limit value for a resource group for roles is 20. A value of 0 means that no query is allowed to run for this resource group.

Greenplum Database queues any transactions submitted after the resource group reaches its `CONCURRENCY` limit. When a running transaction completes, Greenplum Database un-queues and runs the earliest queued transaction if sufficient memory resources exist. Note that if a transaction is in `idle in transaction` state, even if no statement is running, the concurrency slot is still in use.

You can set the server configuration parameter [gp_resource_group_queuing_timeout](../ref_guide/config_params/guc-list.html#gp_resource_group_queuing_timeout) to specify the amount of time a transaction remains in the queue before Greenplum Database cancels the transaction. The default timeout is zero, Greenplum queues transactions indefinitely.

### <a id="bypass"></a>Bypass and Unassign from Resource Groups

A query bypasses the resource group concurrency limit if you set the server configuration parameter [gp_resource_group_bypass](../ref_guide/config_params/guc-list.html#gp_resource_group_bypass). This parameter enables or disables the concurrent transaction limit for the resource group so a query can run immediately. The default value is false, which enforces the limit of the `CONCURRENCY` limit. You may only set this parameter for a single session, not within a transaction or a function. If you set `gp_resource_group_bypass` to true, the query no longer enforces the CPU or memory limits assigned to its resource group. Instead, the memory quota assigned to this query is `statement_mem` per query. If there is not enough memory to satisfy the memory allocation request, the query will fail.

You may bypass queries that only use catalog tables, such as the database Graphical User Interface (GUI) client, which runs catalog queries to obtain metadata. If the server configuration parameter [gp_resource_group_bypass_catalog_query](../ref_guide/config_params/guc-list.html#gp_resource_group_bypass_catalog_query) is set to true (the default), Greenplum Database's resource group scheduler bypasses all queries that read exclusively from system catalogs, or queries that contain in their query text `pg_catalog` schema tables only. These queries are automatically unassigned from its current resource group; they do not enforce the limits of the resource group and do not account for resource usage. The query resources are assigned out of the resource groups and run immediately. The memory quota is `statement_mem` per the query.

You may bypass direct dispatch queries with the server configuration parameter [gp_resource_group_bypass_direct_dispatch](../ref_guide/config_params/guc-list.html#gp_resource_group_bypass_direct_dispatch). A direct dispatch query is a special type of query that only requires a single segment to participate in the execution. In order to improve efficiency, Greenplum optimizes this type of query, using direct dispatch optimization. The system sends the query plan to the execution of a single segment that needs to execute the plan, instead of sending it to all segments for execution. If you set `gp_resource_group_bypass_direct_dispatch` to true, the query no longer enforces the CPU or memory limits assigned to its resource group, so it runs immediately. Instead, the memory quota assigned to this query is `statement_mem` per query. If there is not enough memory to satisfy the memory allocation request, the query will fail. You may only set this parameter for a single session, not within a transaction or a function.

Queries whose plan cost is less than the limit `MIN_COST` are automatically unassigned from their resource group and do not enforce any of its limits. The resources used by the query do not account for the resources of the resource group. The query has a memory quota of `statement_mem`. The default value of `MIN_COST` is 0.

### <a id="topic833971717"></a>CPU Limits 

Greenplum Database leverages Linux control groups to implement CPU resource management. Greenplum Database allocates CPU resources in two ways: 

- By assigning a percentage of CPU resources to resource groups. 
- By assigning specific CPU cores to resource groups 

When you set one of the allocation modes for a resource group, Greenplum Database deactivates the other allocation mode. You may employ both modes of CPU resource allocation simultaneously for different resource groups on the same Greenplum Database cluster. You may also change the CPU resource allocation mode for a resource group at runtime.

Greenplum Database uses the server configuration parameter [gp_resource_group_cpu_limit](../ref_guide/config_params/guc-list.html#gp_resource_group_cpu_limit) to identify the maximum percentage of system CPU resources to allocate to resource groups on each Greenplum Database segment node. The remaining unreserved CPU resources are used for the operating system kernel and Greenplum Database daemons. The amount of CPU available to Greenplum Database queries per host is then divided equally among each segment on the Greenplum node.

> **Note** The default `gp_resource_group_cpu_limit` value may not leave sufficient CPU resources if you are running other workloads on your Greenplum Database cluster nodes, so be sure to adjust this server configuration parameter accordingly. 

> **Caution** Avoid setting `gp_resource_group_cpu_limit` to a value higher than .9. Doing so may result in high workload queries taking near all CPU resources, potentially starving Greenplum Database auxiliary processes.

#### <a id="cpuset"></a>Assigning CPU Resources by Core 

You identify the CPU cores that you want to reserve for a resource group with the `CPUSET` property. The CPU cores that you specify must be available in the system and cannot overlap with any CPU cores that you reserved for other resource groups. Although Greenplum Database uses the cores that you assign to a resource group exclusively for that group, note that those CPU cores may also be used by non-Greenplum processes in the system. When you configure `CPUSET` for a resource group, Greenplum Database deactivates `CPU_MAX_PERCENT` and `CPU_WEIGHT` for the group and sets their value to -1.

Specify CPU cores separately for the coordinator host and segment hosts, separated by a semicolon. Use a comma-separated list of single core numbers or number intervals when you configure cores for `CPUSET`. You must enclose the core numbers/intervals in single quotes, for example, '1;1,3-4' uses core 1 on the coordinator host, and cores 1, 3, and 4 on segment hosts.

When you assign CPU cores to `CPUSET` groups, consider the following:

-   A resource group that you create with `CPUSET` uses the specified cores exclusively. If there are no running queries in the group, the reserved cores are idle and cannot be used by queries in other resource groups. Consider minimizing the number of `CPUSET` groups to avoid wasting system CPU resources.
-   Consider keeping CPU core 0 unassigned. CPU core 0 is used as a fallback mechanism in the following cases:
    -   `admin_group` and `default_group` require at least one CPU core. When all CPU cores are reserved, Greenplum Database assigns CPU core 0 to these default groups. In this situation, the resource group to which you assigned CPU core 0 shares the core with `admin_group` and `default_group`.
    -   If you restart your Greenplum Database cluster with one node replacement and the node does not have enough cores to service all `CPUSET` resource groups, the groups are automatically assigned CPU core 0 to avoid system start failure.
-   Use the lowest possible core numbers when you assign cores to resource groups. If you replace a Greenplum Database node and the new node has fewer CPU cores than the original, or if you back up the database and want to restore it on a cluster with nodes with fewer CPU cores, the operation may fail. For example, if your Greenplum Database cluster has 16 cores, assigning cores 1-7 is optimal. If you create a resource group and assign CPU core 9 to this group, database restore to an 8 core node will fail.

Resource groups that you configure with `CPUSET` have a higher priority on CPU resources. The maximum CPU resource usage percentage for all resource groups configured with `CPUSET` on a segment host is the number of CPU cores reserved divided by the number of all CPU cores, multiplied by 100.

> **Note** You must configure `CPUSET` for a resource group *after* you have enabled resource group-based resource management for your Greenplum Database cluster with the [gp_resource_manager](../ref_guide/config_params/guc-list.html#gp_resource_manager) server configuration parameter.

#### <a id="cpu_max_percent"></a>Assigning CPU Resources by Percentage 

You configure a resource group with `CPU_MAX_PERCENT` in order to assign CPU resources by percentage. When you configure `CPU_MAX_PERCENT` for a resource group, Greenplum Database deactivates `CPUSET` for the group. 

The parameter `CPU_MAX_PERCENT` sets a hard upper limit for the percentage of the segment CPU for resource management. The minimum `CPU_MAX_PERCENT` percentage you can specify for a resource group is 1, the maximum is 100. The sum of `CPU_MAX_PERCENT`s specified for all resource groups that you define in your Greenplum Database cluster can exceed 100. It specifies the total time ratio that all tasks in a resource group can run in a given CPU time period. Once the tasks in the resource group have used up all the time specified by the quota, they are throttled for the remainder of the time specified in that time period, and are not allowed to run until the next time period. 

When tasks in a resource group are idle and not using any CPU time, the leftover time is collected in a global pool of unused CPU cycles. Other resource groups can borrow CPU cycles from this pool. The actual amount of CPU time available to a resource group may vary, depending on the number of resource groups present on the system.

The parameter `CPU_MAX_PERCENT` enforces a hard upper limit of CPU usage. For example, if it is set to 40%, it indicates that although the resource group can temporarily use some idle CPU resources from other groups, the maximum it can use is 40% of the CPU resources available to Greenplum. 

You set the parameter `CPU_WEIGHT` to assign the scheduling priority of the current group. The default value is 100, and the range of values is 1 to 500. The value specifies the relative share of CPU time available to tasks in the resource group. For example, if one resource group has a relative share of 100 and another two groups have a relative share of 50, when processes in all the resource groups try to use 100% of the CPU (that means, the value of `CPU_MAX_PERCENT` for all groups is set to 100), the first resource group gets 50% of all CPU time, and the other two get 25% each. However, if you add another group with a relative share of 100, the first group is only allowed to use 33% of the CPU, and the remaining groups get 16.5%, 16.5%, and 33% respectively. 

For example, consider the following groups:

| Group Name | CONCURRENCY | CPU_MAX_PERCENT | CPU_WEIGHT |
| --------- | ----------- | -------------------- | ----------------- |
| default_group | 20 | 50 | 10 |
| admin_group | 10 | 70 | 30 |
| system_group |10 | 30 | 10 |
| test | 10 | 10 | 10 | 

Roles in `default_group` have an available CPU ratio (determined by `CPU_WEIGHT`) of 10/(10+30+10+10)=16%. This means that they can use at least 16% of the CPU when the system workload is high. When the system has idle CPU resources, they can use more resources, as the hard limit (set by `CPU_MAX_PERCENT`) is 50%.

Roles in `admin_group` have an available CPU ratio of 30/(10+30+10+10)=50% when the system workload is high. When the system has idle CPU resources, they can use resources up to the hard limit of 70%.

Roles in `test` have a CPU ratio of 10/(10+30+10+10)=16%. However, as the hard limit determined by `CPU_MAX_PERCENT` is 10%, they can only use up to 10% of the resources even when the system is idle.

### <a id="topic8339717"></a>Memory Limits 

When you enable resource groups, memory usage is managed at the Greenplum Database segment and resource group levels. You can also manage memory at the transaction level. See [Greenplum Database Memory Overview](wlmgmt_intro.html) to estimate how much memory each Greenplum Database segment has available to use. This will help you estimate how much memory to assign to the resource groups. 

The amount of memory allocated to a query is determined by the following parameters:

The parameter `MEMORY_LIMIT` of a resource group sets the maximum amount of memory reserved for this resource group on a segment. This determines the total amount of memory that all worker processes for a query can consume on the segment host during query execution. The amount of memory allotted to a query is the group memory limit divided by the group concurrency limit: `MEMORY_LIMIT` / `CONCURRENCY`. 

If a query requires a large amount of memory, you may use the server configuration parameter [gp_resgroup_memory_query_fixed_mem](../ref_guide/config_params/guc-list.html#gp_resgroup_memory_query_fixed_mem) to set a fixed memory amount for the query at the session level. This parameter overrides and can surpass the allocated memory of the resource group.

Greenplum allocates memory for an incoming query using the `gp_resgroup_memory_query_fixed_mem` value, if set, to bypass the resource group settings. Otherwise, it uses `MEMORY_LIMIT` / `CONCURRENCY` as the memory allocated for the query. If `MEMORY_LIMIT` is not set, the value for the query memory allocation defaults to [statement_mem](../ref_guide/config_params/guc-list.html#statement_mem).

For all queries, if there is not enough memory in the system, they spill to disk. When the limit [gp_workfile_limit_files_per_query](../ref_guide/config_params/guc-list.html#gp_workfile_limit_files_per_query) is reached, Greenplum Database generates an out of memory (OOM) error.

For example, consider a resource group named `adhoc` with `MEMORY_LIMIT`set to 1.5 GB and `CONCURRENCY` set to 3. By default, each statement submitted to the group is allocated 500 MB of memory. Now consider the following series of events:

1. User `ADHOC_1` submits query `Q1`, overriding `gp_resgroup_memory_query_fixed_mem` to 800MB. The `Q1` statement is admitted into the system.
1. User `ADHOC_2` submits query `Q2`, using the default 500MB.
1. With `Q1` and `Q2` still running, user `ADHOC3` submits query `Q3`, using the default 500MB.

    Queries `Q1` and `Q2` have used 1300MB of the group's 1500MB. However, if there is enough system memory available for query `Q3` in the segment at that time, it can run normally.

1. User `ADHOC4` submits query `Q4`, using `gp_resgroup_memory_query_fixed_mem` set to 700 MB.

    Query `Q4` runs immediately as it bypasses the resource group limits.

There are some special usage considerations regarding memory limits:
- If you set the configuration parameters `gp_resource_group_bypass` or `gp_resource_group_bypass_catalog_query` to bypass the resource group limits, the memory limit for the query takes the value of `statement_mem`.
- When (`MEMORY_LIMIT` / `CONCURRENCY`) < `statement_mem`, Greenplum Database uses `statement_mem` as the fixed amount of memory allocated by query.
- The maximum value of `statement_mem` is capped at [max_statement_mem](../ref_guide/config_params/guc-list.html#max_statement_mem).
- Queries whose plan cost is less than the limit `MIN_COST` use a memory quota of `statement_mem`. 

### <a id="diskio"></a>Disk I/O Limits

Greenplum Database leverages Linux control groups to implement disk I/O limits. The parameter `IO_LIMIT` limits the maximum read/write disk I/O throughput, and the maximum read/write I/O operations per second for the queries assigned to a specific resource group. It allocates bandwidth, ensures the use of high-priority resource groups, and avoids excessive use of disk bandwidth. The value of the parameter is set on a per-tablespace basis. 

> **Note** Disk I/O limits are only available when you use Linux Control Groups v2. See [Configuring and Using Resource Groups](#topic71717999) for more information.

When you limit disk I/O you specify:

- The tablespace name or the tablespace object ID (OID) you set the limits for. Use `*` to set limits for all tablespaces.

- The values for `rbps` and `wbps` to limit the maximum read and write disk I/O throughput in the resource group, in MB/sec. The default value is `max`, which means there is no limit. 

- The values for `riops` and `wiops` to limit the maximum read and write I/O operations per second in the resource group. The default value is `max`, which means there is no limit. 

If the parameter `IO_LIMIT` is not set, the default value for `rbps`, `wpbs`, `riops`, and `wiops`s is set to `max`, which means that there are no disk I/O limits. In this scenario, the `gp_toolkit.gp_resgroup_config` system view displays its value as `-1`. If only some of the values of `IO_LIMIT` are set (for example. `rbps`), the parameters that are not set default to `max` (in this example, `wbps`, `riops`, wiops`).

## <a id="topic71717999"></a>Configuring and Using Resource Groups 

### <a id="topic833"></a>Prerequisites

Greenplum Database resource groups use Linux Control Groups \(cgroups\) to manage CPU resources and disk I/O. There are two versions of cgroups: cgroup v1 and cgroup v2. Greenplum Database 7 supports both versions, but it only supports the parameter `IO_LIMIT` for cgroup v2. The version of Linux Control Groups shipped by default with your Linux distribution depends on the operating system version. For Enterprise Linux 8 and older, the default version is v1. For Enterprise Linux 9 and later, the default version is v2. For detailed information about cgroups, refer to the Control Groups documentation for your Linux distribution.

Verify what version of cgroup is configured in your environment by checking what filesystem is mounted by default during system boot:

```
stat -fc %T /sys/fs/cgroup/
```

For cgroup v1, the output is `tmpfs`. For cgroup v2, output is `cgroup2fs`.

You do not need to change your version of cgroup, you can simply skip to [Configuring cgroup v1](#cgroupv1) or [Configuring cgroup v2](#cgroupv2) in order to complete the configuration prerequisites. However, if you prefer to switch from cgroup v1 to v2, run the following commands as root:

- Red Hat 8/Rocky 8/Oracle 8 systems:
    ```
    grubby --update-kernel=/boot/vmlinuz-$(uname -r) --args="systemd.unified_cgroup_hierarchy=1"
    ```
- Ubuntu systems:
    ```
    vim /etc/default/grub
    # add or modify: GRUB_CMDLINE_LINUX="systemd.unified_cgroup_hierarchy=1"
    update-grub
    ```

If you want to switch from cgroup v2 to v1, run the following commands as root:

- Red Hat 8/Rocky 8/Oracle 8 systems:
    ```
    grubby --update-kernel=/boot/vmlinuz-$(uname -r) --args="systemd.unified_cgroup_hierarchy=0 systemd.legacy_systemd_cgroup_controller"
    ```
- Ubuntu systems:
    ```
    vim /etc/default/grub
    # add or modify: GRUB_CMDLINE_LINUX="systemd.unified_cgroup_hierarchy=0"
    update-grub
    ```

After that, reboot your host in order for the changes to take effect.

#### <a id="cgroupv1"></a>Configuring cgroup v1

Complete the following tasks on each node in your Greenplum Database cluster to set up cgroups v1 for use with resource groups:

1. Locate the cgroups configuration file `/etc/cgconfig.conf`. You must be the superuser or have `sudo` access to edit this file:

    ```
    vi /etc/cgconfig.conf
    ```

1. Add the following configuration information to the file:

    ```
    group gpdb {
         perm {
             task {
                 uid = gpadmin;
                 gid = gpadmin;
             }
             admin {
                 uid = gpadmin;
                 gid = gpadmin;
             }
         }
         cpu {
         }
         cpuacct {
         }
         cpuset {
         }
         memory {
         }
    } 
    ```

    This content configures CPU, CPU accounting, CPU core set, and memory control groups managed by the `gpadmin` user. Greenplum Database uses the memory control group only for monitoring the memory usage.

1. Start the cgroups service on each Greenplum Database node. For Redhat/Oracle/Rocky 8.x and older, run the following as root:

    ```
    cgconfigparser -l /etc/cgconfig.conf 
    ```

1. To automatically recreate Greenplum Database required cgroup hierarchies and parameters when your system is restarted, configure your system to enable the Linux cgroup service daemon `cgconfig.service` at node start-up. To ensure the configuration is persisten after reboot, run the following commands as user root. For Redhat/Oracle/Rocky 8.x and older:

    ```
    systemctl enable cgconfig.service
    ```

    To start the service immediately \(without having to reboot\) enter:

    ```
    systemctl start cgconfig.service
    ```

1. Identify the `cgroup` directory mount point for the node. For Redhat/Oracle/Rocky 8.x and older, run the following as root:

    ```
    grep cgroup /proc/mounts
    ```

    The first line of output identifies the `cgroup` mount point.

1. Verify that you set up the Greenplum Database cgroups configuration correctly by running the following commands. Replace \<cgroup\_mount\_point\> with the mount point that you identified in the previous step:

    ```
    ls -l <cgroup_mount_point>/cpu/gpdb
    ls -l <cgroup_mount_point>/cpuset/gpdb
    ls -l <cgroup_mount_point>/memory/gpdb
    ```

    If these directories exist and are owned by `gpadmin:gpadmin`, you have successfully configured cgroups for Greenplum Database resource management.

#### <a id="cgroupv2"></a>Configuring cgroup v2

1. Configure the system to mount `cgroups-v2` by default during system boot by the `systemd` system and service manager as user root.

    ```
    grubby --update-kernel=ALL --args="systemd.unified_cgroup_hierarchy=1"
    ```

1. Reboot the system for the changes to take effect.
    ```
    reboot now
    ```
1. Create the directory `/sys/fs/cgroup/gpdb`, add all the necessary controllers, and ensure `gpadmin` user has read and write permission on it.
    ```
    mkdir -p /sys/fs/cgroup/gpdb
    echo "+cpuset +io +cpu +memory" | tee -a /sys/fs/cgroup/cgroup.subtree_control
    chown -R gpadmin:gpadmin /sys/fs/cgroup/gpdb
    ```

You may encounter the error `Invalid argument` after running the above commands. This is because cgroups v2 do not support control of real-time processes, and the `cpu` controller can only be enabled when all the real-time processes are in the root cgroup. In this situation, find all real-time processes and move them to the root cgroup before you re-enable the controllers. 

1. Ensure that `gpadmin` has write permission on `/sys/fs/cgroup/cgroup.procs`. This is required to move the Greenplum processes from the user slices to `/sys/fs/cgroup/gpdb/` after the cluster is started in order to manage the postmaster services and all its auxiliary processes.
    ```
    chmod a+w /sys/fs/cgroup/cgroup.procs
    ```
Since resource groups manually manage cgroup files, the above settings will become ineffective after a system reboot. Add the following bash script for systemd so it runs automatically during system startup. Perform the following steps as user root:

1. Create `greenplum-cgroup-v2-config.service`.
   ```
   vim /etc/systemd/system/greenplum-cgroup-v2-config.service
   ```

2. Write the following content into `greenplum-cgroup-v2-config.service`, if the user is not `gpadmin`, replace it with the appropriate user.

   ```
   [Unit]
   Description=Greenplum Cgroup v2 Configuration Service
   
   [Service]
   Type=oneshot
   RemainAfterExit=yes
   WorkingDirectory=/sys/fs/cgroup
   Delegate=yes
   # set hierarchies only if cgroup v2 mounted
   ExecCondition=bash -c '[ xcgroup2fs = x$(stat -fc "%%T" /sys/fs/cgroup) ] || exit 1'
   ExecStart=bash -ec " \
                       [ -d gpdb ] || mkdir gpdb; \
                       chown -R gpadmin:gpadmin gpdb; \
                       chmod a+w cgroup.procs;"
   
   [Install]
   WantedBy=basic.target
   ```
1. Reload systemd daemon and enable the service:
   ```
   systemctl daemon-reload
   systemctl enable greenplum-cgroup-v2-config.service
   ```

## <a id="topic8"></a>Enabling Resource Groups 

When you install Greenplum Database, no resource management policy is enabled by default. To use resource groups, set the [gp_resource_manager](../ref_guide/config_params/guc-list.html#gp_resource_manager) server configuration parameter.

1.  Set the `gp_resource_manager` server configuration parameter to the value `"group"` or `"group-v2"`, depending on the version of cgroup configured on your Linux distribution. For example:

    ```
    gpconfig -c gp_resource_manager -v "group"
    gpconfig -c gp_resource_manager -v "group-v2"
    ```

1.  Restart Greenplum Database:

    ```
    gpstop
    gpstart
    ```

Once enabled, any transaction submitted by a role is directed to the resource group assigned to the role, and is governed by that resource group's concurrency, memory, CPU, and disk I/O limits. 

Greenplum Database creates three default resource groups for roles named `admin_group`, `default_group`, and `system_group`. When you enable resources groups, any role that was not explicitly assigned a resource group is assigned the default group for the role's capability. `SUPERUSER` roles are assigned the `admin_group`, non-admin roles are assigned the group named `default_group`. The resources of the Greenplum Database system processes are assigned to the `system_group`. You cannot manually assign any roles to the `system_group`.

The default resource groups `admin_group`, `default_group`, and `system_group`  are created with the following resource limits:

|Limit Type|admin\_group|default\_group|system_group|
|----------|------------|--------------|------------|
|CONCURRENCY|10|5|0|
|CPU_MAX_PERCENT|10|20|10|
|CPU_WEIGHT|100|100|100|
|CPUSET|-1|-1|-1|
|IO_LIMIT|-1|-1|-1|
|MEMORY\_LIMIT|-1|-1|-1|
|MIN_COST|0|0|0|

## <a id="topic10"></a>Creating Resource Groups 

When you create a resource group for a role, you provide a name and a CPU resource allocation mode (core or percentage). You can optionally provide a concurrent transaction limit, a memory limit, a CPU soft priority, disk I/O limits, and a minimum cost. Use the [CREATE RESOURCE GROUP](../ref_guide/sql_commands/CREATE_RESOURCE_GROUP.html) command to create a new resource group.

When you create a resource group for a role, you must provide a `CPU_MAX_PERCENT` or `CPUSET` limit value. These limits identify the percentage of Greenplum Database CPU resources to allocate to this resource group. You may specify a `MEMORY_LIMIT` to reserve a fixed amount of memory for the resource group. 

For example, to create a resource group named *rgroup1* with a CPU limit of 20, a memory limit of 25, a CPU soft priority of 500, a minimum cost of 50, and disk I/O limits for the `pg_default` tablespace:

```
CREATE RESOURCE GROUP rgroup1 WITH (CONCURRENCY=20, CPU_MAX_PERCENT=20, MEMORY_LIMIT=250, CPU_WEIGHT=500, MIN_COST=50, 
  IO_LIMIT=’pg_default: wbps=1000, rbps=1000, wiops=100, riops=100’);
```

The CPU limit of 20 is shared by every role to which `rgroup1` is assigned. Similarly, the memory limit of 25 is shared by every role to which `rgroup1` is assigned. `rgroup1` utilizes the default `CONCURRENCY` setting of 20.

The [ALTER RESOURCE GROUP](../ref_guide/sql_commands/ALTER_RESOURCE_GROUP.html) command updates the limits of a resource group. To change the limits of a resource group, specify the new values that you want for the group. For example:

```
ALTER RESOURCE GROUP rg_role_light SET CONCURRENCY 7;
ALTER RESOURCE GROUP exec SET MEMORY_LIMIT 30;
ALTER RESOURCE GROUP rgroup1 SET CPUSET '1;2,4';
ALTER RESOURCE GROUP sales SET IO_LIMIT 'tablespace1:wbps=2000,wiops=2000;tablespace2:rbps=2024,riops=2024'; 
```

> **Note** You cannot set or alter the `CONCURRENCY` value for the `admin_group` to zero \(0\).

The [DROP RESOURCE GROUP](../ref_guide/sql_commands/DROP_RESOURCE_GROUP.html) command drops a resource group. To drop a resource group for a role, the group cannot be assigned to any role, nor can there be any transactions active or waiting in the resource group.

To drop a resource group:

```
DROP RESOURCE GROUP exec; 
```

## <a id="topic_jlz_hzg_pkb"></a>Configuring Automatic Query Termination Based on Memory Usage 

Greenplum Database supports the Runaway detector, which automatically terminates queries based on the amount of memory used by the query. For resource group-managed queries, Greenplum Database terminates a running query based on the amount of memory used by the query. The relevant configuration parameters are:

- [gp_vmem_protect_limit](../ref_guide/config_params/guc-list.html#gp_vmem_protect_limit) sets the amount of memory that all `postgres` processes of the active segment instance can consume. If a query causes this limit to be exceeded, no memory will be allocated and the query will fail. 

- [runaway_detector_activation_percent](../ref_guide/config_params/guc-list.html#runaway_detector_activation_percent). When resource groups are enabled, if the used memory exceeds the specified value `gp_vmem_protect_limit` * `runaway_detector_activation_percent`, Greenplum Database terminates queries based on memory usage, selecting queries from the queries managed by user resource groups (excluding those in the `system_group` resource group). Greenplum Database starts with the query that consumes the largest amount of memory. The query will terminate until the percentage of memory used falls below the specified percentage.

## <a id="topic17"></a>Assigning a Resource Group to a Role 

You assign a resource group to a database role using the `RESOURCE GROUP` clause of the [CREATE ROLE](../ref_guide/sql_commands/CREATE_ROLE.html) or [ALTER ROLE](../ref_guide/sql_commands/ALTER_ROLE.html) commands. If you do not specify a resource group for a role, the role is assigned the default group for the role's capability. `SUPERUSER` roles are assigned the `admin_group`, non-admin roles are assigned the group named `default_group`.

Use the `ALTER ROLE` or `CREATE ROLE` commands to assign a resource group to a role. For example:

```
ALTER ROLE bill RESOURCE GROUP rg_light;
CREATE ROLE mary RESOURCE GROUP exec;
```

You can assign a resource group to one or more roles. If you have defined a role hierarchy, assigning a resource group to a parent role does not propagate down to the members of that role group.

If you wish to remove a resource group assignment from a role and assign the role the default group, change the role's group name assignment to `NONE`. For example:

```
ALTER ROLE mary RESOURCE GROUP NONE;
```

## <a id="topic22"></a>Monitoring Resource Group Status 

Monitoring the status of your resource groups and queries may involve the following tasks.

### <a id="topic221"></a>Viewing Resource Group Limits 

The [gp\_resgroup\_config](../ref_guide/system_catalogs/catalog_ref-views.html#gp_resgroup_config) `gp_toolkit` system view displays the current limits for a resource group. To view the limits of all resource groups:

```
SELECT * FROM gp_toolkit.gp_resgroup_config;
```

### <a id="topic23"></a>Viewing Resource Group Query Status

The [gp\_resgroup\_status](../ref_guide/system_catalogs/catalog_ref-views.html#gp_resgroup_status) `gp_toolkit` system view enables you to view the status and activity of a resource group. The view displays the number of running and queued transactions. To view this information:

```
SELECT * FROM gp_toolkit.gp_resgroup_status;
```

### <a id="topic23a"></a>Viewing Resource Group Memory Usage Per Host 

The [gp\_resgroup\_status\_per\_host](../ref_guide/system_catalogs/catalog_ref-views.html#gp_resgroup_status_per_host) `gp_toolkit` system view enables you to view the real-time memory usage of a resource group on a per-host basis. To view this information:

```
SELECT * FROM gp_toolkit.gp_resgroup_status_per_host;
```

### <a id="topic25"></a>Viewing the Resource Group Assigned to a Role 

To view the resource group-to-role assignments, perform the following query on the [pg\_roles](../ref_guide/system_catalogs/pg_roles.html) and [pg\_resgroup](../ref_guide/system_catalogs/pg_resgroup.html) system catalog tables:

```
SELECT rolname, rsgname FROM pg_roles, pg_resgroup
WHERE pg_roles.rolresgroup=pg_resgroup.oid;
```

### <a id="topic25"></a>Viewing Resource Group Disk I/O Usage Per Host

The [gp_resgroup_iostats_per_host](../ref_guide/system_catalogs/catalog_ref-views.html#gp_resgroup_iostats_per_host) `gp_toolkit` system view enables you to view the real-time disk I/O usage of a resource group on a per-host basis. To view this information:

```
SELECT * FROM gp_toolkit.gp_resgroup_iostats_per_host;
```

### <a id="topic252525"></a>Viewing a Resource Group's Running and Pending Queries 

To view a resource group's running queries, pending queries, and how long the pending queries have been queued, examine the [pg\_stat\_activity](../ref_guide/system_catalogs/catalog_ref-views.html#pg_stat_activity) system catalog table:

```
SELECT query, rsgname,wait_event_type, wait_event 
FROM pg_stat_activity;
```

### <a id="topic27"></a>Cancelling a Running or Queued Transaction in a Resource Group 

There may be cases when you want to cancel a running or queued transaction in a resource group. For example, you may want to remove a query that is waiting in the resource group queue but has not yet been run. Or, you may want to stop a running query that is taking too long to run, or one that is sitting idle in a transaction and taking up resource group transaction slots that are needed by other users.

By default, transactions can remain queued in a resource group indefinitely. If you want Greenplum Database to cancel a queued transaction after a specific amount of time, set the server configuration parameter [gp\_resource\_group\_queuing\_timeout](../ref_guide/config_params/guc-list.html#gp_resource_group_queuing_timeout). When this parameter is set to a value \(milliseconds\) greater than 0, Greenplum cancels any queued transaction when it has waited longer than the configured timeout.

To manually cancel a running or queued transaction, you must first determine the process id \(pid\) associated with the transaction. Once you have obtained the process id, you can invoke `pg_cancel_backend()` to end that process, as shown below.

For example, to view the process information associated with all statements currently active or waiting in all resource groups, run the following query. If the query returns no results, then there are no running or queued transactions in any resource group.

```
SELECT rolname, g.rsgname, pid, waiting, state, query, datname 
FROM pg_roles, gp_toolkit.gp_resgroup_status g, pg_stat_activity 
WHERE pg_roles.rolresgroup=g.groupid
AND pg_stat_activity.usename=pg_roles.rolname;
```

Sample partial query output:

```
 rolname | rsgname  | pid     | waiting | state  |          query           | datname 
---------+----------+---------+---------+--------+------------------------ -+---------
  sammy  | rg_light |  31861  |    f    | idle   | SELECT * FROM mytesttbl; | testdb
  billy  | rg_light |  31905  |    t    | active | SELECT * FROM topten;    | testdb
```

Use this output to identify the process id \(`pid`\) of the transaction you want to cancel, and then cancel the process. For example, to cancel the pending query identified in the sample output above:

```
SELECT pg_cancel_backend(31905);
```

You can provide an optional message in a second argument to `pg_cancel_backend()` to indicate to the user why the process was cancelled.

> **Note** Do not use an operating system `KILL` command to cancel any Greenplum Database process.

## <a id="moverg"></a>Moving a Query to a Different Resource Group 

A user with Greenplum Database superuser privileges can run the `gp_toolkit.pg_resgroup_move_query()` function to move a running query from one resource group to another, without stopping the query. Use this function to expedite a long-running query by moving it to a resource group with a higher resource allotment or availability.

> **Note** You can move only an active or running query to a new resource group. You cannot move a queued or pending query that is in an idle state due to concurrency or memory limits.

`pg_resgroup_move_query()` requires the process id \(pid\) of the running query, as well as the name of the resource group to which you want to move the query. The signature of the function follows:

```
pg_resgroup_move_query( pid int4, group_name text );
```

You can obtain the pid of a running query from the `pg_stat_activity` system view as described in [Cancelling a Running or Queued Transaction in a Resource Group](#topic27). Use the `gp_toolkit.gp_resgroup_status` view to list the name, id, and status of each resource group.

When you invoke `pg_resgroup_move_query()`, the query is subject to the limits configured for the destination resource group:

-   If the group has already reached its concurrent task limit, Greenplum Database queues the query until a slot opens or for `gp_resource_group_queuing_timeout` milliseconds if set. 
-   If the group has a free slot, `pg_resgroup_move_query()` tries to give slot control away to the target process for up to `gp_resource_group_move_timeout` milliseconds. If target process can't handle movement request until `gp_resource_group_queuing_timeout` exceeds, Greenplum Database returns the error: `target process failed to move to a new group`.
-   If `pg_resgroup_move_query()` was cancelled, but target process already got all slot controls, segment's processes will not be moved to new group, and target process will hold the slot. Such inconsistent state will be fixed by the end of transaction or by any next command dispatched by target process inside same transaction.
-   If the destination resource group does not have enough memory available to service the query's current memory requirements, Greenplum Database returns the error: `group <group_name> doesn't have enough memory ...`. In this situation, you may choose to increase the group shared memory allotted to the destination resource group, or perhaps wait a period of time for running queries to complete and then invoke the function again.

After Greenplum moves the query, there is no way to guarantee that a query currently running in the destination resource group does not exceed the group memory quota. In this situation, one or more running queries in the destination group may fail, including the moved query. Reserve enough resource group global shared memory to minimize the potential for this scenario to occur.

`pg_resgroup_move_query()` moves only the specified query to the destination resource group. Greenplum Database assigns subsequent queries that you submit in the session to the original resource group.

## <a id="topic999"></a>Using VMware Greenplum Command Center to Manage Resource Groups

Using VMware Greenplum Command Center, an administrator can create and manage resource groups, change roles' resource groups, and create workload management rules.
Workload management assignment rules assign transactions to different resource groups based on user-defined criteria. If no assignment rule is matched, Greenplum Database assigns the transaction to the role's default resource group.
Refer to the [Greenplum Command Center documentation](http://docs.vmware.com/en/VMware-Greenplum-Command-Center/index.html) for more information about creating and managing resource groups and workload management rules.

## <a id="topic777999"></a>Frequently Asked Questions 

**Why is CPU usage lower than the `CPU_MAX_PERCENT` configured for the resource group?**

You may run into this situation when a low number of queries and slices are running in the resource group, and these processes are not utilizing all of the cores on the system.

**My resource group has a `CPU_WEIGHT` equivalent to 40%. Why is the CPU usage never reaching this limit?

The value of `CPU_MAX_PERCENT` might be lower than 40, hence it might be limiting the CPU usage even with idle resources.

**Why is the number of running transactions lower than the `CONCURRENCY` limit configured for the resource group?**

Greenplum Database considers memory availability before running a transaction, and will queue the transaction if there is not enough memory available to serve it. If you use `ALTER RESOURCE GROUP` to increase the `CONCURRENCY` limit for a resource group but do not also adjust memory limits, currently running transactions may be consuming all allotted memory resources for the group. When in this state, Greenplum Database queues subsequent transactions in the resource group.

**Why is the number of running transactions in the resource group higher than the configured `CONCURRENCY` limit?**

This behaviour is expected. There are several reasons why this may happen:

- Resource groups do not enforce resource restrictions on `SET`, `RESET` and `SHOW` commands
- The server configuration parameter `gp_resource_group_bypass` disables the concurrent transaction limit for the resource group so a query can run immediately.
- If the server configuration parameter `gp_resource_group_bypass_catalog_query` is set to true (the default), all queries that read exclusively from system catalogs, or queries that contain in their query text `pg_catalog` schema tables only will not enforce the limits of the resource group. 
- Queries whose plan cost is less than the limit `MIN_COST` will be automatically unassigned from their resource group and will not enforce any of the limits set for this.

**Why did my query return a "memory limit reached" error?**

Greenplum Database automatically adjusts transaction and group memory to the new settings when you use `ALTER RESOURCE GROUP` to change a resource group's memory and/or concurrency limits. An "out of memory" error may occur if you recently altered resource group attributes and there is no longer a sufficient amount of memory available for a currently running query.

**My query cannot run due to insufficient memory, resulting in memory leak Out of Memory (OOM).**

First, ensure that the resource group is allocating enough memory required by the query by tuning resource group parameters such as `CONCURRENCY` and `MEMORY_LIMIT`. Analyze the type of query, whether there will be a lot of intermediate results using memory. If it does exist, you can set a reasonable `gp_resgroup_memory_query_fixed_mem` to allocate more memory at the session level for this specific query. 

**After a memory leak OOM the system has a high concurrent load**.

When the system starts to clean up the sessions left over by the memory leak, the concurrent load of the system is high at this time, and the OOM error message may reappear. Due to the current design, we cannot expedite the cleanup process of the Runaway Session. The solution to this problem is to adjust the `runaway_detector_activation_percent` to 0.85 or 0.8, or even lower, in order to increase the available memory of the segment host. 

**Some transaction requests only run during a certain period of time, and do not run at other times.**

You may change the configuration of resource groups can be changed dynamically at regular intervals to match the requirements of your workload, and customize resource allocation at different times to achieve higher efficiency. For example, change the configuration of resources within a group, add or delete resource groups. 

**After upgrading Greenplum Database, the performance seems to be degraded.**

There are many factors that can affect performance degradation. A possible cause is that after upgrading to Greenplum Database 7, SWAP is enabled by default and hence affecting your performance. We recommend disabling SWAP and use RAM memory instead, in order to improve running speed and efficiency. If your memory configuration is sufficient, there is no need to use SWAP space. If you decide to use SWAP, be sure you understand how it takes part in the calculation of memory management allocation. See [Greenplum Database Memory Overview](wlmgmt_intro.html) for more information.
