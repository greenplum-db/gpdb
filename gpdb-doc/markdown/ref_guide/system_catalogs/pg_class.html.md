# pg_class 

The system catalog table `pg_class` catalogs tables and most everything else that has columns or is otherwise similar to a table. This includes indexes \(see also [pg\_index](pg_index.html)\), sequences, views, materialized views, composite types, and TOAST tables. Below, when we mean all of these kinds of objects we speak of "relations". Not all columns are meaningful for all relation types.

|column|type|references|description|
|------|----|----------|-----------|
|`oid`|oid| |Row identifier|
|`relname`|name| |Name of the table, index, view, etc.|
|`relnamespace`|oid|[pg\_namespace](pg_namespace.html).oid|The object identifier of the namespace \(schema\) that contains this relation|
|`reltype`|oid|[pg\_type](pg_type.html).oid|The object identifier of the data type that corresponds to this table's row type, if any \(zero for indexes, which have no `pg_type` entry\)|
|`reloftype`|oid|[pg\_type](pg_type.html).oid|For typed tables, the object identifier of the underlying composite type, zero for all other relations|
|`relowner`|oid|[pg\_authid](pg_authid.html).oid|Owner of the relation|
|`relam`|oid|[pg\_am](pg_am.html).oid|If this is a table or an index, the access method used \(heap, B-tree, hash, etc.\)|
|`relfilenode`|oid| |Name of the on-disk file of this relation; zero means this is a "mapped" relation whose disk file name is determined by low-level state|
|`reltablespace`|oid|[pg\_tablespace](pg_tablespace.html).oid|The tablespace in which this relation is stored. If zero, the database's default tablespace is implied. \(Not meaningful if the relation has no on-disk file.\)|
|`relpages`|int4| |Size of the on-disk representation of this table in pages \(of size `BLCKSZ`\). This is only an estimate used by the planner. It is updated by `VACUUM`, `ANALYZE`, and a few DDL commands such as `CREATE INDEX`.|
|`reltuples`|float4| |Number of rows in the table. This is only an estimate used by the planner. It is updated by `VACUUM`, `ANALYZE`, and a few DDL commands such as `CREATE INDEX`.|
|`relallvisible`|int4| |Number of pages that are marked all-visible in the table's visibility map. This is only an estimate used by the planner. It is updated by `VACUUM`, `ANALYZE`, and a few DDL commands such as `CREATE INDEX`.|
|`reltoastrelid`|oid|pg\_class.oid|The object identifier of the TOAST table associated with this table, `0` if none. The TOAST table stores large attributes "out of line" in a secondary table.|
|`relhasindex`|boolean| |True if this is a table and it has \(or recently had\) any indexes. |
|`relisshared`|boolean| |True if this table is shared across all databases in the system. Only certain system catalog tables \(such as `pg_database`\) are shared.|
|`relpersistence`|char| |The type of object persistence: `p` = heap or append-optimized permanent table, `u` = unlogged temporary table, `t` = temporary table.|
|`relkind`|char| |The type of object<br/><br/>`r` = heap or append-optimized ordinary table, `i` = index, `S` = sequence, `t` = TOAST table, `v` = view, `m` = materialized view, `c` = composite type, `f` = foreign table, `p` = partitioned table, `I` = partitioned index, `u` = uncatalogued temporary heap table, `o` = internal append-optimized segment files and EOFs, `b` = append-only block directory, `M` = append-only visibility map.|
|`relnatts`|int2| |Number of user columns in the relation \(system columns not counted\). There must be this many corresponding entries in `pg_attribute`. See also `pg_attribute.attnum`.|
|`relchecks`|int2| |Number of `CHECK` constraints on the table; see [pg\_constraint](pg_constraint.html) catalog.|
|`relhasrules`|boolean| |True if table has \(or once had\) rules; see [pg\_rewrite](pg_rewrite.html) catalog.|
|`relhastriggers`|boolean| |True if table has \(or once had\) triggers.|
|`relhassubclass`|boolean| |True if table has \(or once had\) any inheritance children.|
|`relrowsecurity`|boolean| |True if table has row level security enabled; see `pg_policy` catalog.|
|`relforcerowsecurity`|boolean| |True if row level security \(when enabled\) will also apply to the table owner; see `pg_policy` catalog.|
|`relispopulated`|boolean| |True if relation is populated \(this is true for all relations other than some materialized views\).|
|`relreplident`|char| |Columns used to form "replica identity" for rows: `d` = default \(primary key, if any\), `n` = nothing, `f` = all columns, `i` = index with `indisreplident` set \(same as nothing if the index used has been dropped\).|
|`relispartition`|boolean| | True if table or index is a partition.|
|`relrewrite`|oid|[pg\_class](pg_class.html).oid | For new relations being written during a DDL operation that requires a table rewrite, this contains the object identifier of the original relation; otherwise 0. That state is only visible internally; this field should never contain anything other than 0 for a user-visible relation.|
|`relfrozenxid`|xid| |All transaction IDs before this one have been replaced with a permanent \(frozen\) transaction ID in this table. This is used to track whether the table needs to be vacuumed in order to prevent transaction ID wraparound or to allow `pg_xact` to be shrunk.<br/><br/>The value is `0` \(`InvalidTransactionId`\) if the relation is not a table or if the table does not require vacuuming to prevent transaction ID wraparound. The table still might require vacuuming to reclaim disk space.|
|`relminmxid`|xid| |All multixact IDs before this one have been replaced by a transaction ID in this table. This is used to track whether the table needs to be vacuumed in order to prevent multixact ID wraparound or to allow `pg_multixact` to be shrunk. Zero \(`InvalidMultiXactId`\) if the relation is not a table.|
|`relacl`|aclitem\[\]| |Access privileges assigned by `GRANT` and `REVOKE`.|
|`reloptions`|text\[\]| |Access-method-specific options, as "keyword=value" strings.|
|`relpartbound`|pg\_node\_tree| |If table is a partition \(see `relispartition`\), internal representation of the partition bound.|

Several of the Boolean flags in `pg_class` are maintained lazily: they are guaranteed to be true if that's the correct state, but may not be reset to false immediately when the condition is no longer true. For example, `relhasindex` is set by `CREATE INDEX`, but it is never cleared by `DROP INDEX`. Instead, `VACUUM` clears `relhasindex` if it finds the table has no indexes. This arrangement avoids race conditions and improves concurrency.

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

