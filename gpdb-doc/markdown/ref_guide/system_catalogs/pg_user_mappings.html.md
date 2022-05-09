# pg_user_mappings 

The `pg_user_mappings` view provides access to information about user mappings. This view is essentially a public-readble view of the `pg_user_mapping` system catalog table that omits the options field if the user does not have access rights to view it.

|column|type|references|description|
|------|----|----------|-----------|
|`umid`|oid|pg\_user\_mapping.oid|OID of the user mapping.|
|`srvid`|oid|pg\_foreign\_server.oid|OID of the foreign server that contains this mapping.|
|`srvname`|text|pg\_foreign\_server.srvname|Name of the foreign server.|
|`umuser`|oid|pg\_authid.oid|OID of the local role being mapped, 0 if the user mapping is public.|
|`usename`|name| |Name of the local user to be mapped.|
|`umoptions`|text\[\]| |User mapping-specific options, as "keyword=value" strings.|

To protect password information stored as a user mapping option, the `umoptions` column reads as null unless one of the following applies:

-   The current user is the user being mapped, and owns the server or holds `USAGE` privilege on it.
-   The current user is the server owner and the mapping is for `PUBLIC`.
-   The current user is a superuser.

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

