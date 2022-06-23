# Summary of Greenplum Features 

This section provides a high-level overview of the system requirements and feature set of Greenplum Database. It contains the following topics:

-   [Greenplum SQL Standard Conformance](#topic2)
-   [Greenplum and PostgreSQL Compatibility](#topic8)

**Parent topic:** [Greenplum Database Reference Guide](ref_guide.html)

## Greenplum SQL Standard Conformance 

The SQL language was first formally standardized in 1986 by the American National Standards Institute \(ANSI\) as SQL 1986. Subsequent versions of the SQL standard have been released by ANSI and as International Organization for Standardization \(ISO\) standards: SQL 1989, SQL 1992, SQL 1999, SQL 2003, SQL 2006, and finally SQL 2008, which is the current SQL standard. The official name of the standard is ISO/IEC 9075-14:2008. In general, each new version adds more features, although occasionally features are deprecated or removed.

It is important to note that there are no commercial database systems that are fully compliant with the SQL standard. Greenplum Database is almost fully compliant with the SQL 1992 standard, with most of the features from SQL 1999. Several features from SQL 2003 have also been implemented \(most notably the SQL OLAP features\).

This section addresses the important conformance issues of Greenplum Database as they relate to the SQL standards. For a feature-by-feature list of Greenplum's support of the latest SQL standard, see [SQL 2008 Optional Feature Compliance](SQL2008_support.html).

### Core SQL Conformance 

In the process of building a parallel, shared-nothing database system and query optimizer, certain common SQL constructs are not currently implemented in Greenplum Database. The following SQL constructs are not supported:

1.  Some set returning subqueries in `EXISTS` or `NOT EXISTS` clauses that Greenplum's parallel optimizer cannot rewrite into joins.
2.  Backwards scrolling cursors, including the use of `FETCH PRIOR`, `FETCH FIRST`, `FETCH ABOLUTE`, and `FETCH RELATIVE`.
3.  In `CREATE TABLE` statements \(on hash-distributed tables\): a `UNIQUE` or `PRIMARY KEY` clause must include all of \(or a superset of\) the distribution key columns. Because of this restriction, only one `UNIQUE` clause or `PRIMARY KEY` clause is allowed in a `CREATE TABLE` statement. `UNIQUE` or `PRIMARY KEY` clauses are not allowed on randomly-distributed tables.
4.  `CREATE UNIQUE INDEX` statements that do not contain all of \(or a superset of\) the distribution key columns. `CREATE UNIQUE INDEX` is not allowed on randomly-distributed tables.

    Note that `UNIQUE INDEXES` \(but not `UNIQUE CONSTRAINTS`\) are enforced on a part basis within a partitioned table. They guarantee the uniqueness of the key within each part or sub-part.

5.  `VOLATILE` or `STABLE` functions cannot execute on the segments, and so are generally limited to being passed literal values as the arguments to their parameters.
6.  Triggers are not supported since they typically rely on the use of `VOLATILE` functions.
7.  Referential integrity constraints \(foreign keys\) are not enforced in Greenplum Database. Users can declare foreign keys and this information is kept in the system catalog, however.
8.  Sequence manipulation functions `CURRVAL` and `LASTVAL`.

### SQL 1992 Conformance 

The following features of SQL 1992 are not supported in Greenplum Database:

1.  `NATIONAL CHARACTER` \(`NCHAR`\) and `NATIONAL CHARACTER VARYING` \(`NVARCHAR`\). Users can declare the `NCHAR` and `NVARCHAR` types, however they are just synonyms for `CHAR` and `VARCHAR` in Greenplum Database.
2.  `CREATE ASSERTION` statement.
3.  `INTERVAL` literals are supported in Greenplum Database, but do not conform to the standard.
4.  `GET DIAGNOSTICS` statement.
5.  `GRANT INSERT` or `UPDATE` privileges on columns. Privileges can only be granted on tables in Greenplum Database.
6.  `GLOBAL TEMPORARY TABLE`s and `LOCAL TEMPORARY TABLE`s. Greenplum `TEMPORARY TABLE`s do not conform to the SQL standard, but many commercial database systems have implemented temporary tables in the same way. Greenplum temporary tables are the same as `VOLATILE TABLE`s in Teradata.
7.  `UNIQUE` predicate.
8.  `MATCH PARTIAL` for referential integrity checks \(most likely will not be implemented in Greenplum Database\).

### SQL 1999 Conformance 

The following features of SQL 1999 are not supported in Greenplum Database:

1.  Large Object data types: `BLOB`, `CLOB`, `NCLOB`. However, the `BYTEA` and `TEXT` columns can store very large amounts of data in Greenplum Database \(hundreds of megabytes\).
2.  `MODULE` \(SQL client modules\).
3.  `CREATE PROCEDURE` \(`SQL/PSM`\). This can be worked around in Greenplum Database by creating a `FUNCTION` that returns `void`, and invoking the function as follows:

    ```
    SELECT myfunc(args);
    
    ```

4.  The PostgreSQL/Greenplum function definition language \(`PL/PGSQL`\) is a subset of Oracle's `PL/SQL`, rather than being compatible with the `SQL/PSM` function definition language. Greenplum Database also supports function definitions written in Python, Perl, Java, and R.
5.  `BIT` and `BIT VARYING` data types \(intentionally omitted\). These were deprecated in SQL 2003, and replaced in SQL 2008.
6.  Greenplum supports identifiers up to 63 characters long. The SQL standard requires support for identifiers up to 128 characters long.
7.  Prepared transactions \(`PREPARE TRANSACTION`, `COMMIT PREPARED`, `ROLLBACK PREPARED`\). This also means Greenplum does not support `XA` Transactions \(2 phase commit coordination of database transactions with external transactions\).
8.  `CHARACTER SET` option on the definition of `CHAR()` or `VARCHAR()` columns.
9.  Specification of `CHARACTERS` or `OCTETS` \(`BYTES`\) on the length of a `CHAR()` or `VARCHAR()` column. For example, `VARCHAR(15 CHARACTERS)` or `VARCHAR(15 OCTETS)` or `VARCHAR(15 BYTES)`.
10. `CURRENT_SCHEMA` function.
11. `CREATE DISTINCT TYPE`statement. `CREATE DOMAIN` can be used as a work-around in Greenplum.
12. The *explicit table* construct.

### SQL 2003 Conformance 

The following features of SQL 2003 are not supported in Greenplum Database:

1.  `MERGE` statements.
2.  `IDENTITY` columns and the associated `GENERATED ALWAYS/GENERATED BY DEFAULT` clause. The `SERIAL` or `BIGSERIAL` data types are very similar to `INT` or `BIGINT GENERATED BY DEFAULT AS IDENTITY`.
3.  `MULTISET` modifiers on data types.
4.  `ROW` data type.
5.  Greenplum Database syntax for using sequences is non-standard. For example, `nextval('``seq``')` is used in Greenplum instead of the standard `NEXT VALUE FOR``seq`.
6.  `GENERATED ALWAYS AS`columns. Views can be used as a work-around.
7.  The sample clause \(`TABLESAMPLE`\) on `SELECT` statements. The `random()` function can be used as a work-around to get random samples from tables.
8.  The *partitioned join tables* construct \(`PARTITION BY` in a join\).
9.  `GRANT SELECT` privileges on columns. Privileges can only be granted on tables in Greenplum Database. Views can be used as a work-around.
10. For `CREATE TABLE x (LIKE(y))` statements, Greenplum does not support the `[INCLUDING|EXCLUDING]``[DEFAULTS|CONSTRAINTS|INDEXES]` clauses.
11. Greenplum array data types are almost SQL standard compliant with some exceptions. Generally customers should not encounter any problems using them.

### SQL 2008 Conformance 

The following features of SQL 2008 are not supported in Greenplum Database:

1.  `BINARY` and `VARBINARY` data types. `BYTEA` can be used in place of `VARBINARY` in Greenplum Database.
2.  `FETCH FIRST` or `FETCH NEXT` clause for `SELECT`, for example:

    ```
    SELECT id, name FROM tab1 ORDER BY id OFFSET 20 ROWS FETCH 
    NEXT 10 ROWS ONLY; 
    ```

    Greenplum has `LIMIT` and `LIMIT OFFSET` clauses instead.

3.  The `ORDER BY` clause is ignored in views and subqueries unless a `LIMIT` clause is also used. This is intentional, as the Greenplum optimizer cannot determine when it is safe to avoid the sort, causing an unexpected performance impact for such `ORDER BY` clauses. To work around, you can specify a really large `LIMIT`. For example: `SELECT * FROM``mytable``ORDER BY 1 LIMIT 9999999999`
4.  The *row subquery* construct is not supported.
5.  `TRUNCATE TABLE` does not accept the `CONTINUE IDENTITY` and `RESTART IDENTITY` clauses.

## Greenplum and PostgreSQL Compatibility 

Greenplum Database is based on PostgreSQL 8.3 with additional features from newer PostgreSQL releases. To support the distributed nature and typical workload of a Greenplum Database system, some SQL commands have been added or modified, and there are a few PostgreSQL features that are not supported. Greenplum has also added features not found in PostgreSQL, such as physical data distribution, parallel query optimization, external tables, resource queues, and enhanced table partitioning. For full SQL syntax and references, see the [SQL Command Reference](sql_commands/sql_ref.html).

<div class="tablenoborder">
<table cellpadding="4" cellspacing="0" summary="" id="topic8__ik213423" class="table" frame="border" border="1" rules="all">
<caption><span class="tablecap">Table 1. SQL Support in Greenplum Database</span></caption>
          <thead class="thead" align="left">
            <tr class="row">
              <th class="entry" valign="top" width="143pt" id="d133063e651">SQL Command</th>
              <th class="entry" valign="top" width="73pt" id="d133063e654">Supported in Greenplum</th>
              <th class="entry" valign="top" width="233pt" id="d133063e657">Modifications, Limitations, Exceptions</th>
            </tr>
          </thead>
          <tbody class="tbody">
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">ALTER AGGREGATE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">ALTER CONVERSION</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">ALTER DATABASE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">ALTER DOMAIN</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">ALTER EXTENSION</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">Changes the definition of a Greenplum Database extension - based
                on PostgreSQL 9.6. </td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">ALTER FILESPACE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">Greenplum Database parallel tablespace feature - not in
                PostgreSQL 8.3.</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">ALTER FUNCTION</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">ALTER GROUP</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">An alias for <a class="xref" href="sql_commands/ALTER_ROLE.html#topic1">ALTER ROLE</a>
</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">ALTER INDEX</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">ALTER LANGUAGE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">ALTER OPERATOR</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">ALTER OPERATOR CLASS</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">ALTER OPERATOR FAMILY</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">ALTER PROTOCOL</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">ALTER RESOURCE QUEUE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">Greenplum Database resource management feature - not in
                PostgreSQL.</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">ALTER ROLE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">
<strong class="ph b">Greenplum Database Clauses:</strong><p class="p"><samp class="ph codeph">RESOURCE QUEUE
                    </samp><em class="ph i">queue_name</em><samp class="ph codeph"> | none</samp></p>
</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">ALTER SCHEMA</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">ALTER SEQUENCE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">ALTER TABLE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">
<strong class="ph b">Unsupported Clauses / Options:</strong><p class="p"><samp class="ph codeph">CLUSTER
                    ON</samp></p>
<p class="p"><samp class="ph codeph">ENABLE/DISABLE TRIGGER</samp></p>
<p class="p"><strong class="ph b">Greenplum
                    Database Clauses:</strong></p>
<p class="p"><samp class="ph codeph">ADD | DROP | RENAME | SPLIT | EXCHANGE
                    PARTITION | SET SUBPARTITION TEMPLATE | SET WITH
                    </samp><samp class="ph codeph">(REORGANIZE=true | false) | SET DISTRIBUTED
                BY</samp></p>
</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">ALTER TABLESPACE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">ALTER TRIGGER</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 "><strong class="ph b">NO</strong></td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">ALTER TYPE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">ALTER USER</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">An alias for <a class="xref" href="sql_commands/ALTER_ROLE.html#topic1">ALTER ROLE</a>
</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">ALTER VIEW</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">ANALYZE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">BEGIN</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">CHECKPOINT</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">CLOSE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">CLUSTER</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">COMMENT</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">COMMIT</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">COMMIT PREPARED</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 "><strong class="ph b">NO</strong></td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">COPY</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">
<strong class="ph b">Modified Clauses:</strong><p class="p"><samp class="ph codeph">ESCAPE [ AS ]
                    '</samp><em class="ph i">escape</em><samp class="ph codeph">' | 'OFF'</samp></p>
<p class="p"><strong class="ph b">Greenplum Database
                    Clauses:</strong></p>
<p class="p"><samp class="ph codeph">[LOG ERRORS] SEGMENT REJECT LIMIT
                    </samp><em class="ph i">count</em><samp class="ph codeph"> [ROWS|PERCENT]</samp></p>
</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">CREATE AGGREGATE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">
<strong class="ph b">Unsupported Clauses / Options:</strong><p class="p"><samp class="ph codeph">[ , SORTOP =
                    </samp><em class="ph i">sort_operator</em><samp class="ph codeph"> ]</samp></p>
<p class="p"><strong class="ph b">Greenplum Database
                    Clauses:</strong></p>
<p class="p"><samp class="ph codeph">[ , PREFUNC = </samp><em class="ph i">prefunc</em><samp class="ph codeph">
                    ]</samp></p>
<p class="p"><strong class="ph b">Limitations:</strong></p>
<p class="p">The functions used to implement the
                  aggregate must be <samp class="ph codeph">IMMUTABLE</samp> functions.</p>
</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">CREATE CAST</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">CREATE CONSTRAINT TRIGGER</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 "><strong class="ph b">NO</strong></td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">CREATE CONVERSION</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">CREATE DATABASE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">CREATE DOMAIN</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">CREATE EXTENSION</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">Loads a new extension into Greenplum Database -  based on
                PostgreSQL 9.6.</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">CREATE EXTERNAL TABLE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">Greenplum Database parallel ETL feature - not in PostgreSQL
                8.3.</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">CREATE FUNCTION</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">
<strong class="ph b">Limitations:</strong><p class="p">Functions defined as
                    <samp class="ph codeph">STABLE</samp> or <samp class="ph codeph">VOLATILE</samp> can be executed in
                  Greenplum Database provided that they are executed on the master only.
                    <samp class="ph codeph">STABLE</samp> and <samp class="ph codeph">VOLATILE</samp> functions cannot be used
                  in statements that execute at the segment level. </p>
</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">CREATE GROUP</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">An alias for <a class="xref" href="sql_commands/CREATE_ROLE.html#topic1">CREATE ROLE</a>
</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">CREATE INDEX</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">
<strong class="ph b">Greenplum Database Clauses:</strong><p class="p"><samp class="ph codeph">USING
                    bitmap</samp> (bitmap
                    indexes)</p>
<p class="p"><strong class="ph b">Limitations:</strong></p>
<p class="p"><samp class="ph codeph">UNIQUE</samp> indexes are
                  allowed only if they contain all of (or a superset of) the Greenplum distribution
                  key columns. On partitioned tables, a unique index is only supported within an
                  individual partition - not across all
                    partitions.</p>
<p class="p"><samp class="ph codeph">CONCURRENTLY</samp> keyword not supported in
                  Greenplum.</p>
</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">CREATE LANGUAGE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">CREATE OPERATOR</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">
<strong class="ph b">Limitations:</strong><p class="p">The function used to implement the
                  operator must be an <samp class="ph codeph">IMMUTABLE</samp> function.</p>
</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">CREATE OPERATOR CLASS</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">CREATE OPERATOR FAMILY</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">CREATE PROTOCOL</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">CREATE RESOURCE QUEUE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">Greenplum Database resource management feature - not in
                PostgreSQL 8.3.</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">CREATE ROLE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">
<strong class="ph b">Greenplum Database Clauses:</strong><p class="p"><samp class="ph codeph">RESOURCE QUEUE
                    </samp><em class="ph i">queue_name</em><samp class="ph codeph"> | none</samp></p>
</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">CREATE RULE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">CREATE SCHEMA</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">CREATE SEQUENCE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">
<strong class="ph b">Limitations:</strong><p class="p">The <samp class="ph codeph">lastval()</samp> and
                    <samp class="ph codeph">currval()</samp> functions are not supported.</p>
<p class="p">The
                    <samp class="ph codeph">setval()</samp> function is only allowed in queries that do not operate
                  on distributed data.</p>
</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">CREATE TABLE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">
<strong class="ph b">Unsupported Clauses / Options:</strong><p class="p"><samp class="ph codeph">[GLOBAL |
                    LOCAL]</samp></p>
<p class="p"><samp class="ph codeph">REFERENCES</samp></p>
<p class="p"><samp class="ph codeph">FOREIGN
                    KEY</samp></p>
<p class="p"><samp class="ph codeph">[DEFERRABLE | NOT DEFERRABLE]
                    </samp></p>
<p class="p"><strong class="ph b">Limited Clauses:</strong></p>
<p class="p"><samp class="ph codeph">UNIQUE</samp> or
                    <samp class="ph codeph">PRIMARY KEY </samp>constraints are only allowed on hash-distributed
                  tables (<samp class="ph codeph">DISTRIBUTED BY</samp>), and the constraint columns must be the
                  same as or a superset of the distribution key columns of the table and must
                  include all the distribution key columns of the partitioning
                    key.</p>
<p class="p"><strong class="ph b">Greenplum Database Clauses:</strong></p>
<p class="p"><samp class="ph codeph">DISTRIBUTED BY
                      (<em class="ph i">column</em>, [ ... ] ) |</samp></p>
<p class="p"><samp class="ph codeph">DISTRIBUTED
                    RANDOMLY</samp></p>
<p class="p"><samp class="ph codeph">PARTITION BY <em class="ph i">type</em> (<em class="ph i">column</em> [, ...])
                    &nbsp;&nbsp;&nbsp;( <em class="ph i">partition_specification</em>, [...] )</samp></p>
<p class="p"><samp class="ph codeph">WITH
                    (appendonly=true &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;[,compresslevel=value,blocksize=value]
                )</samp></p>
</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">CREATE TABLE AS</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">See <a class="xref" href="sql_commands/CREATE_TABLE.html#topic1">CREATE TABLE</a>
</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">CREATE TABLESPACE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 "><strong class="ph b">NO</strong></td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">
<strong class="ph b">Greenplum Database Clauses:</strong><p class="p"><samp class="ph codeph">FILESPACE
                    </samp><em class="ph i">filespace_name</em></p>
</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">CREATE TRIGGER</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 "><strong class="ph b">NO</strong></td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">CREATE TYPE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">
<strong class="ph b">Limitations:</strong><p class="p">The functions used to implement a new base
                  type must be <samp class="ph codeph">IMMUTABLE</samp> functions.</p>
</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">CREATE USER</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">An alias for <a class="xref" href="sql_commands/CREATE_ROLE.html#topic1">CREATE ROLE</a>
</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">CREATE VIEW</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">DEALLOCATE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">DECLARE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">
<strong class="ph b">Unsupported Clauses /
                  Options:</strong><p class="p"><samp class="ph codeph">SCROLL</samp></p>
<p class="p"><samp class="ph codeph">FOR UPDATE [ OF column [,
                    ...] ]</samp></p>
<p class="p"><strong class="ph b">Limitations:</strong></p>
<p class="p">Cursors cannot be
                  backward-scrolled. Forward scrolling is supported.</p>
<p class="p">PL/pgSQL does not have
                  support for updatable cursors. </p>
</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">DELETE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">
<strong class="ph b">Unsupported Clauses /
                    Options:</strong><p class="p"><samp class="ph codeph">RETURNING</samp></p>
</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">DISCARD</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">
                <p class="p"><strong class="ph b">Limitation:</strong>
                  <samp class="ph codeph">DISCARD ALL</samp> is not supported.</p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">DO</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">PostgreSQL 9.0 feature</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">DROP AGGREGATE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">DROP CAST</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">DROP CONVERSION</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">DROP DATABASE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">DROP DOMAIN</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">DROP EXTENSION</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">Removes an extension from Greenplum Database – based on
                PostgreSQL 9.6.</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">DROP EXTERNAL TABLE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">Greenplum Database parallel ETL feature - not in PostgreSQL
                8.3.</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">DROP FILESPACE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">Greenplum Database parallel tablespace feature - not in
                PostgreSQL 8.3.</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">DROP FUNCTION</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">DROP GROUP</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">An alias for <a class="xref" href="sql_commands/DROP_ROLE.html#topic1">DROP ROLE</a>
</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">DROP INDEX</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">DROP LANGUAGE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">DROP OPERATOR</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">DROP OPERATOR CLASS</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">DROP OPERATOR FAMILY</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">DROP OWNED</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 "><strong class="ph b">NO</strong></td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">DROP PROTOCOL</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">DROP RESOURCE QUEUE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">Greenplum Database resource management feature - not in
                PostgreSQL 8.3.</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">DROP ROLE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">DROP RULE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">DROP SCHEMA</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">DROP SEQUENCE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">DROP TABLE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">DROP TABLESPACE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 "><strong class="ph b">NO</strong></td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">DROP TRIGGER</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 "><strong class="ph b">NO</strong></td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">DROP TYPE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">DROP USER</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">An alias for <a class="xref" href="sql_commands/DROP_ROLE.html#topic1">DROP ROLE</a>
</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">DROP VIEW</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">END</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">EXECUTE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">EXPLAIN</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">FETCH</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">
<strong class="ph b">Unsupported Clauses /
                  Options:</strong><p class="p"><samp class="ph codeph">LAST</samp></p>
<p class="p"><samp class="ph codeph">PRIOR</samp></p>
<p class="p"><samp class="ph codeph">BACKWARD</samp></p>
<p class="p"><samp class="ph codeph">BACKWARD
                    ALL</samp></p>
<p class="p"><strong class="ph b">Limitations:</strong></p>
<p class="p">Cannot fetch rows in a
                  nonsequential fashion; backward scan is not supported.</p>
</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">GRANT</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">INSERT</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">
<strong class="ph b">Unsupported Clauses /
                    Options:</strong><p class="p"><samp class="ph codeph">RETURNING</samp></p>
</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">LISTEN</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 "><strong class="ph b">NO</strong></td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">LOAD</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">LOCK</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">MOVE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">See <a class="xref" href="sql_commands/FETCH.html#topic1">FETCH</a>
</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">NOTIFY</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 "><strong class="ph b">NO</strong></td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">PREPARE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">PREPARE TRANSACTION</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 "><strong class="ph b">NO</strong></td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">REASSIGN OWNED</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">REINDEX</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">RELEASE SAVEPOINT</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">RESET</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">REVOKE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">ROLLBACK</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">ROLLBACK PREPARED</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 "><strong class="ph b">NO</strong></td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">ROLLBACK TO SAVEPOINT</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">SAVEPOINT</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">SELECT</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">
<strong class="ph b">Limitations:</strong><p class="p">Limited use of <samp class="ph codeph">VOLATILE</samp>
                  and <samp class="ph codeph">STABLE</samp> functions in <samp class="ph codeph">FROM</samp> or
                    <samp class="ph codeph">WHERE</samp> clauses</p>
<p class="p">Text search (<samp class="ph codeph">Tsearch2</samp>) is
                  not supported</p>
<p class="p"><samp class="ph codeph">FETCH FIRST</samp> or <samp class="ph codeph">FETCH NEXT</samp>
                  clauses not supported </p>
<p class="p"><strong class="ph b">Greenplum Database Clauses
                    (OLAP):</strong></p>
<p class="p"><samp class="ph codeph">[GROUP BY </samp><em class="ph i">grouping_element</em><samp class="ph codeph"> [,
                    ...]]</samp></p>
<p class="p"><samp class="ph codeph">[WINDOW </samp><em class="ph i">window_name</em><samp class="ph codeph"> AS
                    (</samp><em class="ph i">window_specification</em><samp class="ph codeph">)]</samp></p>
<p class="p"><samp class="ph codeph">[FILTER
                    (WHERE </samp><em class="ph i">condition</em><samp class="ph codeph">)]</samp> applied to an aggregate
                  function in the <samp class="ph codeph">SELECT</samp> list</p>
</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">SELECT INTO</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">See <a class="xref" href="sql_commands/SELECT.html#topic1">SELECT</a>
</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">SET</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">SET CONSTRAINTS</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 "><strong class="ph b">NO</strong></td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">In PostgreSQL, this only applies to foreign key constraints,
                which are currently not enforced in Greenplum Database.</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">SET ROLE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">SET SESSION AUTHORIZATION</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">Deprecated as of PostgreSQL 8.1 - see <a class="xref" href="sql_commands/SET_ROLE.html#topic1">SET ROLE</a>
</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">SET TRANSACTION</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">SHOW</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">START TRANSACTION</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">TRUNCATE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">UNLISTEN</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 "><strong class="ph b">NO</strong></td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">UPDATE</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">
<strong class="ph b">Unsupported
                    Clauses:</strong><p class="p"><samp class="ph codeph">RETURNING</samp></p>
<p class="p"><strong class="ph b">Limitations:</strong></p>
<p class="p"><samp class="ph codeph">SET</samp>
                  not allowed for Greenplum distribution key columns.</p>
</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">VACUUM</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">
<strong class="ph b">Limitations:</strong><p class="p"><samp class="ph codeph">VACUUM FULL</samp> is not
                  recommended in Greenplum Database.</p>
</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="143pt" headers="d133063e651 "><samp class="ph codeph">VALUES</samp></td>
              <td class="entry" valign="top" width="73pt" headers="d133063e654 ">YES</td>
              <td class="entry" valign="top" width="233pt" headers="d133063e657 ">&nbsp;</td>
            </tr>
          </tbody>
        </table>
</div>
