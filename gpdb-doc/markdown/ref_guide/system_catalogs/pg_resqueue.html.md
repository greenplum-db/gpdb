# pg_resqueue 

> **Note** The `pg_resqueue` system catalog table is valid only when resource queue-based resource management is active.

The `pg_resqueue` system catalog table contains information about Greenplum Database resource queues, which are used for the resource management feature. This table is populated only on the coordinator. This table is defined in the `pg_global` tablespace, meaning it is globally shared across all databases in the system.

|column|type|references|description|
|------|----|----------|-----------|
|`rsqname`|name| |The name of the resource queue.|
|`rsqcountlimit`|real| |The active query threshold of the resource queue.|
|`rsqcostlimit`|real| |The query cost threshold of the resource queue.|
|`rsqovercommit`|boolean| |Allows queries that exceed the cost threshold to run when the system is idle.|
|`rsqignorecostlimit`|real| |The query cost limit of what is considered a 'small query'. Queries with a cost under this limit will not be queued and run immediately.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

