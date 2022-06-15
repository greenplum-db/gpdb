# pg\_aggregate 

The `pg_aggregate` table stores information about aggregate functions. An aggregate function is a function that operates on a set of values \(typically one column from each row that matches a query condition\) and returns a single value computed from all these values. Typical aggregate functions are `sum`, `count`, and `max`. Each entry in `pg_aggregate` is an extension of an entry in `pg_proc`. The `pg_proc` entry carries the aggregate's name, input and output data types, and other information that is similar to ordinary functions.

|column|type|references|description|
|------|----|----------|-----------|
|`aggfnoid`|regproc|pg\_proc.oid|Aggregate function OID|
|`aggtransfn`|regproc|pg\_proc.oid|Transition function OID|
|`aggprelimfn`|regproc| |Preliminary function OID \(zero if none\)|
|`aggfinalfn`|regproc|pg\_proc.oid|Final function OID \(zero if none\)|
|`agginitval`|text| |The initial value of the transition state. This is a text field containing the initial value in its external string representation. If this field is NULL, the transition state value starts out NULL|
|`agginvtransfn`|regproc|pg\_proc.oid|The OID in pg\_proc of the inverse function of *aggtransfn*|
|`agginvprelimfn`|regproc|pg\_proc.oid|The OID in pg\_proc of the inverse function of aggprelimfn|
|`aggordered`|Boolean| |If `true`, the aggregate is defined as `ORDERED`.|
|`aggsortop`|oid|pg\_operator.oid|Associated sort operator OID \(zero if none\)|
|`aggtranstype`|oid|pg\_type.oid|Data type of the aggregate function's internal transition \(state\) data|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

