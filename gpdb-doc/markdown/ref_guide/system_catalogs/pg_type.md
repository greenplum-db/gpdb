# pg\_type 

The `pg_type` system catalog table stores information about data types. Base types \(scalar types\) are created with `CREATE TYPE`, and domains with `CREATE DOMAIN`. A composite type is automatically created for each table in the database, to represent the row structure of the table. It is also possible to create composite types with `CREATE TYPE AS`.

|column|type|references|description|
|------|----|----------|-----------|
|`typname`|name| |Data type name.|
|`typnamespace`|oid|pg\_namespace.oid|The OID of the namespace that contains this type.|
|`typowner`|oid|pg\_authid.oid|Owner of the type.|
|`typlen`|int2| |For a fixed-size type, `typlen` is the number of bytes in the internal representation of the type. But for a variable-length type, `typlen` is negative. `-1` indicates a 'varlena' type \(one that has a length word\), `-2` indicates a null-terminated C string.|
|`typbyval`|boolean| |Determines whether internal routines pass a value of this type by value or by reference. `typbyval`had better be false if `typlen` is not 1, 2, or 4 \(or 8 on machines where Datum is 8 bytes\). Variable-length types are always passed by reference. Note that `typbyval` can be false even if the length would allow pass-by-value; this is currently true for type `float4`, for example.|
|`typtype`|char| |`b` for a base type, `c` for a composite type, `d` for a domain, `e` for an enum type, or `p` for a pseudo-type.|
|`typisdefined`|boolean| |True if the type is defined, false if this is a placeholder entry for a not-yet-defined type. When false, nothing except the type name, namespace, and OID can be relied on.|
|`typdelim`|char| |Character that separates two values of this type when parsing array input. Note that the delimiter is associated with the array element data type, not the array data type.|
|`typrelid`|oid|pg\_class.oid|If this is a composite type, then this column points to the `pg_class` entry that defines the corresponding table. \(For a free-standing composite type, the `pg_class` entry does not really represent a table, but it is needed anyway for the type's `pg_attribute` entries to link to.\) Zero for non-composite types.|
|`typelem`|oid|pg\_type.oid|If not `0` then it identifies another row in pg\_type. The current type can then be subscripted like an array yielding values of type `typelem`. A true array type is variable length \(`typlen` = `-1`\), but some fixed-length \(`tylpen` \> `0`\) types also have nonzero `typelem`, for example `name` and `point`. If a fixed-length type has a `typelem` then its internal representation must be some number of values of the `typelem` data type with no other data. Variable-length array types have a header defined by the array subroutines.|
|`typarray`|oid|pg\_type.oid|If not 0, identifies another row in `pg_type`, which is the "true" array type having this type as its element. Use `pg_type.typarray` to locate the array type associated with a specific type.|
|`typinput`|regproc|pg\_proc.oid|Input conversion function \(text format\).|
|`typoutput`|regproc|pg\_proc.oid|Output conversion function \(text format\).|
|`typreceive`|regproc|pg\_proc.oid|Input conversion function \(binary format\), or 0 if none.|
|`typsend`|regproc|pg\_proc.oid|Output conversion function \(binary format\), or 0 if none.|
|`typmodin`|regproc|pg\_proc.oid|Type modifier input function, or 0 if the type does not support modifiers.|
|`typmodout`|regproc|pg\_proc.oid|Type modifier output function, or 0 to use the standard format.|
|`typanalyze`|regproc|pg\_proc.oid|Custom `ANALYZE` function, or 0 to use the standard function.|
|`typalign`|char| |The alignment required when storing a value of this type. It applies to storage on disk as well as most representations of the value inside Greenplum Database. When multiple values are stored consecutively, such as in the representation of a complete row on disk, padding is inserted before a datum of this type so that it begins on the specified boundary. The alignment reference is the beginning of the first datum in the sequence. Possible values are:<br/>`c` = char alignment \(no alignment needed\).<br/>`s` = short alignment \(2 bytes on most machines\).<br/>`i` = int alignment \(4 bytes on most machines\).<br/>`d` = double alignment \(8 bytes on many machines, but not all\).<br/>|
|`typstorage`|char| |For varlena types \(those with `typlen` = -1\) tells if the type is prepared for toasting and what the default strategy for attributes of this type should be. Possible values are:<br/>`p`: Value must always be stored plain.<br/>`e`: Value can be stored in a secondary relation \(if relation has one, see `pg_class.reltoastrelid`\).<br/>`m`: Value can be stored compressed inline.<br/>`x`: Value can be stored compressed inline or stored in secondary storage.<br/>Note that `m` columns can also be moved out to secondary storage, but only as a last resort \(`e` and `x` columns are moved first\).<br/>|
|`typnotnull`|boolean| |Represents a not-null constraint on a type. Used for domains only.|
|`typbasetype`|oid|pg\_type.oid|Identifies the type that a domain is based on. Zero if this type is not a domain.|
|`typtypmod`|int4| |Domains use typtypmod to record the typmod to be applied to their base type \(-1 if base type does not use a typmod\). -1 if this type is not a domain.|
|`typndims`|int4| |The number of array dimensions for a domain that is an array \(if `typbasetype` is an array type; the domain's `typelem` will match the base type's `typelem`\). Zero for types other than array domains.|
|`typdefaultbin`|text| |If not null, it is the `nodeToString()` representation of a default expression for the type. This is only used for domains.|
|`typdefault`|text| |Null if the type has no associated default value. If not null, typdefault must contain a human-readable version of the default expression represented by typdefaultbin. If typdefaultbin is null and typdefault is not, then typdefault is the external representation of the type's default value, which may be fed to the type's input converter to produce a constant.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

