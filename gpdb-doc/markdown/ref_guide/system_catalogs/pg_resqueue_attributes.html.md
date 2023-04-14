# pg_resqueue_attributes 

> **Note** The `pg_resqueue_attributes` view is valid only when resource queue-based resource management is active.

The `pg_resqueue_attributes` view allows administrators to see the attributes set for a resource queue, such as its active statement limit, query cost limits, and priority.

|column|type|references|description|
|------|----|----------|-----------|
|`rsqname`|name|pg\_resqueue.rsqname|The name of the resource queue.|
|`resname`|text| |The name of the resource queue attribute.|
|`resetting`|text| |The current value of a resource queue attribute.|
|`restypid`|integer| |System assigned resource type id.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

