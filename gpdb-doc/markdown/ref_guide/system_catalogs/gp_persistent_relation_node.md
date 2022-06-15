# gp\_persistent\_relation\_node 

The gp\_persistent\_relation\_node table table keeps track of the status of file system objects in relation to the transaction status of relation objects \(tables, view, indexes, and so on\). This information is used to make sure the state of the system catalogs and the file system files persisted to disk are synchronized. This information is used by the primary to mirror file replication process.

|column|type|references|description|
|------|----|----------|-----------|
|`tablespace_oid`|oid|pg\_tablespace.oid|Tablespace object id|
|`database_oid`|oid|pg\_database.oid|Database object id|
|`relfilenode_oid`|oid|pg\_class.relfilenode|The object id of the relation file node.|
|`segment_file_num`|integer| |For append-optimized tables, the append-optimized segment file number.|
|`relation_storage_manager`|smallint| |Whether the relation is heap storage or append-optimized storage.|
|`persistent_state`|smallint| |0 - free1 - create pending

2 - created

3 - drop pending

4 - aborting create

5 - "Just in Time" create pending

6 - bulk load create pending

|
|`mirror_existence_state`|smallint| |0 - none1 - not mirrored

2 - mirror create pending

3 - mirrorcreated

4 - mirror down before create

5 - mirror down during create

6 - mirror drop pending

7 - only mirror drop remains

|
|`parent_xid`|integer| |Global transaction id.|
|`persistent_serial_num`|bigint| |Log sequence number position in the transaction log for a file block.|
|`previous_free_tid`|tid| |Used by Greenplum Database to internally manage persistent representations of file system objects.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

