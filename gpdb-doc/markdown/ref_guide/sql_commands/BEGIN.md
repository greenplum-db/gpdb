# BEGIN 

Starts a transaction block.

## Synopsis 

``` {#sql_command_synopsis}
BEGIN [WORK | TRANSACTION] [<transaction_mode>]
      [READ ONLY | READ WRITE]
```

where transaction\_mode is one of:

```
ISOLATION LEVEL | {SERIALIZABLE | READ COMMITTED | READ UNCOMMITTED}
```

## Description 

`BEGIN` initiates a transaction block, that is, all statements after a `BEGIN` command will be executed in a single transaction until an explicit `COMMIT` or `ROLLBACK` is given. By default \(without `BEGIN`\), Greenplum Database executes transactions in autocommit mode, that is, each statement is executed in its own transaction and a commit is implicitly performed at the end of the statement \(if execution was successful, otherwise a rollback is done\).

Statements are executed more quickly in a transaction block, because transaction start/commit requires significant CPU and disk activity. Execution of multiple statements inside a transaction is also useful to ensure consistency when making several related changes: other sessions will be unable to see the intermediate states wherein not all the related updates have been done.

If the isolation level or read/write mode is specified, the new transaction has those characteristics, as if [SET TRANSACTION](SET_TRANSACTION.html) was executed.

## Parameters 

WORK
TRANSACTION
:   Optional key words. They have no effect.

SERIALIZABLE
READ COMMITTED
READ UNCOMMITTED
:   The SQL standard defines four transaction isolation levels: `READ COMMITTED`, `READ UNCOMMITTED`, `SERIALIZABLE`, and `REPEATABLE READ`. The default behavior is that a statement can only see rows committed before it began \(`READ COMMITTED`\). In Greenplum Database `READ UNCOMMITTED` is treated the same as `READ COMMITTED`. `REPEATABLE READ` is not supported; use `SERIALIZABLE` if this behavior is required. `SERIALIZABLE` is the strictest transaction isolation. This level emulates serial transaction execution, as if transactions had been executed one after another, serially, rather than concurrently. Applications using this level must be prepared to retry transactions due to serialization failures.

READ WRITE
READ ONLY
:   Determines whether the transaction is read/write or read-only. Read/write is the default. When a transaction is read-only, the following SQL commands are disallowed: `INSERT`, `UPDATE`, `DELETE`, and `COPY FROM` if the table they would write to is not a temporary table; all `CREATE`, `ALTER`, and `DROP` commands; `GRANT`, `REVOKE`, `TRUNCATE`; and `EXPLAIN ANALYZE` and `EXECUTE` if the command they would execute is among those listed.

## Notes 

[START TRANSACTION](START_TRANSACTION.html) has the same functionality as `BEGIN`.

Use [COMMIT](COMMIT.html) or [ROLLBACK](ROLLBACK.html) to terminate a transaction block.

Issuing `BEGIN` when already inside a transaction block will provoke a warning message. The state of the transaction is not affected. To nest transactions within a transaction block, use savepoints \(see `SAVEPOINT`\).

## Examples 

To begin a transaction block:

```
BEGIN;
```

To begin a transaction block with the serializable isolation level:

```
BEGIN TRANSACTION ISOLATION LEVEL SERIALIZABLE;
```

## Compatibility 

`BEGIN` is a Greenplum Database language extension. It is equivalent to the SQL-standard command [START TRANSACTION](START_TRANSACTION.html).

Incidentally, the `BEGIN` key word is used for a different purpose in embedded SQL. You are advised to be careful about the transaction semantics when porting database applications.

## See Also 

[COMMIT](COMMIT.html), [ROLLBACK](ROLLBACK.html), [START TRANSACTION](START_TRANSACTION.html), [SAVEPOINT](SAVEPOINT.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

