---
title: Summary of Greenplum Features 
---

This section provides a high-level overview of the system requirements and feature set of Greenplum Database. It contains the following topics:

-   [Greenplum SQL Standard Conformance](#topic2)
-   [Greenplum and PostgreSQL Compatibility](#topic8)

## <a id="topic2"></a>Greenplum SQL Standard Conformance 

The SQL language was first formally standardized in 1986 by the American National Standards Institute \(ANSI\) as SQL 1986. Subsequent versions of the SQL standard have been released by ANSI and as International Organization for Standardization \(ISO\) standards: SQL 1989, SQL 1992, SQL 1999, SQL 2003, SQL 2006, and finally SQL 2008, which is the current SQL standard. The official name of the standard is ISO/IEC 9075-14:2008. In general, each new version adds more features, although occasionally features are deprecated or removed.

It is important to note that there are no commercial database systems that are fully compliant with the SQL standard. Greenplum Database is almost fully compliant with the SQL 1992 standard, with most of the features from SQL 1999. Several features from SQL 2003 have also been implemented \(most notably the SQL OLAP features\).

This section addresses the important conformance issues of Greenplum Database as they relate to the SQL standards. For a feature-by-feature list of Greenplum's support of the latest SQL standard, see [SQL 2008 Optional Feature Compliance](SQL2008_support.html).

### <a id="topic3"></a>Core SQL Conformance 

In the process of building a parallel, shared-nothing database system and query optimizer, certain common SQL constructs are not currently implemented in Greenplum Database. The following SQL constructs are not supported:

1.  Some set returning subqueries in `EXISTS` or `NOT EXISTS` clauses that Greenplum's parallel optimizer cannot rewrite into joins.
2.  Backwards scrolling cursors, including the use of `FETCH PRIOR`, `FETCH FIRST`, `FETCH ABSOLUTE`, and `FETCH RELATIVE`.
3.  In `CREATE TABLE` statements \(on hash-distributed tables\): a `UNIQUE` or `PRIMARY KEY` clause must include all of \(or a superset of\) the distribution key columns. Because of this restriction, only one `UNIQUE` clause or `PRIMARY KEY` clause is allowed in a `CREATE TABLE` statement. `UNIQUE` or `PRIMARY KEY` clauses are not allowed on randomly-distributed tables.
4.  `CREATE UNIQUE INDEX` statements that do not contain all of \(or a superset of\) the distribution key columns. `CREATE UNIQUE INDEX` is not allowed on randomly-distributed tables.

    Note that `UNIQUE INDEXES` \(but not `UNIQUE CONSTRAINTS`\) are enforced on a part basis within a partitioned table. They guarantee the uniqueness of the key within each part or sub-part.

5.  `VOLATILE` or `STABLE` functions cannot run on the segments, and so are generally limited to being passed literal values as the arguments to their parameters.
6.  Triggers are not supported since they typically rely on the use of `VOLATILE` functions.
7.  Referential integrity constraints \(foreign keys\) are not enforced in Greenplum Database. Users can declare foreign keys and this information is kept in the system catalog, however.
8.  Sequence manipulation functions `CURRVAL` and `LASTVAL`.

### <a id="topic4"></a>SQL 1992 Conformance 

The following features of SQL 1992 are not supported in Greenplum Database:

1.  `NATIONAL CHARACTER` \(`NCHAR`\) and `NATIONAL CHARACTER VARYING` \(`NVARCHAR`\). Users can declare the `NCHAR` and `NVARCHAR` types, however they are just synonyms for `CHAR` and `VARCHAR` in Greenplum Database.
2.  `CREATE ASSERTION` statement.
3.  `INTERVAL` literals are supported in Greenplum Database, but do not conform to the standard.
4.  `GET DIAGNOSTICS` statement.
5.  `GLOBAL TEMPORARY TABLE`s and `LOCAL TEMPORARY TABLE`s. Greenplum `TEMPORARY TABLE`s do not conform to the SQL standard, but many commercial database systems have implemented temporary tables in the same way. Greenplum temporary tables are the same as `VOLATILE TABLE`s in Teradata.
6.  `UNIQUE` predicate.
7.  `MATCH PARTIAL` for referential integrity checks \(most likely will not be implemented in Greenplum Database\).

### <a id="topic5"></a>SQL 1999 Conformance 

The following features of SQL 1999 are not supported in Greenplum Database:

1.  Large Object data types: `BLOB`, `CLOB`, `NCLOB`. However, the `BYTEA` and `TEXT` columns can store very large amounts of data in Greenplum Database \(hundreds of megabytes\).
2.  `MODULE` \(SQL client modules\).
3.  `CREATE PROCEDURE` \(`SQL/PSM`\). This can be worked around in Greenplum Database by creating a `FUNCTION` that returns `void`, and invoking the function as follows:

    ```
    SELECT <myfunc>(<args>);
    
    ```

4.  The PostgreSQL/Greenplum function definition language \(`PL/PGSQL`\) is a subset of Oracle's `PL/SQL`, rather than being compatible with the `SQL/PSM` function definition language. Greenplum Database also supports function definitions written in Python, Perl, Java, and R.
5.  `BIT` and `BIT VARYING` data types \(intentionally omitted\). These were deprecated in SQL 2003, and replaced in SQL 2008.
6.  Greenplum supports identifiers up to 63 characters long. The SQL standard requires support for identifiers up to 128 characters long.
7.  Prepared transactions \(`PREPARE TRANSACTION`, `COMMIT PREPARED`, `ROLLBACK PREPARED`\). This also means Greenplum does not support `XA` Transactions \(2 phase commit coordination of database transactions with external transactions\).
8.  `CHARACTER SET` option on the definition of `CHAR()` or `VARCHAR()` columns.
9.  Specification of `CHARACTERS` or `OCTETS` \(`BYTES`\) on the length of a `CHAR()` or `VARCHAR()` column. For example, `VARCHAR(15 CHARACTERS)` or `VARCHAR(15 OCTETS)` or `VARCHAR(15 BYTES)`.
10. `CREATE DISTINCT TYPE`statement. `CREATE DOMAIN` can be used as a work-around in Greenplum.
11. The *explicit table* construct.

### <a id="topic6"></a>SQL 2003 Conformance 

The following features of SQL 2003 are not supported in Greenplum Database:

1.  `MERGE` statements.
2.  `IDENTITY` columns and the associated `GENERATED ALWAYS/GENERATED BY DEFAULT` clause. The `SERIAL` or `BIGSERIAL` data types are very similar to `INT` or `BIGINT GENERATED BY DEFAULT AS IDENTITY`.
3.  `MULTISET` modifiers on data types.
4.  `ROW` data type.
5.  Greenplum Database syntax for using sequences is non-standard. For example, `nextval('``seq``')` is used in Greenplum instead of the standard `NEXT VALUE FOR``seq`.
6.  `GENERATED ALWAYS AS`columns. Views can be used as a work-around.
7.  The sample clause \(`TABLESAMPLE`\) on `SELECT` statements. The `random()` function can be used as a work-around to get random samples from tables.
8.  The *partitioned join tables* construct \(`PARTITION BY` in a join\).
9.  Greenplum array data types are almost SQL standard compliant with some exceptions. Generally customers should not encounter any problems using them.

### <a id="topic7"></a>SQL 2008 Conformance 

The following features of SQL 2008 are not supported in Greenplum Database:

1.  `BINARY` and `VARBINARY` data types. `BYTEA` can be used in place of `VARBINARY` in Greenplum Database.
2.  The `ORDER BY` clause is ignored in views and subqueries unless a `LIMIT` clause is also used. This is intentional, as the Greenplum optimizer cannot determine when it is safe to avoid the sort, causing an unexpected performance impact for such `ORDER BY` clauses. To work around, you can specify a really large `LIMIT`. For example:

    ```
    SELECT * FROM mytable ORDER BY 1 LIMIT 9999999999
    ```

3.  The *row subquery* construct is not supported.
4.  `TRUNCATE TABLE` does not accept the `CONTINUE IDENTITY` and `RESTART IDENTITY` clauses.

## <a id="topic8"></a>Greenplum and PostgreSQL Compatibility 

Greenplum Database is based on PostgreSQL 9.4. To support the distributed nature and typical workload of a Greenplum Database system, some SQL commands have been added or modified, and there are a few PostgreSQL features that are not supported. Greenplum has also added features not found in PostgreSQL, such as physical data distribution, parallel query optimization, external tables, resource queues, and enhanced table partitioning. For full SQL syntax and references, see the [SQL Commands](sql_commands/sql_ref.html).

**Note:** Greenplum Database does not support the PostgreSQL [large object facility](https://www.postgresql.org/docs/9.4/largeobjects.html) for streaming user data that is stored in large-object structures.

**Note:** VMware does not support using `WITH OIDS` or `oids=TRUE` to assign an OID system column when creating or altering a table. This syntax is deprecated and will be removed in a future Greenplum release.

|SQL Command|Supported in Greenplum|Modifications, Limitations, Exceptions|
|-----------|----------------------|--------------------------------------|
|`ALTER AGGREGATE`|YES| |
|`ALTER CONVERSION`|YES| |
|`ALTER DATABASE`|YES| |
|`ALTER DOMAIN`|YES| |
|`ALTER EXTENSION`|YES|Changes the definition of a Greenplum Database extension - based on PostgreSQL 9.6.|
|`ALTER FUNCTION`|YES| |
|`ALTER GROUP`|YES|An alias for [ALTER ROLE](sql_commands/ALTER_ROLE.html)|
|`ALTER INDEX`|YES| |
|`ALTER LANGUAGE`|YES| |
|`ALTER OPERATOR`|YES| |
|`ALTER OPERATOR CLASS`|YES| |
|`ALTER OPERATOR FAMILY`|YES| |
|`ALTER PROTOCOL`|YES| |
|`ALTER RESOURCE QUEUE`|YES|Greenplum Database resource management feature - not in PostgreSQL.|
|`ALTER ROLE`|YES|**Greenplum Database Clauses:**`RESOURCE QUEUE`*queue\_name*`| none`

|
|`ALTER SCHEMA`|YES| |
|`ALTER SEQUENCE`|YES| |
|`ALTER SYSTEM`|**NO**| |
|`ALTER TABLE`|YES|**Unsupported Clauses / Options:**`CLUSTER ON`

`ENABLE/DISABLE TRIGGER`

**Greenplum Database Clauses:**

`ADD | DROP | RENAME | SPLIT | EXCHANGE PARTITION | SET SUBPARTITION TEMPLATE | SET WITH``(REORGANIZE=true | false) | SET DISTRIBUTED BY`

|
|`ALTER TABLESPACE`|YES| |
|`ALTER TRIGGER`|**NO**| |
|`ALTER TYPE`|YES|**Greenplum Database Clauses:**`SET DEFAULT ENCODING`

|
|`ALTER USER`|YES|An alias for [ALTER ROLE](sql_commands/ALTER_ROLE.html)|
|`ALTER VIEW`|YES| |
|`ANALYZE`|YES| |
|`BEGIN`|YES| |
|`CHECKPOINT`|YES| |
|`CLOSE`|YES| |
|`CLUSTER`|YES| |
|`COMMENT`|YES| |
|`COMMIT`|YES| |
|`COMMIT PREPARED`|**NO**| |
|`COPY`|YES|**Modified Clauses:**`ESCAPE [ AS ] '`*escape*`' | 'OFF'`

**Greenplum Database Clauses:**

`[LOG ERRORS] SEGMENT REJECT LIMIT`*count*`[ROWS|PERCENT]`

|
|`CREATE AGGREGATE`|YES|**Unsupported Clauses / Options:**`[ , SORTOP =`*sort\_operator*`]`

**Greenplum Database Clauses:**

`[ , COMBINEFUNC =`*combinefunc*`]`

**Limitations:**

The functions used to implement the aggregate must be `IMMUTABLE` functions.

|
|`CREATE CAST`|YES| |
|`CREATE CONSTRAINT TRIGGER`|**NO**| |
|`CREATE CONVERSION`|YES| |
|`CREATE DATABASE`|YES| |
|`CREATE DOMAIN`|YES| |
|`CREATE EXTENSION`|YES|Loads a new extension into Greenplum Database - based on PostgreSQL 9.6.|
|`CREATE EXTERNAL TABLE`|YES|Greenplum Database parallel ETL feature - not in PostgreSQL 9.4.|
|`CREATE FUNCTION`|YES|**Limitations:**Functions defined as `STABLE` or `VOLATILE` can be run in Greenplum Database provided that they are run on the master only. `STABLE` and `VOLATILE` functions cannot be used in statements that run at the segment level.

|
|`CREATE GROUP`|YES|An alias for [CREATE ROLE](sql_commands/CREATE_ROLE.html)|
|`CREATE INDEX`|YES|**Greenplum Database Clauses:**`USING bitmap` \(bitmap indexes\)

**Limitations:**

`UNIQUE` indexes are allowed only if they contain all of \(or a superset of\) the Greenplum distribution key columns. On partitioned tables, a unique index is only supported within an individual partition - not across all partitions.

`CONCURRENTLY` keyword not supported in Greenplum.

|
|`CREATE LANGUAGE`|YES| |
|`CREATE MATERIALIZED VIEW`|YES|Based on PostgreSQL 9.4.|
|`CREATE OPERATOR`|YES|**Limitations:**The function used to implement the operator must be an `IMMUTABLE` function.

|
|`CREATE OPERATOR CLASS`|YES| |
|`CREATE OPERATOR FAMILY`|YES| |
|`CREATE PROTOCOL`|YES| |
|`CREATE RESOURCE QUEUE`|YES|Greenplum Database resource management feature - not in PostgreSQL 9.4.|
|`CREATE ROLE`|YES|**Greenplum Database Clauses:**`RESOURCE QUEUE`*queue\_name*`| none`

|
|`CREATE RULE`|YES| |
|`CREATE SCHEMA`|YES| |
|`CREATE SEQUENCE`|YES|**Limitations:**The `lastval()` and `currval()` functions are not supported.

The `setval()` function is only allowed in queries that do not operate on distributed data.

|
|`CREATE TABLE`|YES|**Unsupported Clauses / Options:**`[GLOBAL | LOCAL]`

`REFERENCES`

`FOREIGN KEY`

`[DEFERRABLE | NOT DEFERRABLE]`

**Limited Clauses:**

`UNIQUE` or `PRIMARY KEY`constraints are only allowed on hash-distributed tables \(`DISTRIBUTED BY`\), and the constraint columns must be the same as or a superset of the distribution key columns of the table and must include all the distribution key columns of the partitioning key.

**Greenplum Database Clauses:**

`DISTRIBUTED BY (*column*, [ ... ] ) |`

`DISTRIBUTED RANDOMLY`

`PARTITION BY *type* (*column* [, ...])    ( *partition\_specification*, [...] )`

`WITH (appendoptimized=true      [,compresslevel=value,blocksize=value] )`

|
|`CREATE TABLE AS`|YES|See [CREATE TABLE](sql_commands/CREATE_TABLE.html)|
|`CREATE TABLESPACE`|**YES**|**Greenplum Database Clauses:**Specify host file system locations for specific segment instances.

`WITH (contentID\_1='/path/to/dir1...)`

|
|`CREATE TRIGGER`|**NO**| |
|`CREATE TYPE`|YES|**Greenplum Database Clauses:**`COMPRESSTYPE | COMPRESSLEVEL | BLOCKSIZE`

**Limitations:**

The functions used to implement a new base type must be `IMMUTABLE` functions.

|
|`CREATE USER`|YES|An alias for [CREATE ROLE](sql_commands/CREATE_ROLE.html)|
|`CREATE VIEW`|YES| |
|`DEALLOCATE`|YES| |
|`DECLARE`|YES|**Unsupported Clauses / Options:**`SCROLL`

`FOR UPDATE [ OF column [, ...] ]`

**Limitations:**

Cursors cannot be backward-scrolled. Forward scrolling is supported.

PL/pgSQL does not have support for updatable cursors.

|
|`DELETE`|YES| |
|`DISCARD`|YES|**Limitation:** `DISCARD ALL` is not supported.

|
|`DO`|YES|PostgreSQL 9.0 feature|
|`DROP AGGREGATE`|YES| |
|`DROP CAST`|YES| |
|`DROP CONVERSION`|YES| |
|`DROP DATABASE`|YES| |
|`DROP DOMAIN`|YES| |
|`DROP EXTENSION`|YES|Removes an extension from Greenplum Database – based on PostgreSQL 9.6.|
|`DROP EXTERNAL TABLE`|YES|Greenplum Database parallel ETL feature - not in PostgreSQL 9.4.|
|`DROP FUNCTION`|YES| |
|`DROP GROUP`|YES|An alias for [DROP ROLE](sql_commands/DROP_ROLE.html)|
|`DROP INDEX`|YES| |
|`DROP LANGUAGE`|YES| |
|`DROP OPERATOR`|YES| |
|`DROP OPERATOR CLASS`|YES| |
|`DROP OPERATOR FAMILY`|YES| |
|`DROP OWNED`|**NO**| |
|`DROP PROTOCOL`|YES| |
|`DROP RESOURCE QUEUE`|YES|Greenplum Database resource management feature - not in PostgreSQL 9.4.|
|`DROP ROLE`|YES| |
|`DROP RULE`|YES| |
|`DROP SCHEMA`|YES| |
|`DROP SEQUENCE`|YES| |
|`DROP TABLE`|YES| |
|`DROP TABLESPACE`|YES| |
|`DROP TRIGGER`|**NO**| |
|`DROP TYPE`|YES| |
|`DROP USER`|YES|An alias for [DROP ROLE](sql_commands/DROP_ROLE.html)|
|`DROP VIEW`|YES| |
|`END`|YES| |
|`EXECUTE`|YES| |
|`EXPLAIN`|YES| |
|`FETCH`|YES|**Unsupported Clauses / Options:**`LAST`

`PRIOR`

`BACKWARD`

`BACKWARD ALL`

**Limitations:**

Cannot fetch rows in a nonsequential fashion; backward scan is not supported.

|
|`GRANT`|YES| |
|`INSERT`|YES| |
|`LATERAL` Join Type|**NO**| |
|`LISTEN`|**NO**| |
|`LOAD`|YES| |
|`LOCK`|YES| |
|`MOVE`|YES|See [FETCH](sql_commands/FETCH.html)|
|`NOTIFY`|**NO**| |
|`PREPARE`|YES| |
|`PREPARE TRANSACTION`|**NO**| |
|`REASSIGN OWNED`|YES| |
|`REFRESH MATERIALIZED VIEW`|YES|Based on PostgreSQL 9.4.|
|`REINDEX`|YES| |
|`RELEASE SAVEPOINT`|YES| |
|`RESET`|YES| |
|`RETRIEVE`|YES|Greenplum Database parallel retrieve cursor - not in PostgreSQL 9.4.|
|`REVOKE`|YES| |
|`ROLLBACK`|YES| |
|`ROLLBACK PREPARED`|**NO**| |
|`ROLLBACK TO SAVEPOINT`|YES| |
|`SAVEPOINT`|YES| |
|`SELECT`|YES|**Limitations:**Limited use of `VOLATILE` and `STABLE` functions in `FROM` or `WHERE` clauses

Text search \(`Tsearch2`\) is not supported

**Greenplum Database Clauses \(OLAP\):**

`[GROUP BY`*grouping\_element*`[, ...]]`

`[WINDOW`*window\_name*`AS (`*window\_specification*`)]`

`[FILTER (WHERE`*condition*`)]` applied to an aggregate function in the `SELECT` list

|
|`SELECT INTO`|YES|See [SELECT](sql_commands/SELECT.html)|
|`SET`|YES| |
|`SET CONSTRAINTS`|**NO**|In PostgreSQL, this only applies to foreign key constraints, which are currently not enforced in Greenplum Database.|
|`SET ROLE`|YES| |
|`SET SESSION AUTHORIZATION`|YES|Deprecated as of PostgreSQL 8.1 - see [SET ROLE](sql_commands/SET_ROLE.html)|
|`SET TRANSACTION`|YES|**Limitations:**`DEFERRABLE` clause has no effect.

`SET TRANSACTION SNAPSHOT` command is not supported.

|
|`SHOW`|YES| |
|`START TRANSACTION`|YES| |
|`TRUNCATE`|YES| |
|`UNLISTEN`|**NO**| |
|`UPDATE`|YES|**Limitations:**`SET` not allowed for Greenplum distribution key columns.

|
|`VACUUM`|YES|**Limitations:**`VACUUM FULL` is not recommended in Greenplum Database.

|
|`VALUES`|YES| |

