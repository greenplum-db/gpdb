# END 

Commits the current transaction.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
END [WORK | TRANSACTION] [AND [NO] CHAIN]
```

## <a id="section3"></a>Description 

`END` commits the current transaction. All changes made by the transaction become visible to others and are guaranteed to be durable if a crash occurs. This command is a Greenplum Database extension that is equivalent to [COMMIT](COMMIT.html).

## <a id="section4"></a>Parameters 

WORK
TRANSACTION
:   Optional keywords. They have no effect.

AND CHAIN
:   If `AND CHAIN` is specified, a new transaction is immediately started with the same transaction characteristics \(see [SET TRANSACTION](SET_TRANSACTION.html)\) as the just finished one. Otherwise, no new transaction is started.

## <a id="section4a"></a>Notes

Use [ROLLBACK](ROLLBACK.html) to terminate a transaction.

Issuing `END` when not inside a transaction does no harm, but it will provoke a warning message.

## <a id="section5"></a>Examples 

To commit the current transaction and make all changes permanent:

```
END;
```

## <a id="section6"></a>Compatibility 

`END` is a Greenplum Database extension that provides functionality equivalent to [COMMIT](COMMIT.html), which is specified in the SQL standard.

## <a id="section7"></a>See Also 

[BEGIN](BEGIN.html), [COMMIT](COMMIT.html), [ROLLBACK](ROLLBACK.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

