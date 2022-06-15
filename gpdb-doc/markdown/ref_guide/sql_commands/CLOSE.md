# CLOSE 

Closes a cursor.

## Synopsis 

``` {#sql_command_synopsis}
CLOSE <cursor_name>
```

## Description 

`CLOSE` frees the resources associated with an open cursor. After the cursor is closed, no subsequent operations are allowed on it. A cursor should be closed when it is no longer needed.

Every non-holdable open cursor is implicitly closed when a transaction is terminated by `COMMIT` or `ROLLBACK`. A holdable cursor is implicitly closed if the transaction that created it aborts via `ROLLBACK`. If the creating transaction successfully commits, the holdable cursor remains open until an explicit `CLOSE` is executed, or the client disconnects.

## Parameters 

cursor\_name
:   The name of an open cursor to close.

## Notes 

Greenplum Database does not have an explicit `OPEN` cursor statement. A cursor is considered open when it is declared. Use the `DECLARE` statement to declare \(and open\) a cursor.

You can see all available cursors by querying the `pg_cursors` system view.

## Examples 

Close the cursor `portala`:

```
CLOSE portala;
```

## Compatibility 

`CLOSE` is fully conforming with the SQL standard.

## See Also 

[DECLARE](DECLARE.html), [FETCH](FETCH.html), [MOVE](MOVE.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

