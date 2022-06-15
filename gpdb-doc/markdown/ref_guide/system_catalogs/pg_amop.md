# pg\_amop 

The `pg_amop` table stores information about operators associated with index access method operator classes. There is one row for each operator that is a member of an operator class.

|column|type|references|description|
|------|----|----------|-----------|
|`amopclaid`|oid|pg\_opclass.oid|The index operator class this entry is for|
|`amopsubtype`|oid|pg\_type.oid|Subtype to distinguish multiple entries for one strategy; zero for default|
|`amopstrategy`|int2| |Operator strategy number|
|`amopreqcheck`|boolean| |Index hit must be rechecked|
|`amopopr`|oid|pg\_operator.oid|OID of the operator|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

