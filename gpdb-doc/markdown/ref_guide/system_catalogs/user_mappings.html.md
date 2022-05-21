# user_mappings 

The `user_mappings` view contains all of the user mappings defined in the current database. Greenplum Database displays only those user mappings to which the current user has access \(by way of being the owner or having some privilege\).

|column|type|references|description|
|------|----|----------|-----------|
|`authorization_identifier`|sql\_identifier| |Name of the user being mapped, or `PUBLIC` if the mapping is public.|
|`foreign_server_catalog`|sql\_identifier| |Name of the database in which the foreign server used by this mapping is defined \(always the current database\).|
|`foreign_server_name`|sql\_identifier| |Name of the foreign server used by this mapping.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

