# pg_statistic_ext_data

The `pg_statistic_ext_data` system catalog table holds data for extended planner statistics defined in [pg\_statistic\_ext](pg_statistic_ext.html). Each row in this catalog corresponds to a *statistics object* created with [CREATE STATISTICS](../sql_commands/CREATE_STATISTICS.html).

Like `pg_statistic`, `pg_statistic_ext_data` should not be readable by the public, since the contents might be considered sensitive. \(Example: most common combinations of values in columns might be quite interesting.\) [pg\_stats\_ext](pg_stats_ext.html) is a publicly readable view on `pg_statistic_ext_data` \(after joining with `pg_statistic_ext`\) that only exposes information about those tables and columns that are readable by the current user.

|column|type|references|description|
|------|----|----------|-----------|
|`stxoid`|oid|[pg\_statistic_ext](pg_statistic_ext.html).oid | The extended statistic object containing the definition for this data. |
|`stxdndistinct`|pg\_ndistinct| | N-distinct counts, serialized as `pg_ndistinct` type. |
|`stxddependencies`|pg\_dependencies| | Functional dependency statistics, serialized as `pg_dependencies` type. |
|`stxdmcv`|pg\_mcv\_list| | MCV \(most-common values\) list statistics, serialized as `pg_mcv_list` type. |

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

