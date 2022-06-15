# TRUNCATE 

Empties a table of all rows.

## Synopsis 

``` {#sql_command_synopsis}
TRUNCATE [TABLE] <name> [, ...] [CASCADE | RESTRICT]
```

## Description 

`TRUNCATE` quickly removes all rows from a table or set of tables. It has the same effect as an unqualified [DELETE](DELETE.html) on each table, but since it does not actually scan the tables it is faster. This is most useful on large tables.

You must have the `TRUNCATE` privilege on the table to truncate table rows.

## Parameters 

name
:   The name \(optionally schema-qualified\) of a table to be truncated.

CASCADE
:   Since this key word applies to foreign key references \(which are not supported in Greenplum Database\) it has no effect.

RESTRICT
:   Since this key word applies to foreign key references \(which are not supported in Greenplum Database\) it has no effect.

## Notes 

`TRUNCATE` will not run any user-defined `ON DELETE` triggers that might exist for the tables.

`TRUNCATE` will not truncate any tables that inherit from the named table. Only the named table is truncated, not its child tables.

`TRUNCATE` will not truncate any sub-tables of a partitioned table. If you specify a sub-table of a partitioned table, `TRUNCATE` will not remove rows from the sub-table and its child tables.

## Examples 

Empty the table `films`:

```
TRUNCATE films;
```

## Compatibility 

There is no `TRUNCATE` command in the SQL standard.

## See Also 

[DELETE](DELETE.html), [DROP TABLE](DROP_TABLE.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

