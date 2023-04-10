# Summary of Greenplum Features 

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
6.  Triggers are not generally supported because they typically rely on the use of `VOLATILE` functions. PostgreSQL [Event Triggers](https://www.postgresql.org/docs/12/event-triggers.html) are supported because they capture only DDL events.
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
    SELECT *myfunc*(*args*);
    
    ```

4.  The PostgreSQL/Greenplum function definition language \(`PL/PGSQL`\) is a subset of Oracle's `PL/SQL`, rather than being compatible with the `SQL/PSM` function definition language. Greenplum Database also supports function definitions written in Python, Perl, Java, and R.
5.  `BIT` and `BIT VARYING` data types \(intentionally omitted\). These were deprecated in SQL 2003, and replaced in SQL 2008.
6.  Greenplum supports identifiers up to 63 characters long. The SQL standard requires support for identifiers up to 128 characters long.
7.  Prepared transactions \(`PREPARE TRANSACTION`, `COMMIT PREPARED`, `ROLLBACK PREPARED`\). This also means Greenplum does not support `XA` Transactions \(2 phase commit coordination of database transactions with external transactions\).
8.  `CHARACTER SET` option on the definition of `CHAR()` or `VARCHAR()` columns.
9.  Specification of `CHARACTERS` or `OCTETS` \(`BYTES`\) on the length of a `CHAR()` or `VARCHAR()` column. For example, `VARCHAR(15 CHARACTERS)` or `VARCHAR(15 OCTETS)` or `VARCHAR(15 BYTES)`.
10. `CREATE DISTINCT TYPE` statement. `CREATE DOMAIN` can be used as a work-around in Greenplum.
11. The *explicit table* construct.

### <a id="topic6"></a>SQL 2003 Conformance 

The following features of SQL 2003 are not supported in Greenplum Database:

1.  `MERGE` statements.
2.  `IDENTITY` columns and the associated `GENERATED ALWAYS/GENERATED BY DEFAULT` clause. The `SERIAL` or `BIGSERIAL` data types are very similar to `INT` or `BIGINT GENERATED BY DEFAULT AS IDENTITY`.
3.  `MULTISET` modifiers on data types.
4.  `ROW` data type.
5.  Greenplum Database syntax for using sequences is non-standard. For example, `nextval('seq')` is used in Greenplum instead of the standard `NEXT VALUE FOR seq`.
6.  `GENERATED ALWAYS AS` columns. Views can be used as a work-around.
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

> **Note** Greenplum Database does not support the PostgreSQL [large object facility](https://www.postgresql.org/docs/12/largeobjects.html) for streaming user data that is stored in large-object structures.

> **Note** VMware does not support using `WITH OIDS` or `oids=TRUE` to assign an OID system column when creating or altering a table. This syntax is deprecated and will be removed in a future Greenplum release.

<table cellpadding="4" cellspacing="0" summary="" id="topic8__ik213423" class="table" frame="border" border="1" rules="all"><caption><span class="tablecap"><span class="table--title-label">Table 1. </span>SQL Support in Greenplum Database</span></caption><colgroup><col style="width:143pt" /><col style="width:73pt" /><col style="width:233pt" /></colgroup><thead class="thead" style="text-align:left;">
<tr class="row">
<th class="entry nocellnorowborder" style="vertical-align:top;" id="d119146e602">SQL Command
<th class="entry nocellnorowborder" style="vertical-align:top;" id="d119146e605">Supported in Greenplum
<th class="entry cell-norowborder" style="vertical-align:top;" id="d119146e608">Modifications, Limitations, Exceptions
</tr>
</thead>
<tbody class="tbody">
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">ALTER AGGREGATE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">ALTER CONVERSION</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">ALTER DATABASE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">ALTER DOMAIN</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">ALTER EVENT TRIGGER</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">ALTER EXTENSION</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 ">Changes the definition of a Greenplum Database extension - based
    on PostgreSQL 9.6. </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">ALTER FUNCTION</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">ALTER GROUP</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 ">An alias for <a class="xref" href="sql_commands/ALTER_ROLE.html#topic1">ALTER ROLE</a></td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">ALTER INDEX</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">ALTER LANGUAGE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">ALTER OPERATOR</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">ALTER OPERATOR CLASS</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">ALTER OPERATOR FAMILY</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">ALTER PROTOCOL</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">ALTER PUBLICATION</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">NO</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">ALTER RESOURCE QUEUE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 ">Greenplum Database resource management feature - not in
    PostgreSQL.</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">ALTER ROLE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "><strong class="ph b">Greenplum Database Clauses:</strong><p class="p"><code class="ph codeph">RESOURCE QUEUE
        </code><em class="ph i">queue_name</em><code class="ph codeph"> | none</code></p>
</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">ALTER SCHEMA</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">ALTER SEQUENCE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">ALTER SUBSCRIPTION</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">NO</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">ALTER SYSTEM</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 "><strong class="ph b">NO</strong></td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">ALTER TABLE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "><strong class="ph b">Unsupported Clauses / Options:</strong>
<p class="p"><code class="ph codeph">ENABLE/DISABLE TRIGGER</code></p>
<p class="p"><strong class="ph b">Greenplum
                    Database Clauses:</strong></p>
<p class="p"><code class="ph codeph">ADD | DROP | RENAME | SPLIT | EXCHANGE
                    PARTITION | SET SUBPARTITION TEMPLATE | SET WITH
                    </code><code class="ph codeph">(REORGANIZE=true | false) | SET DISTRIBUTED
                BY</code></p>
</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">ALTER TABLESPACE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">ALTER TRIGGER</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 "><strong class="ph b">NO</strong></td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">ALTER TYPE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "><strong class="ph b">Greenplum Database Clauses:</strong><p class="p"><code class="ph codeph">SET DEFAULT
        ENCODING</code></p>
</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">ALTER USER</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 ">An alias for <a class="xref" href="sql_commands/ALTER_ROLE.html#topic1">ALTER ROLE</a></td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">ALTER VIEW</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">ANALYZE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">BEGIN</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">CHECKPOINT</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">CLOSE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">CLUSTER</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">COMMENT</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">COMMIT</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">COMMIT PREPARED</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 "><strong class="ph b">NO</strong></td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">COPY</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "><strong class="ph b">Modified Clauses:</strong><p class="p"><code class="ph codeph">ESCAPE [ AS ]
        '</code><em class="ph i">escape</em><code class="ph codeph">' | 'OFF'</code></p>
<p class="p"><strong class="ph b">Greenplum Database
                    Clauses:</strong></p>
<p class="p"><code class="ph codeph">[LOG ERRORS] SEGMENT REJECT LIMIT
                    </code><em class="ph i">count</em><code class="ph codeph"> [ROWS|PERCENT]</code></p>
</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">CREATE AGGREGATE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "><strong class="ph b">Unsupported Clauses / Options:</strong><p class="p"><code class="ph codeph">[ , SORTOP =
        </code><em class="ph i">sort_operator</em><code class="ph codeph"> ]</code></p>
<p class="p"><strong class="ph b">Greenplum Database
                    Clauses:</strong></p>
<p class="p"><code class="ph codeph">[ , COMBINEFUNC = </code><em class="ph i">combinefunc</em><code class="ph codeph">
                    ]</code></p>
<p class="p"><strong class="ph b">Limitations:</strong></p>
<p class="p">The functions used to implement the
                  aggregate must be <code class="ph codeph">IMMUTABLE</code> functions.</p>
</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">CREATE CAST</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">CREATE CONSTRAINT TRIGGER</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 "><strong class="ph b">NO</strong></td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">CREATE CONVERSION</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">CREATE DATABASE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">CREATE DOMAIN</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">CREATE EVENT TRIGGER</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">CREATE EXTENSION</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 ">Loads a new extension into Greenplum Database - based on
    PostgreSQL 9.6.</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">CREATE EXTERNAL TABLE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 ">Greenplum Database parallel ETL feature - not in PostgreSQL
    9.4.</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">CREATE FUNCTION</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "><strong class="ph b">Limitations:</strong><p class="p">Functions defined as
        <code class="ph codeph">STABLE</code> or <code class="ph codeph">VOLATILE</code> can be run in
      Greenplum Database provided that they are run on the coordinator only.
        <code class="ph codeph">STABLE</code> and <code class="ph codeph">VOLATILE</code> functions cannot be used
      in statements that run at the segment level. </p>
</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">CREATE GROUP</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 ">An alias for <a class="xref" href="sql_commands/CREATE_ROLE.html#topic1">CREATE ROLE</a></td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">CREATE INDEX</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "><strong class="ph b">Greenplum Database Clauses:</strong><p class="p"><code class="ph codeph">USING
        bitmap</code> (bitmap
        indexes)</p>
<p class="p"><strong class="ph b">Limitations:</strong></p>
<p class="p"><code class="ph codeph">UNIQUE</code> indexes are
                  allowed only if they contain all of (or a superset of) the Greenplum distribution
                  key columns. On partitioned tables, a unique index is only supported within an
                  individual partition - not across all
                    partitions.</p>
<p class="p"><code class="ph codeph">CONCURRENTLY</code> keyword not supported in
                  Greenplum.</p>
</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">CREATE LANGUAGE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">CREATE MATERIALIZED VIEW</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 ">Based on PostgreSQL 9.4.</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">CREATE OPERATOR</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "><strong class="ph b">Limitations:</strong><p class="p">The function used to implement the
      operator must be an <code class="ph codeph">IMMUTABLE</code> function.</p>
</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">CREATE OPERATOR CLASS</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">CREATE OPERATOR FAMILY</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">CREATE PROTOCOL</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">CREATE PUBLICATION</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">NO</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">CREATE RESOURCE QUEUE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 ">Greenplum Database resource management feature - not in
    PostgreSQL 9.4.</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">CREATE ROLE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "><strong class="ph b">Greenplum Database Clauses:</strong><p class="p"><code class="ph codeph">RESOURCE QUEUE
        </code><em class="ph i">queue_name</em><code class="ph codeph"> | none</code></p>
</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">CREATE RULE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">CREATE SCHEMA</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">CREATE SEQUENCE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "><strong class="ph b">Limitations:</strong><p class="p">The <code class="ph codeph">lastval()</code> and
        <code class="ph codeph">currval()</code> functions are not supported.</p>
<p class="p">The
                    <code class="ph codeph">setval()</code> function is only allowed in queries that do not
                  operate on distributed data.</p>
</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">CREATE SUBSCRIPTION</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">NO</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">CREATE TABLE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "><strong class="ph b">Unsupported Clauses / Options:</strong><p class="p"><code class="ph codeph">[GLOBAL |
        LOCAL]</code></p>
<p class="p"><code class="ph codeph">REFERENCES</code></p>
<p class="p"><code class="ph codeph">FOREIGN
                    KEY</code></p>
<p class="p"><code class="ph codeph">[DEFERRABLE | NOT DEFERRABLE]
                    </code></p>
<p class="p"><strong class="ph b">Limited Clauses:</strong></p>
<p class="p"><code class="ph codeph">UNIQUE</code> or
                    <code class="ph codeph">PRIMARY KEY </code>constraints are only allowed on hash-distributed
                  tables (<code class="ph codeph">DISTRIBUTED BY</code>), and the constraint columns must be the
                  same as or a superset of the distribution key columns of the table and must
                  include all the distribution key columns of the partitioning
                    key.</p>
<p class="p"><strong class="ph b">Greenplum Database Clauses:</strong></p>
<p class="p"><code class="ph codeph">DISTRIBUTED BY
                      (<em class="ph i">column</em>, [ ... ] ) |</code></p>
<p class="p"><code class="ph codeph">DISTRIBUTED
                    RANDOMLY</code></p>
<p class="p"><code class="ph codeph">PARTITION BY <em class="ph i">type</em> (<em class="ph i">column</em> [, ...])
                       ( <em class="ph i">partition_specification</em>, [...] )</code></p>
<p class="p"><code class="ph codeph">WITH
                    (appendoptimized=true      [,compresslevel=value,blocksize=value]
                )</code></p>
</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">CREATE TABLE AS</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 ">See <a class="xref" href="sql_commands/CREATE_TABLE.html#topic1">CREATE TABLE</a></td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">CREATE TABLESPACE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 "><strong class="ph b">YES</strong></td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "><strong class="ph b">Greenplum Database Clauses:</strong><p class="p">Specify host file system
      locations for specific segment instances.</p>
<p class="p"><code class="ph codeph">WITH
                      (content<var class="keyword varname">ID_1</var>='<var class="keyword varname">/path/to/dir1</var>...)</code></p>
</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">CREATE TRIGGER</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 "><strong class="ph b">NO</strong></td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">CREATE TYPE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "><strong class="ph b">Greenplum Database Clauses:</strong><p class="p"><code class="ph codeph">COMPRESSTYPE |
        COMPRESSLEVEL | BLOCKSIZE</code></p>
<p class="p"><strong class="ph b">Limitations:</strong></p>
<p class="p">The functions
                  used to implement a new base type must be <code class="ph codeph">IMMUTABLE</code>
                  functions.</p>
</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">CREATE USER</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 ">An alias for <a class="xref" href="sql_commands/CREATE_ROLE.html#topic1">CREATE ROLE</a></td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">CREATE VIEW</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">DEALLOCATE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">DECLARE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "><strong class="ph b">Unsupported Clauses /
      Options:</strong><p class="p"><code class="ph codeph">SCROLL</code></p>
<p class="p"><code class="ph codeph">FOR UPDATE [ OF column [,
                    ...] ]</code></p>
<p class="p"><strong class="ph b">Limitations:</strong></p>
<p class="p">Cursors cannot be
                  backward-scrolled. Forward scrolling is supported.</p>
<p class="p">PL/pgSQL does not have
                  support for updatable cursors. </p>
</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">DELETE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">DISCARD</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 ">
                <p class="p"><strong class="ph b">Limitation:</strong>
                  <code class="ph codeph">DISCARD ALL</code> is not supported.</p>
</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">DO</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 ">PostgreSQL 9.0 feature</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">DROP AGGREGATE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">DROP CAST</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">DROP CONVERSION</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">DROP DATABASE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">DROP DOMAIN</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">DROP EVENT TRIGGER</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">DROP EXTENSION</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 ">Removes an extension from Greenplum Database – based on
                PostgreSQL 9.6.</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">DROP EXTERNAL TABLE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 ">Greenplum Database parallel ETL feature - not in PostgreSQL
                9.4.</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">DROP FUNCTION</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">DROP GROUP</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 ">An alias for <a class="xref" href="sql_commands/DROP_ROLE.html#topic1">DROP ROLE</a></td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">DROP INDEX</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">DROP LANGUAGE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">DROP OPERATOR</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">DROP OPERATOR CLASS</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">DROP OPERATOR FAMILY</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">DROP OWNED</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 "><strong class="ph b">NO</strong></td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">DROP PROTOCOL</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">DROP PUBLICATION</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">NO</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">DROP RESOURCE QUEUE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 ">Greenplum Database resource management feature - not in
                PostgreSQL 9.4.</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">DROP ROLE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">DROP RULE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">DROP SCHEMA</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">DROP SEQUENCE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">DROP SUBSCRIPTION</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">NO</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">DROP TABLE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">DROP TABLESPACE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">DROP TRIGGER</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 "><strong class="ph b">NO</strong></td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">DROP TYPE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">DROP USER</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 ">An alias for <a class="xref" href="sql_commands/DROP_ROLE.html#topic1">DROP ROLE</a></td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">DROP VIEW</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">END</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">EXECUTE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">EXPLAIN</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">FETCH</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "><strong class="ph b">Unsupported Clauses /
                  Options:</strong><p class="p"><code class="ph codeph">LAST</code></p>
<p class="p"><code class="ph codeph">PRIOR</code></p>
<p class="p"><code class="ph codeph">BACKWARD</code></p>
<p class="p"><code class="ph codeph">BACKWARD
                    ALL</code></p>
<p class="p"><strong class="ph b">Limitations:</strong></p>
<p class="p">Cannot fetch rows in a
                  nonsequential fashion; backward scan is not supported.</p>
</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">GRANT</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">INSERT</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">LATERAL</code> Join Type</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 "><strong class="ph b">NO</strong></td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">LISTEN</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 "><strong class="ph b">YES</strong></td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">LOAD</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">LOCK</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">MOVE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 ">See <a class="xref" href="sql_commands/FETCH.html#topic1">FETCH</a></td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">NOTIFY</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 "><strong class="ph b">YES</strong></td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">PREPARE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">PREPARE TRANSACTION</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 "><strong class="ph b">NO</strong></td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">REASSIGN OWNED</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">REFRESH MATERIALIZED VIEW</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 ">Based on PostgreSQL 9.4.</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">REINDEX</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">RELEASE SAVEPOINT</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">RESET</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">RETRIEVE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 ">Greenplum Database parallel retrieve cursor - not in PostgreSQL 9.4.</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">REVOKE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">ROLLBACK</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">ROLLBACK PREPARED</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 "><strong class="ph b">NO</strong></td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">ROLLBACK TO SAVEPOINT</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">SAVEPOINT</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">SELECT</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "><strong class="ph b">Limitations:</strong><p class="p">Limited use of <code class="ph codeph">VOLATILE</code>
                  and <code class="ph codeph">STABLE</code> functions in <code class="ph codeph">FROM</code> or
                    <code class="ph codeph">WHERE</code> clauses</p>
<p class="p">Text search (<code class="ph codeph">Tsearch2</code>) is
                  not supported</p>
<p class="p"><strong class="ph b">Greenplum Database Clauses (OLAP):</strong></p>
<p class="p"><code class="ph codeph">[GROUP
                    BY </code><em class="ph i">grouping_element</em><code class="ph codeph"> [,
                    ...]]</code></p>
<p class="p"><code class="ph codeph">[WINDOW </code><em class="ph i">window_name</em><code class="ph codeph"> AS
                    (</code><em class="ph i">window_specification</em><code class="ph codeph">)]</code></p>
<p class="p"><code class="ph codeph">[FILTER
                    (WHERE </code><em class="ph i">condition</em><code class="ph codeph">)]</code> applied to an aggregate
                  function in the <code class="ph codeph">SELECT</code> list</p>
</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">SELECT INTO</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 ">See <a class="xref" href="sql_commands/SELECT.html#topic1">SELECT</a></td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">SET</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">SET CONSTRAINTS</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 "><strong class="ph b">NO</strong></td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 ">In PostgreSQL, this only applies to foreign key constraints,
                which are currently not enforced in Greenplum Database.</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">SET ROLE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">SET SESSION AUTHORIZATION</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 ">Deprecated as of PostgreSQL 8.1 - see <a class="xref" href="sql_commands/SET_ROLE.html#topic1">SET ROLE</a></td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">SET TRANSACTION</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "><strong class="ph b">Limitations:</strong><p class="p"><code class="ph codeph">DEFERRABLE</code> clause has no
                  effect.</p>
</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">SHOW</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">START TRANSACTION</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">TRUNCATE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">UNLISTEN</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 "><strong class="ph b">YES</strong></td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">UPDATE</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "><strong class="ph b">Limitations:</strong><p class="p"><code class="ph codeph">SET</code> not allowed for
                  Greenplum distribution key columns.</p>
</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">VACUUM</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cell-norowborder" style="vertical-align:top;" headers="d119146e608 "><strong class="ph b">Limitations:</strong><p class="p"><code class="ph codeph">VACUUM FULL</code> is not
                  recommended in Greenplum Database.</p>
</td>
</tr>
<tr class="row">
<td class="entry row-nocellborder" style="vertical-align:top;" headers="d119146e602 "><code class="ph codeph">VALUES</code></td>
<td class="entry row-nocellborder" style="vertical-align:top;" headers="d119146e605 ">YES</td>
<td class="entry cellrowborder" style="vertical-align:top;" headers="d119146e608 "> </td>
</tr>
</tbody>
</table>

