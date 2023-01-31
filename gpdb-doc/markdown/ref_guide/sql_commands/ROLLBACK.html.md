# ROLLBACK 

Stops the current transaction.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
ROLLBACK [WORK | TRANSACTION] [AND [NO] CHAIN]
```

## <a id="section3"></a>Description 

`ROLLBACK` rolls back the current transaction and causes all the updates made by the transaction to be discarded.

## <a id="section4"></a>Parameters 

WORK
TRANSACTION
:   Optional key words. They have no effect.

AND CHAIN
:   If `AND CHAIN` is specified, a new transaction is immediately started with the same transaction characteristics \(see [SET TRANSACTION](SET_TRANSACTION.html)\) as the just finished one. Otherwise, no new transaction is started.

## <a id="section5"></a>Notes 

Use [COMMIT](COMMIT.html) to successfully end the current transaction.

Issuing `ROLLBACK` when not inside a transaction block emits a warning and otherwise has no effect. `ROLLBACK AND CHAIN` outside of a transaction block is an error.

## <a id="section6"></a>Examples 

To discard all changes made in the current transaction:

```
ROLLBACK;
```

## <a id="section7"></a>Compatibility 

The command `ROLLBACK` conforms to the SQL standard. The form `ROLLBACK TRANSACTION` is a Greenplum Database extension.

## <a id="section8"></a>See Also 

[BEGIN](BEGIN.html), [COMMIT](COMMIT.html), [ROLLBACK TO SAVEPOINT](ROLLBACK_TO_SAVEPOINT.html), [SAVEPOINT](SAVEPOINT.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

