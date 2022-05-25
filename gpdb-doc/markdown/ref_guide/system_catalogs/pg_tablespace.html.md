# pg_tablespace 

The `pg_tablespace` system catalog table stores information about the available tablespaces. Tables can be placed in particular tablespaces to aid administration of disk layout. Unlike most system catalogs, `pg_tablespace` is shared across all databases of a Greenplum system: there is only one copy of `pg_tablespace` per system, not one per database.

|column|type|references|description|
|------|----|----------|-----------|
|`spcname`|name| |Tablespace name.|
|`spcowner`|oid|pg\_authid.oid|Owner of the tablespace, usually the user who created it.|
|`spcacl`|aclitem\[\]| |Tablespace access privileges.|
|`spcoptions`|text\[\]| |Tablespace contentID locations.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

