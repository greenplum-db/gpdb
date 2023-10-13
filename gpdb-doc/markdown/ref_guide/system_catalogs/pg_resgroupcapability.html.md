# pg_resgroupcapability 

> **Note** The `pg_resgroupcapability` system catalog table is valid only when resource group-based resource management is active.

The `pg_resgroupcapability` system catalog table contains information about the capabilities and limits of defined Greenplum Database resource groups. You can join this table to the [pg\_resgroup](pg_resgroup.html) table by resource group object ID.

The `pg_resgroupcapability` table, defined in the `pg_global` tablespace, is globally shared across all databases in the system.

|column|type|references|description|
|------|----|----------|-----------|
|`resgroupid`|oid|`pg_resgroup.oid`|The object ID of the associated resource group.|
|`reslimittype`|smallint|``|The resource group limit type:<br/><br/>1 - Concurrency<br/><br/>2 - CPU_MAX_WEIGHT<br/><br/>3 - CPU_WEIGHT<br/><br/>4 - CPUSET<br/><br/>5 - MEMORY_LIMIT<br/><br/>6 - MIN_COST<br/><br/>7 - IO_LIMIT|
|`value`|opaque type| |The specific value set for the resource limit referenced in this record. This value has the fixed type `text`, and will be converted to a different data type depending upon the limit referenced.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

