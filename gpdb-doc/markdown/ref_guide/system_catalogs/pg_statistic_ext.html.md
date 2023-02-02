# pg_statistic_ext

The `pg_statistic_ext` system catalog table holds definitions of extended planner statistics. Each row in this catalog corresponds to a *statistics object* created with [CREATE STATISTICS](../sql_commands/CREATE_STATISTICS.html).

|column|type|references|description|
|------|----|----------|-----------|
|`stxrelid`|oid|[pg\_class](pg_class.html).oid | The table containing the columns described by this object |
|`stxname`|name| | The name of the statistics object |
|`stxnamespace`|[pg\_namespace](pg_namespace.html).oid| | The object identifier of the namespace that contains this statistics object |
|`stxowner`|oid|[pg\_authid](pg_authid.html).oid | The owner of the statistics object |
|`stxkeys`|int2vector|[pg\_attribute](pg_attribute.html).oid | An array of attribute numbers, indicating which table columns are covered by this statistics object; for example, a value of `1 3` would mean that the first and the third table columns are covered |
|`stxkind`|char[]| | An array containing codes for the enabled statistics kinds; valid values are: `d` for n-distinct statistics, `f` for functional dependency statistics, and `m` for most common values \(MCV\) list statistics |

The `pg_statistic_ext` entry is filled in completely during `CREATE STATISTICS`, but the actual statistical values are not computed then. Subsequent `ANALYZE` commands compute the desired values and populate an entry in the [pg\_statistic\_ext\_data](pg_statistic_ext_data.html) catalog.

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

