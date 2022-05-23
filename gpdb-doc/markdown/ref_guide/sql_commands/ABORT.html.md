---
title: ABORT 
---

Terminates the current transaction.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
ABORT [WORK | TRANSACTION]
```

## <a id="section3"></a>Description 

`ABORT` rolls back the current transaction and causes all the updates made by the transaction to be discarded. This command is identical in behavior to the standard SQL command [ROLLBACK](ROLLBACK.html), and is present only for historical reasons.

## <a id="section4"></a>Parameters 

WORK
TRANSACTION
:   Optional key words. They have no effect.

## <a id="section5"></a>Notes 

Use `COMMIT` to successfully terminate a transaction.

Issuing `ABORT` when not inside a transaction does no harm, but it will provoke a warning message.

## <a id="section6"></a>Compatibility 

This command is a Greenplum Database extension present for historical reasons. `ROLLBACK` is the equivalent standard SQL command.

## <a id="section7"></a>See Also 

[BEGIN](BEGIN.html), [COMMIT](COMMIT.html), [ROLLBACK](ROLLBACK.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

