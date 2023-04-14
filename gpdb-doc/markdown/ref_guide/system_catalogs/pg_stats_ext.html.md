# pg_stats_ext

The `pg_stats` system view provides access to the information stored in the `pg_statistic_ext` and `pg_statistic_ext_data` catalog tables. This view allows access only to rows of `pg_statistic_ext` and `pg_statistic_ext_data` that correspond to tables the user has permission to read, and therefore it is safe to allow public read access to this view.

`pg_stats_ext` is also designed to present the information in a more readable format than the underlying catalogs — at the cost that its schema must be extended whenever new types of extended statistics are added to `pg_statistic_ext`.

|column|type|references|description|
|------|----|----------|-----------|
|`schemaname`|name|[pg\_namespace](pg_namespace.html).nspname|The name of the schema containing table.|
|`tablename`|name|[pg\_class](pg_class.html).relname|The name of the table.|
|`statistics_schemaname`|name|[pg\_namespace](pg_namespace.html).nspname|The name of the schema containing the extended statistic.|
|`statistics_name`|name|[pg\_statistic_ext](pg_statistic_ext.html).stxname|The name of the extended statistic.|
|`statistics_owner`|oid|[pg\_authid](pg_authid.html).oid|The owner of the extended statistic.|
|`attnames`|name[]|[pg\_attribute](pg_attribute.html).attname|The names of the columns on which the extended statistics is defined.|
|`kinds`|text[]| |The types of extended statistics enabled for this record.|
|`n_distinct`|pg_ndistinct| | N-distinct counts for combinations of column values. If greater than zero, the estimated number of distinct values in the combination. If less than zero, the negative of the number of distinct values divided by the number of rows. \(The negated form is used when `ANALYZE` believes that the number of distinct values is likely to increase as the table grows; the positive form is used when the column seems to have a fixed number of possible values.\) For example, `-1` indicates a unique combination of columns in which the number of distinct combinations is the same as the number of rows. |
|`dependencies`|pg_dependencies| | Functional dependency statistics. |
|`most_common_vals`|anyarray| |A list of the most common values in the column. \(Null if no combinations seem to be more common than any others.\)|
|`most_common_vals_null`|anyarray| |A list of NULL flags for the most combinations of values. \(Null when `most_common_vals` is.\) |
|`most_common_freqs`|real[]| |A list of the frequencies of the most common combinations, i.e., number of occurrences of each divided by total number of rows. \(Null when `most_common_vals` is.\)|
|`most_common_base_freqs`|real[]| |A list of the base frequencies of the most common combinations, i.e., product of per-value frequencies. \(Null when `most_common_vals` is.\)|

The maximum number of entries in the array fields can be controlled on a column-by-column basis using the `ALTER TABLE SET STATISTICS` command, or globally by setting the [default\_statistics\_target](../config_params/guc-list.html#default_statistics_target) run-time configuration parameter.

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

