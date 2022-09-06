---
title: CREATE INDEX 
---

Defines a new index.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
CREATE [UNIQUE] INDEX [<name>] ON <table_name> [USING <method>]
       ( {<column_name> | (<expression>)} [COLLATE <parameter>] [<opclass>] [ ASC | DESC ] [ NULLS { FIRST | LAST } ] [, ...] )
       [ WITH ( <storage_parameter> = <value> [, ... ] ) ]
       [ TABLESPACE <tablespace> ]
       [ WHERE <predicate> ]
```

## <a id="section3"></a>Description 

`CREATE INDEX` constructs an index on the specified column\(s\) of the specified table or materialized view. Indexes are primarily used to enhance database performance \(though inappropriate use can result in slower performance\).

The key field\(s\) for the index are specified as column names, or alternatively as expressions written in parentheses. Multiple fields can be specified if the index method supports multicolumn indexes.

An index field can be an expression computed from the values of one or more columns of the table row. This feature can be used to obtain fast access to data based on some transformation of the basic data. For example, an index computed on `upper(col)` would allow the clause `WHERE upper(col) = 'JIM'` to use an index.

Greenplum Database provides the index methods B-tree, bitmap, GiST, SP-GiST, and GIN. Users can also define their own index methods, but that is fairly complicated.

When the `WHERE` clause is present, a partial index is created. A partial index is an index that contains entries for only a portion of a table, usually a portion that is more useful for indexing than the rest of the table. For example, if you have a table that contains both billed and unbilled orders where the unbilled orders take up a small fraction of the total table and yet is most often selected, you can improve performance by creating an index on just that portion.

The expression used in the `WHERE` clause may refer only to columns of the underlying table, but it can use all columns, not just the ones being indexed. Subqueries and aggregate expressions are also forbidden in `WHERE`. The same restrictions apply to index fields that are expressions.

All functions and operators used in an index definition must be immutable. Their results must depend only on their arguments and never on any outside influence \(such as the contents of another table or a parameter value\). This restriction ensures that the behavior of the index is well-defined. To use a user-defined function in an index expression or `WHERE` clause, remember to mark the function `IMMUTABLE` when you create it.

## <a id="section4"></a>Parameters 

UNIQUE
:   Checks for duplicate values in the table when the index is created and each time data is added. Duplicate entries will generate an error. Unique indexes only apply to B-tree indexes. In Greenplum Database, unique indexes are allowed only if the columns of the index key are the same as \(or a superset of\) the Greenplum distribution key. On partitioned tables, a unique index is only supported within an individual partition - not across all partitions.

name
:   The name of the index to be created. The index is always created in the same schema as its parent table. If the name is omitted, Greenplum Database chooses a suitable name based on the parent table's name and the indexed column name\(s\).

table\_name
:   The name \(optionally schema-qualified\) of the table to be indexed.

method
:   The name of the index method to be used. Choices are `btree`, `bitmap`, `gist`, `spgist`, and `gin`. The default method is `btree`.

:   Currently, only the B-tree, GiST, and GIN index methods support multicolumn indexes. Up to 32 fields can be specified by default. Only B-tree currently supports unique indexes.

:   GPORCA supports only B-tree, bitmap, GiST, and GIN indexes. GPORCA ignores indexes created with unsupported indexing methods.

column\_name
:   The name of a column of the table on which to create the index. Only the B-tree, bitmap, GiST, and GIN index methods support multicolumn indexes.

expression
:   An expression based on one or more columns of the table. The expression usually must be written with surrounding parentheses, as shown in the syntax. However, the parentheses may be omitted if the expression has the form of a function call.

collation
:   The name of the collation to use for the index. By default, the index uses the collation declared for the column to be indexed or the result collation of the expression to be indexed. Indexes with non-default collations can be useful for queries that involve expressions using non-default collations.

opclass
:   The name of an operator class. The operator class identifies the operators to be used by the index for that column. For example, a B-tree index on four-byte integers would use the `int4_ops` class \(this operator class includes comparison functions for four-byte integers\). In practice the default operator class for the column's data type is usually sufficient. The main point of having operator classes is that for some data types, there could be more than one meaningful ordering. For example, a complex-number data type could be sorted by either absolute value or by real part. We could do this by defining two operator classes for the data type and then selecting the proper class when making an index.

ASC
:   Specifies ascending sort order \(which is the default\).

DESC
:   Specifies descending sort order.

NULLS FIRST
:   Specifies that nulls sort before non-nulls. This is the default when `DESC` is specified.

NULLS LAST
:   Specifies that nulls sort after non-nulls. This is the default when `DESC` is not specified.

storage\_parameter
:   The name of an index-method-specific storage parameter. Each index method has its own set of allowed storage parameters.

:   `FILLFACTOR` - B-tree, bitmap, GiST, and SP-GiST index methods all accept this parameter. The `FILLFACTOR` for an index is a percentage that determines how full the index method will try to pack index pages. For B-trees, leaf pages are filled to this percentage during initial index build, and also when extending the index at the right \(adding new largest key values\). If pages subsequently become completely full, they will be split, leading to gradual degradation in the index's efficiency. B-trees use a default fillfactor of 90, but any integer value from 10 to 100 can be selected. If the table is static then fillfactor 100 is best to minimize the index's physical size, but for heavily updated tables a smaller fillfactor is better to minimize the need for page splits. The other index methods use fillfactor in different but roughly analogous ways; the default fillfactor varies between methods.

:   `BUFFERING` - In addition to `FILLFACTOR`, GiST indexes additionally accept the `BUFFERING` parameter. `BUFFERING` determines whether Greenplum Database builds the index using the buffering build technique described in [GiST buffering build](https://www.postgresql.org/docs/9.4/gist-implementation.html) in the PostgreSQL documentation. With `OFF` it is deactivated, with `ON` it is enabled, and with `AUTO` it is initially deactivated, but turned on on-the-fly once the index size reaches [effective-cache-size](../config_params/guc-list.html). The default is `AUTO`.

:   `FASTUPDATE` - The GIN index method accepts the `FASTUPDATE` storage parameter. `FASTUPDATE` is a Boolean parameter that deactivates or enables the GIN index fast update technique. A value of ON enables fast update \(the default\), and OFF deactivates it. See [GIN fast update technique](https://www.postgresql.org/docs/9.4/gin-implementation.html#GIN-FAST-UPDATE) in the PostgreSQL documentation for more information.

    **Note:** Turning `FASTUPDATE` off via `ALTER INDEX` prevents future insertions from going into the list of pending index entries, but does not in itself flush previous entries. You might want to VACUUM the table afterward to ensure the pending list is emptied.

tablespace\_name
:   The tablespace in which to create the index. If not specified, the default tablespace is used, or [temp\_tablespaces](../config_params/guc-list.html) for indexes on temporary tables.

predicate
:   The constraint expression for a partial index.

## <a id="section5"></a>Notes 

An *operator class* can be specified for each column of an index. The operator class identifies the operators to be used by the index for that column. For example, a B-tree index on four-byte integers would use the int4\_ops class; this operator class includes comparison functions for four-byte integers. In practice the default operator class for the column's data type is usually sufficient. The main point of having operator classes is that for some data types, there could be more than one meaningful ordering. For example, we might want to sort a complex-number data type either by absolute value or by real part. We could do this by defining two operator classes for the data type and then selecting the proper class when making an index.

For index methods that support ordered scans \(currently, only B-tree\), the optional clauses `ASC`, `DESC`, `NULLS FIRST`, and/or `NULLS LAST` can be specified to modify the sort ordering of the index. Since an ordered index can be scanned either forward or backward, it is not normally useful to create a single-column `DESC` index — that sort ordering is already available with a regular index. The value of these options is that multicolumn indexes can be created that match the sort ordering requested by a mixed-ordering query, such as `SELECT ... ORDER BY x ASC, y DESC`. The `NULLS` options are useful if you need to support "nulls sort low" behavior, rather than the default "nulls sort high", in queries that depend on indexes to avoid sorting steps.

For most index methods, the speed of creating an index is dependent on the setting of `maintenance_work_mem`. Larger values will reduce the time needed for index creation, so long as you don't make it larger than the amount of memory really available, which would drive the machine into swapping.

When an index is created on a partitioned table, the index is propagated to all the child tables created by Greenplum Database. Creating an index on a table that is created by Greenplum Database for use by a partitioned table is not supported.

`UNIQUE` indexes are allowed only if the index columns are the same as \(or a superset of\) the Greenplum distribution key columns.

`UNIQUE` indexes are not allowed on append-optimized tables.

A `UNIQUE` index can be created on a partitioned table. However, uniqueness is enforced only within a partition; uniqueness is not enforced between partitions. For example, for a partitioned table with partitions that are based on year and a subpartitions that are based on quarter, uniqueness is enforced only on each individual quarter partition. Uniqueness is not enforced between quarter partitions

Indexes are not used for `IS NULL` clauses by default. The best way to use indexes in such cases is to create a partial index using an `IS NULL` predicate.

`bitmap` indexes perform best for columns that have between 100 and 100,000 distinct values. For a column with more than 100,000 distinct values, the performance and space efficiency of a bitmap index decline. The size of a bitmap index is proportional to the number of rows in the table times the number of distinct values in the indexed column.

Columns with fewer than 100 distinct values usually do not benefit much from any type of index. For example, a gender column with only two distinct values for male and female would not be a good candidate for an index.

Prior releases of Greenplum Database also had an R-tree index method. This method has been removed because it had no significant advantages over the GiST method. If `USING rtree` is specified, `CREATE INDEX` will interpret it as `USING gist`.

For more information on the GiST index type, refer to the [PostgreSQL documentation](https://www.postgresql.org/docs/9.4/indexes-types.html).

The use of hash indexes has been deactivated in Greenplum Database.

## <a id="section6"></a>Examples 

To create a B-tree index on the column `title` in the table `films`:

```
CREATE UNIQUE INDEX title_idx ON films (title);
```

To create a bitmap index on the column `gender` in the table `employee`:

```
CREATE INDEX gender_bmp_idx ON employee USING bitmap 
(gender);
```

To create an index on the expression `lower(title)`, allowing efficient case-insensitive searches:

```
CREATE INDEX ON films ((lower(title)));
```

\(In this example we have chosen to omit the index name, so the system will choose a name, typically `films_lower_idx`.\)

To create an index with non-default collation:

```
CREATE INDEX title_idx_german ON films (title COLLATE "de_DE");
```

To create an index with non-default fill factor:

```
CREATE UNIQUE INDEX title_idx ON films (title) WITH 
(fillfactor = 70);
```

To create a GIN index with fast updates deactivated:

```
CREATE INDEX gin_idx ON documents_table USING gin (locations) WITH (fastupdate = off);
```

To create an index on the column `code` in the table `films` and have the index reside in the tablespace `indexspace`:

```
CREATE INDEX code_idx ON films(code) TABLESPACE indexspace;
```

To create a GiST index on a point attribute so that we can efficiently use box operators on the result of the conversion function:

```
CREATE INDEX pointloc ON points USING gist (box(location,location));
SELECT * FROM points WHERE box(location,location) && '(0,0),(1,1)'::box;
```

## <a id="section7"></a>Compatibility 

`CREATE INDEX` is a Greenplum Database language extension. There are no provisions for indexes in the SQL standard.

Greenplum Database does not support the concurrent creation of indexes \(`CONCURRENTLY` keyword not supported\).

## <a id="section8"></a>See Also 

[ALTER INDEX](ALTER_INDEX.html), [DROP INDEX](DROP_INDEX.html), [CREATE TABLE](CREATE_TABLE.html), [CREATE OPERATOR CLASS](CREATE_OPERATOR_CLASS.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

