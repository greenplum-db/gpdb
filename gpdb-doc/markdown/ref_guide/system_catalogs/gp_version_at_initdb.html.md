# gp_version_at_initdb 

The `gp_version_at_initdb` table is populated on the coordinator and each segment in the Greenplum Database system. It identifies the version of Greenplum Database used when the system was first initialized. This table is defined in the `pg_global` tablespace, meaning it is globally shared across all databases in the system.

|column|type|references|description|
|------|----|----------|-----------|
|`schemaversion`|integer| |Schema version number.|
|`productversion`|text| |Product version number.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

