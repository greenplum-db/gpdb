# user_mapping_options 

The `user_mapping_options` view contains all of the options defined for user mappings in the current database. Greenplum Database displays only those user mappings to which the current user has access \(by way of being the owner or having some privilege\).

|column|type|references|description|
|------|----|----------|-----------|
|`authorization_identifier`|sql\_identifier| |Name of the user being mapped, or `PUBLIC` if the mapping is public.|
|`foreign_server_catalog`|sql\_identifier| |Name of the database in which the foreign server used by this mapping is defined \(always the current database\).|
|`foreign_server_name`|sql\_identifier| |Name of the foreign server used by this mapping.|
|`option_name`|sql\_identifier| |Name of an option.|
|`option_value`|character\_data| |Value of the option. This column will display null unless:<br/><br/>-   The current user is the user being mapped.<br/><br/>-   The mapping is for `PUBLIC` and the current user is the foreign server owner.<br/><br/>-   The current user is a superuser.<br/><br/> The intent is to protect password information stored as a user mapping option.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

