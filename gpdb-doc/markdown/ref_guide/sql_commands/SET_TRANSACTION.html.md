---
title: SET TRANSACTION 
---

Sets the characteristics of the current transaction.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
SET TRANSACTION [<transaction_mode>] [READ ONLY | READ WRITE]

SET TRANSACTION SNAPSHOT <snapshot_id>

SET SESSION CHARACTERISTICS AS TRANSACTION <transaction_mode> 
     [READ ONLY | READ WRITE]
     [NOT] DEFERRABLE
```

where transaction\_mode is one of:

```
   ISOLATION LEVEL {SERIALIZABLE | REPEATABLE READ | READ COMMITTED | READ UNCOMMITTED}
```

## <a id="section3"></a>Description 

The `SET TRANSACTION` command sets the characteristics of the current transaction. It has no effect on any subsequent transactions.

The available transaction characteristics are the transaction isolation level, the transaction access mode \(read/write or read-only\), and the deferrable mode.

**Note:** Deferrable transactions require the transaction to be serializable. Greenplum Database does not support serializable transactions, so including the `DEFERRABLE` clause has no effect.

Greenplum Database does not support the `SET TRANSACTION SNAPSHOT` command.

The isolation level of a transaction determines what data the transaction can see when other transactions are running concurrently.

-   **READ COMMITTED** — A statement can only see rows committed before it began. This is the default.
-   **REPEATABLE READ** — All statements in the current transaction can only see rows committed before the first query or data-modification statement run in the transaction.

The SQL standard defines two additional levels, `READ UNCOMMITTED` and `SERIALIZABLE`. In Greenplum Database `READ UNCOMMITTED` is treated as `READ COMMITTED`. If you specify `SERIALIZABLE`, Greenplum Database falls back to `REPEATABLE READ`.

The transaction isolation level cannot be changed after the first query or data-modification statement \(`SELECT`, `INSERT`, `DELETE`, `UPDATE`, `FETCH`, or `COPY`\) of a transaction has been run.

The transaction access mode determines whether the transaction is read/write or read-only. Read/write is the default. When a transaction is read-only, the following SQL commands are disallowed: `INSERT`, `UPDATE`, `DELETE`, and `COPY FROM` if the table they would write to is not a temporary table; all `CREATE`, `ALTER`, and `DROP` commands; `GRANT`, `REVOKE`, `TRUNCATE`; and `EXPLAIN ANALYZE` and `EXECUTE` if the command they would run is among those listed. This is a high-level notion of read-only that does not prevent all writes to disk.

The `DEFERRABLE` transaction property has no effect unless the transaction is also `SERIALIZABLE` and `READ ONLY`. When all of these properties are set on a transaction, the transaction may block when first acquiring its snapshot, after which it is able to run without the normal overhead of a `SERIALIZABLE` transaction and without any risk of contributing to or being cancelled by a serialization failure. Because Greenplum Database does not support serializable transactions, the `DEFERRABLE` transaction property has no effect in Greenplum Database.

## <a id="section4"></a>Parameters 

SESSION CHARACTERISTICS
:   Sets the default transaction characteristics for subsequent transactions of a session.

READ UNCOMMITTED
READ COMMITTED
REPEATABLE READ
SERIALIZABLE
:   The SQL standard defines four transaction isolation levels: `READ UNCOMMITTED`, `READ COMMITTED`, `REPEATABLE READ`, and `SERIALIZABLE`.

:   `READ UNCOMMITTED` allows transactions to see changes made by uncomitted concurrent transactions. This is not possible in Greenplum Database, so `READ UNCOMMITTED` is treated the same as `READ COMMITTED`.

:   `READ COMMITTED`, the default isolation level in Greenplum Database, guarantees that a statement can only see rows committed before it began. The same statement run twice in a transaction can produce different results if another concurrent transaction commits after the statement is run the first time.

:   The `REPEATABLE READ` isolation level guarantees that a transaction can only see rows committed before it began. `REPEATABLE READ` is the strictest transaction isolation level Greenplum Database supports. Applications that use the `REPEATABLE READ` isolation level must be prepared to retry transactions due to serialization failures.

:   The `SERIALIZABLE` transaction isolation level guarantees that all statements of the current transaction can only see rows committed before the first query or data-modification statement was run in this transaction. If a pattern of reads and writes among concurrent serializable transactions would create a situation which could not have occurred for any serial \(one-at-a-time\) execution of those transactions, one of the transactions will be rolled back with a `serialization_failure` error. Greenplum Database does not fully support `SERIALIZABLE` as defined by the standard, so if you specify `SERIALIZABLE`, Greenplum Database falls back to `REPEATABLE READ`. See [Compatibility](#section7) for more information about transaction serializability in Greenplum Database.

READ WRITE
READ ONLY
:   Determines whether the transaction is read/write or read-only. Read/write is the default. When a transaction is read-only, the following SQL commands are disallowed: `INSERT`, `UPDATE`, `DELETE`, and `COPY FROM` if the table they would write to is not a temporary table; all `CREATE`, `ALTER`, and `DROP` commands; `GRANT`, `REVOKE`, `TRUNCATE`; and `EXPLAIN ANALYZE` and `EXECUTE` if the command they would run is among those listed.

\[NOT\] DEFERRABLE
:   The `DEFERRABLE` transaction property has no effect in Greenplum Database because `SERIALIZABLE` transactions are not supported. If `DEFERRABLE` is specified and the transaction is also `SERIALIZABLE` and `READ ONLY`, the transaction may block when first acquiring its snapshot, after which it is able to run without the normal overhead of a `SERIALIZABLE` transaction and without any risk of contributing to or being cancelled by a serialization failure. This mode is well suited for long-running reports or backups.

## <a id="section5"></a>Notes 

If `SET TRANSACTION` is run without a prior `START TRANSACTION` or `BEGIN`, a warning is issued and the command has no effect.

It is possible to dispense with `SET TRANSACTION` by instead specifying the desired transaction modes in `BEGIN` or `START TRANSACTION`.

The session default transaction modes can also be set by setting the configuration parameters [default\_transaction\_isolation](../config_params/guc-list.html), [default\_transaction\_read\_only](../config_params/guc-list.html), and [default\_transaction\_deferrable](../config_params/guc-list.html).

## <a id="section6"></a>Examples 

Set the transaction isolation level for the current transaction:

```
BEGIN;
SET TRANSACTION ISOLATION LEVEL REPEATABLE READ;
```

## <a id="section7"></a>Compatibility 

Both commands are defined in the SQL standard. `SERIALIZABLE` is the default transaction isolation level in the standard. In Greenplum Database the default is `READ COMMITTED`. Due to lack of predicate locking, Greenplum Database does not fully support the `SERIALIZABLE` level, so it falls back to the `REPEATABLE READ` level when `SERIAL` is specified. Essentially, a predicate-locking system prevents phantom reads by restricting what is written, whereas a multi-version concurrency control model \(MVCC\) as used in Greenplum Database prevents them by restricting what is read.

PostgreSQL provides a true serializable isolation level, called serializable snapshot isolation \(SSI\), which monitors concurrent transactions and rolls back transactions that could introduce serialization anomalies. Greenplum Database does not implement this isolation mode.

In the SQL standard, there is one other transaction characteristic that can be set with these commands: the size of the diagnostics area. This concept is specific to embedded SQL, and therefore is not implemented in the Greenplum Database server.

The `DEFERRABLE` transaction mode is a Greenplum Database language extension.

The SQL standard requires commas between successive transaction\_modes, but for historical reasons Greenplum Database allows the commas to be omitted.

## <a id="section8"></a>See Also 

[BEGIN](BEGIN.html), [LOCK](LOCK.html)

**Parent topic:**[SQL Commands](../sql_commands/sql_ref.html)

