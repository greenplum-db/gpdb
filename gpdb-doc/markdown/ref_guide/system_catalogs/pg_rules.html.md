# pg_rules 

The view `pg_rules` provides access to useful information about query rewrite rules.

The `pg_rules` view excludes the `ON SELECT` rules of views and materialized views; those can be seen in `pg_views` and `pg_matviews`.

|column|type|references|description|
|------|----|----------|-----------|
|`schemaname`|name|pg\_namespace.nspname|Name of schema containing table|
|`tablename`|name|pg\_class.relname|Name of table the rule is for|
|`rulename`|name|pg\_rewrite.rulename|Name of rule|
|`definition`|text||Rule definition \(a reconstructed creation command\)|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

