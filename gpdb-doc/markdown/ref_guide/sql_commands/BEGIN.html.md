# BEGIN 

Starts a transaction block.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
BEGIN [WORK | TRANSACTION] [<transaction_mode>]

where <transaction_mode> is:

   ISOLATION LEVEL {READ UNCOMMITTED | READ COMMITTED | REPEATABLE READ | SERIALIZABLE}
   READ WRITE | READ ONLY
   [NOT] DEFERRABLE
```

## <a id="section3"></a>Description 

`BEGIN` initiates a transaction block, that is, all statements after a `BEGIN` command will be run in a single transaction until an explicit [COMMIT](COMMIT.html) or [ROLLBACK](ROLLBACK.html) is given. By default \(without `BEGIN`\), Greenplum Database runs transactions in "autocommit" mode, that is, each statement is run in its own transaction and a commit is implicitly performed at the end of the statement \(if execution was successful, otherwise a rollback is done\).

Statements are run more quickly in a transaction block, because transaction start/commit requires significant CPU and disk activity. Execution of multiple statements inside a transaction is also useful to ensure consistency when making several related changes: other sessions will be unable to see the intermediate states wherein not all the related updates have been done.

If the isolation level, read/write mode, or deferrable mode is specified, the new transaction has those characteristics, as if [SET TRANSACTION](SET_TRANSACTION.html) was run.

## <a id="section4"></a>Parameters 

WORK
TRANSACTION
:   Optional key words. They have no effect.

Refer to [SET TRANSACTION](SET_TRANSACTION.html) for information on the meaning of the other parameters to this statement.

## <a id="section5"></a>Notes 

[START TRANSACTION](START_TRANSACTION.html) has the same functionality as `BEGIN`.

Use [COMMIT](COMMIT.html) or [ROLLBACK](ROLLBACK.html) to terminate a transaction block.

Issuing `BEGIN` when already inside a transaction block will provoke a warning message. The state of the transaction is not affected. To nest transactions within a transaction block, use savepoints \(see [SAVEPOINT](SAVEPOINT.html)\).

For reasons of backwards compatibility, the commas between successive transaction\_modes can be omitted.

## <a id="section6"></a>Examples 

To begin a transaction block:

```
BEGIN;
```

## <a id="section7"></a>Compatibility 

`BEGIN` is a Greenplum Database language extension. It is equivalent to the SQL-standard command [START TRANSACTION](START_TRANSACTION.html), whose reference page contains additional compatibility information.

The `DEFERRABLE` transaction\_mode is a Greenplum Database language extension.

Incidentally, the `BEGIN` key word is used for a different purpose in embedded SQL. You are advised to be careful about the transaction semantics when porting database applications.

## <a id="section8"></a>See Also 

[COMMIT](COMMIT.html), [ROLLBACK](ROLLBACK.html), [SAVEPOINT](SAVEPOINT.html), [START TRANSACTION](START_TRANSACTION.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

