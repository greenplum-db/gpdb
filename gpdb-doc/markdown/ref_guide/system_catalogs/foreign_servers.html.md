# foreign_servers 

The `foreign_servers` view contains all foreign servers defined in the current database. Greenplum Database displays only those foreign servers to which the current user has access \(by way of being the owner or having some privilege\).

|column|type|references|description|
|------|----|----------|-----------|
|`foreign_server_catalog`|sql\_identifier| |Name of the database in which the foreign server is defined \(always the current database\).|
|`foreign_server_name`|sql\_identifier| |Name of the foreign server.|
|`foreign_data_wrapper_catalog`|sql\_identifier| |Name of the database in which the foreign-data wrapper used by the foreign server is defined \(always the current database\).|
|`foreign_data_wrapper_name`|sql\_identifier| |Name of the foreign-data wrapper used by the foreign server.|
|`foreign_server_type`|character\_data| |Foreign server type information, if specified upon creation.|
|`foreign_server_version`|character\_data| |Foreign server version information, if specified upon creation.|
|`authorization_identifier`|sql\_identifier| |Name of the owner of the foreign server.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

