# pg_type_encoding 

The `pg_type_encoding` system catalog table contains the column storage type information.

|column|type|modifers|storage|description|
|------|----|--------|-------|-----------|
|`typeid`|oid|not null|plain|Foreign key to [pg\_attribute](pg_attribute.html)|
|`typoptions`|text \[ \]| |extended|The actual options|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

