# ABORT 

Aborts the current transaction.

## Synopsis 

``` {#sql_command_synopsis}
ABORT [WORK | TRANSACTION]
```

## Description 

`ABORT` rolls back the current transaction and causes all the updates made by the transaction to be discarded. This command is identical in behavior to the standard SQL command [ROLLBACK](ROLLBACK.html), and is present only for historical reasons.

## Parameters 

WORK
TRANSACTION
:   Optional key words. They have no effect.

## Notes 

Use `COMMIT` to successfully terminate a transaction.

Issuing `ABORT` when not inside a transaction does no harm, but it will provoke a warning message.

## Compatibility 

This command is a Greenplum Database extension present for historical reasons. `ROLLBACK` is the equivalent standard SQL command.

## See Also 

[BEGIN](BEGIN.html), [COMMIT](COMMIT.html), [ROLLBACK](ROLLBACK.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

