# pg_statistic 

The `pg_statistic` system catalog table stores statistical data about the contents of the database. Entries are created by [ANALYZE](../sql_commands/ANALYZE.html) and subsequently used by the query planner. Note that all the statistical data is inherently approximate, even assuming that it is up-to-date.

Normally, there is one entry, with `stainherit = false`, for each table column that has been analyzed. If the table has inheritance children, Greenplum Database creates a second entry with `stainherit = true`. This row represents the column's statistics over the inheritance tree, for example, statistics for the data you would see with `SELECT column FROM table*`, whereas the `stainherit = false` row represents the results of `SELECT column FROM ONLY table`.

`pg_statistic` also stores statistical data about the values of index expressions. These are described as if they were actual data columns; in particular, `starelid` references the index. No entry is made for an ordinary non-expression index column, however, since it would be redundant with the entry for the underlying table column. Currently, entries for index expressions always have `stainherit = false`.

Since different kinds of statistics may be appropriate for different kinds of data, `pg_statistic` is designed not to assume very much about what sort of statistics it stores. Only extremely general statistics \(such as nullness\) are given dedicated columns in `pg_statistic`. Everything else is stored in slots, which are groups of associated columns whose content is identified by a code number in one of the slot's columns.

`pg_statistic` should not be readable by the public, since even statistical information about a table's contents should be considered sensitive. \(Example: minimum and maximum values of a salary column\). [pg\_stats](pg_stats.html) is a publicly readable view on `pg_statistic` that only exposes information about those tables that are readable by the current user.

XXX
> **Caution** Diagnostic tools such as `gpsd` and `minirepro` collect sensitive information from `pg_statistic`, such as histogram boundaries, in a clear, readable form. Always review the output files of these utilities to ensure that the contents are acceptable for transport outside of the database in your organization.

|column|type|references|description|
|------|----|----------|-----------|
|`starelid`|oid|[pg\_class](pg_class.html).oid|The table or index that the described column belongs to.|
|`staattnum`|int2|[pg\_attribute](pg_attribute.html).attnum|The number of the described column.|
|`stainherit`|bool| |If true, the statistics include inheritance child columns, not just the values in the specified relations.|
|`stanullfrac`|float4| |The fraction of the column's entries that are null.|
|`stawidth`|int4| |The average stored width, in bytes, of nonnull entries.|
|`stadistinct`|float4| |The number of distinct nonnull data values in the column. A value greater than zero is the actual number of distinct values. A value less than zero is the negative of a multiplier for the number rows in the table \(for example, a column in which about 80% of the values are nonnull and each nonnull value appears about twice on the average could be represented by `stadistinct` = -0.4\). A zero value means the number of distinct values is unknown.|
|`stakind*N*`|int2| |A code number indicating the kind of statistics stored in the `N`th slot of the `pg_statistic` row.|
|`staop*N*`|oid|[pg\_operator](pg_operator.html).oid|An operator used to derive the statistics stored in the `N`th slot. For example, a histogram slot would show the `<` operator that defines the sort order of the data.|
|`stacoll*N*`|oid|pg\_collation.oid|The collation used to derive the statistics stored in the `N`th slot. For example, a histogram slot for a collatable column would show the collation that defines the sort order of the data. Zero for noncollatable data.|
|`stanumbers*N*`|float4\[\]| |Numerical statistics of the appropriate kind for the `N`th slot, or NULL if the slot kind does not involve numerical values.|
|`stavalues*N*`|anyarray| |Column data values of the appropriate kind for the `N`th slot, or NULL if the slot kind does not store any data values. Each array's element values are actually of the specific column's data type, or a related type such as an array's element type, so there is no way to define these columns' type more specifically than `anyarray`.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

