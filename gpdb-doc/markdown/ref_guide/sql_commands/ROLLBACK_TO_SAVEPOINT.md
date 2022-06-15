# ROLLBACK TO SAVEPOINT 

Rolls back the current transaction to a savepoint.

## Synopsis 

``` {#sql_command_synopsis}
ROLLBACK [WORK | TRANSACTION] TO [SAVEPOINT] <savepoint_name>
```

## Description 

This command will roll back all commands that were executed after the savepoint was established. The savepoint remains valid and can be rolled back to again later, if needed.

`ROLLBACK TO SAVEPOINT` implicitly destroys all savepoints that were established after the named savepoint.

## Parameters 

WORK
TRANSACTION
:   Optional key words. They have no effect.

savepoint\_name
:   The name of a savepoint to roll back to.

## Notes 

Use `RELEASE SAVEPOINT` to destroy a savepoint without discarding the effects of commands executed after it was established.

Specifying a savepoint name that has not been established is an error.

Cursors have somewhat non-transactional behavior with respect to savepoints. Any cursor that is opened inside a savepoint will be closed when the savepoint is rolled back. If a previously opened cursor is affected by a `FETCH` command inside a savepoint that is later rolled back, the cursor position remains at the position that `FETCH` left it pointing to \(that is, `FETCH` is not rolled back\). Closing a cursor is not undone by rolling back, either. A cursor whose execution causes a transaction to abort is put in a can't-execute state, so while the transaction can be restored using `ROLLBACK TO SAVEPOINT`, the cursor can no longer be used.

## Examples 

To undo the effects of the commands executed after `my_savepoint` was established:

```
ROLLBACK TO SAVEPOINT my_savepoint;
```

Cursor positions are not affected by a savepoint rollback:

```
BEGIN;
DECLARE foo CURSOR FOR SELECT 1 UNION SELECT 2;
SAVEPOINT foo;
FETCH 1 FROM foo;
column 
----------
        1
ROLLBACK TO SAVEPOINT foo;
FETCH 1 FROM foo;
column 
----------
        2
COMMIT;
```

## Compatibility 

The SQL standard specifies that the key word `SAVEPOINT` is mandatory, but Greenplum Database \(and Oracle\) allow it to be omitted. SQL allows only `WORK`, not `TRANSACTION`, as a noise word after `ROLLBACK`. Also, SQL has an optional clause `AND [NO] CHAIN` which is not currently supported by Greenplum Database. Otherwise, this command conforms to the SQL standard.

## See Also 

[BEGIN](BEGIN.html), [COMMIT](COMMIT.html), [SAVEPOINT](SAVEPOINT.html), [RELEASE SAVEPOINT](RELEASE_SAVEPOINT.html), [ROLLBACK](ROLLBACK.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

