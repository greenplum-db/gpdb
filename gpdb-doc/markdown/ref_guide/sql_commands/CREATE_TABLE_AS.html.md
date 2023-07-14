# CREATE TABLE AS 

Defines a new table from the results of a query.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
CREATE [ [ GLOBAL | LOCAL ] { TEMPORARY | TEMP } | UNLOGGED ] TABLE [ IF NOT EXISTS ] <table_name>
        [ (<column_name> [, ...] ) ]
        [ USING <access_method> ]
        [ WITH ( <storage_parameter> [= <value>] [, ... ] ) | WITHOUT OIDS ]
        [ ON COMMIT { PRESERVE ROWS | DELETE ROWS | DROP } ]
        [ TABLESPACE <tablespace_name> ]
        AS <query>
        [ WITH [ NO ] DATA ]
        [ DISTRIBUTED BY ( <column> [<opclass>] [, ... ] ) 
           | DISTRIBUTED RANDOMLY
           | DISTRIBUTED REPLICATED ]
```

## <a id="section3"></a>Description 

`CREATE TABLE AS` creates a table and fills it with data computed by a [SELECT](SELECT.html) command. The table columns have the names and data types associated with the output columns of the `SELECT`, however you can override the column names by giving an explicit list of new column names.

`CREATE TABLE AS` creates a new table and evaluates the query just once to fill the new table initially. The new table will not track subsequent changes to the source tables of the query.

## <a id="section4"></a>Parameters 

GLOBAL \| LOCAL
:   Ignored for compatibility. These keywords are deprecated; refer to [CREATE TABLE](CREATE_TABLE.html) for details.

TEMPORARY \| TEMP
:   If specified, the new table is created as a temporary table. Refer to [CREATE TABLE](CREATE_TABLE.html) for details.

UNLOGGED
:   If specified, the table is created as an unlogged table. Refer to [CREATE TABLE](CREATE_TABLE.html) for details.

IF NOT EXISTS
:   Do not throw an error if a relation with the same name already exists; simply issue a notice and leave the table unmodified.

table\_name
:   The name (optionally schema-qualified) of the new table to be created.

column\_name
:   The name of a column in the new table. If column names are not provided, they are taken from the output column names of the query.

USING access\_method
:   The optional `USING` clause specifies the table access method to use to store the contents for the new table you are creating; the method must be an access method of type [TABLE](SELECT.html#table-command). Set to `heap` to access the table as a heap-storage table, `ao_row` to access the table as an append-optimized table with row-oriented storage (AO), or `ao_column` to access the table as an append-optimized table with column-oriented storage (AO/CO). The default access method is determined by the value of the [default\_table\_access\_method](../config_params/guc-list.html#default_table_access_method) server configuration parameter.

:   <p class="note">
<strong>Note:</strong>
Although you can specify the table's access method using <code>WITH (appendoptimized=true|false, orientation=row|column)</code> VMware recommends that you use <code>USING <access_method></code> instead.
</p>

WITH ( storage\_parameter=value )
:   The `WITH` clause specifies optional storage parameters for the new table. Refer to the [Storage Parameters](CREATE_TABLE.html#storage_parameters) section on the `CREATE TABLE` reference page for more information.

ON COMMIT
:   The behavior of temporary tables at the end of a transaction block can be controlled using `ON COMMIT`. The three options are:

:   PRESERVE ROWS — Greenplum Database takes no special action at the ends of transactions for temporary tables. This is the default behavior.

:   DELETE ROWS — Greenplum Database deletes all rows in the temporary table at the end of each transaction block. Essentially, an automatic [TRUNCATE](TRUNCATE.html) is done at each commit.

:   DROP — Greenplum Database drops the temporary table at the end of the current transaction block.

TABLESPACE tablespace\_name
:   The tablespace\_name parameter is the name of the tablespace in which the new table is to be created. If not specified, the database's [default_tablespace](../config_params/guc-list.html#default_tablespace) is used, or [temp\_tablespaces](../config_params/guc-list.html#temp_tablespaces) if the table is temporary.

AS query
:   A [SELECT](SELECT.html), [TABLE](SELECT.html#table-command), or [VALUES](VALUES.html) command, or an [EXECUTE](EXECUTE.html) command that runs a prepared `SELECT`, `TABLE`, or `VALUES` query.

DISTRIBUTED BY \( column \[opclass\] \[, ... \] \)
DISTRIBUTED RANDOMLY
DISTRIBUTED REPLICATED
:   Used to declare the Greenplum Database distribution policy for the table. Refer to [CREATE TABLE](CREATE_TABLE.html) for details.


## <a id="section5"></a>Notes 

This command is functionally similar to [SELECT INTO](SELECT_INTO.html), but it is preferred since it is less likely to be confused with other uses of the `SELECT INTO` syntax. Furthermore, `CREATE TABLE AS` offers a superset of the functionality offered by `SELECT INTO`.

`CREATE TABLE AS` can be used for fast data loading from external table data sources. See [CREATE EXTERNAL TABLE](CREATE_EXTERNAL_TABLE.html).

## <a id="section6"></a>Examples 

Create a new table `films_recent` consisting of only recent entries from the table `films`:

```
CREATE TABLE films_recent AS
  SELECT * FROM films WHERE date_prod >= '2020-01-01';
```

To copy a table completely, you can also use the short form by specifying the `TABLE` command:

```
CREATE TABLE films2 AS
  TABLE films;
```

Create a new temporary table `films_recent`, consisting only of recent entries from the table `films`, using a prepared statement. The new table will be dropped at commit:

```
PREPARE recentfilms(date) AS
  SELECT * FROM films WHERE date_prod > $1;
CREATE TEMP TABLE films_recent ON COMMIT DROP AS 
  EXECUTE recentfilms('2020-01-01');
```

## <a id="section7"></a>Compatibility 

`CREATE TABLE AS` conforms to the SQL standard, with the following exceptions:

-   The standard requires parentheses around the subquery clause; in Greenplum Database, these parentheses are optional.
-   In the standard, the `WITH [NO] DATA` clause is required, in Greenplum Database it is optional.
-   Greenplum Database handles temporary tables differently from the standard; see [CREATE TABLE](CREATE_TABLE.html) for details.
-   The `WITH` clause is a Greenplum Database extension; storage parameters are not part of the standard.
-   The Greenplum Database concept of tablespaces is not part of the standard. The `TABLESPACE` clause is an extension.

## <a id="section8"></a>See Also 

[CREATE EXTERNAL TABLE](CREATE_EXTERNAL_TABLE.html), [CREATE MATERIALIZED VIEW](CREATE_MATERIALIZED_VIEW.html), [CREATE TABLE](CREATE_TABLE.html), [EXECUTE](EXECUTE.html), [SELECT](SELECT.html), [SELECT INTO](SELECT_INTO.html), [VALUES](VALUES.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

