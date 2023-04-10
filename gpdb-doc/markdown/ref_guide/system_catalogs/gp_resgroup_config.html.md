# gp_resgroup_config 

The `gp_toolkit.gp_resgroup_config` view allows administrators to see the current CPU, memory, and concurrency limits for a resource group.

> **Note** The `gp_resgroup_config` view is valid only when resource group-based resource management is active.

|column|type|references|description|
|------|----|----------|-----------|
|`groupid`|oid|pg\_resgroup.oid|The ID of the resource group.|
|`groupname`|name|pg\_resgroup.rsgname|The name of the resource group.|
|`concurrency`|text|pg\_resgroupcapability.value for pg\_resgroupcapability.reslimittype = 1|The concurrency \(`CONCURRENCY`\) value specified for the resource group.|
|`cpu_hard_quota_limit`|text|pg\_resgroupcapability.value for pg\_resgroupcapability.reslimittype = 2|The CPU limit \(`cpu_hard_quota_limit`\) value specified for the resource group, or -1.|
|`memory_limit`|text|pg\_resgroupcapability.value for pg\_resgroupcapability.reslimittype = 3|The memory limit \(`MEMORY_LIMIT`\) value specified for the resource group.|
|`memory_shared_quota`|text|pg\_resgroupcapability.value for pg\_resgroupcapability.reslimittype = 4|The shared memory quota \(`MEMORY_SHARED_QUOTA`\) value specified for the resource group.|
|`memory_spill_ratio`|text|pg\_resgroupcapability.value for pg\_resgroupcapability.reslimittype = 5|The memory spill ratio \(`MEMORY_SPILL_RATIO`\) value specified for the resource group.|
|`memory_auditor`|text|pg\_resgroupcapability.value for pg\_resgroupcapability.reslimittype = 6|The memory auditor in use for the resource group.|
|`cpuset`|text|pg\_resgroupcapability.value for pg\_resgroupcapability.reslimittype = 7|The CPU cores reserved for the resource group, or -1.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

