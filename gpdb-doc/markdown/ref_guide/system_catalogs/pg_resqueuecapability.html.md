# pg_resqueuecapability 

> **Note** The `pg_resqueuecapability` system catalog table is valid only when resource queue-based resource management is active.

The `pg_resqueuecapability` system catalog table contains information about the extended attributes, or capabilities, of existing Greenplum Database resource queues. Only resource queues that have been assigned an extended capability, such as a priority setting, are recorded in this table. This table is joined to the [pg\_resqueue](pg_resqueue.html) table by resource queue object ID, and to the [pg\_resourcetype](pg_resourcetype.html) table by resource type ID \(`restypid`\).

This table is populated only on the coordinator. This table is defined in the `pg_global` tablespace, meaning it is globally shared across all databases in the system.

|column|type|references|description|
|------|----|----------|-----------|
|`rsqueueid`|oid|`pg_resqueue.oid`|The object ID of the associated resource queue.|
|`restypid`|smallint|`pg_resourcetype. restypid`|The resource type, derived from the *[pg\_resqueuecapability](pg_resourcetype.html)* system table.|
|`resetting`|opaque type| |The specific value set for the capability referenced in this record. Depending on the actual resource type, this value may have different data types.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

