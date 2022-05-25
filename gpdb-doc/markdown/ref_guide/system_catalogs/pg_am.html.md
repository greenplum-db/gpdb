# pg_am 

The `pg_am` table stores information about index access methods. There is one row for each index access method supported by the system.

|column|type|references|description|
|------|----|----------|-----------|
|`oid`|oid| |Row identifier \(hidden attribute; must be explicitly selected\)|
|`amname`|name| |Name of the access method|
|`amstrategies`|int2| |Number of operator strategies for this access method, or zero if the access method does not have a fixed set of operator strategies|
|`amsupport`|int2| |Number of support routines for this access method|
|`amcanorder`|boolean| |Does the access method support ordered scans sorted by the indexed column's value?|
|`amcanorderbyop`|boolean| |Does the access method support ordered scans sorted by the result of an operator on the indexed column?|
|`amcanbackward`|boolean| |Does the access method support backward scanning?|
|`amcanunique`|boolean| |Does the access method support unique indexes?|
|`amcanmulticol`|boolean| |Does the access method support multicolumn indexes?|
|`amoptionalkey`|boolean| |Does the access method support a scan without any constraint for the first index column?|
|`amsearcharray`|boolean| |Does the access method support `ScalarArrayOpExpr` searches?|
|`amsearchnulls`|boolean| |Does the access method support `IS NULL/NOT NULL` searches?|
|`amstorage`|boolean| |Can index storage data type differ from column data type?|
|`amclusterable`|boolean| |Can an index of this type be clustered on?|
|`ampredlocks`|boolean| |Does an index of this type manage fine-grained predicae locks?|
|`amkeytype`|oid|pg\_type.oid|Type of data stored in index, or zero if not a fixed type|
|`aminsert`|regproc|pg\_proc.oid|"Insert this tuple" function|
|`ambeginscan`|regproc|pg\_proc.oid|"Prepare for index scan" function|
|`amgettuple`|regproc|pg\_proc.oid|"Next valid tuple" function, or zero if none|
|`amgetbitmap`|regproc|pg\_proc.oid|"Fetch all tuples" function, or zero if none|
|`amrescan`|regproc|pg\_proc.oid|"\(Re\)start index scan" function|
|`amendscan`|regproc|pg\_proc.oid|"Clean up after index scan" function|
|`ammarkpos`|regproc|pg\_proc.oid|"Mark current scan position" function|
|`amrestrpos`|regproc|pg\_proc.oid|"Restore marked scan position" function|
|`ambuild`|regproc|pg\_proc.oid|"Build new index" function|
|`ambuildempty`|regproc|pg\_proc.oid|"Build empty index" function|
|`ambulkdelete`|regproc|pg\_proc.oid|Bulk-delete function|
|`amvacuumcleanup`|regproc|pg\_proc.oid|Post-`VACUUM` cleanup function|
|`amcanreturn`|regproc|pg\_proc.oid|Function to check whether index supports index-only scans, or zero if none|
|`amcostestimate`|regproc|pg\_proc.oid|Function to estimate cost of an index scan|
|`amoptions`|regproc|pg\_proc.oid|Function to parse and validate `reloptions` for an index|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

