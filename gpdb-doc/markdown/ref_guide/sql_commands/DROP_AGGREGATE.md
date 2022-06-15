# DROP AGGREGATE 

Removes an aggregate function.

## Synopsis 

``` {#sql_command_synopsis}
DROP AGGREGATE [IF EXISTS] <name> ( <type> [, ...] ) [CASCADE | RESTRICT]
```

## Description 

`DROP AGGREGATE` will delete an existing aggregate function. To execute this command the current user must be the owner of the aggregate function.

## Parameters 

IF EXISTS
:   Do not throw an error if the aggregate does not exist. A notice is issued in this case.

name
:   The name \(optionally schema-qualified\) of an existing aggregate function.

type
:   An input data type on which the aggregate function operates. To reference a zero-argument aggregate function, write `*` in place of the list of input data types.

CASCADE
:   Automatically drop objects that depend on the aggregate function.

RESTRICT
:   Refuse to drop the aggregate function if any objects depend on it. This is the default.

## Examples 

To remove the aggregate function `myavg` for type `integer`:

```
DROP AGGREGATE myavg(integer);
```

## Compatibility 

There is no `DROP AGGREGATE` statement in the SQL standard.

## See Also 

[ALTER AGGREGATE](ALTER_AGGREGATE.html), [CREATE AGGREGATE](CREATE_AGGREGATE.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

