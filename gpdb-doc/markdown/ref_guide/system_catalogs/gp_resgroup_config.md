# gp\_resgroup\_config 

The `gp_toolkit.gp_resgroup_config` view allows administrators to see the current CPU, memory, and concurrency limits for a resource group. The view also displays proposed limit settings. A proposed limit will differ from a current limit when the limit has been altered, but the new value could not be immediately applied.

**Note:** The `gp_resgroup_config` view is valid only when resource group-based resource management is active.

|column|type|references|description|
|------|----|----------|-----------|
|`groupid`|oid|pg\_resgroup.oid|The ID of the resource group.|
|`groupname`|name|pg\_resgroup.rsgname|The name of the resource group.|
|`concurrency`|text|pg\_resgroupcapability.value for pg\_resgroupcapability.reslimittype = 1|The concurrency \(`CONCURRENCY`\) value specified for the resource group.|
|`proposed_concurrency`|text|pg\_resgroupcapability.proposed for pg\_resgroupcapability.reslimittype = 1|The pending concurrency value for the resource group.|
|`cpu_rate_limit`|text|pg\_resgroupcapability.value for pg\_resgroupcapability.reslimittype = 2|The CPU limit \(`CPU_RATE_LIMIT`\) value specified for the resource group, or -1.|
|`memory_limit`|text|pg\_resgroupcapability.value for pg\_resgroupcapability.reslimittype = 3|The memory limit \(`MEMORY_LIMIT`\) value specified for the resource group.|
|`proposed_memory_limit`|text|pg\_resgroupcapability.proposed for pg\_resgroupcapability.reslimittype = 3|The pending memory limit value for the resource group.|
|`memory_shared_quota`|text|pg\_resgroupcapability.value for pg\_resgroupcapability.reslimittype = 4|The shared memory quota \(`MEMORY_SHARED_QUOTA`\) value specified for the resource group.|
|`proposed_memory_shared_quota`|text|pg\_resgroupcapability.proposed for pg\_resgroupcapability.reslimittype = 4|The pending shared memory quota value for the resource group.|
|`memory_spill_ratio`|text|pg\_resgroupcapability.value for pg\_resgroupcapability.reslimittype = 5|The memory spill ratio \(`MEMORY_SPILL_RATIO`\) value specified for the resource group.|
|`proposed_memory_spill_ratio`|text|pg\_resgroupcapability.proposed for pg\_resgroupcapability.reslimittype\> = 5|The pending memory spill ratio value for the resource group.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

