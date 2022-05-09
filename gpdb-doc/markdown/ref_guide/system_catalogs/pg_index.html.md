# pg_index 

The `pg_index` system catalog table contains part of the information about indexes. The rest is mostly in [pg\_class](pg_class.html).

|column|type|references|description|
|------|----|----------|-----------|
|`indexrelid`|oid|pg\_class.oid|The OID of the pg\_class entry for this index.|
|`indrelid`|oid|pg\_class.oid|The OID of the pg\_class entry for the table this index is for.|
|`indnatts`|int2| |The number of columns in the index \(duplicates pg\_class.relnatts\).|
|`indisunique`|boolean| |If true, this is a unique index.|
|`indisprimary`|boolean| |If true, this index represents the primary key of the table. \(indisunique should always be true when this is true.\)|
|`indisexclusion`|boolean| |If true, this index supports an exclusion constraint|
|indimmediate|boolean| |If true, the uniqueness check is enforced immediately on insertion \(irrelevant if `indisunique` is not true\)|
|`indisclustered`|boolean| |If true, the table was last clustered on this index via the `CLUSTER` command.|
|`indisvalid`|boolean| |If true, the index is currently valid for queries. False means the index is possibly incomplete: it must still be modified by `INSERT`/`UPDATE` operations, but it cannot safely be used for queries.|
|`indcheckxmin`|boolean| |If true, queries must not use the index until the `xmin` of this `pg_index` row is below their `TransactionXmin` event horizon, because the table may contain broken HOT chains with incompatible rows that they can see|
|`indisready`|boolean| |If true, the index is currently ready for inserts. False means the index must be ignored by `INSERT`/`UPDATE` operations|
|`indislive`|boolean| |If false, the index is in process of being dropped, and should be ignored for all purposes|
|`indisreplident`|boolean| |If true this index has been chosen as "replica identity" using `ALTER TABLE ... REPLICA IDENTITY USING INDEX ...`|
|`indkey`|int2vector|pg\_attribute.attnum|This is an array of `indnatts` values that indicate which table columns this index indexes. For example a value of 1 3 would mean that the first and the third table columns make up the index key. A zero in this array indicates that the corresponding index attribute is an expression over the table columns, rather than a simple column reference.|
|`indcollation`|oidvector| |For each column in the index key, this contains the OID of the collation to use for the index.|
|`indclass`|oidvector|pg\_opclass.oid|For each column in the index key this contains the OID of the operator class to use.|
|`indoption`|int2vector| |This is an array of `indnatts` values that store per-column flag bits. The meaning of the bits is defined by the index's access method.|
|`indexprs`|text| |Expression trees \(in `nodeToString()` representation\) for index attributes that are not simple column references. This is a list with one element for each zero entry in indkey. NULL if all index attributes are simple references.|
|`indpred`|text| |Expression tree \(in `nodeToString()` representation\) for partial index predicate. NULL if not a partial index.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

