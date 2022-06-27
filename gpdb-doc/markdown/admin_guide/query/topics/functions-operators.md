# Using Functions and Operators 

Description of user-defined and built-in functions and operators in Greenplum Database.

-   [Using Functions in Greenplum Database](#topic27)
-   [User-Defined Functions](#topic28)
-   [Built-in Functions and Operators](#topic29)
-   [Window Functions](#topic30)
-   [Advanced Aggregate Functions](#topic31)

**Parent topic:**[Querying Data](../../query/topics/query.html)

## Using Functions in Greenplum Database 

|Function Type|Greenplum Support|Description|Comments|
|-------------|-----------------|-----------|--------|
|IMMUTABLE|Yes|Relies only on information directly in its argument list. Given the same argument values, always returns the same result.| |
|STABLE|Yes, in most cases|Within a single table scan, returns the same result for same argument values, but results change across SQL statements.|Results depend on database lookups or parameter values. `current_timestamp` family of functions is `STABLE`; values do not change within an execution.|
|VOLATILE|Restricted|Function values can change within a single table scan. For example: `random()`, `timeofday()`.|Any function with side effects is volatile, even if its result is predictable. For example: `setval()`.|

Refer to the PostgreSQL [Function Volatility Categories](https://www.postgresql.org/docs/8.3/static/xfunc-volatility.html) documentation for additional information about the Greenplum Database function volatility classifications.

In Greenplum Database, data is divided up across segments — each segment is a distinct PostgreSQL database. To prevent inconsistent or unexpected results, do not execute functions classified as `VOLATILE` at the segment level if they contain SQL commands or modify the database in any way. For example, functions such as `setval()` are not allowed to execute on distributed data in Greenplum Database because they can cause inconsistent data between segment instances.

To ensure data consistency, you can safely use `VOLATILE` and `STABLE` functions in statements that are evaluated on and run from the master. For example, the following statements run on the master \(statements without a `FROM` clause\):

```
SELECT setval('myseq', 201);
SELECT foo();

```

If a statement has a `FROM` clause containing a distributed table *and* the function in the `FROM` clause returns a set of rows, the statement can run on the segments:

```
SELECT * from foo();

```

Greenplum Database does not support functions that return a table reference \(`rangeFuncs`\) or functions that use the `refCursor` datatype.

### Function Volatility and Plan Caching 

There is relatively little difference between the `STABLE` and `IMMUTABLE` function volatility categories for simple interactive queries that are planned and immediately executed. It does not matter much whether a function is executed once during planning or once during query execution startup. But there is a big difference when you save the plan and reuse it later. If you mislabel a function `IMMUTABLE`, Greenplum Database may prematurely fold it to a constant during planning, possibly reusing a stale value during subsequent execution of the plan. You may run into this hazard when using `PREPARE`d statements, or when using languages such as PL/pgSQL that cache plans.

## User-Defined Functions 

Greenplum Database supports user-defined functions. See [Extending SQL](https://www.postgresql.org/docs/8.3/static/extend.html) in the PostgreSQL documentation for more information.

Use the `CREATE FUNCTION` statement to register user-defined functions that are used as described in [Using Functions in Greenplum Database](#topic27). By default, user-defined functions are declared as `VOLATILE`, so if your user-defined function is `IMMUTABLE` or `STABLE`, you must specify the correct volatility level when you register your function.

When you create user-defined functions, avoid using fatal errors or destructive calls. Greenplum Database may respond to such errors with a sudden shutdown or restart.

In Greenplum Database, the shared library files for user-created functions must reside in the same library path location on every host in the Greenplum Database array \(masters, segments, and mirrors\).

You can also create and execute anonymous code blocks that are written in a Greenplum Database procedural language such as PL/pgSQL. The anonymous blocks run as transient anonymous functions. For information about creating and executing anonymous blocks, see the [`DO`](../../../ref_guide/sql_commands/DO.html) command.

## Built-in Functions and Operators 

The following table lists the categories of built-in functions and operators supported by PostgreSQL. All functions and operators are supported in Greenplum Database as in PostgreSQL with the exception of `STABLE` and `VOLATILE` functions, which are subject to the restrictions noted in [Using Functions in Greenplum Database](#topic27). See the [Functions and Operators](https://www.postgresql.org/docs/8.3/static/functions.html) section of the PostgreSQL documentation for more information about these built-in functions and operators.

Greenplum Database includes JSON processing functions that manipulate values the `json` data type. For information about JSON data, see [Working with JSON Data](json-data.html).

|Operator/Function Category|VOLATILE Functions|STABLE Functions|Restrictions|
|--------------------------|------------------|----------------|------------|
|[Logical Operators](https://www.postgresql.org/docs/8.3/static/functions.html#FUNCTIONS-LOGICAL)| | | |
|[Comparison Operators](https://www.postgresql.org/docs/8.3/static/functions-comparison.html)| | | |
|[Mathematical Functions and Operators](https://www.postgresql.org/docs/8.3/static/functions-math.html)|randomsetseed

| | |
|[String Functions and Operators](https://www.postgresql.org/docs/8.3/static/functions-string.html)|*All built-in conversion functions*|convertpg\_client\_encoding

| |
|[Binary String Functions and Operators](https://www.postgresql.org/docs/8.3/static/functions-binarystring.html)| | | |
|[Bit String Functions and Operators](https://www.postgresql.org/docs/8.3/static/functions-bitstring.html)| | | |
|[Pattern Matching](https://www.postgresql.org/docs/8.3/static/functions-matching.html)| | | |
|[Data Type Formatting Functions](https://www.postgresql.org/docs/8.3/static/functions-formatting.html)| |to\_charto\_timestamp

| |
|[Date/Time Functions and Operators](https://www.postgresql.org/docs/8.3/static/functions-datetime.html)|timeofday|agecurrent\_date

current\_time

current\_timestamp

localtime

localtimestamp

now

| |
|[Enum Support Functions](https://www.postgresql.org/docs/8.3/static/functions-enum.html)| | | |
|[Geometric Functions and Operators](https://www.postgresql.org/docs/8.3/static/functions-geometry.html)| | | |
|[Network Address Functions and Operators](https://www.postgresql.org/docs/8.3/static/functions-net.html)| | | |
|[Sequence Manipulation Functions](https://www.postgresql.org/docs/8.3/static/functions-sequence.html)|nextval\(\)setval\(\)

| | |
|[Conditional Expressions](https://www.postgresql.org/docs/8.3/static/functions-conditional.html)| | | |
|[Array Functions and Operators](https://www.postgresql.org/docs/8.3/static/functions-array.html)| |*All array functions*| |
|[Aggregate Functions](https://www.postgresql.org/docs/8.3/static/functions-aggregate.html)| | | |
|[Subquery Expressions](https://www.postgresql.org/docs/8.3/static/functions-subquery.html)| | | |
|[Row and Array Comparisons](https://www.postgresql.org/docs/8.3/static/functions-comparisons.html)| | | |
|[Set Returning Functions](https://www.postgresql.org/docs/8.3/static/functions-srf.html)|generate\_series| | |
|[System Information Functions](https://www.postgresql.org/docs/8.3/static/functions-info.html)| |*All session information functions* *All access privilege inquiry functions*

*All schema visibility inquiry functions*

*All system catalog information functions*

*All comment information functions*

*All transaction ids and snapshots*

| |
|[System Administration Functions](https://www.postgresql.org/docs/8.3/static/functions-admin.html)|set\_configpg\_cancel\_backend

pg\_terminate\_backend

pg\_reload\_conf

pg\_rotate\_logfile

pg\_start\_backup

pg\_stop\_backup

pg\_size\_pretty

pg\_ls\_dir

pg\_read\_file

pg\_stat\_file

|current\_setting*All database object size functions*

|**Note:** The function `pg_column_size` displays bytes required to store the value, possibly with TOAST compression.|
|[XML Functions](https://www.postgresql.org/docs/9.1/static/functions-xml.html) and function-like expressions| |cursor\_to\_xml\(cursor refcursor, count int, nulls boolean, tableforest boolean, targetns text\)

 cursor\_to\_xmlschema\(cursor refcursor, nulls boolean, tableforest boolean, targetns text\)

 database\_to\_xml\(nulls boolean, tableforest boolean, targetns text\)

 database\_to\_xmlschema\(nulls boolean, tableforest boolean, targetns text\)

 database\_to\_xml\_and\_xmlschema\( nulls boolean, tableforest boolean, targetns text\)

 query\_to\_xml\(query text, nulls boolean, tableforest boolean, targetns text\)

 query\_to\_xmlschema\(query text, nulls boolean, tableforest boolean, targetns text\)

 query\_to\_xml\_and\_xmlschema\( query text, nulls boolean, tableforest boolean, targetns text\)

 schema\_to\_xml\(schema name, nulls boolean, tableforest boolean, targetns text\)

 schema\_to\_xmlschema\( schema name, nulls boolean, tableforest boolean, targetns text\)

 schema\_to\_xml\_and\_xmlschema\( schema name, nulls boolean, tableforest boolean, targetns text\)

 table\_to\_xml\(tbl regclass, nulls boolean, tableforest boolean, targetns text\)

 table\_to\_xmlschema\( tbl regclass, nulls boolean, tableforest boolean, targetns text\)

 table\_to\_xml\_and\_xmlschema\( tbl regclass, nulls boolean, tableforest boolean, targetns text\)

 xmlagg\(xml\)

 xmlconcat\(xml\[, ...\]\)

 xmlelement\(name name \[, xmlattributes\(value \[AS attname\] \[, ... \]\)\] \[, content, ...\]\)

 xmlexists\(text, xml\)

 xmlforest\(content \[AS name\] \[, ...\]\)

 xml\_is\_well\_formed\(text\)

 xml\_is\_well\_formed\_document\(text\)

 xml\_is\_well\_formed\_content\(text\)

 xmlparse \( \{ DOCUMENT \| CONTENT \} value\)

 xpath\(text, xml\)

 xpath\(text, xml, text\[\]\)

 xpath\_exists\(text, xml\)

 xpath\_exists\(text, xml, text\[\]\)

 xmlpi\(name target \[, content\]\)

 xmlroot\(xml, version text \| no value \[, standalone yes\|no\|no value\]\)

 xmlserialize \( \{ DOCUMENT \| CONTENT \} value AS type \)

 xml\(text\)

 text\(xml\)

 xmlcomment\(xml\)

 xmlconcat2\(xml, xml\)

| |

## Window Functions 

The following built-in window functions are Greenplum extensions to the PostgreSQL database. All window functions are *immutable*. For more information about window functions, see [Window Expressions](defining-queries.html).

|Function|Return Type|Full Syntax|Description|
|--------|-----------|-----------|-----------|
|`cume_dist()`|`double precision`|`CUME_DIST() OVER ( [PARTITION BY` expr `] ORDER BY` expr `)`|Calculates the cumulative distribution of a value in a group of values. Rows with equal values always evaluate to the same cumulative distribution value.|
|`dense_rank()`|`bigint`|`DENSE_RANK () OVER ( [PARTITION BY` expr `] ORDER BY` expr `)`|Computes the rank of a row in an ordered group of rows without skipping rank values. Rows with equal values are given the same rank value.|
|`first_value(<expr>)`|same as input expr type|`FIRST_VALUE(` expr `) OVER ( [PARTITION BY` expr `] ORDER BY` expr `[ROWS|RANGE` frame\_expr `] )`|Returns the first value in an ordered set of values.|
|`lag(<expr> [,<offset>] [,<default>])`|same as input *expr* type|`LAG(` *expr* `[,` *offset* `] [,` *default* `]) OVER ( [PARTITION BY` *expr* `] ORDER BY` *expr* `)`|Provides access to more than one row of the same table without doing a self join. Given a series of rows returned from a query and a position of the cursor, `LAG` provides access to a row at a given physical offset prior to that position. The default `offset` is 1. *default* sets the value that is returned if the offset goes beyond the scope of the window. If *default* is not specified, the default value is null.|
|`last_value(<expr>`\)|same as input *expr* type|`LAST_VALUE(<expr>) OVER ( [PARTITION BY <expr>] ORDER BY <expr> [ROWS|RANGE <frame_expr>] )`|Returns the last value in an ordered set of values.|
|``lead(<expr> [,<offset>] [,<default>])``|same as input *expr* type|`LEAD(<expr >[,<offset>] [,<expr><default>]) OVER ( [PARTITION BY <expr>] ORDER BY <expr> )`|Provides access to more than one row of the same table without doing a self join. Given a series of rows returned from a query and a position of the cursor, `lead` provides access to a row at a given physical offset after that position. If *offset* is not specified, the default offset is 1. *default* sets the value that is returned if the offset goes beyond the scope of the window. If *default* is not specified, the default value is null.|
|`ntile(<expr>)`|`bigint`|`NTILE(<expr>) OVER ( [PARTITION BY <expr>] ORDER BY <expr> )`|Divides an ordered data set into a number of buckets \(as defined by *expr*\) and assigns a bucket number to each row.|
|`percent_rank()`|`double precision`|`PERCENT_RANK () OVER ( [PARTITION BY <expr>] ORDER BY <expr >)`|Calculates the rank of a hypothetical row `R` minus 1, divided by 1 less than the number of rows being evaluated \(within a window partition\).|
|`rank()`|`bigint`|`RANK () OVER ( [PARTITION BY <expr>] ORDER BY <expr>)`|Calculates the rank of a row in an ordered group of values. Rows with equal values for the ranking criteria receive the same rank. The number of tied rows are added to the rank number to calculate the next rank value. Ranks may not be consecutive numbers in this case.|
|`row_number()`|`bigint`|`ROW_NUMBER () OVER ( [PARTITION BY <expr>] ORDER BY <expr >)`|Assigns a unique number to each row to which it is applied \(either each row in a window partition or each row of the query\).|

## Advanced Aggregate Functions 

The following built-in advanced aggregate functions are Greenplum extensions of the PostgreSQL database. These functions are *immutable*. Greenplum Database does not support the PostgreSQL ordered-set aggregate functions.

**Note:** The Greenplum MADlib Extension for Analytics provides additional advanced functions to perform statistical analysis and machine learning with Greenplum Database data. See [Greenplum MADlib Extension for Analytics](../../../ref_guide/extensions/madlib.html) in the *Greenplum Database Reference Guide*.

|Function|Return Type|Full Syntax|Description|
|--------|-----------|-----------|-----------|
|`MEDIAN (<expr>)`|`timestamp, timestamptz, interval, float`|`MEDIAN (<expression>)` *Example:*

 ```
SELECT department_id, MEDIAN(salary) 
FROM employees 
GROUP BY department_id; 
```

|Can take a two-dimensional array as input. Treats such arrays as matrices.|
|`sum(array[])`|`smallint[], int[], bigint[], float[]`|`sum(array[[1,2],[3,4]])` *Example:*

 ```
CREATE TABLE mymatrix (myvalue int[]);
INSERT INTO mymatrix 
   VALUES (array[[1,2],[3,4]]);
INSERT INTO mymatrix 
   VALUES (array[[0,1],[1,0]]);
SELECT sum(myvalue) FROM mymatrix;
 sum 
---------------
 {{1,3},{4,4}}
```

|Performs matrix summation. Can take as input a two-dimensional array that is treated as a matrix.|
|`pivot_sum (label[], label, expr)`|`int[], bigint[], float[]`|`pivot_sum( array['A1','A2'], attr, value)`|A pivot aggregation using sum to resolve duplicate entries.|
|`unnest (array[])`|set of `anyelement`|`unnest( array['one', 'row', 'per', 'item'])`|Transforms a one dimensional array into rows. Returns a set of `anyelement`, a polymorphic [pseudotype in PostgreSQL](https://www.postgresql.org/docs/8.3/static/datatype-pseudo.html).|

