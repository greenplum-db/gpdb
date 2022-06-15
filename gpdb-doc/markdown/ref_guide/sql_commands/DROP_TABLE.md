# DROP TABLE 

Removes a table.

## Synopsis 

``` {#sql_command_synopsis}
DROP TABLE [IF EXISTS] <name> [, ...] [CASCADE | RESTRICT]
```

## Description 

`DROP TABLE` removes tables from the database. Only its owner may drop a table. To empty a table of rows without removing the table definition, use `DELETE` or `TRUNCATE`.

`DROP TABLE` always removes any indexes, rules, triggers, and constraints that exist for the target table. However, to drop a table that is referenced by a view, `CASCADE` must be specified. `CASCADE` will remove a dependent view entirely.

## Parameters 

IF EXISTS
:   Do not throw an error if the table does not exist. A notice is issued in this case.

name
:   The name \(optionally schema-qualified\) of the table to remove.

CASCADE
:   Automatically drop objects that depend on the table \(such as views\).

RESTRICT
:   Refuse to drop the table if any objects depend on it. This is the default.

## Examples 

Remove the table `mytable`:

```
DROP TABLE mytable;
```

## Compatibility 

`DROP TABLE` is fully conforming with the SQL standard, except that the standard only allows one table to be dropped per command. Also, the `IF EXISTS` option is a Greenplum Database extension.

## See Also 

[CREATE TABLE](CREATE_TABLE.html), [ALTER TABLE](ALTER_TABLE.html), [TRUNCATE](TRUNCATE.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

