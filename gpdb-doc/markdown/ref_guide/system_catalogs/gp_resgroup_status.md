# gp\_resgroup\_status 

The `gp_toolkit.gp_resgroup_status` view allows administrators to see status and activity for a resource group. It shows how many queries are waiting to run and how many queries are currently active in the system for each resource group. The view also displays current memory and CPU usage for the resource group.

**Note:** The `gp_resgroup_status` view is valid only when resource group-based resource management is active.

|column|type|references|description|
|------|----|----------|-----------|
|`rsgname`|name|pg\_resgroup.rsgname|The name of the resource group.|
|`groupid`|oid|pg\_resgroup.oid|The ID of the resource group.|
|`num_running`|integer| |The number of transactions currently executing in the resource group.|
|`num_queueing`|integer| |The number of currently queued transactions for the resource group.|
|`num_queued`|integer| |The total number of queued transactions for the resource group since the Greenplum Database cluster was last started, excluding the `num_queueing`.|
|`num_executed`|integer| |The total number of executed transactions in the resource group since the Greenplum Database cluster was last started, excluding the `num_running`.|
|`total_queue_duration`|interval| |The total time any transaction was queued since the Greenplum Database cluster was last started.|
|`cpu_usage`|json| |The real-time CPU usage of the resource group on each Greenplum Database segment's host.|
|`memory_usage`|json| |The real-time memory usage of the resource group on each Greenplum Database segment's host.|



The `cpu_usage` field is a JSON-formatted, key:value string that identifies, for each resource group, the per-segment CPU usage percentage. The key is segment id, the value is the percentage of CPU usage by the resource group on the segment host. The total CPU usage of all segments running on a segment host should not exceed the `gp_resource_group_cpu_limit`. Example `cpu_usage` column output:

```

{"-1":0.01, "0":0.31, "1":0.31}
```

The `memory_usage` field is also a JSON-formatted, key:value string. The string contents differ depending upon the type of resource group. For each resource group that you assign to a role \(default memory auditor `vmtracker`\), this string identifies the used, available, granted, and proposed fixed and shared memory quota allocations on each segment. The key is segment id. The values are memory values displayed in MB units. The following example shows `memory_usage` column output for a single segment for a resource group that you assign to a role:

```

"0":{"used":0, "available":76, "quota_used":-1, "quota_available":60, "quota_granted":60, "quota_proposed":60, "shared_used":0, "shared_available":16, "shared_granted":16, "shared_proposed":16}
```

For each resource group that you assign to an external component, the `memory_usage` JSON-formatted string identifies the memory used and the memory limit on each segment. The following example shows `memory_usage` column output for an external component resource group for a single segment:

```
"1":{"used":11, "limit_granted":15}
```

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

