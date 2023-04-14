# START TRANSACTION 

Starts a transaction block.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
START TRANSACTION [<transaction_mode>] [, ...]

where <transaction_mode> is one of:

   ISOLATION LEVEL {SERIALIZABLE | REPEATABLE READ | READ COMMITTED | READ UNCOMMITTED}
   READ WRITE | READ ONLY
   [NOT] DEFERRABLE
```

## <a id="section3"></a>Description 

`START TRANSACTION` begins a new transaction block. If the isolation level, read/write mode, or deferrable mode is specified, the new transaction has those characteristics, as if [SET TRANSACTION](SET_TRANSACTION.html) was run. This is the same as the [BEGIN](BEGIN.html) command.

## <a id="section4"></a>Parameters 

Refer to [SET TRANSACTION](SET_TRANSACTION.html) for information on the meaning of the parameters to this statement.


## <a id="section6"></a>Compatibility 

In the standard, it is not necessary to issue `START TRANSACTION` to start a transaction block: any SQL command implicitly begins a block. Greenplum Database's behavior can be seen as implicitly issuing a `COMMIT` after each command that does not follow `START TRANSACTION` \(or `BEGIN`\), and it is therefore often called 'autocommit'. Other relational database systems may offer an autocommit feature as a convenience.

The `DEFERRABLE` transaction\_mode is a Greenplum Database language extension.

The SQL standard requires commas between successive transaction\_modes, but for historical reasons Greenplum Database allows the commas to be omitted.

See also the compatibility section of [SET TRANSACTION](SET_TRANSACTION.html).

## <a id="section7"></a>See Also 

[BEGIN](BEGIN.html), [COMMIT](COMMIT.html), [ROLLBACK](ROLLBACK.html), [SAVEPOINT](SAVEPOINT.html), [SET TRANSACTION](SET_TRANSACTION.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

