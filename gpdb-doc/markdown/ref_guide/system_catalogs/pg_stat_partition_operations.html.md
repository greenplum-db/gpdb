# pg_stat_partition_operations 

The `pg_stat_partition_operations` view shows details about the last operation performed on a partitioned table.

|column|type|references|description|
|------|----|----------|-----------|
|`classname`|text| |The name of the system table in the `pg_catalog` schema where the record about this object is stored \(always `pg_class` for tables and partitions\).|
|`objname`|name| |The name of the object.|
|`objid`|oid| |The OID of the object.|
|`schemaname`|name| |The name of the schema where the object resides.|
|`usestatus`|text| |The status of the role who performed the last operation on the object \(`CURRENT`=a currently active role in the system, `DROPPED`=a role that no longer exists in the system, `CHANGED`=a role name that exists in the system, but its definition has changed since the last operation was performed\).|
|`usename`|name| |The name of the role that performed the operation on this object.|
|`actionname`|name| |The action that was taken on the object.|
|`subtype`|text| |The type of object operated on or the subclass of operation performed.|
|`statime`|timestamptz| |The timestamp of the operation. This is the same timestamp that is written to the Greenplum Database server log files in case you need to look up more detailed information about the operation in the logs.|
|`partitionlevel`|smallint| |The level of this partition in the hierarchy.|
|`parenttablename`|name| |The relation name of the parent table one level up from this partition.|
|`parentschemaname`|name| |The name of the schema where the parent table resides.|
|`parent_relid`|oid| |The OID of the parent table one level up from this partition.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

