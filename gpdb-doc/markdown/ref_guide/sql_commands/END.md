# END 

Commits the current transaction.

## Synopsis 

``` {#sql_command_synopsis}
END [WORK | TRANSACTION]
```

## Description 

`END` commits the current transaction. All changes made by the transaction become visible to others and are guaranteed to be durable if a crash occurs. This command is a Greenplum Database extension that is equivalent to [COMMIT](COMMIT.html).

## Parameters 

WORK
TRANSACTION
:   Optional keywords. They have no effect.

## Examples 

Commit the current transaction:

```
END;
```

## Compatibility 

`END` is a Greenplum Database extension that provides functionality equivalent to [COMMIT](COMMIT.html), which is specified in the SQL standard.

## See Also 

[BEGIN](BEGIN.html), [ROLLBACK](ROLLBACK.html), [COMMIT](COMMIT.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

