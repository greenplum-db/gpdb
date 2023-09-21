# SELECT INTO 

Defines a new table from the results of a query.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
[ WITH [ RECURSIVE ] <with_query> [, ...] ]
SELECT [ALL | DISTINCT [ON ( <expression> [, ...] )]]
    * | <expression> [ [AS] <output_name> ] [, ...]
    INTO [TEMPORARY | TEMP | UNLOGGED ] [TABLE] <new_table>
    [FROM <from_item> [, ...]]
    [WHERE <condition>]
    [GROUP BY <expression> [, ...]]
    [HAVING <condition> [, ...]]
    [WINDOW <window_name> AS ( <window_definition> ) [, ...]]
    [{UNION | INTERSECT | EXCEPT} [ALL | DISTINCT ] <select>]
    [ORDER BY <expression> [ASC | DESC | USING <operator>] [NULLS {FIRST | LAST}] [, ...]]
    [LIMIT {<count> | ALL}]
    [OFFSET <start> [ ROW | ROWS ] ]
    [FETCH { FIRST | NEXT } [ <count> ] { ROW | ROWS } ONLY ]
    [FOR {UPDATE | SHARE} [OF <table_name> [, ...]] [NOWAIT] [...]]
```

## <a id="section3"></a>Description 

`SELECT INTO` creates a new table and fills it with data computed by a query. The data is not returned to the client, as it is with a normal `SELECT`. The new table's columns have the names and data types associated with the output columns of the `SELECT`.

## <a id="section4"></a>Parameters 

TEMPORARY
TEMP
:   If specified, the table is created as a temporary table. Refer to [CREATE TABLE](CREATE_TABLE.html) for details.

UNLOGGED
:   If specified, the table is created as an unlogged table. Refer to [CREATE TABLE](CREATE_TABLE.html) for details.

new\_table
:   The name \(optionally schema-qualified\) of the table to be created.

All other parameters for `SELECT INTO` are described in detail on the [SELECT](SELECT.html) reference page.


## <a id="section4n"></a>Notes

[CREATE TABLE AS](CREATE_TABLE_AS.html) is functionally similar to `SELECT INTO`. `CREATE TABLE AS` is the recommended syntax, since this form of `SELECT INTO` is not available in ECPG or PL/pgSQL, because they interpret the `INTO` clause differently. Also, `CREATE TABLE AS` offers a superset of the functionality provided by `SELECT INTO`.

In contrast to `CREATE TABLE AS`, `SELECT INTO` does not allow specifying properties like a table's access method with `USING <method>` or the table's tablespace with `TABLESPACE <tablespace_name>`. Use [CREATE TABLE AS](CREATE_TABLE_AS.html) if necessary. Therefore, the default table access method is chosen for the new table. See [default_table_access_method](../config_params/guc-list.html#default_table_access_method) for more information.

## <a id="section5"></a>Examples 

Create a new table `films_recent` consisting only of recent entries from the table `films`:

```
SELECT * INTO films_recent FROM films WHERE date_prod >= '2016-01-01';
```

## <a id="section6"></a>Compatibility 

The SQL standard uses `SELECT INTO` to represent selecting values into scalar variables of a host program, rather than creating a new table. The Greenplum Database usage of `SELECT INTO` to represent table creation is historical. It is best to use [CREATE TABLE AS](CREATE_TABLE_AS.html) for this purpose in new applications.

## <a id="section7"></a>See Also 

[SELECT](SELECT.html), [CREATE TABLE AS](CREATE_TABLE_AS.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

