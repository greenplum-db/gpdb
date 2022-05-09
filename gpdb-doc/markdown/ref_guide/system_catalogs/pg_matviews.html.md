# pg_matviews 

The view `pg_matviews` provides access to useful information about each materialized view in the database.

|column|type|references|description|
|------|----|----------|-----------|
|`schemaname`|name|pg\_namespace.nspname|Name of the schema containing the materialized view|
|`matviewname`|name|pg\_class.relname|Name of the materialized view|
|`matviewowner`|name|pg\_authid.rolname|Name of the materialized view's owner|
|`tablespace`|name|pg\_tablespace.spcname|Name of the tablespace containing the materialized view \(NULL if default for the database\)|
|`hasindexes`|boolean||True if the materialized view has \(or recently had\) any indexes|
|`ispopulated`|boolean||True if the materialized view is currently populated|
|`definition`|text||Materialized view definition \(a reconstructed `SELECT` command\)|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

