# pg_user_mapping 

The system catalog table `pg_user_mapping` stores the mappings from local user to remote user. You must have administrator privileges to view this catalog. Access to this catalog is restricted from normal users, use the `pg_user_mappings` view instead.

|column|type|references|description|
|------|----|----------|-----------|
|`umuser`|oid|pg\_authid.oid|OID of the local role being mapped, 0 if the user mapping is public.|
|`umserver`|oid|pg\_foreign\_server.oid|OID of the foreign server that contains this mapping.|
|`umoptions`|text\[\]| |User mapping-specific options, as "keyword=value" strings.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

