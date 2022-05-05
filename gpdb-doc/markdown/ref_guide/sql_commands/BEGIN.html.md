---
title: BEGIN 
---

Starts a transaction block.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
BEGIN [WORK | TRANSACTION] [<transaction_mode>]
```

where transaction\_mode is:

```
   ISOLATION LEVEL {READ UNCOMMITTED | READ COMMITTED | REPEATABLE READ | SERIALIZABLE}
   READ WRITE | READ ONLY
   [ NOT ] DEFERRABLE
```

## <a id="section3"></a>Description 

`BEGIN` initiates a transaction block, that is, all statements after a `BEGIN` command will be run in a single transaction until an explicit `COMMIT` or `ROLLBACK` is given. By default \(without `BEGIN`\), Greenplum Database runs transactions in autocommit mode, that is, each statement is run in its own transaction and a commit is implicitly performed at the end of the statement \(if execution was successful, otherwise a rollback is done\).

Statements are run more quickly in a transaction block, because transaction start/commit requires significant CPU and disk activity. Execution of multiple statements inside a transaction is also useful to ensure consistency when making several related changes: other sessions will be unable to see the intermediate states wherein not all the related updates have been done.

If the isolation level, read/write mode, or deferrable mode is specified, the new transaction has those characteristics, as if [SET TRANSACTION](SET_TRANSACTION.html) was run.

## <a id="section4"></a>Parameters 

WORK
TRANSACTION
:   Optional key words. They have no effect.

SERIALIZABLE
READ COMMITTED
READ UNCOMMITTED
:   The SQL standard defines four transaction isolation levels: `READ UNCOMMITTED`, `READ COMMITTED`, `REPEATABLE READ`, and `SERIALIZABLE`.

:   `READ UNCOMMITTED` allows transactions to see changes made by uncomitted concurrent transactions. This is not possible in Greenplum Database, so `READ UNCOMMITTED` is treated the same as `READ COMMITTED`.

:   `READ COMMITTED`, the default isolation level in Greenplum Database, guarantees that a statement can only see rows committed before it began. The same statement run twice in a transaction can produce different results if another concurrent transaction commits after the statement is run the first time.

:   The `REPEATABLE READ` isolation level guarantees that a transaction can only see rows committed before it began. `REPEATABLE READ` is the strictest transaction isolation level Greenplum Database supports. Applications that use the `REPEATABLE READ` isolation level must be prepared to retry transactions due to serialization failures.

:   The `SERIALIZABLE` transaction isolation level guarantees that running multiple concurrent transactions produces the same effects as running the same transactions one at a time. If you specify `SERIALIZABLE`, Greenplum Database falls back to `REPEATABLE READ`.

:   Specifying `DEFERRABLE` has no effect in Greenplum Database, but the syntax is supported for compatibility with PostgreSQL. A transaction can only be deferred if it is `READ ONLY` and `SERIALIZABLE`, and Greenplum Database does not support `SERIALIAZABLE` transactions.

## <a id="section5"></a>Notes 

[START TRANSACTION](START_TRANSACTION.html) has the same functionality as `BEGIN`.

Use [COMMIT](COMMIT.html) or [ROLLBACK](ROLLBACK.html) to terminate a transaction block.

Issuing `BEGIN` when already inside a transaction block will provoke a warning message. The state of the transaction is not affected. To nest transactions within a transaction block, use savepoints \(see `SAVEPOINT`\).

## <a id="section6"></a>Examples 

To begin a transaction block:

```
BEGIN;
```

To begin a transaction block with the repeatable read isolation level:

```
BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ;
```

## <a id="section7"></a>Compatibility 

`BEGIN` is a Greenplum Database language extension. It is equivalent to the SQL-standard command [START TRANSACTION](START_TRANSACTION.html).

`DEFERRABLE` transaction\_mode is a Greenplum Database language extension.

Incidentally, the `BEGIN` key word is used for a different purpose in embedded SQL. You are advised to be careful about the transaction semantics when porting database applications.

## <a id="section8"></a>See Also 

[COMMIT](COMMIT.html), [ROLLBACK](ROLLBACK.html), [START TRANSACTION](START_TRANSACTION.html), [SAVEPOINT](SAVEPOINT.html)

**Parent topic: **[SQL Commands](../sql_commands/sql_ref.html)

