# pg\_filespace 

The `pg_filespace` table contains information about the filespaces created in a Greenplum Database system. Every system contains a default filespace, `pg_system`, which is a collection of all the data directory locations created at system initialization time.

A tablespace requires a file system location to store its database files. In Greenplum Database, the master and each segment \(primary and mirror\) needs its own distinct storage location. This collection of file system locations for all components in a Greenplum system is referred to as a filespace.

|column|type|references|description|
|------|----|----------|-----------|
|`fsname`|name| |The name of the filespace.|
|`fsowner`|oid|pg\_roles.oid|The object id of the role that created the filespace.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

