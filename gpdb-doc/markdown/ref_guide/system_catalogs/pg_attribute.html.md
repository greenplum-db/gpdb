---
title: pg_attribute 
---

The `pg_attribute` table stores information about table columns. There will be exactly one `pg_attribute` row for every column in every table in the database. \(There will also be attribute entries for indexes, and all objects that have `pg_class` entries.\) The term attribute is equivalent to column.

|column|type|references|description|
|------|----|----------|-----------|
|`attrelid`|oid|pg\_class.oid|The table this column belongs to.|
|`attname`|name| |The column name.|
|`atttypid`|oid|pg\_type.oid|The data type of this column.|
|`attstattarget`|int4| |Controls the level of detail of statistics accumulated for this column by `ANALYZE`. A zero value indicates that no statistics should be collected. A negative value says to use the system default statistics target. The exact meaning of positive values is data type-dependent. For scalar data types, it is both the target number of "most common values" to collect, and the target number of histogram bins to create.|
|`attlen`|int2| |A copy of `pg_type.typlen` of this column's type.|
|`attnum`|int2| |The number of the column. Ordinary columns are numbered from 1 up. System columns, such as oid, have \(arbitrary\) negative numbers.|
|`attndims`|int4| |Number of dimensions, if the column is an array type; otherwise `0`. \(Presently, the number of dimensions of an array is not enforced, so any nonzero value effectively means it is an array.\)|
|`attcacheoff`|int4| |Always `-1` in storage, but when loaded into a row descriptor in memory this may be updated to cache the offset of the attribute within the row.|
|`atttypmod`|int4| |Records type-specific data supplied at table creation time \(for example, the maximum length of a `varchar` column\). It is passed to type-specific input functions and length coercion functions. The value will generally be `-1` for types that do not need it.|
|`attbyval`|boolean| |A copy of `pg_type.typbyval` of this column's type.|
|`attstorage`|char| |Normally a copy of `pg_type.typstorage` of this column's type. For TOAST-able data types, this can be altered after column creation to control storage policy.|
|`attalign`|char| |A copy of `pg_type.typalign` of this column's type.|
|`attnotnull`|boolean| |This represents a not-null constraint. It is possible to change this column to activate or deactivate the constraint.|
|`atthasdef`|boolean| |This column has a default value, in which case there will be a corresponding entry in the `pg_attrdef` catalog that actually defines the value.|
|`attisdropped`|boolean| |This column has been dropped and is no longer valid. A dropped column is still physically present in the table, but is ignored by the parser and so cannot be accessed via SQL.|
|`attislocal`|boolean| |This column is defined locally in the relation. Note that a column may be locally defined and inherited simultaneously.|
|`attinhcount`|int4| |The number of direct ancestors this column has. A column with a nonzero number of ancestors cannot be dropped nor renamed.|
|`attcollation`|oid|pg\_collation.oid|The defined collation of the column, or zero if the is not of a collatable data type.|
|`attacl`|aclitem\[\]| |Column-level access privileges, if any have been granted specifically on this column.|
|`attoptions`|text\[\]| |Attribute-level options, as "keyword=value" strings.|
|`attfdwoptions`|text\[\]| |Attribute-level foreign data wrapper options, as "keyword=value" strings.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

