# pg_amproc 

The `pg_amproc` table stores information about support procedures associated with index access method operator classes. There is one row for each support procedure belonging to an operator class.

|column|type|references|description|
|------|----|----------|-----------|
|`oid`|oid| |Row identifier \(hidden attribute; must be explicitly selected\)|
|`amprocfamily`|oid|pg\_opfamily.oid|The operator family this entry is for|
|`amproclefttype`|oid|pg\_type.oid|Left-hand input data type of associated operator|
|`amprocrighttype`|oid|pg\_type.oid|Right-hand input data type of associated operator|
|`amprocnum`|int2| |Support procedure number|
|`amproc`|regproc|pg\_proc.oid|OID of the procedure|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

