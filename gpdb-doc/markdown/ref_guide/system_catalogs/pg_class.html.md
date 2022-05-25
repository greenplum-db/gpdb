# pg_class 

The system catalog table `pg_class` catalogs tables and most everything else that has columns or is otherwise similar to a table \(also known as *relations*\). This includes indexes \(see also [pg\_index](pg_index.html)\), sequences, views, composite types, and TOAST tables. Not all columns are meaningful for all relation types.

|column|type|references|description|
|------|----|----------|-----------|
|`relname`|name| |Name of the table, index, view, etc.|
|`relnamespace`|oid|pg\_namespace.oid|The OID of the namespace \(schema\) that contains this relation|
|`reltype`|oid|pg\_type.oid|The OID of the data type that corresponds to this table's row type, if any \(zero for indexes, which have no pg\_type entry\)|
|`reloftype`|oid|pg\_type.oid|The OID of an entry in `pg_type` for an underlying composite type.|
|`relowner`|oid|pg\_authid.oid|Owner of the relation|
|`relam`|oid|pg\_am.oid|If this is an index, the access method used \(B-tree, Bitmap, hash, etc.\)|
|`relfilenode`|oid| |Name of the on-disk file of this relation; `0` if none.|
|`reltablespace`|oid|pg\_tablespace.oid|The tablespace in which this relation is stored. If zero, the database's default tablespace is implied. \(Not meaningful if the relation has no on-disk file.\)|
|`relpages`|int4| |Size of the on-disk representation of this table in pages \(of 32K each\). This is only an estimate used by the planner. It is updated by `VACUUM`, `ANALYZE`, and a few DDL commands.|
|`reltuples`|float4| |Number of rows in the table. This is only an estimate used by the planner. It is updated by `VACUUM`, `ANALYZE`, and a few DDL commands.|
|`relallvisible`|int32| |Number of all-visible blocks \(this value may not be up-to-date\).|
|`reltoastrelid`|oid|pg\_class.oid|OID of the TOAST table associated with this table, `0` if none. The TOAST table stores large attributes "out of line" in a secondary table.|
|`relhasindex`|boolean| |True if this is a table and it has \(or recently had\) any indexes. This is set by `CREATE INDEX`, but not cleared immediately by `DROP INDEX`. `VACUUM` will clear if it finds the table has no indexes.|
|`relisshared`|boolean| |True if this table is shared across all databases in the system. Only certain system catalog tables are shared.|
|`relpersistence`|char| |The type of object persistence: `p` = heap or append-optimized table, `u` = unlogged temporary table, `t` = temporary table.|
|`relkind`|char| |The type of object<br/><br/>`r` = heap or append-optimized table, `i` = index, `S` = sequence, `t` = TOAST value, `v` = view, `c` = composite type, `f` = foreign table, `u` = uncatalogued temporary heap table, `o` = internal append-optimized segment files and EOFs, `b` = append-only block directory, `M` = append-only visibility map|
|`relstorage`|char| |The storage mode of a table<br/><br/>`a`= append-optimized, `c`= column-oriented, `h` = heap, `v` = virtual, `x`= external table.|
|`relnatts`|int2| |Number of user columns in the relation \(system columns not counted\). There must be this many corresponding entries in pg\_attribute.|
|`relchecks`|int2| |Number of check constraints on the table.|
|`relhasoids`|boolean| |True if an OID is generated for each row of the relation.|
|`relhaspkey`|boolean| |True if the table has \(or once had\) a primary key.|
|`relhasrules`|boolean| |True if table has rules.|
|`relhastriggers`|boolean| |True if table has \(or once had\) triggers.|
|`relhassubclass`|boolean| |True if table has \(or once had\) any inheritance children.|
|`relispopulated`|boolean| |True if relation is populated \(this is true for all relations other than some materialized views\).|
|`relreplident`|char| |Columns used to form “replica identity” for rows: d = default \(primary key, if any\), n = nothing, f = all columns i = index with `indisreplident` set, or default|
|`relfrozenxid`|xid| |All transaction IDs before this one have been replaced with a permanent \(frozen\) transaction ID in this table. This is used to track whether the table needs to be vacuumed in order to prevent transaction ID wraparound or to allow `pg_xact` to be shrunk.<br/><br/>The value is `0` \(`InvalidTransactionId`\) if the relation is not a table or if the table does not require vacuuming to prevent transaction ID wraparound. The table still might require vacuuming to reclaim disk space.|
|`relminmxid`|xid| |All multixact IDs before this one have been replaced by a transaction ID in this table. This is used to track whether the table needs to be vacuumed in order to prevent multixact ID wraparound or to allow `pg_multixact` to be shrunk. Zero \(`InvalidMultiXactId`\) if the relation is not a table.|
|`relacl`|aclitem\[\]| |Access privileges assigned by `GRANT` and `REVOKE`.|
|`reloptions`|text\[\]| |Access-method-specific options, as "keyword=value" strings.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

