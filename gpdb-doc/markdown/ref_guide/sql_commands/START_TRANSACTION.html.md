---
title: START TRANSACTION 
---

Starts a transaction block.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
START TRANSACTION [<transaction_mode>] [READ WRITE | READ ONLY]
```

where transaction\_mode is:

```
   ISOLATION LEVEL {SERIALIZABLE | READ COMMITTED | READ UNCOMMITTED}
```

## <a id="section3"></a>Description 

`START TRANSACTION` begins a new transaction block. If the isolation level or read/write mode is specified, the new transaction has those characteristics, as if [SET TRANSACTION](SET_TRANSACTION.html) was run. This is the same as the `BEGIN` command.

## <a id="section4"></a>Parameters 

READ UNCOMMITTED
READ COMMITTED
REPEATABLE READ
SERIALIZABLE
:   The SQL standard defines four transaction isolation levels: `READ UNCOMMITTED`, `READ COMMITTED`, `REPEATABLE READ`, and `SERIALIZABLE`.

:   `READ UNCOMMITTED` allows transactions to see changes made by uncomitted concurrent transactions. This is not possible in Greenplum Database, so `READ UNCOMMITTED` is treated the same as `READ COMMITTED`.

:   `READ COMMITTED`, the default isolation level in Greenplum Database, guarantees that a statement can only see rows committed before it began. The same statement run twice in a transaction can produce different results if another concurrent transaction commits after the statement is run the first time.

:   The `REPEATABLE READ` isolation level guarantees that a transaction can only see rows committed before it began. `REPEATABLE READ` is the strictest transaction isolation level Greenplum Database supports. Applications that use the `REPEATABLE READ` isolation level must be prepared to retry transactions due to serialization failures.

:   The `SERIALIZABLE` transaction isolation level guarantees that running multiple concurrent transactions produces the same effects as running the same transactions one at a time. If you specify `SERIALIZABLE`, Greenplum Database falls back to `REPEATABLE READ`.

READ WRITE
READ ONLY
:   Determines whether the transaction is read/write or read-only. Read/write is the default. When a transaction is read-only, the following SQL commands are disallowed: `INSERT`, `UPDATE`, `DELETE`, and `COPY FROM` if the table they would write to is not a temporary table; all `CREATE`, `ALTER`, and `DROP` commands; `GRANT`, `REVOKE`, `TRUNCATE`; and `EXPLAIN ANALYZE` and `EXECUTE` if the command they would run is among those listed.

## <a id="section5"></a>Examples 

To begin a transaction block:

```
START TRANSACTION;
```

## <a id="section6"></a>Compatibility 

In the standard, it is not necessary to issue `START TRANSACTION` to start a transaction block: any SQL command implicitly begins a block. Greenplum Database behavior can be seen as implicitly issuing a `COMMIT` after each command that does not follow `START TRANSACTION` \(or `BEGIN`\), and it is therefore often called 'autocommit'. Other relational database systems may offer an autocommit feature as a convenience.

The SQL standard requires commas between successive transaction\_modes, but for historical reasons Greenplum Database allows the commas to be omitted.

See also the compatibility section of [SET TRANSACTION](SET_TRANSACTION.html).

## <a id="section7"></a>See Also 

[BEGIN](BEGIN.html),

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

