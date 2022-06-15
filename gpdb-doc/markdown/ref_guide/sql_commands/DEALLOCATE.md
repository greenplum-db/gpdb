# DEALLOCATE 

Deallocates a prepared statement.

## Synopsis 

``` {#sql_command_synopsis}
DEALLOCATE [PREPARE] <name>
```

## Description 

`DEALLOCATE` is used to deallocate a previously prepared SQL statement. If you do not explicitly deallocate a prepared statement, it is deallocated when the session ends.

For more information on prepared statements, see [PREPARE](PREPARE.html).

## Parameters 

PREPARE
:   Optional key word which is ignored.

name
:   The name of the prepared statement to deallocate.

## Examples 

Deallocated the previously prepared statement named `insert_names`:

```
DEALLOCATE insert_names;
```

## Compatibility 

The SQL standard includes a `DEALLOCATE` statement, but it is only for use in embedded SQL.

## See Also 

[EXECUTE](EXECUTE.html), [PREPARE](PREPARE.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

