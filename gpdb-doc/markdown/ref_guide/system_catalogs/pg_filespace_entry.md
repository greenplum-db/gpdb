# pg\_filespace\_entry 

A tablespace requires a file system location to store its database files. In Greenplum Database, the master and each segment \(primary and mirror\) needs its own distinct storage location. This collection of file system locations for all components in a Greenplum system is referred to as a *filespace*. The `pg_filespace_entry` table contains information about the collection of file system locations across a Greenplum Database system that comprise a Greenplum Database filespace.

|column|type|references|description|
|------|----|----------|-----------|
|`fsefsoid`|OID|pg\_filespace.oid|Object id of the filespace.|
|`fsedbid`|integer|gp\_segment\_ configuration.dbid|Segment id.|
|`fselocation`|text| |File system location for this segment id.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

