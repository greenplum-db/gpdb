# foreign_data_wrapper_options 

The `foreign_data_wrapper_options` view contains all of the otpions defined for foreign-data wrappers in the current database. Greenplum Database displays only those foreign-data wrappers to which the current user has access \(by way of being the owner or having some privilege\).

|column|type|references|description|
|------|----|----------|-----------|
|`foreign_data_wrapper_catalog`|sql\_identifier| |Name of the database in which the foreign-data wrapper is defined \(always the current database\).|
|`foreign_data_wrapper_name`|sql\_identifier| |Name of the foreign-data wrapper.|
|`option_name`|sql\_identifier| |Name of an option.|
|`option_value`|character\_data| |Value of the option.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

