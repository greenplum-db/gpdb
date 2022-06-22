# pg\_user\_mapping 

The `pg_user_mapping` catalog table stores the mappings from local users to remote users. You must have administrator privileges to view this catalog.

|column|type|references|description|
|------|----|----------|-----------|
|`umuser`|oid|pg\_authid.oid|OID of the local role being mapped, 0 if the user mapping is public|
|`umserver`|oid|pg\_foreign\_server.oid|The OID of the foreign server that contains this mapping|
|`umoptions`|text<br/>\[ \]| |User mapping specific options, as "keyword=value" strings.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

