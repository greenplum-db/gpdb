---
title: Inserting, Updating, and Deleting Data 
---

This section provides information about manipulating data and concurrent access in Greenplum Database.

This topic includes the following subtopics:

-   [About Concurrency Control in Greenplum Database](#topic2)
-   [Inserting Rows](#topic3)
-   [Updating Existing Rows](#topic4)
-   [Deleting Rows](#topic5)
-   [Working With Transactions](#topic7)
-   [Global Deadlock Detector](#topic_gdd)
-   [Vacuuming the Database](#topic9)
-   [Running Out of Locks](#topic11)

**Parent topic:** [Greenplum Database Administrator Guide](admin_guide.html)

## <a id="topic2"></a>About Concurrency Control in Greenplum Database 

Greenplum Database and PostgreSQL do not use locks for concurrency control. They maintain data consistency using a multiversion model, Multiversion Concurrency Control \(MVCC\). MVCC achieves transaction isolation for each database session, and each query transaction sees a snapshot of data. This ensures the transaction sees consistent data that is not affected by other concurrent transactions.

Because MVCC does not use explicit locks for concurrency control, lock contention is minimized and Greenplum Database maintains reasonable performance in multiuser environments. Locks acquired for querying \(reading\) data do not conflict with locks acquired for writing data.

Greenplum Database provides multiple lock modes to control concurrent access to data in tables. Most Greenplum Database SQL commands automatically acquire the appropriate locks to ensure that referenced tables are not dropped or modified in incompatible ways while a command runs. For applications that cannot adapt easily to MVCC behavior, you can use the `LOCK` command to acquire explicit locks. However, proper use of MVCC generally provides better performance.

|Lock Mode|Associated SQL Commands|Conflicts With|
|---------|-----------------------|--------------|
|ACCESS SHARE|`SELECT`|ACCESS EXCLUSIVE|
|ROW SHARE|`SELECT...FOR lock_strength`|EXCLUSIVE, ACCESS EXCLUSIVE|
|ROW EXCLUSIVE|`INSERT`, `COPY`|SHARE, SHARE ROW EXCLUSIVE, EXCLUSIVE, ACCESS EXCLUSIVE|
|SHARE UPDATE EXCLUSIVE|`VACUUM` \(without `FULL`\), `ANALYZE`|SHARE UPDATE EXCLUSIVE, SHARE, SHARE ROW EXCLUSIVE, EXCLUSIVE, ACCESS EXCLUSIVE|
|SHARE|`CREATE INDEX`|ROW EXCLUSIVE, SHARE UPDATE EXCLUSIVE, SHARE ROW EXCLUSIVE, EXCLUSIVE, ACCESS EXCLUSIVE|
|SHARE ROW EXCLUSIVE| |ROW EXCLUSIVE, SHARE UPDATE EXCLUSIVE, SHARE, SHARE ROW EXCLUSIVE, EXCLUSIVE, ACCESS EXCLUSIVE|
|EXCLUSIVE|`DELETE`, `UPDATE`, `SELECT...FOR lock_strength`, `REFRESH MATERIALIZED VIEW CONCURRENTLY`|ROW SHARE, ROW EXCLUSIVE, SHARE UPDATE EXCLUSIVE, SHARE, SHARE ROW EXCLUSIVE, EXCLUSIVE, ACCESS EXCLUSIVE|
|ACCESS EXCLUSIVE|`ALTER TABLE`, `DROP TABLE`, `TRUNCATE`, `REINDEX`, `CLUSTER`, `REFRESH MATERIALIZED VIEW` \(without `CONCURRENTLY`\), `VACUUM FULL`|ACCESS SHARE, ROW SHARE, ROW EXCLUSIVE, SHARE UPDATE EXCLUSIVE, SHARE, SHARE ROW EXCLUSIVE, EXCLUSIVE, ACCESS EXCLUSIVE|

> **Note** By default, the Global Deadlock Detector is deactivated, and Greenplum Database acquires the more restrictive `EXCLUSIVE` lock \(rather than `ROW EXCLUSIVE` in PostgreSQL\) for `UPDATE`, `DELETE`, and `SELECT` queries with a locking clause \(`FOR lock_strength`\).

When the Global Deadlock Detector is enabled:

-   The lock mode for some `DELETE` and `UPDATE` operations on heap tables is `ROW EXCLUSIVE`. See [Global Deadlock Detector](#topic_gdd).
-   The lock mode for some queries with a locking clause \(`SELECT...FOR lock_strength`\) is `ROW SHARE`. See "The Locking Clause" in [SELECT](../ref_guide/sql_commands/SELECT.html).

## <a id="topic3"></a>Inserting Rows 

Use the `INSERT` command to create rows in a table. This command requires the table name and a value for each column in the table; you may optionally specify the column names in any order. If you do not specify column names, list the data values in the order of the columns in the table, separated by commas.

For example, to specify the column names and the values to insert:

```
INSERT INTO products (name, price, product_no) VALUES ('Cheese', 9.99, 1);
```

To specify only the values to insert:

```
INSERT INTO products VALUES (1, 'Cheese', 9.99);

```

Usually, the data values are literals \(constants\), but you can also use scalar expressions. For example:

```
INSERT INTO films SELECT * FROM tmp_films WHERE date_prod < 
'2016-05-07';

```

You can insert multiple rows in a single command. For example:

```
INSERT INTO products (product_no, name, price) VALUES
    (1, 'Cheese', 9.99),
    (2, 'Bread', 1.99),
    (3, 'Milk', 2.99);

```

To insert data into a partitioned table, you specify the root partitioned table, the table created with the `CREATE TABLE` command. You also can specify a leaf child table of the partitioned table in an `INSERT` command. An error is returned if the data is not valid for the specified leaf child table. Specifying a child table that is not a leaf child table in the `INSERT` command is not supported.

To insert large amounts of data, use external tables or the `COPY` command. These load mechanisms are more efficient than `INSERT` for inserting large quantities of rows. See [Loading and Unloading Data](load/topics/g-loading-and-unloading-data.html) for more information about bulk data loading.

The storage model of append-optimized tables is optimized for bulk data loading. Greenplum does not recommend single row `INSERT` statements for append-optimized tables. For append-optimized tables, Greenplum Database supports a maximum of 127 concurrent `INSERT` transactions into a single append-optimized table.

## <a id="topic4"></a>Updating Existing Rows 

The `UPDATE` command updates rows in a table. You can update all rows, a subset of all rows, or individual rows in a table. You can update each column separately without affecting other columns.

To perform an update, you need:

-   The name of the table and columns to update
-   The new values of the columns
-   One or more conditions specifying the row or rows to be updated.

For example, the following command updates all products that have a price of *5* to have a price of *10*:

```
UPDATE products SET price = 10 WHERE price = 5;

```

Using `UPDATE` in Greenplum Database has the following restrictions:

-   While GPORCA supports updates to Greenplum distribution key columns, the Postgres Planner does not.
-   If mirrors are enabled, you cannot use `STABLE` or `VOLATILE` functions in an `UPDATE` statement.
-   Greenplum Database partitioning columns cannot be updated.

## <a id="topic5"></a>Deleting Rows 

The `DELETE` command deletes rows from a table. Specify a `WHERE` clause to delete rows that match certain criteria. If you do not specify a `WHERE` clause, all rows in the table are deleted. The result is a valid, but empty, table. For example, to remove all rows from the products table that have a price of *10*:

```
DELETE FROM products WHERE price = 10;

```

To delete all rows from a table:

```
DELETE FROM products; 

```

Using `DELETE` in Greenplum Database has similar restrictions to using `UPDATE`:

-   If mirrors are enabled, you cannot use `STABLE` or `VOLATILE` functions in an `UPDATE` statement.

### <a id="topic6"></a>Truncating a Table 

Use the `TRUNCATE` command to quickly remove all rows in a table. For example:

```
TRUNCATE mytable;

```

This command empties a table of all rows in one operation. Note that `TRUNCATE` does not scan the table, therefore it does not process inherited child tables or `ON DELETE` rewrite rules. The command truncates only rows in the named table.

## <a id="topic7"></a>Working With Transactions 

Transactions allow you to bundle multiple SQL statements in one all-or-nothing operation.

The following are the Greenplum Database SQL transaction commands:

-   `BEGIN` or `START TRANSACTION` starts a transaction block.
-   `END` or `COMMIT` commits the results of a transaction.
-   `ROLLBACK` abandons a transaction without making any changes.
-   `SAVEPOINT` marks a place in a transaction and enables partial rollback. You can roll back commands run after a savepoint while maintaining commands run before the savepoint.
-   `ROLLBACK TO SAVEPOINT` rolls back a transaction to a savepoint.
-   `RELEASE SAVEPOINT` destroys a savepoint within a transaction.

### <a id="topic8"></a>Transaction Isolation Levels 

Greenplum Database accepts the standard SQL transaction levels as follows:

-   `READ UNCOMMITTED` and `READ COMMITTED` behave like the standard `READ COMMITTED`.
-   `REPEATABLE READ` and `SERIALIZABLE` behave like `REPEATABLE READ`.

The following information describes the behavior of the Greenplum transaction levels.

#### <a id="readunc"></a>Read Uncommitted and Read Committed 

Greenplum Database does not allow any command to see an uncommitted update in another concurrent transaction, so `READ UNCOMMITTED` behaves the same as `READ COMMITTED`. `READ COMMITTED` provides fast, simple, partial transaction isolation. `SELECT`, `UPDATE`, and `DELETE` commands operate on a snapshot of the database taken when the query started.

A `SELECT` query:

-   Sees data committed before the query starts.
-   Sees updates run within the transaction.
-   Does not see uncommitted data outside the transaction.
-   Can possibly see changes that concurrent transactions made if the concurrent transaction is committed after the initial read in its own transaction.

Successive `SELECT` queries in the same transaction can see different data if other concurrent transactions commit changes between the successive queries. `UPDATE` and `DELETE` commands find only rows committed before the commands started.

`READ COMMITTED` transaction isolation allows concurrent transactions to modify or lock a row before `UPDATE` or `DELETE` find the row. `READ COMMITTED` transaction isolation may be inadequate for applications that perform complex queries and updates and require a consistent view of the database.

#### <a id="repread"></a>Repeatable Read and Serializable 

`SERIALIZABLE` transaction isolation, as defined by the SQL standard, ensures that transactions that run concurrently produce the same results as if they were run one after another. If you specify `SERIALIZABLE` Greenplum Database falls back to `REPEATABLE READ`. `REPEATABLE READ` transactions prevent dirty reads, non-repeatable reads, and phantom reads without expensive locking, but Greenplum Database does not detect all serializability interactions that can occur during concurrent transaction execution. Concurrent transactions should be examined to identify interactions that are not prevented by disallowing concurrent updates of the same data. You can prevent these interactions by using explicit table locks or by requiring the conflicting transactions to update a dummy row introduced to represent the conflict.

With `REPEATABLE READ` transactions, a `SELECT` query:

-   Sees a snapshot of the data as of the start of the transaction \(not as of the start of the current query within the transaction\).
-   Sees only data committed before the query starts.
-   Sees updates run within the transaction.
-   Does not see uncommitted data outside the transaction.
-   Does not see changes that concurrent transactions make.
-   Successive `SELECT` commands within a single transaction always see the same data.
-   `UPDATE`, `DELETE`, `SELECT FOR UPDATE`, and `SELECT FOR SHARE` commands find only rows committed before the command started. If a concurrent transaction has updated, deleted, or locked a target row, the `REPEATABLE READ` transaction waits for the concurrent transaction to commit or roll back the change. If the concurrent transaction commits the change, the `REPEATABLE READ` transaction rolls back. If the concurrent transaction rolls back its change, the `REPEATABLE READ` transaction can commit its changes.

The default transaction isolation level in Greenplum Database is `READ COMMITTED`. To change the isolation level for a transaction, declare the isolation level when you `BEGIN` the transaction or use the `SET TRANSACTION` command after the transaction starts.

## <a id="topic_gdd"></a>Global Deadlock Detector 

The Greenplum Database Global Deadlock Detector background worker process collects lock information on all segments and uses a directed algorithm to detect the existence of local and global deadlocks. This algorithm allows Greenplum Database to relax concurrent update and delete restrictions on heap tables. \(Greenplum Database still employs table-level locking on AO/CO tables, restricting concurrent `UPDATE`, `DELETE`, and `SELECT...FOR lock_strength` operations.\)

By default, the Global Deadlock Detector is deactivated and Greenplum Database runs the concurrent `UPDATE` and `DELETE` operations on a heap table serially. You can activate these concurrent updates and have the Global Deadlock Detector determine when a deadlock exists by setting the server configuration parameter [`gp_enable_global_deadlock_detector`](../ref_guide/config_params/guc-list.html).

When the Global Deadlock Detector is enabled, the background worker process is automatically started on the coordinator host when you start Greenplum Database. You configure the interval at which the Global Deadlock Detector collects and analyzes lock waiting data via the [gp\_global\_deadlock\_detector\_period](../ref_guide/config_params/guc-list.html) server configuration parameter.

If the Global Deadlock Detector determines that deadlock exists, it breaks the deadlock by cancelling one or more backend processes associated with the youngest transaction\(s\) involved.

When the Global Deadlock Detector determines a deadlock exists for the following types of transactions, only one of the transactions will succeed. The other transactions will fail with an error indicating that concurrent updates to the same row is not allowed.

-   Concurrent transactions on the same row of a heap table where the first transaction is an update operation and a later transaction runs an update or delete and the query plan contains a motion operator.
-   Concurrent update transactions on the same distribution key of a heap table that are run by the Postgres Planner.
-   Concurrent update transactions on the same row of a hash table that are run by the GPORCA optimizer.

> **Note** Greenplum Database uses the interval specified in the [deadlock\_timeout](../ref_guide/config_params/guc-list.html) server configuration parameter for local deadlock detection. Because the local and global deadlock detection algorithms differ, the cancelled process\(es\) may differ depending upon which detector \(local or global\) Greenplum Database triggers first.

> **Note** If the [lock\_timeout](../ref_guide/config_params/guc-list.html) server configuration parameter is turned on and set to a value smaller than `deadlock_timeout` and `gp_global_deadlock_detector_period`, Greenplum Database will cancel a statement before it would ever trigger a deadlock check in that session.

To view lock waiting information for all segments, run the `gp_dist_wait_status()` user-defined function. You can use the output of this function to determine which transactions are waiting on locks, which transactions are holding locks, the lock types and mode, the waiter and holder session identifiers, and which segments are running the transactions. Sample output of the `gp_dist_wait_status()` function follows:

```
SELECT * FROM pg_catalog.gp_dist_wait_status();
-[ RECORD 1 ]----+--------------
segid            | 0
waiter_dxid      | 11
holder_dxid      | 12
holdTillEndXact  | t
waiter_lpid      | 31249
holder_lpid      | 31458
waiter_lockmode  | ShareLock
waiter_locktype  | transactionid
waiter_sessionid | 8
holder_sessionid | 9
-[ RECORD 2 ]----+--------------
segid            | 1
waiter_dxid      | 12
holder_dxid      | 11
holdTillEndXact  | t
waiter_lpid      | 31467
holder_lpid      | 31250
waiter_lockmode  | ShareLock
waiter_locktype  | transactionid
waiter_sessionid | 9
holder_sessionid | 8

```

When it cancels a transaction to break a deadlock, the Global Deadlock Detector reports the following error message:

```
ERROR:  canceling statement due to user request: "cancelled by global deadlock detector"
```

### <a id="gdd_example"></a>Global Deadlock Detector UPDATE and DELETE Compatibility 

The Global Deadlock Detector can manage concurrent updates for these types of `UPDATE` and `DELETE` commands on heap tables:

-   Simple `UPDATE` of a single table. Update a non-distribution key with the Postgres Planner. The command does not contain a `FROM` clause, or a sub-query in the `WHERE` clause.

    ```
    UPDATE t SET c2 = c2 + 1 WHERE c1 > 10;
    ```

-   Simple `DELETE` of a single table. The command does not contain a sub-query in the `FROM` or `WHERE` clauses.

    ```
    DELETE FROM t WHERE c1 > 10;
    ```

-   Split `UPDATE`. For the Postgres Planner, the `UPDATE` command updates a distribution key.

    ```
    UPDATE t SET c = c + 1; -- c is a distribution key
    ```

    For GPORCA, the `UPDATE` command updates a distribution key or references a distribution key.

    ```
    UPDATE t SET b = b + 1 WHERE c = 10; -- c is a distribution key
    ```

-   Complex `UPDATE`. The `UPDATE` command includes multiple table joins.

    ```
    UPDATE t1 SET c = t1.c+1 FROM t2 WHERE t1.c = t2.c;
    ```

    Or the command contains a sub-query in the `WHERE` clause.

    ```
    UPDATE t SET c = c + 1 WHERE c > ALL(SELECT * FROM t1);
    ```

-   Complex `DELETE`. A complex `DELETE` command is similar to a complex `UPDATE`, and involves multiple table joins or a sub-query.

    ```
    DELETE FROM t USING t1 WHERE t.c > t1.c;
    ```


The following table shows the concurrent `UPDATE` or `DELETE` commands that are managed by the Global Deadlock Detector. For example, concurrent simple `UPDATE` commands on the same table row are managed by the Global Deadlock Detector. For a concurrent complex `UPDATE` and a simple `UPDATE`, only one `UPDATE` is performed, and an error is returned for the other `UPDATE`.

|Command|Simple `UPDATE`|Simple `DELETE`|Split `UPDATE`|Complex `UPDATE`|Complex `DELETE`|
|-------|---------------|---------------|--------------|----------------|----------------|
|Simple `UPDATE`|YES|YES|NO|NO|NO|
|Simple `DELETE`|YES|YES|NO|YES|YES|
|Split `UPDATE`|NO|NO|NO|NO|NO|
|Complex `UPDATE`|NO|YES|NO|NO|NO|
|Complex `DELETE`|NO|YES|NO|NO|YES|

## <a id="topic9"></a>Vacuuming the Database 

Deleted or updated data rows occupy physical space on disk even though new transactions cannot see them. Periodically running the `VACUUM` command removes these expired rows. For example:

```
VACUUM mytable;

```

The `VACUUM` command collects table-level statistics such as the number of rows and pages. Vacuum all tables after loading data, including append-optimized tables. For information about recommended routine vacuum operations, see [Routine Vacuum and Analyze](managing/maintain.html).

> **Important** The `VACUUM`, `VACUUM FULL`, and `VACUUM ANALYZE` commands should be used to maintain the data in a Greenplum database especially if updates and deletes are frequently performed on your database data. See the `VACUUM` command in the *Greenplum Database Reference Guide* for information about using the command.

## <a id="topic11"></a>Running Out of Locks 

Greenplum Database can potentially run out of locks when a database operation accesses multiple tables in a single transaction. Backup and restore are examples of such operations.

When Greenplum Database runs out of locks, the error message that you may observe references a shared memory error:

```
... "WARNING","53200","out of shared memory",,,,,,"LOCK TABLE ...
... "ERROR","53200","out of shared memory",,"You might need to increase max_locks_per_transaction.",,,,"LOCK TABLE ...
```

> **Note** "shared memory" in this context refers to the shared memory of the internal object: the lock slots. "Out of shared memory" does *not* refer to exhaustion of system- or Greenplum-level memory resources.

As the hint describes, consider increasing the [`max_locks_per_transaction`](../ref_guide/config_params/guc-list.html) server configuration parameter when you encounter this error.

