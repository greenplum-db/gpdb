# pg_cast 

The `pg_cast` table stores data type conversion paths, both built-in paths and those defined with `CREATE CAST`.

Note that `pg_cast` does not represent every type conversion known to the system, only those that cannot be deduced from some generic rule. For example, casting between a domain and its base type is not explicitly represented in `pg_cast`. Another important exception is that "automatic I/O conversion casts", those performed using a data type's own I/O functions to convert to or from `text` or other string types, are not explicitly represented in `pg_cast`.

The cast functions listed in `pg_cast` must always take the cast source type as their first argument type, and return the cast destination type as their result type. A cast function can have up to three arguments. The second argument, if present, must be type `integer`; it receives the type modifier associated with the destination type, or `-1` if there is none. The third argument, if present, must be type `boolean`; it receives `true` if the cast is an explicit cast, `false` otherwise.

It is legitimate to create a `pg_cast` entry in which the source and target types are the same, if the associated function takes more than one argument. Such entries represent 'length coercion functions' that coerce values of the type to be legal for a particular type modifier value.

When a `pg_cast` entry has different source and target types and a function that takes more than one argument, the entry converts from one type to another and applies a length coercion in a single step. When no such entry is available, coercion to a type that uses a type modifier involves two steps, one to convert between data types and a second to apply the modifier.

|column|type|references|description|
|------|----|----------|-----------|
|`castsource`|oid|pg\_type.oid|OID of the source data type.|
|`casttarget`|oid|pg\_type.oid|OID of the target data type.|
|`castfunc`|oid|pg\_proc.oid|The OID of the function to use to perform this cast. Zero is stored if the cast method does not require a function.|
|`castcontext`|char| |Indicates what contexts the cast may be invoked in. `e` means only as an explicit cast \(using `CAST` or `::` syntax\). `a` means implicitly in assignment to a target column, as well as explicitly. `i` means implicitly in expressions, as well as the other cases*.*|
|`castmethod`|char| |Indicates how the cast is performed:<br/><br/>`f` - The function identified in the `castfunc` field is used.<br/><br/>`i` - The input/output functions are used.<br/><br/>`b` - The types are binary-coercible, and no conversion is required.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

