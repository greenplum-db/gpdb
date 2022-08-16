# pg_namespace 

The `pg_namespace` system catalog table stores namespaces. A namespace is the structure underlying SQL schemas: each namespace can have a separate collection of relations, types, etc. without name conflicts.

|column|type|references|description|
|------|----|----------|-----------|
|`oid`|oid| |Row identifier \(hidden attribute; must be explicitly selected\)|
|`nspname`|name| |Name of the namespace|
|`nspowner`|oid|pg\_authid.oid|Owner of the namespace|
|`nspacl`|aclitem\[\]| |Access privileges as given by `GRANT` and `REVOKE`|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

