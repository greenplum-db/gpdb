# gp\_relation\_node 

The `gp_relation_node` table contains information about the file system objects for a relation \(table, view, index, and so on\).

|column|type|references|description|
|------|----|----------|-----------|
|`relfilenode_oid`|oid|pg\_class.relfilenode|The object id of the relation file node.|
|`segment_file_num`|integer| |For append-optimized tables, the append-optimized segment file number.|
|`persistent_tid`|tid| |Used by Greenplum Database to internally manage persistent representations of file system objects.|
|`persistent_serial_num`|bigint| |Log sequence number position in the transaction log for a file block.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

