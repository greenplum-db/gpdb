# pg\_proc 

The `pg_proc` system catalog table stores information about functions \(or procedures\), both built-in functions and those defined by `CREATE FUNCTION`. The table contains data for aggregate and window functions as well as plain functions. If `proisagg` is true, there should be a matching row in `pg_aggregate`. If `proiswin` is true, there should be a matching row in `pg_window`.

For compiled functions, both built-in and dynamically loaded, `prosrc` contains the function's C-language name \(link symbol\). For all other currently-known language types, `prosrc` contains the function's source text. `probin` is unused except for dynamically-loaded C functions, for which it gives the name of the shared library file containing the function.

|column|type|references|description|
|------|----|----------|-----------|
|`proname`|name| |Name of the function.|
|`pronamespace`|oid|pg\_namespace.oid|The OID of the namespace that contains this function.|
|`proowner`|oid|pg\_authid.oid|Owner of the function.|
|`prolang`|oid|pg\_language.oid|Implementation language or call interface of this function.|
|`procost`|float4| |Estimated execution cost \(in cpu\_operator\_cost units\); if proretset is `true`, identifies the cost per row returned.|
|`prorows`|float4| |Estimated number of result rows \(zero if not proretset\)|
|`provariadic`|oid|pg\_type.oid|Data type of the variadic array parameter's elements, or zero if the function does not have a variadic parameter.|
|`proisagg`|boolean| |Function is an aggregate function.|
|`prosecdef`|boolean| |Function is a security definer \(for example, a 'setuid' function\).|
|`proisstrict`|boolean| |Function returns NULL if any call argument is NULL. In that case the function will not actually be called at all. Functions that are not strict must be prepared to handle NULL inputs.|
|`proretset`|boolean| |Function returns a set \(multiple values of the specified data type\).|
|`provolatile`|char| |Tells whether the function's result depends only on its input arguments, or is affected by outside factors. `i` = *immutable* \(always delivers the same result for the same inputs\), `s` = *stable* \(results \(for fixed inputs\) do not change within a scan\), or `v` = *volatile* \(results may change at any time or functions with side-effects\).|
|`pronargs`|int2| |Number of arguments.|
|`pronargdefaults`|int2| |Number of arguments that have default values.|
|`prorettype`|oid|pg\_type.oid|Data type of the return value.|
|`proiswin`|boolean| |Function is neither an aggregate nor a scalar function, but a pure window function.|
|`proargtypes`|oidvector|pg\_type.oid|An array with the data types of the function arguments. This includes only input arguments \(including `INOUT` and `VARIADIC` arguments\), and thus represents the call signature of the function.|
|`proallargtypes`|oid\[\]|pg\_type.oid|An array with the data types of the function arguments. This includes all arguments \(including `OUT` and `INOUT` arguments\); however, if all the arguments are `IN` arguments, this field will be null. Note that subscripting is 1-based, whereas for historical reasons proargtypes is subscripted from 0.|
|`proargmodes`|char\[\]| |An array with the modes of the function arguments: `i` = `IN`, `o` = `OUT` , `b` = `INOUT`, `v` = `VARIADIC`. If all the arguments are IN arguments, this field will be null. Note that subscripts correspond to positions of proallargtypes not proargtypes.|
|`proargnames`|text\[\]| |An array with the names of the function arguments. Arguments without a name are set to empty strings in the array. If none of the arguments have a name, this field will be null. Note that subscripts correspond to positions of proallargtypes not proargtypes.|
|`proargdefaults`|text| |Expression trees for default argument values. This is a list with pronargdefaults elements, corresponding to the last N input arguments \(i.e., the last N proargtypes positions\). If none of the arguments have defaults, this field is empty.|
|`prosrc`|text| |This tells the function handler how to invoke the function. It might be the actual source code of the function for interpreted languages, a link symbol, a file name, or just about anything else, depending on the implementation language/call convention.|
|`probin`|bytea| |Additional information about how to invoke the function. Again, the interpretation is language-specific.|
|`proacl`|aclitem\[\]| |Access privileges for the function as given by `GRANT`/`REVOKE`.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

