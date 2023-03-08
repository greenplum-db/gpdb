# CREATE INDEX 

Defines a new index.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
CREATE [UNIQUE] INDEX [[IF NOT EXISTS] <name>] ON [ONLY] <table_name> [USING <method>]
       ( {<column_name> | (<expression>)} [COLLATE <collation>] [<opclass>] [ ASC | DESC ] [ NULLS { FIRST | LAST } ] [, ...] )
       [ WITH ( <storage_parameter> [= <value>] [, ... ] ) ]
       [ TABLESPACE <tablespace_name> ]
       [ WHERE <predicate> ]
```

## <a id="section3"></a>Description 

`CREATE INDEX` constructs an index on the specified column\(s\) of the specified table or materialized view. Indexes are primarily used to enhance database performance \(though inappropriate use can result in slower performance\).

The key field\(s\) for the index are specified as column names, or alternatively as expressions written in parentheses. Multiple fields can be specified if the index method supports multicolumn indexes.

An index field can be an expression computed from the values of one or more columns of the table row. This feature can be used to obtain fast access to data based on some transformation of the basic data. For example, an index computed on `upper(col)` would allow the clause `WHERE upper(col) = 'JIM'` to use an index.

Greenplum Database provides the index methods B-tree, hash, bitmap, GiST, SP-GiST, GIN, and BRIN. Users can also define their own index methods, but that is fairly complicated.

When the `WHERE` clause is present, a partial index is created. A partial index is an index that contains entries for only a portion of a table, usually a portion that is more useful for indexing than the rest of the table. For example, if you have a table that contains both billed and unbilled orders where the unbilled orders take up a small fraction of the total table and yet is most often selected, you can improve performance by creating an index on just that portion. Another possible application is to use `WHERE` with `UNIQUE` to enforce uniqueness over a subset of a table. See [Partial Indexes](https://www.postgresql.org/docs/12/indexes-partial.html) in the PostgreSQL documentation for more information.

The expression used in the `WHERE` clause may refer only to columns of the underlying table, but it can use all columns, not just the ones being indexed. Subqueries and aggregate expressions are also forbidden in `WHERE`. The same restrictions apply to index fields that are expressions.

All functions and operators used in an index definition must be immutable. Their results must depend only on their arguments and never on any outside influence \(such as the contents of another table or the current time\). This restriction ensures that the behavior of the index is well-defined. To use a user-defined function in an index expression or `WHERE` clause, remember to mark the function `IMMUTABLE` when you create it.

## <a id="section4"></a>Parameters 

UNIQUE
:   Checks for duplicate values in the table when the index is created \(if data already exist\) and each time data is added. Duplicate entries will generate an error. Unique indexes only apply to B-tree indexes.
:   Additional restrictions apply when unique indexes are applied to partitioned tables; see [CREATE TABLE](CREATE_TABLE.html).

IF NOT EXISTS
:   Do not throw an error if a relation with the same name already exists. A notice is issued in this case. Note that there is no guarantee that the existing index is anything like the one that would have been created. Index name is required when `IF NOT EXISTS` is specified.

name
:   The name of the index to be created. The index is always created in the same schema as its parent table. If the name is omitted, Greenplum Database chooses a suitable name based on the parent table's name and the indexed column name\(s\).

ONLY
:   Indicates not to recurse creating indexes on partitions, if the table is partitioned. The default is to recurse.

table\_name
:   The name \(optionally schema-qualified\) of the table to be indexed.

method
:   The name of the index method to be used. Choices are `btree`, `bitmap`, `hash`, `gist`, `spgist`, `gin`, and `brin`. The default method is `btree`.

:   GPORCA supports only B-tree, bitmap, GiST, GIN, and BRIN indexes. GPORCA ignores indexes created with unsupported indexing methods.

column\_name
:   The name of a column of the table on which to create the index.

expression
:   An expression based on one or more columns of the table. The expression usually must be written with surrounding parentheses, as shown in the syntax. However, the parentheses may be omitted if the expression has the form of a function call.

collation
:   The name of the collation to use for the index. By default, the index uses the collation declared for the column to be indexed or the result collation of the expression to be indexed. Indexes with non-default collations can be useful for queries that involve expressions using non-default collations.

opclass
:   The name of an operator class.

ASC
:   Specifies ascending sort order \(which is the default\).

DESC
:   Specifies descending sort order.

NULLS FIRST
:   Specifies that nulls sort before non-nulls. This is the default when `DESC` is specified.

NULLS LAST
:   Specifies that nulls sort after non-nulls. This is the default when `DESC` is not specified.

storage\_parameter
:   The name of an index-method-specific storage parameter. Each index method has its own set of allowed storage parameters. See [Index Storage Parameters](#section4isp) for details.

tablespace\_name
	:   The tablespace in which to create the index. If not specified, [default\_tablespace](../config_params/guc-list.html) is consulted, or [temp\_tablespaces](../config_params/guc-list.html) for indexes on temporary tables.

predicate
:   The constraint expression for a partial index.

## <a id="section4isp"></a>Index Storage Parameters

The optional `WITH` clause specifies *storage parameters* for the index. Each index method has its own set of allowed storage parameters. The B-tree, bitmap, hash, GiST, and SP-GiST index methods all accept this parameter:

`fillfactor`
:    The `fillfactor` for an index is a percentage that determines how full the index method will try to pack index pages. For B-trees, leaf pages are filled to this percentage during initial index build, and also when extending the index at the right \(adding new largest key values\). If pages subsequently become completely full, they will be split, leading to gradual degradation in the index's efficiency. B-trees use a default fillfactor of 90, but any integer value from 10 to 100 can be selected. If the table is static then fillfactor 100 is best to minimize the index's physical size, but for heavily updated tables a smaller fillfactor is better to minimize the need for page splits. The other index methods use fillfactor in different but roughly analogous ways; the default fillfactor varies between methods.

B-tree indexes additionally accept this parameter:

vacuum_cleanup_index_scale_factor
:   Per-index value for `vacuum_cleanup_index_scale_factor`.

GiST indexes additionally accept this parameter:

`buffering`
:   Determines whether Greenplum Database builds the index using the buffering build technique described in [GiST buffering build](https://www.postgresql.org/docs/12/gist-implementation.html) in the PostgreSQL documentation. With `OFF` it is deactivated, with `ON` it is activated, and with `AUTO` it is initially deactivated, but turned on on-the-fly once the index size reaches [effective\_cache\_size](../config_params/guc-list.html). The default is `AUTO`.

GIN indexes accept different parameters:

`fastupdate`
:   This setting controls usage of the fast update technique described in [GIN Fast Update Technique](https://www.postgresql.org/docs/12/gin-implementation.html#GIN-FAST-UPDATE) in the PostgreSQL documentation. It is a Boolean parameter that deactivates or activates the GIN index fast update technique. A value of `ON` activates fast update \(the default\), and `OFF` deactivates it.

:   **Note:** Turning `fastupdate` off via `ALTER INDEX` prevents future insertions from going into the list of pending index entries, but does not in itself flush previous entries. You might want to `VACUUM` the table or call `gin_clean_pending_list()` function afterward to ensure the pending list is emptied.

gin_pending_list_limit
:   Custom `gin_pending_list_limit` parameter. This value is specified in kilobytes.

BRIN indexes accept different parameters:

pages_per_range
:   Defines the number of table blocks that make up one block range for each entry of a BRIN index (see the [BRIN Index Introduction](https://www.postgresql.org/docs/12/brin-intro.html) in the PostgreSQL documentation for details). The default is 128.

autosummarize
:   Defines whether a summarization run is queued for the previous page range whenever an insertion is detected on the next one. See [BRIN Index Maintenance] (https://www.postgresql.org/docs/12/brin-intro.html#BRIN-OPERATION) in the PostgreSQL documentation for more information. The default is `off`.

## <a id="section5"></a>Notes 
Refer to the [Indexes](https://www.postgresql.org/docs/12/indexes.html) topics in the PostgreSQL documentation for information about when indexes can be used, when they are not used, and in which particular situations they can be useful.

Currently, only the B-tree, bitmap, GiST, GIN, and BRIN index methods support multicolumn indexes. You can specify up to 32 fields by default. Only B-tree currently supports unique indexes.

An operator class can be specified for each column of an index. The operator class identifies the operators to be used by the index for that column. For example, a B-tree index on four-byte integers would use the `int4_ops` class; this operator class includes comparison functions for four-byte integers. In practice the default operator class for the column's data type is usually sufficient. The main point of having operator classes is that for some data types, there could be more than one meaningful ordering. For example, we might want to sort a complex-number data type either by absolute value or by real part. We could do this by defining two operator classes for the data type and then selecting the proper class when creating an index. Refer to [Operator Classes and Operator Families](https://www.postgresql.org/docs/12/indexes-opclass.html) and [Interfacing Extensions to Indexes
](https://www.postgresql.org/docs/12/xindex.html) in the PostgreSQL documentation for more information about operator classes.

When `CREATE INDEX` is invoked on a partitioned table, the default behavior is to recurse to all partitions to ensure they all have matching indexes. Each partition is first checked to determine whether an equivalent index already exists, and if so, that index will become attached as a partition index to the index being created, which will become its parent index. If no matching index exists, a new index will be created and automatically attached; the name of the new index in each partition will be determined as if no index name had been specified in the command. If the `ONLY` option is specified, no recursion is done, and the index is marked invalid. \(`ALTER INDEX ... ATTACH PARTITION` marks the index valid, once all partitions acquire matching indexes.\) Note, however, that any partition that is created in the future using `CREATE TABLE ... PARTITION OF` or `ALTER TABLE ... ADD PARTITION`  will automatically have a matching index, regardless of whether `ONLY` is specified

For index methods that support ordered scans \(currently, only B-tree\), you can specify the optional clauses `ASC`, `DESC`, `NULLS FIRST`, and/or `NULLS LAST` to modify the sort ordering of the index. Since an ordered index can be scanned either forward or backward, it is not normally useful to create a single-column `DESC` index — that sort ordering is already available with a regular index. The value of these options is that multicolumn indexes can be created that match the sort ordering requested by a mixed-ordering query, such as `SELECT ... ORDER BY x ASC, y DESC`. The `NULLS` options are useful if you need to support "nulls sort low" behavior, rather than the default "nulls sort high", in queries that depend on indexes to avoid sorting steps.

The system regularly collects statistics on all of a table's columns. Newly-created non-expression indexes can immediately use these statistics to determine an index's usefulness. For new expression indexes, you must run [ANALYZE](ANALYZE.html) to generate statistics for these indexes.

For most index methods, the speed of creating an index is dependent on the setting of [maintenance\_work\_mem](../config_params/guc-list.html#maintenance_work_mem). Larger values will reduce the time needed for index creation, so long as you don't make it larger than the amount of memory really available, which would drive the machine into swapping.

`bitmap` indexes perform best for columns that have between 100 and 100,000 distinct values. For a column with more than 100,000 distinct values, the performance and space efficiency of a bitmap index decline. The size of a bitmap index is proportional to the number of rows in the table times the number of distinct values in the indexed column.

Use [DROP INDEX](DROP_INDEX.html) to remove an index.

Like any long-running transaction, `CREATE INDEX` on a table can affect which tuples can be removed by concurrent `VACUUM` on any other table.

Prior releases of Greenplum Database also had an R-tree index method. This method has been removed because it had no significant advantages over the GiST method. If `USING rtree` is specified, `CREATE INDEX` will interpret it as `USING gist`, to simplify conversion of old databases to GiST.

## <a id="section6"></a>Examples 

To create a unique B-tree index on the column `title` in the table `films`:

```
CREATE UNIQUE INDEX title_idx ON films (title);
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

To create an index with non-default sort ordering of nulls:

```
CREATE INDEX title_idx_nulls_low ON films (title NULLS FIRST);
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
CREATE INDEX code_idx ON films (code) TABLESPACE indexspace;
```

To create a GiST index on a point attribute so that we can efficiently use box operators on the result of the conversion function:

```
CREATE INDEX pointloc ON points USING gist (box(location,location));
SELECT * FROM points WHERE box(location,location) && '(0,0),(1,1)'::box;
```

## <a id="section7"></a>Compatibility 

`CREATE INDEX` is a Greenplum Database extension to the SQL standard. There are no provisions for indexes in the SQL standard.

Greenplum Database does not support the concurrent creation of indexes \(`CONCURRENTLY` keyword is not supported\).

## <a id="section8"></a>See Also 

[ALTER INDEX](ALTER_INDEX.html), [DROP INDEX](DROP_INDEX.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

