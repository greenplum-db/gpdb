# COMMIT 

Commits the current transaction.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
COMMIT [WORK | TRANSACTION] [AND [NO] CHAIN]
```

## <a id="section3"></a>Description 

`COMMIT` commits the current transaction. All changes made by the transaction become visible to others and are guaranteed to be durable if a crash occurs.

## <a id="section4"></a>Parameters 

WORK
TRANSACTION
:   Optional key words. They have no effect.

AND CHAIN
:   If `AND CHAIN` is specified, a new transaction is immediately started with the same transaction characteristics \(see [SET TRANSACTION](SET_TRANSACTION.html)\) as the just finished one. Otherwise, no new transaction is started.

## <a id="section5"></a>Notes 

Use [ROLLBACK](ROLLBACK.html) to prematurely end a transaction.

Issuing `COMMIT` when not inside a transaction does no harm, but it will provoke a warning message. `COMMIT AND CHAIN` when not inside a transaction is an error.

## <a id="section6"></a>Examples 

To commit the current transaction and make all changes permanent:

```
COMMIT;
```

## <a id="section7"></a>Compatibility 

The command `COMMIT` conforms to the SQL standard. The form `COMMIT TRANSACTION` is a Greenplum Database extension.

## <a id="section8"></a>See Also 

[BEGIN](BEGIN.html), [END](END.html), [ROLLBACK](ROLLBACK.html), [START TRANSACTION](START_TRANSACTION.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

