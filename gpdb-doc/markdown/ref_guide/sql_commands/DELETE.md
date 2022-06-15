# DELETE 

Deletes rows from a table.

## Synopsis 

``` {#sql_command_synopsis}
DELETE FROM [ONLY] <table> [[AS] <alias>]
      [USING <usinglist>]
      [WHERE <condition> | WHERE CURRENT OF <cursor_name> ]
```

## Description 

`DELETE` deletes rows that satisfy the `WHERE` clause from the specified table. If the `WHERE` clause is absent, the effect is to delete all rows in the table. The result is a valid, but empty table.

By default, `DELETE` will delete rows in the specified table and all its child tables. If you wish to delete only from the specific table mentioned, you must use the `ONLY` clause.

There are two ways to delete rows in a table using information contained in other tables in the database: using sub-selects, or specifying additional tables in the `USING` clause. Which technique is more appropriate depends on the specific circumstances.

If the `WHERE CURRENT OF` clause is specified, the row that is deleted is the one most recently fetched from the specified cursor.

You must have the `DELETE` privilege on the table to delete from it.

**Outputs**

On successful completion, a `DELETE` command returns a command tag of the form

```
DELETE <count>
```

The count is the number of rows deleted. If count is 0, no rows matched the condition \(this is not considered an error\).

## Parameters 

ONLY
:   If specified, delete rows from the named table only. When not specified, any tables inheriting from the named table are also processed.

table
:   The name \(optionally schema-qualified\) of an existing table.

alias
:   A substitute name for the target table. When an alias is provided, it completely hides the actual name of the table. For example, given `DELETE FROM foo AS f`, the remainder of the `DELETE` statement must refer to this table as `f` not `foo`.

usinglist
:   A list of table expressions, allowing columns from other tables to appear in the `WHERE` condition. This is similar to the list of tables that can be specified in the `FROM` Clause of a [SELECT](SELECT.html) statement; for example, an alias for the table name can be specified. Do not repeat the target table in the `usinglist`, unless you wish to set up a self-join.

condition
:   An expression returning a value of type `boolean`, which determines the rows that are to be deleted.

cursor\_name
:   The name of the cursor to use in a `WHERE CURRENT OF` condition. The row to be deleted is the one most recently fetched from this cursor. The cursor must be a simple \(non-join, non-aggregate\) query on the `DELETE` target table.

:   `WHERE CURRENT OF` cannot be specified together with a Boolean condition.

:   The `DELETE...WHERE CURRENT OF` cursor statement can only be executed on the server, for example in an interactive psql session or a script. Language extensions such as PL/pgSQL do not have support for updatable cursors.

:   See [DECLARE](DECLARE.html) for more information about creating cursors.

## Notes 

Greenplum Database lets you reference columns of other tables in the `WHERE` condition by specifying the other tables in the `USING` clause. For example, to the name `Hannah` from the `rank` table, one might do:

```
DELETE FROM rank USING names WHERE names.id = rank.id AND 
name = 'Hannah';
```

What is essentially happening here is a join between `rank` and `names`, with all successfully joined rows being marked for deletion. This syntax is not standard. However, this join style is usually easier to write and faster to execute than a more standard sub-select style, such as:

```
DELETE FROM rank WHERE id IN (SELECT id FROM names WHERE name 
= 'Hannah');
```

Execution of `UPDATE` and `DELETE` commands directly on a specific partition \(child table\) of a partitioned table is not supported. Instead, these commands must be executed on the root partitioned table, the table created with the `CREATE TABLE` command.

For a partitioned table, all the child tables are locked during the `DELETE` operation.

## Examples 

Delete all films but musicals:

```
DELETE FROM films WHERE kind <> 'Musical';
```

Clear the table films:

```
DELETE FROM films;
```

Delete using a join:

```
DELETE FROM rank USING names WHERE names.id = rank.id AND 
name = 'Hannah';
```

## Compatibility 

This command conforms to the SQL standard, except that the `USING` clause is a Greenplum Database extension.

## See Also 

[DECLARE](DECLARE.html), [TRUNCATE](TRUNCATE.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

