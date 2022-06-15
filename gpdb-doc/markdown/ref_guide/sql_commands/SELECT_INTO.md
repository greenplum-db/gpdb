# SELECT INTO 

Defines a new table from the results of a query.

## Synopsis 

``` {#sql_command_synopsis}
[ WITH [ RECURSIVE1 ] <with_query> [, ...] ]
SELECT [ALL | DISTINCT [ON ( <expression> [, ...] )]]
    * | <expression> [AS <output_name>] [, ...]
    INTO [TEMPORARY | TEMP] [TABLE] <new_table>
    [FROM <from_item> [, ...]]
    [WHERE <condition>]
    [GROUP BY <expression> [, ...]]
    [HAVING <condition> [, ...]]
    [{UNION | INTERSECT | EXCEPT} [ALL] <select>]
    [ORDER BY <expression> [ASC | DESC | USING <operator>] [NULLS {FIRST | LAST}] [, ...]]
    [LIMIT {<count> | ALL}]
    [OFFSET <start>]
    [FOR {UPDATE | SHARE} [OF <table_name> [, ...]] [NOWAIT] 
    [...]]
```

## Description 

**Note:** 1The `RECURSIVE` keyword is a Beta feature.

`SELECT INTO` creates a new table and fills it with data computed by a query. The data is not returned to the client, as it is with a normal `SELECT`. The new table's columns have the names and data types associated with the output columns of the `SELECT`.

The `RECURSIVE` keyword can be enabled by setting the server configuration parameter `gp_recursive_cte_prototype` to `true`.

**Note:** The `RECURSIVE` keyword is a Beta feature.

## Parameters 

The majority of parameters for `SELECT INTO` are the same as [SELECT](SELECT.html).

TEMPORARY
TEMP
:   If specified, the table is created as a temporary table.

new\_table
:   The name \(optionally schema-qualified\) of the table to be created.

## Examples 

Create a new table `films_recent` consisting of only recent entries from the table `films`:

```
SELECT * INTO films_recent FROM films WHERE date_prod >= 
'2016-01-01';
```

## Compatibility 

The SQL standard uses `SELECT INTO` to represent selecting values into scalar variables of a host program, rather than creating a new table. The Greenplum Database usage of `SELECT INTO` to represent table creation is historical. It is best to use [CREATE TABLE AS](CREATE_TABLE_AS.html) for this purpose in new applications.

## See Also 

[SELECT](SELECT.html), [CREATE TABLE AS](CREATE_TABLE_AS.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

