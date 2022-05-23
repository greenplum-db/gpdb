# pg_foreign_server 

The system catalog table `pg_foreign_server` stores foreign server definitions. A foreign server describes a source of external data, such as a remote server. You access a foreign server via a foreign-data wrapper.

|column|type|references|description|
|------|----|----------|-----------|
|`srvname`|name| |Name of the foreign server.|
|`srvowner`|oid|pg\_authid.oid|Owner of the foreign server.|
|`srvfdw`|oid|pg\_foreign\_data\_wrapper.oid|OID of the foreign-data wrapper of this foreign server.|
|`srvtype`|text| |Type of server \(optional\).|
|`srvversion`|text| |Version of the server \(optional\).|
|`srvacl`|aclitem\[\]| |Access privileges; see [GRANT](../sql_commands/GRANT.html) and [REVOKE](../sql_commands/REVOKE.html) for details.|
|`srvoptions`|text\[\]| |Foreign server-specific options, as "keyword=value" strings.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

