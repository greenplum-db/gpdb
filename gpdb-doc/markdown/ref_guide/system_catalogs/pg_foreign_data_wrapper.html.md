# pg_foreign_data_wrapper 

The system catalog table `pg_foreign_data_wrapper` stores foreign-data wrapper definitions. A foreign-data wrapper is a mechanism by which you access external data residing on foreign servers.

|column|type|references|description|
|------|----|----------|-----------|
|`fdwname`|name| |Name of the foreign-data wrapper.|
|`fdwowner`|oid|pg\_authid.oid|Owner of the foreign-data wrapper.|
|`fdwhandler`|oid|pg\_proc.oid|A reference to a handler function that is responsible for supplying execution routines for the foreign-data wrapper. Zero if no handler is provided.|
|`fdwvalidator`|oid|pg\_proc.oid|A reference to a validator function that is responsible for checking the validity of the options provided to the foreign-data wrapper. This function also checks the options for foreign servers and user mappings using the foreign-data wrapper. Zero if no validator is provided.|
|`fdwacl`|aclitem\[\]| |Access privileges; see [GRANT](../sql_commands/GRANT.html) and [REVOKE](../sql_commands/REVOKE.html) for details.|
|`fdwoptions`|text\[\]| |Foreign-data wrapper-specific options, as "keyword=value" strings.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

