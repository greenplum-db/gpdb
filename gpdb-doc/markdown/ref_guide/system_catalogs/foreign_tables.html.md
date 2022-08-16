# foreign_tables 

The `foreign_tables` view contains all foreign tables defined in the current database. Greenplum Database displays only those foreign tables to which the current user has access \(by way of being the owner or having some privilege\).

|column|type|references|description|
|------|----|----------|-----------|
|`foreign_table_catalog`|sql\_identifier| |Name of the database in which the foreign table is defined \(always the current database\).|
|`foreign_table_schema`|sql\_identifier| |Name of the schema that contains the foreign table.|
|`foreign_table_name`|sql\_identifier| |Name of the foreign table.|
|`foreign_server_catalog`|sql\_identifier| |Name of the database in which the foreign server is defined \(always the current database\).|
|`foreign_server_name`|sql\_identifier| |Name of the foreign server.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

