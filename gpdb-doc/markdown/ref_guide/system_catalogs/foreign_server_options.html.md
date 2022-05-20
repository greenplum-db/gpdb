# foreign_server_options 

The `foreign_server_options` view contains all of the options defined for foreign servers in the current database. Greenplum Database displays only those foreign servers to which the current user has access \(by way of being the owner or having some privilege\).

|column|type|references|description|
|------|----|----------|-----------|
|`foreign_server_catalog`|sql\_identifier| |Name of the database in which the foreign server is defined \(always the current database\).|
|`foreign_server_name`|sql\_identifier| |Name of the foreign server.|
|`option_name`|sql\_identifier| |Name of an option.|
|`option_value`|character\_data| |Value of the option.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

