# gp_resgroup_status_per_host 

The `gp_toolkit.gp_resgroup_status_per_host` view allows administrators to see current memory and CPU usage and allocation for each resource group on a per-host basis.

Memory amounts are specified in MBs.

> **Note** The `gp_resgroup_status_per_host` view is valid only when resource group-based resource management is active.

|column|type|references|description|
|------|----|----------|-----------|
|`rsgname`|name|pg\_resgroup.rsgname|The name of the resource group.|
|`groupid`|oid|pg\_resgroup.oid|The ID of the resource group.|
|`hostname`|text|gp\_segment\_configuration.hostname|The hostname of the segment host.|
|`cpu`|numeric| |The real-time CPU core usage by the resource group on a host. The value is the sum of the percentages \(as a decimal value\) of the CPU cores that are used by the resource group on the host.|
|`memory_used`|integer| |The real-time memory usage of the resource group on the host. This total includes resource group fixed and shared memory. It also includes global shared memory used by the resource group.|
|`memory_available`|integer| |The unused fixed and shared memory for the resource group that is available on the host. This total does not include available resource group global shared memory.|
|`memory_quota_used`|integer| |The real-time fixed memory usage for the resource group on the host.|
|`memory_quota_available`|integer| |The fixed memory available to the resource group on the host.|
|`memory_shared_used`|integer| |The group shared memory used by the resource group on the host. If any global shared memory is used by the resource group, this amount is included in the total as well.|
|`memory_shared_available`|integer| |The amount of group shared memory available to the resource group on the host. Resource group global shared memory is not included in this total.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

