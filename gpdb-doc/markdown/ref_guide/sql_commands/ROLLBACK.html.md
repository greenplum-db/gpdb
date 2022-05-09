# ROLLBACK 

Stops the current transaction.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
ROLLBACK [WORK | TRANSACTION]
```

## <a id="section3"></a>Description 

`ROLLBACK` rolls back the current transaction and causes all the updates made by the transaction to be discarded.

## <a id="section4"></a>Parameters 

WORK
TRANSACTION
:   Optional key words. They have no effect.

## <a id="section5"></a>Notes 

Use `COMMIT` to successfully end the current transaction.

Issuing `ROLLBACK` when not inside a transaction does no harm, but it will provoke a warning message.

## <a id="section6"></a>Examples 

To discard all changes made in the current transaction:

```
ROLLBACK;
```

## <a id="section7"></a>Compatibility 

The SQL standard only specifies the two forms `ROLLBACK` and `ROLLBACK WORK`. Otherwise, this command is fully conforming.

## <a id="section8"></a>See Also 

[BEGIN](BEGIN.html), [COMMIT](COMMIT.html), [SAVEPOINT](SAVEPOINT.html), [ROLLBACK TO SAVEPOINT](ROLLBACK_TO_SAVEPOINT.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

