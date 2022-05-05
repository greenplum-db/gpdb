---
title: END 
---

Commits the current transaction.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
END [WORK | TRANSACTION]
```

## <a id="section3"></a>Description 

`END` commits the current transaction. All changes made by the transaction become visible to others and are guaranteed to be durable if a crash occurs. This command is a Greenplum Database extension that is equivalent to [COMMIT](COMMIT.html).

## <a id="section4"></a>Parameters 

WORK
TRANSACTION
:   Optional keywords. They have no effect.

## <a id="section5"></a>Examples 

Commit the current transaction:

```
END;
```

## <a id="section6"></a>Compatibility 

`END` is a Greenplum Database extension that provides functionality equivalent to [COMMIT](COMMIT.html), which is specified in the SQL standard.

## <a id="section7"></a>See Also 

[BEGIN](BEGIN.html), [ROLLBACK](ROLLBACK.html), [COMMIT](COMMIT.html)

**Parent topic:**[SQL Commands](../sql_commands/sql_ref.html)

