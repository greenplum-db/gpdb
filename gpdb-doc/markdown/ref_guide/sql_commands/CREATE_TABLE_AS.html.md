# CREATE TABLE AS 

Defines a new table from the results of a query.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
CREATE [ [ GLOBAL | LOCAL ] { TEMPORARY | TEMP } | UNLOGGED ] TABLE <table_name>
        [ (<column_name> [, ...] ) ]
        [ WITH ( <storage_parameter> [= <value>] [, ... ] ) | WITHOUT OIDS ]
        [ ON COMMIT { PRESERVE ROWS | DELETE ROWS | DROP } ]
        [ TABLESPACE <tablespace_name> ]
        AS <query>
        [ WITH [ NO ] DATA ]
        [ DISTRIBUTED BY (column [, ... ] ) | DISTRIBUTED RANDOMLY | DISTRIBUTED REPLICATED ]
      
```

where storage\_parameter is:

```
   appendoptimized={TRUE|FALSE}
   blocksize={8192-2097152}
   orientation={COLUMN|ROW}
   compresstype={ZLIB|ZSTD|QUICKLZ|RLE_TYPE|NONE}
   compresslevel={1-19 | 1}
   fillfactor={10-100}
   [oids=FALSE]
```

## <a id="section3"></a>Description 

`CREATE TABLE AS` creates a table and fills it with data computed by a [SELECT](SELECT.html) command. The table columns have the names and data types associated with the output columns of the `SELECT`, however you can override the column names by giving an explicit list of new column names.

`CREATE TABLE AS` creates a new table and evaluates the query just once to fill the new table initially. The new table will not track subsequent changes to the source tables of the query.

## <a id="section4"></a>Parameters 

GLOBAL \| LOCAL
:   Ignored for compatibility. These keywords are deprecated; refer to [CREATE TABLE](CREATE_TABLE.html) for details.

TEMPORARY \| TEMP
:   If specified, the new table is created as a temporary table. Temporary tables are automatically dropped at the end of a session, or optionally at the end of the current transaction \(see `ON COMMIT`\). Existing permanent tables with the same name are not visible to the current session while the temporary table exists, unless they are referenced with schema-qualified names. Any indexes created on a temporary table are automatically temporary as well.

UNLOGGED
:   If specified, the table is created as an unlogged table. Data written to unlogged tables is not written to the write-ahead \(WAL\) log, which makes them considerably faster than ordinary tables. However, the contents of an unlogged table are not replicated to mirror segment instances. Also an unlogged table is not crash-safe. After a segment instance crash or unclean shutdown, the data for the unlogged table on that segment is truncated. Any indexes created on an unlogged table are automatically unlogged as well.

table\_name
:   The name \(optionally schema-qualified\) of the new table to be created.

column\_name
:   The name of a column in the new table. If column names are not provided, they are taken from the output column names of the query.

WITH \( storage\_parameter=value \)
:   The `WITH` clause can be used to set storage options for the table or its indexes. Note that you can also set different storage parameters on a particular partition or subpartition by declaring the `WITH` clause in the partition specification. The following storage options are available:

:   **appendoptimized** — Set to `TRUE` to create the table as an append-optimized table. If `FALSE` or not declared, the table will be created as a regular heap-storage table.

:   **blocksize** — Set to the size, in bytes for each block in a table. The `blocksize` must be between 8192 and 2097152 bytes, and be a multiple of 8192. The default is 32768. The `blocksize` option is valid only if `appendoptimized=TRUE`.

:   **orientation** — Set to `column` for column-oriented storage, or `row` \(the default\) for row-oriented storage. This option is only valid if `appendoptimized=TRUE`. Heap-storage tables can only be row-oriented.

:   **compresstype** — Set to `ZLIB` \(the default\), `ZSTD`, `RLE_TYPE`, or `QUICKLZ`<sup>1</sup> to specify the type of compression used. The value `NONE` deactivates compression. Zstd provides for both speed or a good compression ratio, tunable with the `compresslevel` option. QuickLZ and zlib are provided for backwards-compatibility. Zstd outperforms these compression types on usual workloads. The `compresstype` option is valid only if `appendoptimized=TRUE`.

    > **Note** <sup>1</sup>QuickLZ compression is available only in the commercial release of VMware Greenplum.

    The value `RLE_TYPE`, which is supported only if `orientation`=`column` is specified, enables the run-length encoding \(RLE\) compression algorithm. RLE compresses data better than the Zstd, zlib, or QuickLZ compression algorithms when the same data value occurs in many consecutive rows.

    For columns of type `BIGINT`, `INTEGER`, `DATE`, `TIME`, or `TIMESTAMP`, delta compression is also applied if the `compresstype` option is set to `RLE_TYPE` compression. The delta compression algorithm is based on the delta between column values in consecutive rows and is designed to improve compression when data is loaded in sorted order or the compression is applied to column data that is in sorted order.

    For information about using table compression, see [Choosing the Table Storage Model](../../admin_guide/ddl/ddl-storage.html#topic1) in the *Greenplum Database Administrator Guide*.

:   **compresslevel** — For Zstd compression of append-optimized tables, set to an integer value from 1 \(fastest compression\) to 19 \(highest compression ratio\). For zlib compression, the valid range is from 1 to 9. QuickLZ compression level can only be set to 1. If not declared, the default is 1. The `compresslevel` option is valid only if `appendoptimized=TRUE`.

:   **fillfactor** — See [CREATE INDEX](CREATE_INDEX.html) for more information about this index storage parameter.

:   **oids=FALSE** — This setting is the default, and it ensures that rows do not have object identifiers assigned to them. VMware does not support using `WITH OIDS` or `oids=TRUE` to assign an OID system column.On large tables, such as those in a typical Greenplum Database system, using OIDs for table rows can cause wrap-around of the 32-bit OID counter. Once the counter wraps around, OIDs can no longer be assumed to be unique, which not only makes them useless to user applications, but can also cause problems in the Greenplum Database system catalog tables. In addition, excluding OIDs from a table reduces the space required to store the table on disk by 4 bytes per row, slightly improving performance. You cannot create OIDS on a partitioned or column-oriented table \(an error is displayed\). This syntax is deprecated and will be removed in a future Greenplum release.

ON COMMIT
:   The behavior of temporary tables at the end of a transaction block can be controlled using `ON COMMIT`. The three options are:

:   PRESERVE ROWS — No special action is taken at the ends of transactions for temporary tables. This is the default behavior.

:   DELETE ROWS — All rows in the temporary table will be deleted at the end of each transaction block. Essentially, an automatic `TRUNCATE` is done at each commit.

:   DROP — The temporary table will be dropped at the end of the current transaction block.

TABLESPACE tablespace\_name
:   The tablespace\_name parameter is the name of the tablespace in which the new table is to be created. If not specified, the database's default tablespace is used, or [temp\_tablespaces](../config_params/guc-list.html) if the table is temporary.

AS query
:   A [SELECT](SELECT.html), [TABLE](SELECT.html#table-command), or [VALUES](VALUES.html) command, or an [EXECUTE](EXECUTE.html) command that runs a prepared `SELECT` or `VALUES` query.

DISTRIBUTED BY \(\{column \[opclass\]\}, \[ ... \] \)
DISTRIBUTED RANDOMLY
DISTRIBUTED REPLICATED
:   Used to declare the Greenplum Database distribution policy for the table. `DISTRIBUTED BY` uses hash distribution with one or more columns declared as the distribution key. For the most even data distribution, the distribution key should be the primary key of the table or a unique column \(or set of columns\). If that is not possible, then you may choose `DISTRIBUTED RANDOMLY`, which will send the data round-robin to the segment instances.

:   `DISTRIBUTED REPLICATED` replicates all rows in the table to all Greenplum Database segments. It cannot be used with partitioned tables or with tables that inhert from other tables.

:   The Greenplum Database server configuration parameter `gp_create_table_random_default_distribution` controls the default table distribution policy if the DISTRIBUTED BY clause is not specified when you create a table. Greenplum Database follows these rules to create a table if a distribution policy is not specified.

    -   If the Postgres Planner creates the table, and the value of the parameter is `off`, the table distribution policy is determined based on the command.
    -   If the Postgres Planner creates the table, and the value of the parameter is `on`, the table distribution policy is random.
    -   If GPORCA creates the table, the table distribution policy is random. The parameter value has no effect.

:   For more information about setting the default table distribution policy, see [`gp_create_table_random_default_distribution`](../config_params/guc-list.html). For information about the Postgres Planner and GPORCA, see [Querying Data](../../admin_guide/query/topics/query.html#topic1) in the *Greenplum Database Administrator Guide*.

## <a id="section5"></a>Notes 

This command is functionally similar to [SELECT INTO](SELECT_INTO.html), but it is preferred since it is less likely to be confused with other uses of the `SELECT INTO` syntax. Furthermore, `CREATE TABLE AS` offers a superset of the functionality offered by `SELECT INTO`.

`CREATE TABLE AS` can be used for fast data loading from external table data sources. See [CREATE EXTERNAL TABLE](CREATE_EXTERNAL_TABLE.html).

## <a id="section6"></a>Examples 

Create a new table `films_recent` consisting of only recent entries from the table `films`:

```
CREATE TABLE films_recent AS SELECT * FROM films WHERE 
date_prod >= '2007-01-01';
```

Create a new temporary table `films_recent`, consisting of only recent entries from the table films, using a prepared statement. The new table will be dropped at commit:

```
PREPARE recentfilms(date) AS SELECT * FROM films WHERE 
date_prod > $1;
CREATE TEMP TABLE films_recent ON COMMIT DROP AS 
EXECUTE recentfilms('2007-01-01');
```

## <a id="section7"></a>Compatibility 

`CREATE TABLE AS` conforms to the SQL standard, with the following exceptions:

-   The standard requires parentheses around the subquery clause; in Greenplum Database, these parentheses are optional.
-   The standard defines a `WITH [NO] DATA` clause; this is not currently implemented by Greenplum Database. The behavior provided by Greenplum Database is equivalent to the standard's `WITH DATA` case. `WITH NO DATA` can be simulated by appending `LIMIT 0` to the query.
-   Greenplum Database handles temporary tables differently from the standard; see `CREATE TABLE` for details.
-   The `WITH` clause is a Greenplum Database extension; neither storage parameters nor `OIDs` are in the standard. The syntax for creating OID system columns is deprecated and will be removed in a future Greenplum release.
-   The Greenplum Database concept of tablespaces is not part of the standard. The `TABLESPACE` clause is an extension.

## <a id="section8"></a>See Also 

[CREATE EXTERNAL TABLE](CREATE_EXTERNAL_TABLE.html), [CREATE EXTERNAL TABLE](CREATE_EXTERNAL_TABLE.html), [EXECUTE](EXECUTE.html), [SELECT](SELECT.html), [SELECT INTO](SELECT_INTO.html), [VALUES](VALUES.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

