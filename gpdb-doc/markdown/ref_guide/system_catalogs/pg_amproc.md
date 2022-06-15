# pg\_amproc 

The `pg_amproc` table stores information about support procedures associated with index access method operator classes. There is one row for each support procedure belonging to an operator class.

|column|type|references|description|
|------|----|----------|-----------|
|`amopclaid`|oid|pg\_opclass.oid|The index operator class this entry is for|
|`amprocsubtype`|oid|pg\_type.oid|Subtype, if cross-type routine, else zero|
|`amprocnum`|int2| |Support procedure number|
|`amproc`|regproc|pg\_proc.oid|OID of the procedure|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

