# pg_am 

The `pg_am` table stores information about index access methods. There is one row for each index access method supported by the system.

|column|type|references|description|
|------|----|----------|-----------|
|`oid`|oid| |Row identifier \(hidden attribute; must be explicitly selected\)|
|`amname`|name| |Name of the access method|
|`amhandler`|regproc| | OID of a handler function responsible for supplying information about the access method|
|`amtype`|char| |`t` for table (including materialized views), `i` for index|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

