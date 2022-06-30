# Inserting, Updating, and Deleting Data 

This section provides information about manipulating data and concurrent access in Greenplum Database.

This topic includes the following subtopics:

-   [About Concurrency Control in Greenplum Database](#topic2)
-   [Inserting Rows](#topic3)
-   [Updating Existing Rows](#topic4)
-   [Deleting Rows](#topic5)
-   [Working With Transactions](#topic7)
-   [Vacuuming the Database](#topic9)

**Parent topic:** [Greenplum Database Administrator Guide](admin_guide.html)

## About Concurrency Control in Greenplum Database 

Greenplum Database and PostgreSQL do not use locks for concurrency control. They maintain data consistency using a multiversion model, Multiversion Concurrency Control \(MVCC\). MVCC achieves transaction isolation for each database session, and each query transaction sees a snapshot of data. This ensures the transaction sees consistent data that is not affected by other concurrent transactions.

Because MVCC does not use explicit locks for concurrency control, lock contention is minimized and Greenplum Database maintains reasonable performance in multiuser environments. Locks acquired for querying \(reading\) data do not conflict with locks acquired for writing data.

Greenplum Database provides multiple lock modes to control concurrent access to data in tables. Most Greenplum Database SQL commands automatically acquire the appropriate locks to ensure that referenced tables are not dropped or modified in incompatible ways while a command executes. For applications that cannot adapt easily to MVCC behavior, you can use the `LOCK` command to acquire explicit locks. However, proper use of MVCC generally provides better performance.

|Lock Mode|Associated SQL Commands|Conflicts With|
|---------|-----------------------|--------------|
|ACCESS SHARE|`SELECT`|ACCESS EXCLUSIVE|
|ROW SHARE|`SELECT FOR SHARE, SELECT FOR UPDATE`|EXCLUSIVE, ACCESS EXCLUSIVE|
|ROW EXCLUSIVE|`INSERT`, `COPY`, `UPDATE`, `DELETE` on non-partitioned tables, see [Note](#lock_note).|SHARE, SHARE ROW EXCLUSIVE, EXCLUSIVE, ACCESS EXCLUSIVE|
|SHARE UPDATE EXCLUSIVE|`VACUUM` \(without `FULL`\), `ANALYZE`|SHARE UPDATE EXCLUSIVE, SHARE, SHARE ROW EXCLUSIVE, EXCLUSIVE, ACCESS EXCLUSIVE|
|SHARE|`CREATE INDEX`|ROW EXCLUSIVE, SHARE UPDATE EXCLUSIVE, SHARE ROW EXCLUSIVE, EXCLUSIVE, ACCESS EXCLUSIVE|
|SHARE ROW EXCLUSIVE| |ROW EXCLUSIVE, SHARE UPDATE EXCLUSIVE, SHARE, SHARE ROW EXCLUSIVE, EXCLUSIVE, ACCESS EXCLUSIVE|
|EXCLUSIVE|`SELECT FOR UPDATE`<br/><br/>`DELETE`, `UPDATE` on partitioned tables.<br/><br/>See [Note](#lock_note).|ROW SHARE, ROW EXCLUSIVE, SHARE UPDATE EXCLUSIVE, SHARE, SHARE ROW EXCLUSIVE, EXCLUSIVE, ACCESS EXCLUSIVE|
|ACCESS EXCLUSIVE|`ALTER TABLE`, `DROP TABLE`, `TRUNCATE`, `REINDEX`, `CLUSTER`, `VACUUM FULL`|ACCESS SHARE, ROW SHARE, ROW EXCLUSIVE, SHARE UPDATE EXCLUSIVE, SHARE, SHARE ROW EXCLUSIVE, EXCLUSIVE, ACCESS EXCLUSIVE|

**Note:** Greenplum Database acquires an `EXCLUSIVE` lock for `SELECT FOR UPDATE`. PostgreSQL acquires a less restrictive `ROW EXCLUSIVE` lock.

For an `UPDATE` or `DELETE` command on a partitioned table, Greenplum Database acquires an `EXCLUSIVE` lock on the root partition table. On a non-partitioned table, Greenplum Database acquires a `ROW EXCLUSIVE` lock.

## Inserting Rows 

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

## Updating Existing Rows 

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

-   While GPORCA supports updates to Greenplum distribution key columns, the Postgres planner does not.
-   If mirrors are enabled, you cannot use `STABLE` or `VOLATILE` functions in an `UPDATE` statement.
-   Greenplum Database does not support the `RETURNING` clause.
-   Greenplum Database partitioning columns cannot be updated.

## Deleting Rows 

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
-   The `RETURNING` clause is not supported in Greenplum Database.

### Truncating a Table 

Use the `TRUNCATE` command to quickly remove all rows in a table. For example:

```
TRUNCATE mytable;

```

This command empties a table of all rows in one operation. Note that `TRUNCATE` does not scan the table, therefore it does not process inherited child tables or `ON DELETE` rewrite rules. The command truncates only rows in the named table.

## Working With Transactions 

Transactions allow you to bundle multiple SQL statements in one all-or-nothing operation.

The following are the Greenplum Database SQL transaction commands:

-   `BEGIN` or `START TRANSACTION`starts a transaction block.
-   `END` or `COMMIT` commits the results of a transaction.
-   `ROLLBACK` abandons a transaction without making any changes.
-   `SAVEPOINT` marks a place in a transaction and enables partial rollback. You can roll back commands executed after a savepoint while maintaining commands executed before the savepoint.
-   `ROLLBACK TO SAVEPOINT`rolls back a transaction to a savepoint.
-   `RELEASE SAVEPOINT`destroys a savepoint within a transaction.

### Transaction Isolation Levels 

Greenplum Database accepts the standard SQL transaction levels as follows:

-   *read uncommitted* and *read committed* behave like the standard *read committed*
-   *repeatable read* is disallowed. If the behavior of *repeatable read* is required, use *serializable*.
-   *serializable* behaves in a manner similar to SQL standard *serializable*

The following information describes the behavior of the Greenplum transaction levels:

-   **read committed/read uncommitted** — Provides fast, simple, partial transaction isolation. With read committed and read uncommitted transaction isolation, `SELECT`, `UPDATE`, and `DELETE` transactions operate on a snapshot of the database taken when the query started.

A `SELECT` query:

-   Sees data committed before the query starts.
-   Sees updates executed within the transaction.
-   Does not see uncommitted data outside the transaction.
-   Can possibly see changes that concurrent transactions made if the concurrent transaction is committed after the initial read in its own transaction.

Successive `SELECT` queries in the same transaction can see different data if other concurrent transactions commit changes before the queries start. `UPDATE` and `DELETE` commands find only rows committed before the commands started.

Read committed or read uncommitted transaction isolation allows concurrent transactions to modify or lock a row before `UPDATE` or `DELETE` finds the row. Read committed or read uncommitted transaction isolation may be inadequate for applications that perform complex queries and updates and require a consistent view of the database.

-   **serializable** — Provides strict transaction isolation in which transactions execute as if they run one after another rather than concurrently. Applications on the serializable level must be designed to retry transactions in case of serialization failures. In Greenplum Database, `SERIALIZABLE` prevents dirty reads, non-repeatable reads, and phantom reads without expensive locking, but there are other interactions that can occur between some `SERIALIZABLE` transactions in Greenplum Database that prevent them from being truly serializable. Transactions that run concurrently should be examined to identify interactions that are not prevented by disallowing concurrent updates of the same data. Problems identified can be prevented by using explicit table locks or by requiring the conflicting transactions to update a dummy row introduced to represent the conflict.

A `SELECT` query:

-   Sees a snapshot of the data as of the start of the transaction \(not as of the start of the current query within the transaction\).
-   Sees only data committed before the query starts.
-   Sees updates executed within the transaction.
-   Does not see uncommitted data outside the transaction.
-   Does not see changes that concurrent transactions made.

    Successive `SELECT` commands within a single transaction always see the same data.

    `UPDATE`, `DELETE, SELECT FOR UPDATE,` and `SELECT FOR SHARE` commands find only rows committed before the command started. If a concurrent transaction has already updated, deleted, or locked a target row when the row is found, the serializable or repeatable read transaction waits for the concurrent transaction to update the row, delete the row, or roll back.

    If the concurrent transaction updates or deletes the row, the serializable or repeatable read transaction rolls back. If the concurrent transaction rolls back, then the serializable or repeatable read transaction updates or deletes the row.


The default transaction isolation level in Greenplum Database is *read committed*. To change the isolation level for a transaction, declare the isolation level when you `BEGIN` the transaction or use the `SET TRANSACTION` command after the transaction starts.

## Vacuuming the Database 

Deleted or updated data rows occupy physical space on disk even though new transactions cannot see them. Periodically running the `VACUUM` command removes these expired rows. For example:

```
VACUUM mytable;

```

The `VACUUM` command collects table-level statistics such as the number of rows and pages. Vacuum all tables after loading data, including append-optimized tables. For information about recommended routine vacuum operations, see [Routine Vacuum and Analyze](managing/maintain.html).

**Important:** The `VACUUM`, `VACUUM FULL`, and `VACUUM ANALYZE` commands should be used to maintain the data in a Greenplum database especially if updates and deletes are frequently performed on your database data. See the `VACUUM` command in the *Greenplum Database Reference Guide* for information about using the command.

### Configuring the Free Space Map 

Expired rows are held in the *free space map*. The free space map must be sized large enough to hold all expired rows in your database. If not, a regular `VACUUM` command cannot reclaim space occupied by expired rows that overflow the free space map.

`VACUUM FULL` reclaims all expired row space, but it is an expensive operation and can take an unacceptably long time to finish on large, distributed Greenplum Database tables. If the free space map overflows, you can recreate the table with a `CREATE TABLE AS`statement and drop the old table. Using `VACUUM FULL` is discouraged.

Size the free space map with the following server configuration parameters:

-   `max_fsm_pages`
-   `max_fsm_relations`

