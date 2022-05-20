# SELECT INTO 

Defines a new table from the results of a query.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
[ WITH [ RECURSIVE ] <with_query> [, ...] ]
SELECT [ALL | DISTINCT [ON ( <expression> [, ...] )]]
    * | <expression> [AS <output_name>] [, ...]
    INTO [TEMPORARY | TEMP | UNLOGGED ] [TABLE] <new_table>
    [FROM <from_item> [, ...]]
    [WHERE <condition>]
    [GROUP BY <expression> [, ...]]
    [HAVING <condition> [, ...]]
    [{UNION | INTERSECT | EXCEPT} [ALL | DISTINCT ] <select>]
    [ORDER BY <expression> [ASC | DESC | USING <operator>] [NULLS {FIRST | LAST}] [, ...]]
    [LIMIT {<count> | ALL}]
    [OFFSET <start> [ ROW | ROWS ] ]
    [FETCH { FIRST | NEXT } [ <count> ] { ROW | ROWS } ONLY ]
    [FOR {UPDATE | SHARE} [OF <table_name> [, ...]] [NOWAIT] 
    [...]]
```

## <a id="section3"></a>Description 

`SELECT INTO` creates a new table and fills it with data computed by a query. The data is not returned to the client, as it is with a normal `SELECT`. The new table's columns have the names and data types associated with the output columns of the `SELECT`.

## <a id="section4"></a>Parameters 

The majority of parameters for `SELECT INTO` are the same as [SELECT](SELECT.html).

TEMPORARY
TEMP
:   If specified, the table is created as a temporary table.

UNLOGGED
:   If specified, the table is created as an unlogged table. Data written to unlogged tables is not written to the write-ahead \(WAL\) log, which makes them considerably faster than ordinary tables. However, the contents of an unlogged table are not replicated to mirror segment instances. Also an unlogged table is not crash-safe. After a segment instance crash or unclean shutdown, the data for the unlogged table on that segment is truncated. Any indexes created on an unlogged table are automatically unlogged as well.

new\_table
:   The name \(optionally schema-qualified\) of the table to be created.

## <a id="section5"></a>Examples 

Create a new table `films_recent` consisting of only recent entries from the table `films`:

```
SELECT * INTO films_recent FROM films WHERE date_prod >= 
'2016-01-01';
```

## <a id="section6"></a>Compatibility 

The SQL standard uses `SELECT INTO` to represent selecting values into scalar variables of a host program, rather than creating a new table. The Greenplum Database usage of `SELECT INTO` to represent table creation is historical. It is best to use [CREATE TABLE AS](CREATE_TABLE_AS.html) for this purpose in new applications.

## <a id="section7"></a>See Also 

[SELECT](SELECT.html), [CREATE TABLE AS](CREATE_TABLE_AS.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

