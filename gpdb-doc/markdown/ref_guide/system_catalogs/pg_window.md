# pg\_window 

The `pg_window` table stores information about window functions. Window functions are often used to compose complex OLAP \(online analytical processing\) queries. Window functions are applied to partitioned result sets within the scope of a single query expression. A window partition is a subset of rows returned by a query, as defined in a special `OVER()` clause. Typical window functions are `rank`, `dense_rank`, and `row_number`. Each entry in `pg_window` is an extension of an entry in [pg\_proc](pg_proc.html). The [pg\_proc](pg_proc.html) entry carries the window function's name, input and output data types, and other information that is similar to ordinary functions.

|column|type|references|description|
|------|----|----------|-----------|
|`winfnoid`|regproc|pg\_proc.oid|The OID in `pg_proc` of the window function.|
|`winrequireorder`|boolean| |The window function requires its window specification to have an `ORDER BY` clause.|
|`winallowframe`|boolean| |The window function permits its window specification to have a `ROWS` or `RANGE` framing clause.|
|`winpeercount`|boolean| |The peer group row count is required to compute this window function, so the Window node implementation must 'look ahead' as necessary to make this available in its internal state.|
|`wincount`|boolean| |The partition row count is required to compute this window function.|
|`winfunc`|regproc|pg\_proc.oid|The OID in `pg_proc` of a function to compute the value of an immediate-type window function.|
|`winprefunc`|regproc|pg\_proc.oid|The OID in `pg_proc` of a preliminary window function to compute the partial value of a deferred-type window function.|
|`winpretype`|oid|pg\_type.oid|The OID in `pg_type` of the preliminary window function's result type.|
|winfinfunc|regproc|pg\_proc.oid|The OID in `pg_proc` of a function to compute the final value of a deferred-type window function from the partition row count and the result of `winprefunc`.|
|winkind|char| |A character indicating membership of the window function in a class of related functions:`w` - ordinary window functions<br/>`n` - NTILE functions<br/>`f` - FIRST\_VALUE functions<br/>`l` - LAST\_VALUE functions<br/>`g` - LAG functions<br/>`d` - LEAD functions<br/>|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

