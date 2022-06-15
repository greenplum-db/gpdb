# COMMIT 

Commits the current transaction.

## Synopsis 

``` {#sql_command_synopsis}
COMMIT [WORK | TRANSACTION]
```

## Description 

`COMMIT` commits the current transaction. All changes made by the transaction become visible to others and are guaranteed to be durable if a crash occurs.

## Parameters 

WORK
TRANSACTION
:   Optional key words. They have no effect.

## Notes 

Use [ROLLBACK](ROLLBACK.html) to abort a transaction.

Issuing `COMMIT` when not inside a transaction does no harm, but it will provoke a warning message.

## Examples 

To commit the current transaction and make all changes permanent:

```
COMMIT;
```

## Compatibility 

The SQL standard only specifies the two forms `COMMIT` and `COMMIT WORK`. Otherwise, this command is fully conforming.

## See Also 

[BEGIN](BEGIN.html), [END](END.html), [START TRANSACTION](START_TRANSACTION.html), [ROLLBACK](ROLLBACK.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

