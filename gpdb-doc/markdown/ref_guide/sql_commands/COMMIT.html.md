# COMMIT 

Commits the current transaction.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
COMMIT [WORK | TRANSACTION]
```

## <a id="section3"></a>Description 

`COMMIT` commits the current transaction. All changes made by the transaction become visible to others and are guaranteed to be durable if a crash occurs.

## <a id="section4"></a>Parameters 

WORK
TRANSACTION
:   Optional key words. They have no effect.

## <a id="section5"></a>Notes 

Use [ROLLBACK](ROLLBACK.html) to prematurely end a transaction.

Issuing `COMMIT` when not inside a transaction does no harm, but it will provoke a warning message.

## <a id="section6"></a>Examples 

To commit the current transaction and make all changes permanent:

```
COMMIT;
```

## <a id="section7"></a>Compatibility 

The SQL standard only specifies the two forms `COMMIT` and `COMMIT WORK`. Otherwise, this command is fully conforming.

## <a id="section8"></a>See Also 

[BEGIN](BEGIN.html), [END](END.html), [START TRANSACTION](START_TRANSACTION.html), [ROLLBACK](ROLLBACK.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

