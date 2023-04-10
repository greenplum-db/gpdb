# SQL 2008 Optional Feature Compliance 

The following table lists the features described in the 2008 SQL standard. Features that are supported in Greenplum Database are marked as YES in the 'Supported' column, features that are not implemented are marked as NO.

For information about Greenplum features and SQL compliance, see the *Greenplum Database Administrator Guide*.

|ID|Feature|Supported|Comments|
|--|-------|---------|--------|
|B011|Embedded Ada|NO| |
|B012|Embedded C|NO|Due to issues with PostgreSQL `ecpg`|
|B013|Embedded COBOL|NO| |
|B014|Embedded Fortran|NO| |
|B015|Embedded MUMPS|NO| |
|B016|Embedded Pascal|NO| |
|B017|Embedded PL/I|NO| |
|B021|Direct SQL|YES| |
|B031|Basic dynamic SQL|NO| |
|B032|Extended dynamic SQL|NO| |
|B033|Untyped SQL-invoked function arguments|NO| |
|B034|Dynamic specification of cursor attributes|NO| |
|B035|Non-extended descriptor names|NO| |
|B041|Extensions to embedded SQL exception declarations|NO| |
|B051|Enhanced execution rights|NO| |
|B111|Module language Ada|NO| |
|B112|Module language C|NO| |
|B113|Module language COBOL|NO| |
|B114|Module language Fortran|NO| |
|B115|Module language MUMPS|NO| |
|B116|Module language Pascal|NO| |
|B117|Module language PL/I|NO| |
|B121|Routine language Ada|NO| |
|B122|Routine language C|NO| |
|B123|Routine language COBOL|NO| |
|B124|Routine language Fortran|NO| |
|B125|Routine language MUMPS|NO| |
|B126|Routine language Pascal|NO| |
|B127|Routine language PL/I|NO| |
|B128|Routine language SQL|NO| |
|E011|Numeric data types|YES| |
|E011-01|`INTEGER` and `SMALLINT` data types|YES| |
|E011-02|`DOUBLE PRECISION` and `FLOAT` data types|YES| |
|E011-03|`DECIMAL` and `NUMERIC` data types|YES| |
|E011-04|Arithmetic operators|YES| |
|E011-05|Numeric comparison|YES| |
|E011-06|Implicit casting among the numeric data types|YES| |
|E021|Character data types|YES| |
|E021-01|`CHARACTER` data type|YES| |
|E021-02|`CHARACTER VARYING` data type|YES| |
|E021-03|Character literals|YES| |
|E021-04|`CHARACTER_LENGTH` function|YES|Trims trailing spaces from `CHARACTER` values before counting|
|E021-05|`OCTET_LENGTH` function|YES| |
|E021-06|`SUBSTRING` function|YES| |
|E021-07|Character concatenation|YES| |
|E021-08|`UPPER` and `LOWER` functions|YES| |
|E021-09|`TRIM` function|YES| |
|E021-10|Implicit casting among the character string types|YES| |
|E021-11|`POSITION` function|YES| |
|E021-12|Character comparison|YES| |
|E031|Identifiers|YES| |
|E031-01|Delimited identifiers|YES| |
|E031-02|Lower case identifiers|YES| |
|E031-03|Trailing underscore|YES| |
|E051|Basic query specification|YES| |
|E051-01|`SELECT DISTINCT`|YES| |
|E051-02|`GROUP BY` clause|YES| |
|E051-03|`GROUP BY` can contain columns not in `SELECT` list|YES| |
|E051-04|`SELECT` list items can be renamed|YES| |
|E051-05|`HAVING` clause|YES| |
|E051-06|Qualified \* in `SELECT` list|YES| |
|E051-07|Correlation names in the `FROM` clause|YES| |
|E051-08|Rename columns in the `FROM` clause|YES| |
|E061|Basic predicates and search conditions|YES| |
|E061-01|Comparison predicate|YES| |
|E061-02|`BETWEEN` predicate|YES| |
|E061-03|`IN` predicate with list of values|YES| |
|E061-04|`LIKE` predicate|YES| |
|E061-05|`LIKE` predicate `ESCAPE` clause|YES| |
|E061-06|`NULL` predicate|YES| |
|E061-07|Quantified comparison predicate|YES| |
|E061-08|`EXISTS` predicate|YES|Not all uses work in Greenplum|
|E061-09|Subqueries in comparison predicate|YES| |
|E061-11|Subqueries in IN predicate|YES| |
|E061-12|Subqueries in quantified comparison predicate|YES| |
|E061-13|Correlated subqueries|YES| |
|E061-14|Search condition|YES| |
|E071|Basic query expressions|YES| |
|E071-01|`UNION DISTINCT` table operator|YES| |
|E071-02|`UNION ALL` table operator|YES| |
|E071-03|`EXCEPT DISTINCT` table operator|YES| |
|E071-05|Columns combined via table operators need not have exactly the same data type|YES| |
|E071-06|Table operators in subqueries|YES| |
|E081|Basic Privileges|NO|Partial sub-feature support|
|E081-01|`SELECT` privilege|YES| |
|E081-02|`DELETE` privilege|YES| |
|E081-03|`INSERT` privilege at the table level|YES| |
|E081-04|`UPDATE` privilege at the table level|YES| |
|E081-05|`UPDATE` privilege at the column level|YES| |
|E081-06|`REFERENCES` privilege at the table level|NO| |
|E081-07|`REFERENCES` privilege at the column level|NO| |
|E081-08|`WITH GRANT OPTION`|YES| |
|E081-09|`USAGE` privilege|YES| |
|E081-10|`EXECUTE` privilege|YES| |
|E091|Set Functions|YES| |
|E091-01|`AVG`|YES| |
|E091-02|`COUNT`|YES| |
|E091-03|`MAX`|YES| |
|E091-04|`MIN`|YES| |
|E091-05|`SUM`|YES| |
|E091-06|`ALL` quantifier|YES| |
|E091-07|`DISTINCT` quantifier|YES| |
|E101|Basic data manipulation|YES| |
|E101-01|`INSERT` statement|YES| |
|E101-03|Searched `UPDATE` statement|YES| |
|E101-04|Searched `DELETE` statement|YES| |
|E111|Single row `SELECT` statement|YES| |
|E121|Basic cursor support|YES| |
|E121-01|`DECLARE CURSOR`|YES| |
|E121-02|`ORDER BY` columns need not be in select list|YES| |
|E121-03|Value expressions in `ORDER BY` clause|YES| |
|E121-04|`OPEN` statement|YES| |
|E121-06|Positioned `UPDATE` statement|NO| |
|E121-07|Positioned `DELETE` statement|NO| |
|E121-08|`CLOSE` statement|YES| |
|E121-10|`FETCH` statement implicit `NEXT`|YES| |
|E121-17|`WITH HOLD` cursors|YES| |
|E131|Null value support|YES| |
|E141|Basic integrity constraints|YES| |
|E141-01|`NOT NULL` constraints|YES| |
|E141-02|`UNIQUE` constraints of `NOT NULL` columns|YES|Must be the same as or a superset of the Greenplum distribution key|
|E141-03|`PRIMARY KEY` constraints|YES|Must be the same as or a superset of the Greenplum distribution key|
|E141-04|Basic `FOREIGN KEY` constraint with the `NO ACTION` default for both referential delete action and referential update action|NO| |
|E141-06|`CHECK` constraints|YES| |
|E141-07|Column defaults|YES| |
|E141-08|`NOT NULL` inferred on `PRIMARY KEY`|YES| |
|E141-10|Names in a foreign key can be specified in any order|YES|Foreign keys can be declared but are not enforced in Greenplum|
|E151|Transaction support|YES| |
|E151-01|`COMMIT` statement|YES| |
|E151-02|`ROLLBACK` statement|YES| |
|E152|Basic SET TRANSACTION statement|YES| |
|E152-01|`ISOLATION LEVEL SERIALIZABLE` clause|NO|Can be declared but is treated as a synonym for `REPEATABLE READ`|
|E152-02|`READ ONLY` and `READ WRITE` clauses|YES| |
|E153|Updatable queries with subqueries|NO| |
|E161|SQL comments using leading double minus|YES| |
|E171|SQLSTATE support|YES| |
|E182|Module language|NO| |
|F021|Basic information schema|YES| |
|F021-01|`COLUMNS` view|YES| |
|F021-02|`TABLES` view|YES| |
|F021-03|`VIEWS` view|YES| |
|F021-04|`TABLE_CONSTRAINTS` view|YES| |
|F021-05|`REFERENTIAL_CONSTRAINTS` view|YES| |
|F021-06|`CHECK_CONSTRAINTS` view|YES| |
|F031|Basic schema manipulation|YES| |
|F031-01|`CREATE TABLE` statement to create persistent base tables|YES| |
|F031-02|`CREATE VIEW` statement|YES| |
|F031-03|`GRANT` statement|YES| |
|F031-04|`ALTER TABLE` statement: `ADD COLUMN` clause|YES| |
|F031-13|`DROP TABLE` statement: `RESTRICT` clause|YES| |
|F031-16|`DROP VIEW` statement: `RESTRICT` clause|YES| |
|F031-19|`REVOKE` statement: `RESTRICT` clause|YES| |
|F032|`CASCADE` drop behavior|YES| |
|F033|`ALTER TABLE` statement: `DROP COLUMN` clause|YES| |
|F034|Extended REVOKE statement|YES| |
|F034-01|`REVOKE` statement performed by other than the owner of a schema object|YES| |
|F034-02|`REVOKE` statement: `GRANT OPTION FOR` clause|YES| |
|F034-03|`REVOKE` statement to revoke a privilege that the grantee has `WITH GRANT OPTION`|YES| |
|F041|Basic joined table|YES| |
|F041-01|Inner join \(but not necessarily the `INNER` keyword\)|YES| |
|F041-02|`INNER` keyword|YES| |
|F041-03|`LEFT OUTER JOIN`|YES| |
|F041-04|`RIGHT OUTER JOIN`|YES| |
|F041-05|Outer joins can be nested|YES| |
|F041-07|The inner table in a left or right outer join can also be used in an inner join|YES| |
|F041-08|All comparison operators are supported \(rather than just `=`\)|YES| |
|F051|Basic date and time|YES| |
|F051-01|`DATE` data type \(including support of `DATE` literal\)|YES| |
|F051-02|`TIME` data type \(including support of `TIME` literal\) with fractional seconds precision of at least 0|YES| |
|F051-03|`TIMESTAMP` data type \(including support of `TIMESTAMP` literal\) with fractional seconds precision of at least 0 and 6|YES| |
|F051-04|Comparison predicate on `DATE`, `TIME`, and `TIMESTAMP` data types|YES| |
|F051-05|Explicit `CAST` between datetime types and character string types|YES| |
|F051-06|`CURRENT_DATE`|YES| |
|F051-07|`LOCALTIME`|YES| |
|F051-08|`LOCALTIMESTAMP`|YES| |
|F052|Intervals and datetime arithmetic|YES| |
|F053|`OVERLAPS` predicate|YES| |
|F081|`UNION` and `EXCEPT` in views|YES| |
|F111|Isolation levels other than SERIALIZABLE|YES| |
|F111-01|`READ UNCOMMITTED`isolation level|NO|Can be declared but is treated as a synonym for `READ COMMITTED`|
|F111-02|`READ COMMITTED` isolation level|YES| |
|F111-03|`REPEATABLE READ` isolation level|YES||
|F121|Basic diagnostics management|NO| |
|F122|Enhanced diagnostics management|NO| |
|F123|All diagnostics|NO| |
|F131-|Grouped operations|YES| |
|F131-01|`WHERE`, `GROUP BY`, and `HAVING` clauses supported in queries with grouped views|YES| |
|F131-02|Multiple tables supported in queries with grouped views|YES| |
|F131-03|Set functions supported in queries with grouped views|YES| |
|F131-04|Subqueries with `GROUP BY` and `HAVING` clauses and grouped views|YES| |
|F131-05|Single row `SELECT` with `GROUP BY` and `HAVING` clauses and grouped views|YES| |
|F171|Multiple schemas per user|YES| |
|F181|Multiple module support|NO| |
|F191|Referential delete actions|NO| |
|F200|`TRUNCATE TABLE` statement|YES| |
|F201|`CAST` function|YES| |
|F202|`TRUNCATE TABLE`: identity column restart option|NO| |
|F221|Explicit defaults|YES| |
|F222|`INSERT` statement: `DEFAULT VALUES` clause|YES| |
|F231|Privilege tables|YES| |
|F231-01|`TABLE_PRIVILEGES` view|YES| |
|F231-02|`COLUMN_PRIVILEGES` view|YES| |
|F231-03|`USAGE_PRIVILEGES` view|YES| |
|F251|Domain support| | |
|F261|CASE expression|YES| |
|F261-01|Simple `CASE`|YES| |
|F261-02|Searched `CASE`|YES| |
|F261-03|`NULLIF`|YES| |
|F261-04|`COALESCE`|YES| |
|F262|Extended `CASE` expression|NO| |
|F263|Comma-separated predicates in simple `CASE` expression|NO| |
|F271|Compound character literals|YES| |
|F281|`LIKE` enhancements|YES| |
|F291|`UNIQUE` predicate|NO| |
|F301|`CORRESPONDING`in query expressions|NO| |
|F302|INTERSECT table operator|YES| |
|F302-01|`INTERSECT DISTINCT` table operator|YES| |
|F302-02|`INTERSECT ALL` table operator|YES| |
|F304|`EXCEPT ALL` table operator| | |
|F311|Schema definition statement|YES|Partial sub-feature support|
|F311-01|`CREATE SCHEMA`|YES| |
|F311-02|`CREATE TABLE` for persistent base tables|YES| |
|F311-03|`CREATE VIEW`|YES| |
|F311-04|`CREATE VIEW: WITH CHECK OPTION`|NO| |
|F311-05|`GRANT` statement|YES| |
|F312|`MERGE` statement|NO| |
|F313|Enhanced `MERGE` statement|NO| |
|F321|User authorization|YES| |
|F341|Usage Tables|NO| |
|F361|Subprogram support|YES| |
|F381|Extended schema manipulation|YES| |
|F381-01|`ALTER TABLE` statement: `ALTER COLUMN` clause| |Some limitations on altering distribution key columns|
|F381-02|`ALTER TABLE` statement: `ADD CONSTRAINT` clause| | |
|F381-03|`ALTER TABLE` statement: `DROP CONSTRAINT` clause| | |
|F382|Alter column data type|YES|Some limitations on altering distribution key columns|
|F391|Long identifiers|YES| |
|F392|Unicode escapes in identifiers|NO| |
|F393|Unicode escapes in literals|NO| |
|F394|Optional normal form specification|NO| |
|F401|Extended joined table|YES| |
|F401-01|`NATURAL JOIN`|YES| |
|F401-02|`FULL OUTER JOIN`|YES| |
|F401-04|`CROSS JOIN`|YES| |
|F402|Named column joins for LOBs, arrays, and multisets|NO| |
|F403|Partitioned joined tables|NO| |
|F411|Time zone specification|YES|Differences regarding literal interpretation|
|F421|National character|YES| |
|F431|Read-only scrollable cursors|YES|Forward scrolling only|
|01|`FETCH` with explicit `NEXT`|YES| |
|02|`FETCH FIRST`|NO| |
|03|`FETCH LAST`|YES| |
|04|`FETCH PRIOR`|NO| |
|05|`FETCH ABSOLUTE`|NO| |
|06|`FETCH RELATIVE`|NO| |
|F441|Extended set function support|YES| |
|F442|Mixed column references in set functions|YES| |
|F451|Character set definition|NO| |
|F461|Named character sets|NO| |
|F471|Scalar subquery values|YES| |
|F481|Expanded `NULL` predicate|YES| |
|F491|Constraint management|YES| |
|F501|Features and conformance views|YES| |
|F501-01|`SQL_FEATURES` view|YES| |
|F501-02|`SQL_SIZING` view|YES| |
|F501-03|`SQL_LANGUAGES` view|YES| |
|F502|Enhanced documentation tables|YES| |
|F502-01|`SQL_SIZING_PROFILES` view|YES| |
|F502-02|`SQL_IMPLEMENTATION_INFO` view|YES| |
|F502-03|`SQL_PACKAGES` view|YES| |
|F521|Assertions|NO| |
|F531|Temporary tables|YES|Non-standard form|
|F555|Enhanced seconds precision|YES| |
|F561|Full value expressions|YES| |
|F571|Truth value tests|YES| |
|F591|Derived tables|YES| |
|F611|Indicator data types|YES| |
|F641|Row and table constructors|NO| |
|F651|Catalog name qualifiers|YES| |
|F661|Simple tables|NO| |
|F671|Subqueries in `CHECK`|NO|Intentionally omitted|
|F672|Retrospective check constraints|YES| |
|F690|Collation support|NO| |
|F692|Enhanced collation support|NO| |
|F693|SQL-session and client module collations|NO| |
|F695|Translation support|NO| |
|F696|Additional translation documentation|NO| |
|F701|Referential update actions|NO| |
|F711|`ALTER` domain|YES| |
|F721|Deferrable constraints|NO| |
|F731|`INSERT` column privileges|YES| |
|F741|Referential `MATCH` types|NO|No partial match|
|F751|View `CHECK` enhancements|NO| |
|F761|Session management|YES| |
|F762|`CURRENT_CATALOG`|NO| |
|F763|`CURRENT_SCHEMA`|NO| |
|F771|Connection management|YES| |
|F781|Self-referencing operations|YES| |
|F791|Insensitive cursors|YES| |
|F801|Full set function|YES| |
|F812|Basic flagging|NO| |
|F813|Extended flagging|NO| |
|F831|Full cursor update|NO| |
|F841|`LIKE_REGEX` predicate|NO|Non-standard syntax for regex|
|F842|`OCCURENCES_REGEX` function|NO| |
|F843|`POSITION_REGEX` function|NO| |
|F844|`SUBSTRING_REGEX`function|NO| |
|F845|`TRANSLATE_REGEX` function|NO| |
|F846|Octet support in regular expression operators|NO| |
|F847|Nonconstant regular expressions|NO| |
|F850|Top-level `ORDER BY` clause in *query expression*|YES| |
|F851|Top-level `ORDER BY` clause in subqueries|NO| |
|F852|Top-level `ORDER BY` clause in views|NO| |
|F855|Nested `ORDER BY` clause in *query expression*|NO| |
|F856|Nested `FETCH FIRST`clause in *query expression*|NO| |
|F857|Top-level `FETCH FIRST`clause in *query expression*|NO| |
|F858|`FETCH FIRST`clause in subqueries|NO| |
|F859|Top-level `FETCH FIRST`clause in views|NO| |
|F860|`FETCH FIRST ROW`*count* in `FETCH FIRST` clause|NO| |
|F861|Top-level `RESULT OFFSET` clause in *query expression*|NO| |
|F862|`RESULT OFFSET` clause in subqueries|NO| |
|F863|Nested `RESULT OFFSET` clause in *query expression*|NO| |
|F864|Top-level `RESULT OFFSET` clause in views|NO| |
|F865|`OFFSET ROW`*count* in `RESULT OFFSET` clause|NO| |
|S011|Distinct data types|NO| |
|S023|Basic structured types|NO| |
|S024|Enhanced structured types|NO| |
|S025|Final structured types|NO| |
|S026|Self-referencing structured types|NO| |
|S027|Create method by specific method name|NO| |
|S028|Permutable UDT options list|NO| |
|S041|Basic reference types|NO| |
|S043|Enhanced reference types|NO| |
|S051|Create table of type|NO| |
|S071|SQL paths in function and type name resolution|YES| |
|S091|Basic array support|NO|Greenplum has arrays, but is not fully standards compliant|
|S091-01|Arrays of built-in data types|NO|Partially compliant|
|S091-02|Arrays of distinct types|NO| |
|S091-03|Array expressions|NO| |
|S092|Arrays of user-defined types|NO| |
|S094|Arrays of reference types|NO| |
|S095|Array constructors by query|NO| |
|S096|Optional array bounds|NO| |
|S097|Array element assignment|NO| |
|S098|`array_agg`|Partially|Supported: Using `array_agg` without a window specification, for example:<br/>`SELECT array_agg(x) FROM ...`,<br/>`SELECT array_agg (x order by y) FROM ...`<br/><br/>Not supported: Using `array_agg` as an aggregate derived window function, for example:<br/>`SELECT array_agg(x) over (ORDER BY y) FROM ...`,<br/>`SELECT array_agg(x order by y) over (PARTITION BY z) FROM ...`,<br/>`SELECT array_agg(x order by y) over (ORDER BY z) FROM ...`|
|S111|`ONLY` in query expressions|YES| |
|S151|Type predicate|NO| |
|S161|Subtype treatment|NO| |
|S162|Subtype treatment for references|NO| |
|S201|SQL-invoked routines on arrays|NO|Functions can be passed Greenplum array types|
|S202|SQL-invoked routines on multisets|NO| |
|S211|User-defined cast functions|YES| |
|S231|Structured type locators|NO| |
|S232|Array locators|NO| |
|S233|Multiset locators|NO| |
|S241|Transform functions|NO| |
|S242|Alter transform statement|NO| |
|S251|User-defined orderings|NO| |
|S261|Specific type method|NO| |
|S271|Basic multiset support|NO| |
|S272|Multisets of user-defined types|NO| |
|S274|Multisets of reference types|NO| |
|S275|Advanced multiset support|NO| |
|S281|Nested collection types|NO| |
|S291|Unique constraint on entire row|NO| |
|S301|Enhanced `UNNEST`|NO| |
|S401|Distinct types based on array types|NO| |
|S402|Distinct types based on distinct types|NO| |
|S403|`MAX_CARDINALITY`|NO| |
|S404|`TRIM_ARRAY`|NO| |
|T011|Timestamp in Information Schema|NO| |
|T021|`BINARY` and `VARBINARY`data types|NO| |
|T022|Advanced support for `BINARY` and `VARBINARY` data types|NO| |
|T023|Compound binary literal|NO| |
|T024|Spaces in binary literals|NO| |
|T031|`BOOLEAN` data type|YES| |
|T041|Basic `LOB` data type support|NO| |
|T042|Extended `LOB` data type support|NO| |
|T043|Multiplier T|NO| |
|T044|Multiplier P|NO| |
|T051|Row types|NO| |
|T052|`MAX` and `MIN` for row types|NO| |
|T053|Explicit aliases for all-fields reference|NO| |
|T061|UCS support|NO| |
|T071|`BIGINT` data type|YES| |
|T101|Enhanced nullability determiniation|NO| |
|T111|Updatable joins, unions, and columns|NO| |
|T121|`WITH` \(excluding `RECURSIVE`\) in query expression|NO| |
|T122|`WITH` \(excluding `RECURSIVE`\) in subquery|NO| |
|T131|Recursive query|NO| |
|T132|Recursive query in subquery|NO| |
|T141|`SIMILAR` predicate|YES| |
|T151|`DISTINCT` predicate|YES| |
|T152|`DISTINCT` predicate with negation|NO| |
|T171|`LIKE` clause in table definition|YES| |
|T172|`AS` subquery clause in table definition|YES| |
|T173|Extended `LIKE` clause in table definition|YES| |
|T174|Identity columns|NO| |
|T175|Generated columns|NO| |
|T176|Sequence generator support|NO| |
|T177|Sequence generator support: simple restart option|NO| |
|T178|Identity columns: simple restart option|NO| |
|T191|Referential action `RESTRICT`|NO| |
|T201|Comparable data types for referential constraints|NO| |
|T211|Basic trigger capability|NO| |
|T211-01|Triggers activated on `UPDATE`, `INSERT`, or `DELETE` of one base table|NO| |
|T211-02|`BEFORE` triggers|NO| |
|T211-03|`AFTER` triggers|NO| |
|T211-04|`FOR EACH ROW` triggers|NO| |
|T211-05|Ability to specify a search condition that must be true before the trigger is invoked|NO| |
|T211-06|Support for run-time rules for the interaction of triggers and constraints|NO| |
|T211-07|`TRIGGER` privilege|YES| |
|T211-08|Multiple triggers for the same event are run in the order in which they were created in the catalog|NO|Intentionally omitted|
|T212|Enhanced trigger capability|NO| |
|T213|`INSTEAD OF` triggers|NO| |
|T231|Sensitive cursors|YES| |
|T241|`START TRANSACTION` statement|YES| |
|T251|`SET TRANSACTION` statement: `LOCAL` option|NO| |
|T261|Chained transactions|NO| |
|T271|Savepoints|YES| |
|T272|Enhanced savepoint management|NO| |
|T281|`SELECT` privilege with column granularity|YES| |
|T285|Enhanced derived column names|NO| |
|T301|Functional dependencies|NO| |
|T312|OVERLAY function|YES| |
|T321|Basic SQL-invoked routines|NO|Partial support|
|T321-01|User-defined functions with no overloading|YES| |
|T321-02|User-defined stored procedures with no overloading|NO| |
|T321-03|Function invocation|YES| |
|T321-04|CALL statement|NO| |
|T321-05|RETURN statement|NO| |
|T321-06|ROUTINES view|YES| |
|T321-07|PARAMETERS view|YES| |
|T322|Overloading of SQL-invoked functions and procedures|YES| |
|T323|Explicit security for external routines|YES| |
|T324|Explicit security for SQL routines|NO| |
|T325|Qualified SQL parameter references|NO| |
|T326|Table functions|NO| |
|T331|Basic roles|NO| |
|T332|Extended roles|NO| |
|T351|Bracketed SQL comments \(`/*...*/` comments\)|YES| |
|T431|Extended grouping capabilities|NO| |
|T432|Nested and concatenated `GROUPING SETS`|NO| |
|T433|Multiargument `GROUPING` function|NO| |
|T434|`GROUP BY DISTINCT`|NO| |
|T441|`ABS` and `MOD` functions|YES| |
|T461|Symmetric `BETWEEN` predicate|YES| |
|T471|Result sets return value|NO| |
|T491|`LATERAL` derived table|NO| |
|T501|Enhanced `EXISTS` predicate|NO| |
|T511|Transaction counts|NO| |
|T541|Updatable table references|NO| |
|T561|Holdable locators|NO| |
|T571|Array-returning external SQL-invoked functions|NO| |
|T572|Multiset-returning external SQL-invoked functions|NO| |
|T581|Regular expression substring function|YES| |
|T591|`UNIQUE` constraints of possibly null columns|YES| |
|T601|Local cursor references|NO| |
|T611|Elementary OLAP operations|YES| |
|T612|Advanced OLAP operations|NO|Partially supported|
|T613|Sampling|NO| |
|T614|`NTILE` function|YES| |
|T615|`LEAD` and `LAG` functions|YES| |
|T616|Null treatment option for `LEAD` and `LAG` functions|NO| |
|T617|`FIRST_VALUE` and `LAST_VALUE` function|YES| |
|T618|`NTH_VALUE`|NO|Function exists in Greenplum but not all options are supported|
|T621|Enhanced numeric functions|YES| |
|T631|N predicate with one list element|NO| |
|T641|Multiple column assignment|NO|Some syntax variants supported|
|T651|SQL-schema statements in SQL routines|NO| |
|T652|SQL-dynamic statements in SQL routines|NO| |
|T653|SQL-schema statements in external routines|NO| |
|T654|SQL-dynamic statements in external routines|NO| |
|T655|Cyclically dependent routines|NO| |
|M001|Datalinks|NO| |
|M002|Datalinks via SQL/CLI|NO| |
|M003|Datalinks via Embedded SQL|NO| |
|M004|Foreign data support|NO| |
|M005|Foreign schema support|NO| |
|M006|GetSQLString routine|NO| |
|M007|TransmitRequest|NO| |
|M009|GetOpts and GetStatistics routines|NO| |
|M010|Foreign data wrapper support|NO| |
|M011|Datalinks via Ada|NO| |
|M012|Datalinks via C|NO| |
|M013|Datalinks via COBOL|NO| |
|M014|Datalinks via Fortran|NO| |
|M015|Datalinks via M|NO| |
|M016|Datalinks via Pascal|NO| |
|M017|Datalinks via PL/I|NO| |
|M018|Foreign data wrapper interface routines in Ada|NO| |
|M019|Foreign data wrapper interface routines in C|NO| |
|M020|Foreign data wrapper interface routines in COBOL|NO| |
|M021|Foreign data wrapper interface routines in Fortran|NO| |
|M022|Foreign data wrapper interface routines in MUMPS|NO| |
|M023|Foreign data wrapper interface routines in Pascal|NO| |
|M024|Foreign data wrapper interface routines in PL/I|NO| |
|M030|SQL-server foreign data support|NO| |
|M031|Foreign data wrapper general routines|NO| |
|X010|XML type|YES| |
|X011|Arrays of XML type|YES| |
|X012|Multisets of XML type|NO| |
|X013|Distinct types of XML type|NO| |
|X014|Attributes of XML type|NO| |
|X015|Fields of XML type|NO| |
|X016|Persistent XML values|YES| |
|X020|XMLConcat|YES|xmlconcat2\(\) supported|
|X025|XMLCast|NO| |
|X030|XMLDocument|NO| |
|X031|XMLElement|YES| |
|X032|XMLForest|YES| |
|X034|XMLAgg|YES| |
|X035|XMLAgg: ORDER BY option|YES| |
|X036|XMLComment|YES| |
|X037|XMLPI|YES| |
|X038|XMLText|NO| |
|X040|Basic table mapping|NO| |
|X041|Basic table mapping: nulls absent|NO| |
|X042|Basic table mapping: null as nil|NO| |
|X043|Basic table mapping: table as forest|NO| |
|X044|Basic table mapping: table as element|NO| |
|X045|Basic table mapping: with target namespace|NO| |
|X046|Basic table mapping: data mapping|NO| |
|X047|Basic table mapping: metadata mapping|NO| |
|X048|Basic table mapping: base64 encoding of binary strings|NO| |
|X049|Basic table mapping: hex encoding of binary strings|NO| |
|X051|Advanced table mapping: nulls absent|NO| |
|X052|Advanced table mapping: null as nil|NO| |
|X053|Advanced table mapping: table as forest|NO| |
|X054|Advanced table mapping: table as element|NO| |
|X055|Advanced table mapping: target namespace|NO| |
|X056|Advanced table mapping: data mapping|NO| |
|X057|Advanced table mapping: metadata mapping|NO| |
|X058|Advanced table mapping: base64 encoding of binary strings|NO| |
|X059|Advanced table mapping: hex encoding of binary strings|NO| |
|X060|XMLParse: Character string input and CONTENT option|YES| |
|X061|XMLParse: Character string input and DOCUMENT option|YES| |
|X065|XMLParse: BLOB input and CONTENT option|NO| |
|X066|XMLParse: BLOB input and DOCUMENT option|NO| |
|X068|XMLSerialize: BOM|NO| |
|X069|XMLSerialize: INDENT|NO| |
|X070|XMLSerialize: Character string serialization and CONTENT option|YES| |
|X071|XMLSerialize: Character string serialization and DOCUMENT option|YES| |
|X072|XMLSerialize: Character string serialization|YES| |
|X073|XMLSerialize: BLOB serialization and CONTENT option|NO| |
|X074|XMLSerialize: BLOB serialization and DOCUMENT option|NO| |
|X075|XMLSerialize: BLOB serialization|NO| |
|X076|XMLSerialize: VERSION|NO| |
|X077|XMLSerialize: explicit ENCODING option|NO| |
|X078|XMLSerialize: explicit XML declaration|NO| |
|X080|Namespaces in XML publishing|NO| |
|X081|Query-level XML namespace declarations|NO| |
|X082|XML namespace declarations in DML|NO| |
|X083|XML namespace declarations in DDL|NO| |
|X084|XML namespace declarations in compound statements|NO| |
|X085|Predefined namespace prefixes|NO| |
|X086|XML namespace declarations in XMLTable|NO| |
|X090|XML document predicate|NO|xml\_is\_well\_formed\_document\(\) supported|
|X091|XML content predicate|NO|xml\_is\_well\_formed\_content\(\) supported|
|X096|XMLExists|NO|xmlexists\(\) supported|
|X100|Host language support for XML: CONTENT option|NO| |
|X101|Host language support for XML: DOCUMENT option|NO| |
|X110|Host language support for XML: VARCHAR mapping|NO| |
|X111|Host language support for XML: CLOB mapping|NO| |
|X112|Host language support for XML: BLOB mapping|NO| |
|X113|Host language support for XML: STRIP WHITESPACE option|YES| |
|X114|Host language support for XML: PRESERVE WHITESPACE option|YES| |
|X120|XML parameters in SQL routines|YES| |
|X121|XML parameters in external routines|YES| |
|X131|Query-level XMLBINARY clause|NO| |
|X132|XMLBINARY clause in DML|NO| |
|X133|XMLBINARY clause in DDL|NO| |
|X134|XMLBINARY clause in compound statements|NO| |
|X135|XMLBINARY clause in subqueries|NO| |
|X141|IS VALID predicate: data-driven case|NO| |
|X142|IS VALID predicate: ACCORDING TO clause|NO| |
|X143|IS VALID predicate: ELEMENT clause|NO| |
|X144|IS VALID predicate: schema location|NO| |
|X145|IS VALID predicate outside check constraints|NO| |
|X151|IS VALID predicate with DOCUMENT option|NO| |
|X152|IS VALID predicate with CONTENT option|NO| |
|X153|IS VALID predicate with SEQUENCE option|NO| |
|X155|IS VALID predicate: NAMESPACE without ELEMENT clause|NO| |
|X157|IS VALID predicate: NO NAMESPACE with ELEMENT clause|NO| |
|X160|Basic Information Schema for registered XML Schemas|NO| |
|X161|Advanced Information Schema for registered XML Schemas|NO| |
|X170|XML null handling options|NO| |
|X171|NIL ON NO CONTENT option|NO| |
|X181|XML\( DOCUMENT \(UNTYPED\)\) type|NO| |
|X182|XML\( DOCUMENT \(ANY\)\) type|NO| |
|X190|XML\( SEQUENCE\) type|NO| |
|X191|XML\( DOCUMENT \(XMLSCHEMA \)\) type|NO| |
|X192|XML\( CONTENT \(XMLSCHEMA\)\) type|NO| |
|X200|XMLQuery|NO| |
|X201|XMLQuery: RETURNING CONTENT|NO| |
|X202|XMLQuery: RETURNING SEQUENCE|NO| |
|X203|XMLQuery: passing a context item|NO| |
|X204|XMLQuery: initializing an XQuery variable|NO| |
|X205|XMLQuery: EMPTY ON EMPTY option|NO| |
|X206|XMLQuery: NULL ON EMPTY option|NO| |
|X211|XML 1.1 support|NO| |
|X221|XML passing mechanism BY VALUE|NO| |
|X222|XML passing mechanism BY REF|NO| |
|X231|XML\( CONTENT \(UNTYPED \)\) type|NO| |
|X232|XML\( CONTENT \(ANY \)\) type|NO| |
|X241|RETURNING CONTENT in XML publishing|NO| |
|X242|RETURNING SEQUENCE in XML publishing|NO| |
|X251|Persistent XML values of XML\( DOCUMENT \(UNTYPED \)\) type|NO| |
|X252|Persistent XML values of XML\( DOCUMENT \(ANY\)\) type|NO| |
|X253|Persistent XML values of XML\( CONTENT \(UNTYPED\)\) type|NO| |
|X254|Persistent XML values of XML\( CONTENT \(ANY\)\) type|NO| |
|X255|Persistent XML values of XML\( SEQUENCE\) type|NO| |
|X256|Persistent XML values of XML\( DOCUMENT \(XMLSCHEMA\)\) type|NO| |
|X257|Persistent XML values of XML\( CONTENT \(XMLSCHEMA \) type|NO| |
|X260|XML type: ELEMENT clause|NO| |
|X261|XML type: NAMESPACE without ELEMENT clause|NO| |
|X263|XML type: NO NAMESPACE with ELEMENT clause|NO| |
|X264|XML type: schema location|NO| |
|X271|XMLValidate: data-driven case|NO| |
|X272|XMLValidate: ACCORDING TO clause|NO| |
|X273|XMLValidate: ELEMENT clause|NO| |
|X274|XMLValidate: schema location|NO| |
|X281|XMLValidate: with DOCUMENT option|NO| |
|X282|XMLValidate with CONTENT option|NO| |
|X283|XMLValidate with SEQUENCE option|NO| |
|X284|XMLValidate NAMESPACE without ELEMENT clause|NO| |
|X286|XMLValidate: NO NAMESPACE with ELEMENT clause|NO| |
|X300|XMLTable|NO| |
|X301|XMLTable: derived column list option|NO| |
|X302|XMLTable: ordinality column option|NO| |
|X303|XMLTable: column default option|NO| |
|X304|XMLTable: passing a context item|NO| |
|X305|XMLTable: initializing an XQuery variable|NO| |
|X400|Name and identifier mapping|NO| |

