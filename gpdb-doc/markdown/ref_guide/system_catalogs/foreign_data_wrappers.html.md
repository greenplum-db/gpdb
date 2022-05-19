# foreign_data_wrappers 

The `foreign_data_wrappers` view contains all foreign-data wrappers defined in the current database. Greenplum Database displays only those foreign-data wrappers to which the current user has access \(by way of being the owner or having some privilege\).

|column|type|references|description|
|------|----|----------|-----------|
|`foreign_data_wrapper_catalog`|sql\_identifier| |Name of the database in which the foreign-data wrapper is defined \(always the current database\).|
|`foreign_data_wrapper_name`|sql\_identifier| |Name of the foreign-data wrapper.|
|`authorization_identifier`|sql\_identifier| |Name of the owner of the foreign server.|
|`library_name`|character\_data| |File name of the library that implements this foreign-data wrapper.|
|`foreign_data_wrapper_language`|character\_data| |Language used to implement the foreign-data wrapper.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

