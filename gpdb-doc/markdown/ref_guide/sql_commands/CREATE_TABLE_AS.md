# CREATE TABLE AS 

Defines a new table from the results of a query.

## Synopsis 

``` {#sql_command_synopsis}
CREATE [ [GLOBAL | LOCAL] {TEMPORARY | TEMP} ] TABLE <table_name>
   [(<column_name> [, ...] )]
   [ WITH ( <storage_parameter>=<value> [, ... ] ) ]
   [ON COMMIT {PRESERVE ROWS | DELETE ROWS | DROP}]
   [TABLESPACE <tablespace>]
   AS <query>
   [DISTRIBUTED BY (<column>, [ ... ] ) | DISTRIBUTED RANDOMLY]
```

where storage\_parameter is:

```
   APPENDONLY={TRUE|FALSE}
   BLOCKSIZE={8192-2097152}
   ORIENTATION={COLUMN|ROW}
   COMPRESSTYPE={ZLIB|QUICKLZ}
   COMPRESSLEVEL={1-9 | 1}
   FILLFACTOR={10-100}
   OIDS[=TRUE|FALSE]
```

## Description 

`CREATE TABLE AS` creates a table and fills it with data computed by a [SELECT](SELECT.html) command. The table columns have the names and data types associated with the output columns of the `SELECT`, however you can override the column names by giving an explicit list of new column names.

`CREATE TABLE AS` creates a new table and evaluates the query just once to fill the new table initially. The new table will not track subsequent changes to the source tables of the query.

## Parameters 

GLOBAL \| LOCAL
:   These keywords are present for SQL standard compatibility, but have no effect in Greenplum Database.

TEMPORARY \| TEMP
:   If specified, the new table is created as a temporary table. Temporary tables are automatically dropped at the end of a session, or optionally at the end of the current transaction \(see `ON COMMIT`\). Existing permanent tables with the same name are not visible to the current session while the temporary table exists, unless they are referenced with schema-qualified names. Any indexes created on a temporary table are automatically temporary as well.

table\_name
:   The name \(optionally schema-qualified\) of the new table to be created.

column\_name
:   The name of a column in the new table. If column names are not provided, they are taken from the output column names of the query. If the table is created from an `EXECUTE` command, a column name list cannot be specified.

WITH \( storage\_parameter=value \)
:   The `WITH` clause can be used to set storage options for the table or its indexes. Note that you can also set different storage parameters on a particular partition or subpartition by declaring the `WITH` clause in the partition specification. The following storage options are available:

:   APPENDONLY — Set to `TRUE` to create the table as an append-optimized table. If `FALSE` or not declared, the table will be created as a regular heap-storage table.

:   BLOCKSIZE — Set to the size, in bytes for each block in a table. The `BLOCKSIZE`must be between 8192 and 2097152 bytes, and be a multiple of 8192. The default is 32768.

:   ORIENTATION — Set to `column` for column-oriented storage, or `row` \(the default\) for row-oriented storage. This option is only valid if `APPENDONLY=TRUE`. Heap-storage tables can only be row-oriented.

:   COMPRESSTYPE — Set to `ZLIB` \(the default\) or `QUICKLZ`<sup>1</sup> to specify the type of compression used. QuickLZ uses less CPU power and compresses data faster at a lower compression ratio than zlib. Conversely, zlib provides more compact compression ratios at lower speeds. This option is only valid if `APPENDONLY=TRUE`.

    **Note:**<sup>1</sup>QuickLZ compression is available only in the commercial release of Tanzu Greenplum.

:   COMPRESSLEVEL — For zlib compression of append-optimized tables, set to a value between 1 \(fastest compression\) to 9 \(highest compression ratio\). QuickLZ compression level can only be set to 1. If not declared, the default is 1. This option is only valid if `APPENDONLY=TRUE`.

:   FILLFACTOR — See [CREATE INDEX](CREATE_INDEX.html) for more information about this index storage parameter.

:   OIDS — Set to `OIDS=FALSE` \(the default\) so that rows do not have object identifiers assigned to them. Greenplum strongly recommends that you do not enable OIDS when creating a table. On large tables, such as those in a typical Greenplum Database system, using OIDs for table rows can cause wrap-around of the 32-bit OID counter. Once the counter wraps around, OIDs can no longer be assumed to be unique, which not only makes them useless to user applications, but can also cause problems in the Greenplum Database system catalog tables. In addition, excluding OIDs from a table reduces the space required to store the table on disk by 4 bytes per row, slightly improving performance. OIDS are not allowed on column-oriented tables.

ON COMMIT
:   The behavior of temporary tables at the end of a transaction block can be controlled using `ON COMMIT`. The three options are:

:   PRESERVE ROWS — No special action is taken at the ends of transactions for temporary tables. This is the default behavior.

:   DELETE ROWS — All rows in the temporary table will be deleted at the end of each transaction block. Essentially, an automatic `TRUNCATE` is done at each commit.

:   DROP — The temporary table will be dropped at the end of the current transaction block.

TABLESPACE tablespace
:   The tablespace is the name of the tablespace in which the new table is to be created. If not specified, the database's default tablespace is used.

AS query
:   A [SELECT](SELECT.html) or [VALUES](VALUES.html) command, or an [EXECUTE](EXECUTE.html) command that runs a prepared `SELECT` or `VALUES`query.

DISTRIBUTED BY \(column, \[ ... \] \)
DISTRIBUTED RANDOMLY
:   Used to declare the Greenplum Database distribution policy for the table. `DISTIBUTED BY` uses hash distribution with one or more columns declared as the distribution key. For the most even data distribution, the distribution key should be the primary key of the table or a unique column \(or set of columns\). If that is not possible, then you may choose `DISTRIBUTED RANDOMLY`, which will send the data round-robin to the segment instances.

:   The Greenplum Database server configuration parameter `gp_create_table_random_default_distribution` controls the default table distribution policy if the DISTRIBUTED BY clause is not specified when you create a table. Greenplum Database follows these rules to create a table if a distribution policy is not specified.

    -   If the legacy query optimizer creates the table, and the value of the parameter is `off`, the table distribution policy is determined based on the command.
    -   If the legacy query optimizer creates the table, and the value of the parameter is `on`, the table distribution policy is random.
    -   If GPORCA creates the table, the table distribution policy is random. The parameter value has no affect.

:   For information about the parameter, see "Server Configuration Parameters." For information about the legacy query optimizer and GPORCA, see "Querying Data" in the *Greenplum Database Administrator Guide*.

## Notes 

This command is functionally similar to [SELECT INTO](SELECT_INTO.html), but it is preferred since it is less likely to be confused with other uses of the `SELECT INTO` syntax. Furthermore, `CREATE TABLE AS` offers a superset of the functionality offered by `SELECT INTO`.

`CREATE TABLE AS` can be used for fast data loading from external table data sources. See [CREATE EXTERNAL TABLE](CREATE_EXTERNAL_TABLE.html).

## Examples 

Create a new table `films_recent` consisting of only recent entries from the table `films`:

```
CREATE TABLE films_recent AS SELECT * FROM films WHERE 
date_prod >= '2007-01-01';
```

Create a new temporary table `films_recent`, consisting of only recent entries from the table films, using a prepared statement. The new table has OIDs and will be dropped at commit:

```
PREPARE recentfilms(date) AS SELECT * FROM films WHERE 
date_prod > $1;
CREATE TEMP TABLE films_recent WITH (OIDS) ON COMMIT DROP AS 
EXECUTE recentfilms('2007-01-01');
```

## Compatibility 

`CREATE TABLE AS` conforms to the SQL standard, with the following exceptions:

-   The standard requires parentheses around the subquery clause; in Greenplum Database, these parentheses are optional.
-   The standard defines a `WITH [NO] DATA` clause; this is not currently implemented by Greenplum Database. The behavior provided by Greenplum Database is equivalent to the standard's `WITH DATA` case. `WITH NO DATA` can be simulated by appending `LIMIT 0` to the query.
-   Greenplum Database handles temporary tables differently from the standard; see `CREATE TABLE` for details.
-   The `WITH` clause is a Greenplum Database extension; neither storage parameters nor `OIDs` are in the standard.
-   The Greenplum Database concept of tablespaces is not part of the standard. The `TABLESPACE` clause is an extension.

## See Also 

[CREATE EXTERNAL TABLE](CREATE_EXTERNAL_TABLE.html), [CREATE EXTERNAL TABLE](CREATE_EXTERNAL_TABLE.html), [EXECUTE](EXECUTE.html), [SELECT](SELECT.html), [SELECT INTO](SELECT_INTO.html), [VALUES](VALUES.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

