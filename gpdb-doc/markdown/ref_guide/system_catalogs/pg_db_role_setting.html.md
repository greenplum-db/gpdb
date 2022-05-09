# pg_db_role_setting 

The `pg_db_role_setting` system catalog table records the default values of server configuration settings for each role and database combination.

There is a single copy of `pg_db_role_settings` per Greenplum Database cluster. This system catalog table is shared across all databases.

You can view the server configuration settings for your Greenplum Database cluster with `psql`'s `\drds` meta-command.

|column|type|references|description|
|------|----|----------|-----------|
|`setdatabase`|oid|pg\_database.oid|The database to which the setting is applicable, or zero if the setting is not database-specific.|
|`setrole`|oid|pg\_authid.oid|The role to which the setting is applicable, or zero if the setting is not role-specific.|
|`setconfig`|text\[\]| |Per-database- and per-role-specific defaults for user-settable server configuration parameters.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

