# Summary of Built-in Functions 

Greenplum Database supports built-in functions and operators including analytic functions and window functions that can be used in window expressions. For information about using built-in Greenplum Database functions see, "Using Functions and Operators" in the *Greenplum Database Administrator Guide*.

-   [Greenplum Database Function Types](#topic27)
-   [Built-in Functions and Operators](#topic29)
-   [JSON Functions and Operators](#topic_gn4_x3w_mq)
-   [Window Functions](#topic30)
-   [Advanced Aggregate Functions](#topic31)

**Parent topic:**[Greenplum Database Reference Guide](ref_guide.html)

## Greenplum Database Function Types 

Greenplum Database evaluates functions and operators used in SQL expressions. Some functions and operators are only allowed to execute on the master since they could lead to inconsistencies in Greenplum Database segment instances. This table describes the Greenplum Database Function Types.

|Function Type|Greenplum Support|Description|Comments|
|-------------|-----------------|-----------|--------|
|IMMUTABLE|Yes|Relies only on information directly in its argument list. Given the same argument values, always returns the same result.| |
|STABLE|Yes, in most cases|Within a single table scan, returns the same result for same argument values, but results change across SQL statements.|Results depend on database lookups or parameter values. `current_timestamp` family of functions is `STABLE`; values do not change within an execution.|
|VOLATILE|Restricted|Function values can change within a single table scan. For example: `random()`, `timeofday()`.|Any function with side effects is volatile, even if its result is predictable. For example: `setval()`.|

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

## Built-in Functions and Operators 

The following table lists the categories of built-in functions and operators supported by PostgreSQL. All functions and operators are supported in Greenplum Database as in PostgreSQL with the exception of `STABLE` and `VOLATILE` functions, which are subject to the restrictions noted in [Greenplum Database Function Types](#topic27). See the [Functions and Operators](https://www.postgresql.org/docs/8.3/static/functions.html) section of the PostgreSQL documentation for more information about these built-in functions and operators.

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

 database\_to\_xml\_and\_xmlschema\(nulls boolean, tableforest boolean, targetns text\)

 query\_to\_xml\(query text, nulls boolean, tableforest boolean, targetns text\)

 query\_to\_xmlschema\(query text, nulls boolean, tableforest boolean, targetns text\)

 query\_to\_xml\_and\_xmlschema\(query text, nulls boolean, tableforest boolean, targetns text\)

 schema\_to\_xml\(schema name, nulls boolean, tableforest boolean, targetns text\)

 schema\_to\_xmlschema\(schema name, nulls boolean, tableforest boolean, targetns text\)

 schema\_to\_xml\_and\_xmlschema\(schema name, nulls boolean, tableforest boolean, targetns text\)

 table\_to\_xml\(tbl regclass, nulls boolean, tableforest boolean, targetns text\)

 table\_to\_xmlschema\(tbl regclass, nulls boolean, tableforest boolean, targetns text\)

 table\_to\_xml\_and\_xmlschema\(tbl regclass, nulls boolean, tableforest boolean, targetns text\)

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

## JSON Functions and Operators 

Built-in functions and operators that create and manipulate JSON data.

-   [JSON Operators](#topic_o5y_14w_2z)
-   [JSON Creation Functions](#topic_u4s_wnw_2z)
-   [JSON Processing Functions](#topic_z5d_snw_2z)

**Note:** For `json` values, all key/value pairs are kept even if a JSON object contains duplicate key/value pairs. The processing functions consider the last value as the operative one.

### JSON Operators 

This table describes the operators that are available for use with the `json` data type.

|Operator|Right Operand Type|Description|Example|Example Result|
|--------|------------------|-----------|-------|--------------|
|`->`|`int`|Get JSON array element \(indexed from zero\).|`'[{"a":"foo"},{"b":"bar"},{"c":"baz"}]'::json->2`|`{"c":"baz"}`|
|`->`|`text`|Get JSON object field by key.|`'{"a": {"b":"foo"}}'::json->'a'`|`{"b":"foo"}`|
|`->>`|`int`|Get JSON array element as text.|`'[1,2,3]'::json->>2`|`3`|
|`->>`|`text`|Get JSON object field as text.|`'{"a":1,"b":2}'::json->>'b'`|`2`|
|`#>`|`text[]`|Get JSON object at specified path.|`'{"a": {"b":{"c": "foo"}}}'::json#>'{a,b}`'|`{"c": "foo"}`|
|`#>>`|`text[]`|Get JSON object at specified path as text.|`'{"a":[1,2,3],"b":[4,5,6]}'::json#>>'{a,2}'`|`3`|

### JSON Creation Functions 

This table describes the functions that create `json` values.

|Function|Description|Example|Example Result|
|--------|-----------|-------|--------------|
|`array_to_json(anyarray [, pretty_bool])`|Returns the array as a JSON array. A Greenplum Database multidimensional array becomes a JSON array of arrays. Line feeds are added between dimension 1 elements if pretty\_bool is `true`.

|`array_to_json('{{1,5},{99,100}}'::int[])`|`[[1,5],[99,100]]`|
|`row_to_json(record [, pretty_bool])`|Returns the row as a JSON object. Line feeds are added between level 1 elements if `pretty_bool` is `true`.

|`row_to_json(row(1,'foo'))`|`{"f1":1,"f2":"foo"}`|

### JSON Processing Functions 

This table describes the functions that process `json` values.

|Function|Return Type|Description|Example|Example Result|
|--------|-----------|-----------|-------|--------------|
|`json_each(json)`|`setof key text, value json` `setof key text, value jsonb`

|Expands the outermost JSON object into a set of key/value pairs.|`select * from json_each('{"a":"foo", "b":"bar"}')`|```
 key | value
-----+-------
 a   | "foo"
 b   | "bar"

```

|
|`json_each_text(json)`|`setof key text, value text`|Expands the outermost JSON object into a set of key/value pairs. The returned values are of type `text`.|`select * from json_each_text('{"a":"foo", "b":"bar"}')`|```
 key | value
-----+-------
 a   | foo
 b   | bar

```

|
|`json_extract_path(from_json json, VARIADIC path_elems text[])`|`json`|Returns the JSON value specified to by `path_elems`. Equivalent to `#>` operator.|`json_extract_path('{"f2":{"f3":1},"f4":{"f5":99,"f6":"foo"}}','f4')`|`{"f5":99,"f6":"foo"}`|
|`json_extract_path_text(from_json json, VARIADIC path_elems text[])`

|`text`|Returns the JSON value specified to by `path_elems` as text. Equivalent to `#>>` operator.|`json_extract_path_text('{"f2":{"f3":1},"f4":{"f5":99,"f6":"foo"}}','f4', 'f6')`|`foo`|
|`json_object_keys(json)`|`setof text`|Returns set of keys in the outermost JSON object.|`json_object_keys('{"f1":"abc","f2":{"f3":"a", "f4":"b"}}')`|```
 json_object_keys
------------------
 f1
 f2

```

|
|`json_populate_record(base anyelement, from_json json)`|`anyelement`|Expands the object in `from_json` to a row whose columns match the record type defined by base. See [Note](#json-note).|`select * from json_populate_record(null::myrowtype, '{"a":1,"b":2}')`|```
 a | b
---+---
 1 | 2

```

|
|`json_populate_recordset(base anyelement, from_json json)`|`setof anyelement`|Expands the outermost array of objects in `from_json` to a set of rows whose columns match the record type defined by `base`. See [Note](#json-note).|`select * from json_populate_recordset(null::myrowtype, '[{"a":1,"b":2},{"a":3,"b":4}]')`|```
 a | b
---+---
 1 | 2
 3 | 4

```

|
|`json_array_elements(json)`|`setof json`|Expands a JSON array to a set of JSON values.|`select * from json_array_elements('[1,true, [2,false]]')`|```
   value
-----------
 1
 true
 [2,false]

```

|

**Note:** Many of these functions and operators convert Unicode escapes in JSON strings to regular characters. The functions throw an error for characters that cannot be represented in the database encoding.

For `json_populate_record` and `json_populate_recordset`, type coercion from JSON is best effort and might not result in desired values for some types. JSON keys are matched to identical column names in the target row type. JSON fields that do not appear in the target row type are omitted from the output, and target columns that do not match any JSON field return `NULL`.

## Window Functions 

The following built-in window functions are Greenplum extensions to the PostgreSQL database. All window functions are *immutable*. For more information about window functions, see "Window Expressions" in the *Greenplum Database Administrator Guide*.

|Function|Return Type|Full Syntax|Description|
|--------|-----------|-----------|-----------|
|`cume_dist()`|`double precision`|`CUME_DIST() OVER ( [PARTITION BY` expr `] ORDER BY` expr `)`|Calculates the cumulative distribution of a value in a group of values. Rows with equal values always evaluate to the same cumulative distribution value.|
|`dense_rank()`|`bigint`|`DENSE_RANK () OVER ( [PARTITION BY` expr `] ORDER BY` expr `)`|Computes the rank of a row in an ordered group of rows without skipping rank values. Rows with equal values are given the same rank value.|
|first\_value\(*expr*\)|same as input expr type|`FIRST_VALUE(` expr `) OVER ( [PARTITION BY` expr `] ORDER BY` expr `[ROWS|RANGE` frame\_expr `] )`|Returns the first value in an ordered set of values.|
|lag\(*expr* \[,*offset*\] \[,*default*\]\)|same as input *expr* type|`LAG(` *expr* `[,` *offset* `] [,` *default* `]) OVER ( [PARTITION BY` *expr* `] ORDER BY` *expr* `)`|Provides access to more than one row of the same table without doing a self join. Given a series of rows returned from a query and a position of the cursor, `LAG` provides access to a row at a given physical offset prior to that position. The default `offset` is 1. *default* sets the value that is returned if the offset goes beyond the scope of the window. If *default* is not specified, the default value is null.|
|last\_value\(*expr*\)|same as input *expr* type|LAST\_VALUE\(*expr*\) OVER \( \[PARTITION BY *expr*\] ORDER BY *expr* \[ROWS\|RANGE *frame\_expr*\] \)|Returns the last value in an ordered set of values.|
|lead\(*expr* \[,*offset*\] \[,*default*\]\)|same as input *expr* type|LEAD\(*expr*\[,*offset*\] \[,*expr**default*\]\) OVER \( \[PARTITION BY *expr*\] ORDER BY *expr* \)|Provides access to more than one row of the same table without doing a self join. Given a series of rows returned from a query and a position of the cursor, `lead` provides access to a row at a given physical offset after that position. If *offset* is not specified, the default offset is 1. *default* sets the value that is returned if the offset goes beyond the scope of the window. If *default* is not specified, the default value is null.|
|ntile\(*expr*\)|`bigint`|NTILE\(*expr*\) OVER \( \[PARTITION BY *expr*\] ORDER BY *expr* \)|Divides an ordered data set into a number of buckets \(as defined by *expr*\) and assigns a bucket number to each row.|
|`percent_rank()`|`double precision`|PERCENT\_RANK \(\) OVER \( \[PARTITION BY *expr*\] ORDER BY *expr*\)|Calculates the rank of a hypothetical row `R` minus 1, divided by 1 less than the number of rows being evaluated \(within a window partition\).|
|`rank()`|`bigint`|RANK \(\) OVER \( \[PARTITION BY *expr*\] ORDER BY *expr*\)|Calculates the rank of a row in an ordered group of values. Rows with equal values for the ranking criteria receive the same rank. The number of tied rows are added to the rank number to calculate the next rank value. Ranks may not be consecutive numbers in this case.|
|`row_number()`|`bigint`|ROW\_NUMBER \(\) OVER \( \[PARTITION BY *expr*\] ORDER BY *expr*\)|Assigns a unique number to each row to which it is applied \(either each row in a window partition or each row of the query\).|

## Advanced Aggregate Functions 

The following built-in advanced analytic functions are Greenplum extensions of the PostgreSQL database. Analytic functions are *immutable*.

**Note:** The Greenplum MADlib Extension for Analytics provides additional advanced functions to perform statistical analysis and machine learning with Greenplum Database data. See [Greenplum MADlib Extension for Analytics](extensions/madlib.html).

|Function|Return Type|Full Syntax|Description|
|--------|-----------|-----------|-----------|
|MEDIAN \(*expr*\)|`timestamp, timestamptz, interval, float`|MEDIAN \(*expression*\) *Example:*

 ```
SELECT department_id, MEDIAN(salary) 
FROM employees 
GROUP BY department_id; 
```

|Can take a two-dimensional array as input. Treats such arrays as matrices.|
|PERCENTILE\_CONT \(*expr*\) WITHIN GROUP \(ORDER BY *expr* \[DESC/ASC\]\)|`timestamp, timestamptz, interval, float`|PERCENTILE\_CONT\(*percentage*\) WITHIN GROUP \(ORDER BY *expression*\) *Example:*

 ```
SELECT department_id,
PERCENTILE_CONT (0.5) WITHIN GROUP (ORDER BY salary DESC)
"Median_cont"; 
FROM employees GROUP BY department_id;
```

|Performs an inverse distribution function that assumes a continuous distribution model. It takes a percentile value and a sort specification and returns the same datatype as the numeric datatype of the argument. This returned value is a computed result after performing linear interpolation. Null are ignored in this calculation.|
|PERCENTILE\_DISC \(*expr*\) WITHIN GROUP \(ORDER BY *expr* \[DESC/ASC\]\)|`timestamp, timestamptz, interval, float`|PERCENTILE\_DISC\(*percentage*\) WITHIN GROUP \(ORDER BY *expression*\) *Example:*

 ```
SELECT department_id, 
PERCENTILE_DISC (0.5) WITHIN GROUP (ORDER BY salary DESC)
"Median_desc"; 
FROM employees GROUP BY department_id;
```

|Performs an inverse distribution function that assumes a discrete distribution model. It takes a percentile value and a sort specification. This returned value is an element from the set. Null are ignored in this calculation.|
|`sum(array[])`|`smallint[]int[], bigint[], float[]`|`sum(array[[1,2],[3,4]])` *Example:*

 ```
CREATE TABLE mymatrix (myvalue int[]);
INSERT INTO mymatrix VALUES (array[[1,2],[3,4]]);
INSERT INTO mymatrix VALUES (array[[0,1],[1,0]]);
SELECT sum(myvalue) FROM mymatrix;
 sum 
---------------
 {{1,3},{4,4}}
```

|Performs matrix summation. Can take as input a two-dimensional array that is treated as a matrix.|
|`pivot_sum (label[], label, expr)`|`int[], bigint[], float[]`|`pivot_sum( array['A1','A2'], attr, value)`|A pivot aggregation using sum to resolve duplicate entries.|
|`unnest (array[])`|set of `anyelement`|`unnest( array['one', 'row', 'per', 'item'])`|Transforms a one dimensional array into rows. Returns a set of `anyelement`, a polymorphic [pseudotype in PostgreSQL](https://www.postgresql.org/docs/8.3/static/datatype-pseudo.html).|

