# pg\_index 

The `pg_index` system catalog table contains part of the information about indexes. The rest is mostly in [pg\_class](pg_class.html).

|column|type|references|description|
|------|----|----------|-----------|
|`indexrelid`|oid|pg\_class.oid|The OID of the pg\_class entry for this index.|
|`indrelid`|oid|pg\_class.oid|The OID of the pg\_class entry for the table this index is for.|
|`indnatts`|int2| |The number of columns in the index \(duplicates pg\_class.relnatts\).|
|`indisunique`|boolean| |If true, this is a unique index.|
|`indisprimary`|boolean| |If true, this index represents the primary key of the table. \(indisunique should always be true when this is true.\)|
|`indisclustered`|boolean| |If true, the table was last clustered on this index via the `CLUSTER` command.|
|`indisvalid`|boolean| |If true, the index is currently valid for queries. False means the index is possibly incomplete: it must still be modified by `INSERT`/`UPDATE` operations, but it cannot safely be used for queries.|
|`indkey`|int2vector|pg\_attribute.attnum|This is an array of indnatts values that indicate which table columns this index indexes. For example a value of 1 3 would mean that the first and the third table columns make up the index key. A zero in this array indicates that the corresponding index attribute is an expression over the table columns, rather than a simple column reference.|
|`indclass`|oidvector|pg\_opclass.oid|For each column in the index key this contains the OID of the operator class to use.|
|`indexprs`|text| |Expression trees \(in `nodeToString()` representation\) for index attributes that are not simple column references. This is a list with one element for each zero entry in indkey. NULL if all index attributes are simple references.|
|`indpred`|text| |Expression tree \(in `nodeToString()` representation\) for partial index predicate. NULL if not a partial index.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

