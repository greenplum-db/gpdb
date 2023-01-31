# ABORT 

Terminates the current transaction.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
ABORT [WORK | TRANSACTION] [AND [NO] CHAIN]
```

## <a id="section3"></a>Description 

`ABORT` rolls back the current transaction and causes all the updates made by the transaction to be discarded. This command is identical in behavior to the standard SQL command [ROLLBACK](ROLLBACK.html), and is present only for historical reasons.

## <a id="section4"></a>Parameters 

WORK
TRANSACTION
:   Optional key words. They have no effect.

AND CHAIN
:   If `AND CHAIN` is specified, a new transaction is immediately started with the same transaction characteristics \(see [SET TRANSACTION](SET_TRANSACTION.html)\) as the just finished one. Otherwise, no new transaction is started.

## <a id="section5"></a>Notes 

Use [COMMIT](COMMIT.html) to successfully terminate a transaction.

Issuing `ABORT` outside of a transaction block emits a warning and otherwise has no effect.

## <a id="section5a"></a>Examples 

To terminate all changes:

```
ABORT;
```

## <a id="section6"></a>Compatibility 

This command is a Greenplum Database extension present for historical reasons. `ROLLBACK` is the equivalent standard SQL command.

## <a id="section7"></a>See Also 

[BEGIN](BEGIN.html), [COMMIT](COMMIT.html), [ROLLBACK](ROLLBACK.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

