# pg_stats

The `pg_stats` system view provides access to the information stored in the `pg_statistic` catalog. This view allows access only to rows of `pg_statistic` that correspond to tables the user has permission to read, and therefore it is safe to allow public read access to this view.

`pg_stats` is also designed to present the information in a more readable format than the underlying catalog — at the cost that its schema must be extended whenever new slot types are defined for `pg_statistic`.

|column|type|references|description|
|------|----|----------|-----------|
|`schemaname`|name|[pg\_namespace](pg_namespace.html).nspname| The name of the schema containing table.|
|`tablename`|name|[pg\_class](pg_class.html).relname|The name of the table.|
|`attname`|name|[pg\_attribute](pg_attribute.html).attname|The name of the column described by this row.|
|`inherited`|bool| |If true, this row includes inheritance child columns, not just the values in the specified table.|
|`null_frac`|real| |The fraction of column entries that are null.|
|`avg_width`|integer| |The average width in bytes of column's entries.|
|`n_distinct`|real| |If greater than zero, the estimated number of distinct values in the column. If less than zero, the negative of the number of distinct values divided by the number of rows. \(The negated form is used when ANALYZE believes that the number of distinct values is likely to increase as the table grows; the positive form is used when the column seems to have a fixed number of possible values.\) For example, `-1` indicates a unique column in which the number of distinct values is the same as the number of rows.|
|`most_common_vals`|anyarray| |A list of the most common values in the column. \(Null if no values seem to be more common than any others.\)|
|`most_common_freqs`|real[]| |A list of the frequencies of the most common values, i.e., number of occurrences of each divided by total number of rows. \(Null when `most_common_vals` is.\)|
|`histogram_bounds`|anyarray| |A list of values that divide the column's values into groups of approximately equal population. The values in `most_common_vals`, if present, are omitted from this histogram calculation. \(This column is null if the column data type does not have a `<` operator or if the `most_common_vals` list accounts for the entire population.\)|
|`correlation`|real| |Statistical correlation between physical row ordering and logical ordering of the column values. This ranges from -1 to +1. When the value is near -1 or +1, an index scan on the column will be estimated to be cheaper than when it is near zero, due to reduction of random access to the disk. \(This column is null if the column data type does not have a `<` operator.\)|
|`most_common_elems`|anyarray| |A list of non-null element values most often appearing within values of the column. \(Null for scalar types.\)|
|`most_common_elem_freqs`|real[]| |A list of the frequencies of the most common element values, i.e., the fraction of rows containing at least one instance of the given value. Two or three additional values follow the per-element frequencies; these are the minimum and maximum of the preceding per-element frequencies, and optionally the frequency of null elements. \(Null when most_common_elems is.\)|
|`element_count_histogram`|real[]| |A histogram of the counts of distinct non-null element values within the values of the column, followed by the average number of distinct non-null elements. \(Null for scalar types.\)|

The maximum number of entries in the array fields can be controlled on a column-by-column basis using the `ALTER TABLE SET STATISTICS` command, or globally by setting the [default\_statistics\_target](../config_params/guc-list.html#default_statistics_target) run-time configuration parameter.

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

