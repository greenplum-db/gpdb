# gp_resqueue_status 

The `gp_toolkit.gp_resqueue_status` view allows administrators to see status and activity for a resource queue. It shows how many queries are waiting to run and how many queries are currently active in the system from a particular resource queue.

> **Note** The `gp_resqueue_status` view is valid only when resource queue-based resource management is active.

|column|type|references|description|
|------|----|----------|-----------|
|`queueid`|oid|gp\_toolkit.gp\_resqueue\_ queueid|The ID of the resource queue.|
|`rsqname`|name|gp\_toolkit.gp\_resqueue\_ rsqname|The name of the resource queue.|
|`rsqcountlimit`|real|gp\_toolkit.gp\_resqueue\_ rsqcountlimit|The active query threshold of the resource queue. A value of -1 means no limit.|
|`rsqcountvalue`|real|gp\_toolkit.gp\_resqueue\_ rsqcountvalue|The number of active query slots currently being used in the resource queue.|
|`rsqcostlimit`|real|gp\_toolkit.gp\_resqueue\_ rsqcostlimit|The query cost threshold of the resource queue. A value of -1 means no limit.|
|`rsqcostvalue`|real|gp\_toolkit.gp\_resqueue\_ rsqcostvalue|The total cost of all statements currently in the resource queue.|
|`rsqmemorylimit`|real|gp\_toolkit.gp\_resqueue\_ rsqmemorylimit|The memory limit for the resource queue.|
|`rsqmemoryvalue`|real|gp\_toolkit.gp\_resqueue\_ rsqmemoryvalue|The total memory used by all statements currently in the resource queue.|
|`rsqwaiters`|integer|gp\_toolkit.gp\_resqueue\_ rsqwaiter|The number of statements currently waiting in the resource queue.|
|`rsqholders`|integer|gp\_toolkit.gp\_resqueue\_ rsqholders|The number of statements currently running on the system from this resource queue.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

