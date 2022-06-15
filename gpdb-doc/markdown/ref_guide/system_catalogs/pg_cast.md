# pg\_cast 

The `pg_cast` table stores data type conversion paths, both built-in paths and those defined with `CREATE CAST`. The cast functions listed in `pg_cast` must always take the cast source type as their first argument type, and return the cast destination type as their result type. A cast function can have up to three arguments. The second argument, if present, must be type `integer`; it receives the type modifier associated with the destination type, or `-1` if there is none. The third argument, if present, must be type `boolean`; it receives `true` if the cast is an explicit cast, `false` otherwise.

It is legitimate to create a `pg_cast` entry in which the source and target types are the same, if the associated function takes more than one argument. Such entries represent 'length coercion functions' that coerce values of the type to be legal for a particular type modifier value. Note however that at present there is no support for associating non-default type modifiers with user-created data types, and so this facility is only of use for the small number of built-in types that have type modifier syntax built into the grammar.

When a `pg_cast` entry has different source and target types and a function that takes more than one argument, it represents converting from one type to another and applying a length coercion in a single step. When no such entry is available, coercion to a type that uses a type modifier involves two steps, one to convert between data types and a second to apply the modifier.

|column|type|references|description|
|------|----|----------|-----------|
|`castsource`|oid|pg\_type.oid|OID of the source data type.|
|`casttarget`|oid|pg\_type.oid|OID of the target data type.|
|`castfunc`|oid|pg\_proc.oid|The OID of the function to use to perform this cast. Zero is stored if the data types are binary compatible \(that is, no run-time operation is needed to perform the cast\).|
|`castcontext`|char| |Indicates what contexts the cast may be invoked in. `e` means only as an explicit cast \(using `CAST` or `::` syntax\). `a` means implicitly in assignment to a target column, as well as explicitly. `i` means implicitly in expressions, as well as the other cases*.*|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

