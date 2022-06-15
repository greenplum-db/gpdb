# pg\_am 

The `pg_am` table stores information about index access methods. There is one row for each index access method supported by the system.

|column|type|references|description|
|------|----|----------|-----------|
|`amname`|name| |Name of the access method|
|`amstrategies`|int2| |Number of operator strategies for this access method|
|`amsupport`|int2| |Number of support routines for this access method|
|`amorderstrategy`|int2| |Zero if the index offers no sort order, otherwise the strategy number of the strategy operator that describes the sort order|
|`amcanunique`|boolean| |Does the access method support unique indexes?|
|`amcanmulticol`|boolean| |Does the access method support multicolumn indexes?|
|`amoptionalkey`|boolean| |Does the access method support a scan without any constraint for the first index column?|
|`amindexnulls`|boolean| |Does the access method support null index entries?|
|`amstorage`|boolean| |Can index storage data type differ from column data type?|
|`amclusterable`|boolean| |Can an index of this type be clustered on?|
|`aminsert`|regproc|pg\_proc.oid|"Insert this tuple" function|
|`ambeginscan`|regproc|pg\_proc.oid|"Start new scan" function|
|`amgettuple`|regproc|pg\_proc.oid|"Next valid tuple" function|
|`amgetmulti`|regproc|pg\_proc.oid|"Fetch multiple tuples" function|
|`amrescan`|regproc|pg\_proc.oid|"Restart this scan" function|
|`amendscan`|regproc|pg\_proc.oid|"End this scan" function|
|`ammarkpos`|regproc|pg\_proc.oid|"Mark current scan position" function|
|`amrestrpos`|regproc|pg\_proc.oid|"Restore marked scan position" function|
|`ambuild`|regproc|pg\_proc.oid|"Build new index" function|
|`ambulkdelete`|regproc|pg\_proc.oid|Bulk-delete function|
|`amvacuumcleanup`|regproc|pg\_proc.oid|Post-`VACUUM` cleanup function|
|`amcostestimate`|regproc|pg\_proc.oid|Function to estimate cost of an index scan|
|`amoptions`|regproc|pg\_proc.oid|Function to parse and validate reloptions for an index|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

