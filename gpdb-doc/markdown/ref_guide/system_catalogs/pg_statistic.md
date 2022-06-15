# pg\_statistic 

The `pg_statistic` system catalog table stores statistical data about the contents of the database. Entries are created by `ANALYZE` and subsequently used by the query optimizer. There is one entry for each table column that has been analyzed. Note that all the statistical data is inherently approximate, even assuming that it is up-to-date.

`pg_statistic` also stores statistical data about the values of index expressions. These are described as if they were actual data columns; in particular, `starelid` references the index. No entry is made for an ordinary non-expression index column, however, since it would be redundant with the entry for the underlying table column.

Since different kinds of statistics may be appropriate for different kinds of data, `pg_statistic` is designed not to assume very much about what sort of statistics it stores. Only extremely general statistics \(such as nullness\) are given dedicated columns in `pg_statistic`. Everything else is stored in slots, which are groups of associated columns whose content is identified by a code number in one of the slot's columns.

Statistical information about a table's contents should be considered sensitive \(for example: minimum and maximum values of a salary column\). `pg_stats` is a publicly readable view on `pg_statistic` that only exposes information about those tables that are readable by the current user.

**Warning:** Diagnostic tools such as `gpsd` and `minirepro` collect sensitive information from `pg_statistic`, such as histogram boundaries, in a clear, readable form. Always review the output files of these utilities to ensure that the contents are acceptable for transport outside of the database in your organization.

|column|type|references|description|
|------|----|----------|-----------|
|`starelid`|oid|pg\_class.oid|The table or index that the described column belongs to.|
|`staattnum`|int2|pg\_attribute.attnum|The number of the described column.|
|`stanullfrac`|float4| |The fraction of the column's entries that are null.|
|`stawidth`|int4| |The average stored width, in bytes, of nonnull entries.|
|`stadistinct`|float4| |The number of distinct nonnull data values in the column. A value greater than zero is the actual number of distinct values. A value less than zero is the negative of a fraction of the number of rows in the table \(for example, a column in which values appear about twice on the average could be represented by `stadistinct` = -0.5\). A zero value means the number of distinct values is unknown.|
|stakind*N*|int2| |A code number indicating the kind of statistics stored in the `N`th slot of the `pg_statistic` row.|
|staop*N*|oid|pg\_operator.oid|An operator used to derive the statistics stored in the `N`th slot. For example, a histogram slot would show the `<` operator that defines the sort order of the data.|
|stanumbers*N*|float4\[\]| |Numerical statistics of the appropriate kind for the `N`th slot, or NULL if the slot kind does not involve numerical values.|
|stavalues*N*|anyarray| |Column data values of the appropriate kind for the `N`th slot, or NULL if the slot kind does not store any data values. Each array's element values are actually of the specific column's data type, so there is no way to define these columns' type more specifically than `anyarray`.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

