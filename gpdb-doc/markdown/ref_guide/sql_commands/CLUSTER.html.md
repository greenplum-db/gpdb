# CLUSTER 

Physically reorders a table on disk according to an index. 

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
CLUSTER <indexname> ON <tablename>

CLUSTER [VERBOSE] <tablename> [ USING index_name ]

CLUSTER [VERBOSE]
```

## <a id="section3"></a>Description 

`CLUSTER` orders a table based on an index. `CLUSTER` on non-B-tree indexes is not supported on append-optimized storage tables. Clustering an index means that the records are physically ordered on disk according to the index information. If the records you need are distributed randomly on disk, then the database has to seek across the disk to get the records requested. If those records are stored more closely together, then the fetching from disk is more sequential. A good example for a clustered index is on a date column where the data is ordered sequentially by date. A query against a specific date range will result in an ordered fetch from the disk, which leverages faster sequential access.

Clustering is a one-time operation: when the table is subsequently updated, the changes are not clustered. That is, no attempt is made to store new or updated rows according to their index order. If you wish, you can periodically recluster by issuing the command again. Setting the table's `FILLFACTOR` storage parameter to less than 100% can aid in preserving cluster ordering during updates, because updated rows are kept on the same page if enough space is available there.

When a table is clustered using this command, Greenplum Database remembers on which index it was clustered. The form `CLUSTER` tablename reclusters the table on the same index that it was clustered before. You can use the `CLUSTER` or `SET WITHOUT CLUSTER` forms of [ALTER TABLE](ALTER_TABLE.html) to set the index to use for future cluster operations, or to clear any previous setting. `CLUSTER` without any parameter reclusters all previously clustered tables in the current database that the calling user owns, or all tables if called by a superuser. This form of `CLUSTER` cannot be run inside a transaction block.

When a table is being clustered, an `ACCESS EXCLUSIVE` lock is acquired on it. This prevents any other database operations \(both reads and writes\) from operating on the table until the `CLUSTER` is finished.

## <a id="section4"></a>Parameters 

indexname
:   The name of an index.

`VERBOSE`
:   Prints a progress report as each table is clustered.

tablename
:   The name \(optionally schema-qualified\) of a table.

## <a id="section5"></a>Notes 

In cases where you are accessing single rows randomly within a table, the actual order of the data in the table is unimportant. However, if you tend to access some data more than others, and there is an index that groups them together, you will benefit from using `CLUSTER`. If you are requesting a range of indexed values from a table, or a single indexed value that has multiple rows that match, `CLUSTER` will help because once the index identifies the table page for the first row that matches, all other rows that match are probably already on the same table page, and so you save disk accesses and speed up the query.

`CLUSTER` can re-sort the table using either an index scan on the specified index, or \(if the index is a b-tree\) a sequential scan followed by sorting. It will attempt to choose the method that will be faster, based on planner cost parameters and available statistical information.

When an index scan is used, a temporary copy of the table is created that contains the table data in the index order. Temporary copies of each index on the table are created as well. Therefore, you need free space on disk at least equal to the sum of the table size and the index sizes.

When a sequential scan and sort is used, a temporary sort file is also created, so that the peak temporary space requirement is as much as double the table size, plus the index sizes. This method is often faster than the index scan method, but if the disk space requirement is intolerable, you can deactivate this choice by temporarily setting the [enable\_sort](../config_params/guc-list.html) configuration parameter to `off`.

It is advisable to set [maintenance\_work\_mem](../config_params/guc-list.html) configuration parameter to a reasonably large value \(but not more than the amount of RAM you can dedicate to the `CLUSTER` operation\) before clustering.

Because the query optimizer records statistics about the ordering of tables, it is advisable to run `ANALYZE` on the newly clustered table. Otherwise, the planner may make poor choices of query plans.

Because `CLUSTER` remembers which indexes are clustered, you can cluster the tables you want clustered manually the first time, then set up a periodic maintenance script that runs `CLUSTER` without any parameters, so that the desired tables are periodically reclustered.

> **Note** `CLUSTER` on non-B-tree indexes is not supported with append-optimized tables.

## <a id="section6"></a>Examples 

Cluster the table `employees` on the basis of its index `emp_ind`:

```
CLUSTER emp_ind ON emp;
```

Cluster a large table by recreating it and loading it in the correct index order:

```
CREATE TABLE newtable AS SELECT * FROM table ORDER BY column;
DROP table;
ALTER TABLE newtable RENAME TO table;
CREATE INDEX column_ix ON table (column);
VACUUM ANALYZE table;
```

## <a id="section7"></a>Compatibility 

There is no `CLUSTER` statement in the SQL standard.

## <a id="section8"></a>See Also 

[CREATE TABLE AS](CREATE_TABLE_AS.html), [CREATE INDEX](CREATE_INDEX.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

