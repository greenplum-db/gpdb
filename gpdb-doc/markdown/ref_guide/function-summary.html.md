---
title: Summary of Built-in Functions 
---

Greenplum Database supports built-in functions and operators including analytic functions and window functions that can be used in window expressions. For information about using built-in Greenplum Database functions see, "Using Functions and Operators" in the *Greenplum Database Administrator Guide*.

-   [Greenplum Database Function Types](#topic27)
-   [Built-in Functions and Operators](#topic29)
-   [JSON Functions and Operators](#topic_gn4_x3w_mq)
-   [Window Functions](#topic30)
-   [Advanced Aggregate Functions](#topic31)
-   [Text Search Functions and Operators](#topic_vpj_ss1_lfb)
-   [Range Functions and Operators](#functions-range)

**Parent topic:**[Greenplum Database Reference Guide](ref_guide.html)

## <a id="topic27"></a>Greenplum Database Function Types 

Greenplum Database evaluates functions and operators used in SQL expressions. Some functions and operators are only allowed to run on the master since they could lead to inconsistencies in Greenplum Database segment instances. This table describes the Greenplum Database Function Types.

|Function Type|Greenplum Support|Description|Comments|
|-------------|-----------------|-----------|--------|
|IMMUTABLE|Yes|Relies only on information directly in its argument list. Given the same argument values, always returns the same result.| |
|STABLE|Yes, in most cases|Within a single table scan, returns the same result for same argument values, but results change across SQL statements.|Results depend on database lookups or parameter values. `current_timestamp` family of functions is `STABLE`; values do not change within an execution.|
|VOLATILE|Restricted|Function values can change within a single table scan. For example: `random()`, `timeofday()`.|Any function with side effects is volatile, even if its result is predictable. For example: `setval()`.|

In Greenplum Database, data is divided up across segments — each segment is a distinct PostgreSQL database. To prevent inconsistent or unexpected results, do not run functions classified as `VOLATILE` at the segment level if they contain SQL commands or modify the database in any way. For example, functions such as `setval()` are not allowed to run on distributed data in Greenplum Database because they can cause inconsistent data between segment instances.

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

## <a id="topic29"></a>Built-in Functions and Operators 

The following table lists the categories of built-in functions and operators supported by PostgreSQL. All functions and operators are supported in Greenplum Database as in PostgreSQL with the exception of `STABLE` and `VOLATILE` functions, which are subject to the restrictions noted in [Greenplum Database Function Types](#topic27). See the [Functions and Operators](https://www.postgresql.org/docs/9.4/functions.html) section of the PostgreSQL documentation for more information about these built-in functions and operators.

|Operator/Function Category|VOLATILE Functions|STABLE Functions|Restrictions|
|--------------------------|------------------|----------------|------------|
|[Logical Operators](https://www.postgresql.org/docs/9.4/functions-logical.html)| | | |
|[Comparison Operators](https://www.postgresql.org/docs/9.4/functions-comparison.html)| | | |
|[Mathematical Functions and Operators](https://www.postgresql.org/docs/9.4/functions-math.html)|randomsetseed

| | |
|[String Functions and Operators](https://www.postgresql.org/docs/9.4/functions-string.html)|*All built-in conversion functions*|convertpg\_client\_encoding

| |
|[Binary String Functions and Operators](https://www.postgresql.org/docs/9.4/functions-binarystring.html)| | | |
|[Bit String Functions and Operators](https://www.postgresql.org/docs/9.4/functions-bitstring.html)| | | |
|[Pattern Matching](https://www.postgresql.org/docs/9.4/functions-matching.html)| | | |
|[Data Type Formatting Functions](https://www.postgresql.org/docs/9.4/functions-formatting.html)| |to\_charto\_timestamp

| |
|[Date/Time Functions and Operators](https://www.postgresql.org/docs/9.4/functions-datetime.html)|timeofday|agecurrent\_date

current\_time

current\_timestamp

localtime

localtimestamp

now

| |
|[Enum Support Functions](https://www.postgresql.org/docs/9.4/functions-enum.html)| | | |
|[Geometric Functions and Operators](https://www.postgresql.org/docs/9.4/functions-geometry.html)| | | |
|[Network Address Functions and Operators](https://www.postgresql.org/docs/9.4/functions-net.html)| | | |
|[Sequence Manipulation Functions](https://www.postgresql.org/docs/9.4/functions-sequence.html)|nextval\(\)setval\(\)

| | |
|[Conditional Expressions](https://www.postgresql.org/docs/9.4/functions-conditional.html)| | | |
|[Array Functions and Operators](https://www.postgresql.org/docs/9.4/functions-array.html)| |*All array functions*| |
|[Aggregate Functions](https://www.postgresql.org/docs/9.4/functions-aggregate.html)| | | |
|[Subquery Expressions](https://www.postgresql.org/docs/9.4/functions-subquery.html)| | | |
|[Row and Array Comparisons](https://www.postgresql.org/docs/9.4/functions-comparisons.html)| | | |
|[Set Returning Functions](https://www.postgresql.org/docs/9.4/functions-srf.html)|generate\_series| | |
|[System Information Functions](https://www.postgresql.org/docs/9.4/functions-info.html)| |*All session information functions* *All access privilege inquiry functions*

*All schema visibility inquiry functions*

*All system catalog information functions*

*All comment information functions*

*All transaction ids and snapshots*

| |
|[System Administration Functions](https://www.postgresql.org/docs/9.4/functions-admin.html)|set\_configpg\_cancel\_backend

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
|[XML Functions](https://www.postgresql.org/docs/9.4/functions-xml.html) and function-like expressions| |cursor\_to\_xml\(cursor refcursor, count int, nulls boolean, tableforest boolean, targetns text\)

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

## <a id="topic_gn4_x3w_mq"></a>JSON Functions and Operators 

Greenplum Database includes built-in functions and operators that create and manipulate JSON data.

-   [JSON Operators](#topic_o5y_14w_2z)
-   [JSON Creation Functions](#topic_u4s_wnw_2z)
-   [JSON Aggregate Functions](#topic_rvp_lk3_sfb)
-   [JSON Processing Functions](#topic_z5d_snw_2z)

**Note:** For `json` data type values, all key/value pairs are kept even if a JSON object contains duplicate keys. For duplicate keys, JSON processing functions consider the last value as the operative one. For the `jsonb` data type, duplicate object keys are not kept. If the input includes duplicate keys, only the last value is kept. See [About JSON Data](../admin_guide/query/topics/json-data.html#topic_upc_tcs_fz)in the *Greenplum Database Administrator Guide*.

### <a id="topic_o5y_14w_2z"></a>JSON Operators 

This table describes the operators that are available for use with the `json` and `jsonb` data types.

|Operator|Right Operand Type|Description|Example|Example Result|
|--------|------------------|-----------|-------|--------------|
|`->`|`int`|Get the JSON array element \(indexed from zero\).|`'[{"a":"foo"},{"b":"bar"},{"c":"baz"}]'::json->2`|`{"c":"baz"}`|
|`->`|`text`|Get the JSON object field by key.|`'{"a": {"b":"foo"}}'::json->'a'`|`{"b":"foo"}`|
|`->>`|`int`|Get the JSON array element as `text`.|`'[1,2,3]'::json->>2`|`3`|
|`->>`|`text`|Get the JSON object field as `text`.|`'{"a":1,"b":2}'::json->>'b'`|`2`|
|`#>`|`text[]`|Get the JSON object at specified path.|`'{"a": {"b":{"c": "foo"}}}'::json#>'{a,b}`'|`{"c": "foo"}`|
|`#>>`|`text[]`|Get the JSON object at specified path as `text`.|`'{"a":[1,2,3],"b":[4,5,6]}'::json#>>'{a,2}'`|`3`|

**Note:** There are parallel variants of these operators for both the `json` and `jsonb` data types. The field, element, and path extraction operators return the same data type as their left-hand input \(either `json` or `jsonb`\), except for those specified as returning `text`, which coerce the value to `text`. The field, element, and path extraction operators return `NULL`, rather than failing, if the JSON input does not have the right structure to match the request; for example if no such element exists.

Operators that require the `jsonb` data type as the left operand are described in the following table. Many of these operators can be indexed by `jsonb` operator classes. For a full description of `jsonb` containment and existence semantics, see [jsonb Containment and Existence](../admin_guide/query/topics/json-data.html#topic_isx_2tw_mq)in the *Greenplum Database Administrator Guide*. For information about how these operators can be used to effectively index `jsonb` data, see [jsonb Indexing](../admin_guide/query/topics/json-data.html#topic_aqt_1tw_mq)in the *Greenplum Database Administrator Guide*.

|Operator|Right Operand Type|Description|Example|
|--------|------------------|-----------|-------|
|`@>`|`jsonb`|Does the left JSON value contain within it the right value?|`'{"a":1, "b":2}'::jsonb @> '{"b":2}'::jsonb`|
|`<@`|`jsonb`|Is the left JSON value contained within the right value?|`'{"b":2}'::jsonb <@ '{"a":1, "b":2}'::jsonb`|
|`?`|`text`|Does the key/element string exist within the JSON value?|`'{"a":1, "b":2}'::jsonb ? 'b'`|
|`?|`|`text[]`|Do any of these key/element strings exist?|`'{"a":1, "b":2, "c":3}'::jsonb ?| array['b', 'c']`|
|`?&`|`text[]`|Do all of these key/element strings exist?|`'["a", "b"]'::jsonb ?& array['a', 'b']`|

The standard comparison operators in the following table are available only for the `jsonb` data type, not for the `json` data type. They follow the ordering rules for B-tree operations described in [jsonb Indexing](../admin_guide/query/topics/json-data.html#topic_aqt_1tw_mq)in the *Greenplum Database Administrator Guide*.

|Operator|Description|
|--------|-----------|
|`<`|less than|
|`>`|greater than|
|`<=`|less than or equal to|
|`>=`|greater than or equal to|
|`=`|equal|
|`<>` or `!=`|not equal|

**Note:** The `!=` operator is converted to `<>` in the parser stage. It is not possible to implement `!=` and `<>` operators that do different things.

### <a id="topic_u4s_wnw_2z"></a>JSON Creation Functions 

This table describes the functions that create `json` data type values. \(Currently, there are no equivalent functions for `jsonb`, but you can cast the result of one of these functions to `jsonb`.\)

|Function|Description|Example|Example Result|
|--------|-----------|-------|--------------|
|`to_json(anyelement)`|Returns the value as a JSON object. Arrays and composites are processed recursively and are converted to arrays and objects. If the input contains a cast from the type to `json`, the cast function is used to perform the conversion; otherwise, a JSON scalar value is produced. For any scalar type other than a number, a Boolean, or a null value, the text representation will be used, properly quoted and escaped so that it is a valid JSON string.|`to_json('Fred said "Hi."'::text)`|`"Fred said \"Hi.\""`|
|`array_to_json(anyarray [, pretty_bool])`|Returns the array as a JSON array. A multidimensional array becomes a JSON array of arrays. Line feeds will be added between dimension-1 elements if `pretty_bool` is true.

|`array_to_json('{{1,5},{99,100}}'::int[])`|`[[1,5],[99,100]]`|
|`row_to_json(record [, pretty_bool])`|Returns the row as a JSON object. Line feeds will be added between level-1 elements if `pretty_bool` is true.

|`row_to_json(row(1,'foo'))`|`{"f1":1,"f2":"foo"}`|
|`json_build_array(VARIADIC "any"`\)|Builds a possibly-heterogeneously-typed JSON array out of a `VARIADIC` argument list.|`json_build_array(1,2,'3',4,5)`|`[1, 2, "3", 4, 5]`|
|`json_build_object(VARIADIC "any")`|Builds a JSON object out of a `VARIADIC` argument list. The argument list is taken in order and converted to a set of key/value pairs.|`json_build_object('foo',1,'bar',2)`|`{"foo": 1, "bar": 2}`|
|`json_object(text[])`|Builds a JSON object out of a text array. The array must be either a one or a two dimensional array.The one dimensional array must have an even number of elements. The elements are taken as key/value pairs.

For a two dimensional array, each inner array must have exactly two elements, which are taken as a key/value pair.

|`json_object('{a, 1, b, "def", c, 3.5}')`

 `json_object('{{a, 1},{b, "def"},{c, 3.5}}')`

|`{"a": "1", "b": "def", "c": "3.5"}`|
|`json_object(keys text[], values text[])`|Builds a JSON object out of a text array. This form of `json_object` takes keys and values pairwise from two separate arrays. In all other respects it is identical to the one-argument form.|`json_object('{a, b}', '{1,2}')`|`{"a": "1", "b": "2"}`|

**Note:** `array_to_json` and `row_to_json` have the same behavior as `to_json` except for offering a pretty-printing option. The behavior described for `to_json` likewise applies to each individual value converted by the other JSON creation functions.

**Note:** The [hstore](modules/hstore.html) extension has a cast from `hstore` to `json`, so that `hstore` values converted via the JSON creation functions will be represented as JSON objects, not as primitive string values.

### <a id="topic_rvp_lk3_sfb"></a>JSON Aggregate Functions 

This table shows the functions aggregate records to an array of JSON objects and pairs of values to a JSON object

|Function|Argument Types|Return Type|Description|
|--------|--------------|-----------|-----------|
|`json_agg(record)`|`record`|`json`|Aggregates records as a JSON array of objects.|
|`json_object_agg(name, value)`|`("any", "any")`|`json`|Aggregates name/value pairs as a JSON object.|

### <a id="topic_z5d_snw_2z"></a>JSON Processing Functions 

This table shows the functions that are available for processing `json` and `jsonb` values.

Many of these processing functions and operators convert Unicode escapes in JSON strings to the appropriate single character. This is a not an issue if the input data type is `jsonb`, because the conversion was already done. However, for `json` data type input, this might result in an error being thrown. See [About JSON Data](../admin_guide/query/topics/json-data.html#topic_upc_tcs_fz)in the *Greenplum Database Administrator Guide*.

|Function|Return Type|Description|Example|Example Result|
|--------|-----------|-----------|-------|--------------|
|`json_array_length(json)` `jsonb_array_length(jsonb)`

|`int`|Returns the number of elements in the outermost JSON array.|`json_array_length('[1,2,3,{"f1":1,"f2":[5,6]},4]')`|`5`|
|`json_each(json)` `jsonb_each(jsonb)`

|`setof key text, value json` `setof key text, value jsonb`

|Expands the outermost JSON object into a set of key/value pairs.|`select * from json_each('{"a":"foo", "b":"bar"}')`|```
 key | value
-----+-------
 a   | "foo"
 b   | "bar"

```

|
|`json_each_text(json)` `jsonb_each_text(jsonb)`

|`setof key text, value text`|Expands the outermost JSON object into a set of key/value pairs. The returned values will be of type `text`.|`select * from json_each_text('{"a":"foo", "b":"bar"}')`|```
 key | value
-----+-------
 a   | foo
 b   | bar

```

|
|`json_extract_path(from_json json, VARIADIC path_elems text[])` `jsonb_extract_path(from_json jsonb, VARIADIC path_elems text[])`

|`json`

 `jsonb`

|Returns the JSON value pointed to by `path_elems` \(equivalent to `#>` operator\).|`json_extract_path('{"f2":{"f3":1},"f4":{"f5":99,"f6":"foo"}}','f4')`|`{"f5":99,"f6":"foo"}`|
|`json_extract_path_text(from_json json, VARIADIC path_elems text[])` `jsonb_extract_path_text(from_json jsonb, VARIADIC path_elems text[])`

|`text`|Returns the JSON value pointed to by `path_elems` as text. Equivalent to `#>>` operator.|`json_extract_path_text('{"f2":{"f3":1},"f4":{"f5":99,"f6":"foo"}}','f4', 'f6')`|`foo`|
|`json_object_keys(json)` `jsonb_object_keys(jsonb)`

|`setof text`|Returns set of keys in the outermost JSON object.|`json_object_keys('{"f1":"abc","f2":{"f3":"a", "f4":"b"}}')`|```
 json_object_keys
------------------
 f1
 f2

```

|
|`json_populate_record(base anyelement, from_json json)``jsonb_populate_record(base anyelement, from_json jsonb)`

|`anyelement`|Expands the object in `from_json` to a row whose columns match the record type defined by base. See [Note 1](#json_proc_1).|`select * from json_populate_record(null::myrowtype, '{"a":1,"b":2}')`|```
 a | b
---+---
 1 | 2

```

|
|`json_populate_recordset(base anyelement, from_json json)` `jsonb_populate_recordset(base anyelement, from_json jsonb)`

|`setof anyelement`|Expands the outermost array of objects in `from_json` to a set of rows whose columns match the record type defined by base. See [Note 1](#json_proc_1).|`select * from json_populate_recordset(null::myrowtype, '[{"a":1,"b":2},{"a":3,"b":4}]')`|```
 a | b
---+---
 1 | 2
 3 | 4

```

|
|`json_array_elements(json)` `jsonb_array_elements(jsonb`\)

|`setof json`

 `setof jsonb`

|Expands a JSON array to a set of JSON values.|`select * from json_array_elements('[1,true, [2,false]]')`|```
   value
-----------
 1
 true
 [2,false]

```

|
|`json_array_elements_text(json)` `jsonb_array_elements_text(jsonb)`

|`setof text`|Expands a JSON array to a set of `text` values.|`select * from json_array_elements_text('["foo", "bar"]')`|```
   value
-----------
 foo
 bar

```

|
|`json_typeof(json)``jsonb_typeof(jsonb)`

|`text`|Returns the type of the outermost JSON value as a text string. Possible types are `object`, `array`, `string`, `number`, `boolean`, and `null`. See [Note 2](#json_proc_2)|`json_typeof('-123.4')`|`number`|
|`json_to_record(json)``jsonb_to_record(jsonb)`

|`record`|Builds an arbitrary record from a JSON object. See [Note 1](#json_proc_1). As with all functions returning record, the caller must explicitly define the structure of the record with an `AS` clause.

|`select * from json_to_record('{"a":1,"b":[1,2,3],"c":"bar"}') as x(a int, b text, d text)`|```
 a |    b    | d
---+---------+---
 1 | [1,2,3] |

```

|
|`json_to_recordset(json)` `jsonb_to_recordset(jsonb)`

|`setof record`|Builds an arbitrary set of records from a JSON array of objects See [Note 1](#json_proc_1). As with all functions returning record, the caller must explicitly define the structure of the record with an `AS` clause.

|`select * from json_to_recordset('[{"a":1,"b":"foo"},{"a":"2","c":"bar"}]') as x(a int, b text);`|```
 a |  b
---+-----
 1 | foo
 2 |

```

|

**Note:**

1.  The examples for the functions `json_populate_record()`, `json_populate_recordset()`, `json_to_record()` and `json_to_recordset()` use constants. However, the typical use would be to reference a table in the `FROM` clause and use one of its `json` or `jsonb` columns as an argument to the function. The extracted key values can then be referenced in other parts of the query. For example the value can be referenced in `WHERE` clauses and target lists. Extracting multiple values in this way can improve performance over extracting them separately with per-key operators.

    JSON keys are matched to identical column names in the target row type. JSON type coercion for these functions might not result in desired values for some types. JSON fields that do not appear in the target row type will be omitted from the output, and target columns that do not match any JSON field will be `NULL`.

2.  The `json_typeof` function null return value of `null` should not be confused with a SQL `NULL`. While calling `json_typeof('null'::json)` will return `null`, calling `json_typeof(NULL::json)` will return a SQL `NULL`.

## <a id="topic30"></a>Window Functions 

The following are Greenplum Database built-in window functions. All window functions are *immutable*. For more information about window functions, see "Window Expressions" in the *Greenplum Database Administrator Guide*.

|Function|Return Type|Full Syntax|Description|
|--------|-----------|-----------|-----------|
|`cume_dist()`|`double precision`|`CUME_DIST() OVER ( [PARTITION BY` expr `] ORDER BY` expr `)`|Calculates the cumulative distribution of a value in a group of values. Rows with equal values always evaluate to the same cumulative distribution value.|
|`dense_rank()`|`bigint`|`DENSE_RANK () OVER ( [PARTITION BY` expr `] ORDER BY` expr `)`|Computes the rank of a row in an ordered group of rows without skipping rank values. Rows with equal values are given the same rank value.|
|`first_value(*expr*)`|same as input expr type|`FIRST_VALUE(` expr `) OVER ( [PARTITION BY` expr `] ORDER BY` expr `[ROWS|RANGE` frame\_expr `] )`|Returns the first value in an ordered set of values.|
|`lag(*expr* [,*offset*] [,*default*])`|same as input *expr* type|`LAG(` *expr* `[,` *offset* `] [,` *default* `]) OVER ( [PARTITION BY` *expr* `] ORDER BY` *expr* `)`|Provides access to more than one row of the same table without doing a self join. Given a series of rows returned from a query and a position of the cursor, `LAG` provides access to a row at a given physical offset prior to that position. The default `offset` is 1. *default* sets the value that is returned if the offset goes beyond the scope of the window. If *default* is not specified, the default value is null.|
|`last_value(*expr*`\)|same as input *expr* type|`LAST_VALUE(*expr*) OVER ( [PARTITION BY *expr*] ORDER BY *expr* [ROWS|RANGE *frame\_expr*] )`|Returns the last value in an ordered set of values.|
|``lead(*expr* [,*offset*] [,*default*])``|same as input *expr* type|`LEAD(*expr*[,*offset*] [,*expr**default*]) OVER ( [PARTITION BY *expr*] ORDER BY *expr* )`|Provides access to more than one row of the same table without doing a self join. Given a series of rows returned from a query and a position of the cursor, `lead` provides access to a row at a given physical offset after that position. If *offset* is not specified, the default offset is 1. *default* sets the value that is returned if the offset goes beyond the scope of the window. If *default* is not specified, the default value is null.|
|`ntile(*expr*)`|`bigint`|`NTILE(*expr*) OVER ( [PARTITION BY *expr*] ORDER BY *expr* )`|Divides an ordered data set into a number of buckets \(as defined by *expr*\) and assigns a bucket number to each row.|
|`percent_rank()`|`double precision`|`PERCENT_RANK () OVER ( [PARTITION BY *expr*] ORDER BY *expr*)`|Calculates the rank of a hypothetical row `R` minus 1, divided by 1 less than the number of rows being evaluated \(within a window partition\).|
|`rank()`|`bigint`|`RANK () OVER ( [PARTITION BY *expr*] ORDER BY *expr*)`|Calculates the rank of a row in an ordered group of values. Rows with equal values for the ranking criteria receive the same rank. The number of tied rows are added to the rank number to calculate the next rank value. Ranks may not be consecutive numbers in this case.|
|`row_number()`|`bigint`|`ROW_NUMBER () OVER ( [PARTITION BY *expr*] ORDER BY *expr*)`|Assigns a unique number to each row to which it is applied \(either each row in a window partition or each row of the query\).|

## <a id="topic31"></a>Advanced Aggregate Functions 

The following built-in advanced analytic functions are Greenplum extensions of the PostgreSQL database. Analytic functions are *immutable*.

**Note:** The Greenplum MADlib Extension for Analytics provides additional advanced functions to perform statistical analysis and machine learning with Greenplum Database data. See [MADlib Extension for Analytics](../analytics/madlib.html#topic1).

|Function|Return Type|Full Syntax|Description|
|--------|-----------|-----------|-----------|
|`MEDIAN (*expr*)`|`timestamp, timestamptz, interval, float`|`MEDIAN (*expression*)` *Example:*

 ```
SELECT department_id, MEDIAN(salary) 
FROM employees 
GROUP BY department_id; 
```

|Can take a two-dimensional array as input. Treats such arrays as matrices.|
|`PERCENTILE_CONT (*expr*) WITHIN GROUP (ORDER BY *expr* [DESC/ASC])`|`timestamp, timestamptz, interval, float`|`PERCENTILE_CONT(*percentage*) WITHIN GROUP (ORDER BY *expression*)` *Example:*

 ```
SELECT department_id,
PERCENTILE_CONT (0.5) WITHIN GROUP (ORDER BY salary DESC)
"Median_cont"; 
FROM employees GROUP BY department_id;
```

|Performs an inverse distribution function that assumes a continuous distribution model. It takes a percentile value and a sort specification and returns the same datatype as the numeric datatype of the argument. This returned value is a computed result after performing linear interpolation. Null are ignored in this calculation.|
|`PERCENTILE_DISC (*expr*) WITHIN GROUP (ORDER BY *expr* [DESC/ASC])`|`timestamp, timestamptz, interval, float`|`PERCENTILE_DISC(*percentage*) WITHIN GROUP (ORDER BY *expression*)` *Example:*

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
|`unnest (array[])`|set of `anyelement`|`unnest( array['one', 'row', 'per', 'item'])`|Transforms a one dimensional array into rows. Returns a set of `anyelement`, a polymorphic [pseudotype in PostgreSQL](https://www.postgresql.org/docs/9.4/datatype-pseudo.html).|

## <a id="topic_vpj_ss1_lfb"></a>Text Search Functions and Operators 

The following tables summarize the functions and operators that are provided for full text searching. See [Using Full Text Search](../admin_guide/textsearch/full-text-search.html) for a detailed explanation of Greenplum Database's text search facility.

|Operator|Description|Example|Result|
|--------|-----------|-------|------|
|`@@`|`tsvector` matches `tsquery` ?|`to_tsvector('fat cats ate rats') @@ to_tsquery('cat & rat')`|`t`|
|`@@@`|deprecated synonym for `@@`|`to_tsvector('fat cats ate rats') @@@ to_tsquery('cat & rat')`|`t`|
|`||`|concatenate`tsvector`s|`'a:1 b:2'::tsvector || 'c:1 d:2 b:3'::tsvector`|`'a':1 'b':2,5 'c':3 'd':4`|
|`&&`|AND `tsquery`s together|`'fat | rat'::tsquery && 'cat'::tsquery`|`( 'fat' | 'rat' ) & 'cat'`|
|`||`|OR `tsquery`s together|`'fat | rat'::tsquery || 'cat'::tsquery`|`( 'fat' | 'rat' ) | 'cat'`|
|`!!`|negate a`tsquery`|`!! 'cat'::tsquery`|`!'cat'`|
|`@>`|`tsquery` contains another ?|`'cat'::tsquery @> 'cat & rat'::tsquery`|`f`|
|`<@`|`tsquery` is contained in ?|`'cat'::tsquery <@ 'cat & rat'::tsquery`|`t`|

**Note:** The `tsquery` containment operators consider only the lexemes listed in the two queries, ignoring the combining operators.

In addition to the operators shown in the table, the ordinary B-tree comparison operators \(=, <, etc\) are defined for types `tsvector` and `tsquery`. These are not very useful for text searching but allow, for example, unique indexes to be built on columns of these types.

|Function|Return Type|Description|Example|Result|
|--------|-----------|-----------|-------|------|
|`get_current_ts_config()`|regconfig|get default text search configuration|get\_current\_ts\_config\(\)|english|
|`length(tsvector)`|integer|number of lexemes in tsvector|length\('fat:2,4 cat:3 rat:5A'::tsvector\)|3|
|`numnode(tsquery)`|integer|number of lexemes plus operators in tsquery|numnode\('\(fat & rat\) \| cat'::tsquery\)|5|
|`plainto_tsquery([ config regconfig , ] querytext)`|tsquery|produce tsquery ignoring punctuation|plainto\_tsquery\('english', 'The Fat Rats'\)|'fat' & 'rat'|
|`querytree(query tsquery)`|text|get indexable part of a tsquery|querytree\('foo & ! bar'::tsquery\)|'foo'|
|`setweight(tsvector, "char")`|tsvector|assign weight to each element of tsvector|setweight\('fat:2,4 cat:3 rat:5B'::tsvector, 'A'\)|'cat':3A 'fat':2A,4A 'rat':5A|
|`strip(tsvector)`|tsvector|remove positions and weights from tsvector|strip\('fat:2,4 cat:3 rat:5A'::tsvector\)|'cat' 'fat' 'rat'|
|`to_tsquery([ config regconfig , ] query text)`|tsquery|normalize words and convert to tsquery|to\_tsquery\('english', 'The & Fat & Rats'\)|'fat' & 'rat'|
|`to_tsvector([ config regconfig , ] documenttext)`|tsvector|reduce document text to tsvector|to\_tsvector\('english', 'The Fat Rats'\)|'fat':2 'rat':3|
|`ts_headline([ config regconfig, ] documenttext, query tsquery [, options text ])`|text|display a query match|ts\_headline\('x y z', 'z'::tsquery\)|x y <b\>z</b\>|
|`ts_rank([ weights float4[], ] vector tsvector,query tsquery [, normalization integer ])`|float4|rank document for query|ts\_rank\(textsearch, query\)|0.818|
|`ts_rank_cd([ weights float4[], ] vectortsvector, query tsquery [, normalizationinteger ])`|float4|rank document for query using cover density|ts\_rank\_cd\('\{0.1, 0.2, 0.4, 1.0\}', textsearch, query\)|2.01317|
|`ts_rewrite(query tsquery, target tsquery,substitute tsquery)`|tsquery|replace target with substitute within query|ts\_rewrite\('a & b'::tsquery, 'a'::tsquery, 'foo\|bar'::tsquery\)|'b' & \( 'foo' \| 'bar' \)|
|`ts_rewrite(query tsquery, select text)`|tsquery|replace using targets and substitutes from a SELECTcommand|SELECT ts\_rewrite\('a & b'::tsquery, 'SELECT t,s FROM aliases'\)|'b' & \( 'foo' \| 'bar' \)|
|`tsvector_update_trigger()`|trigger|trigger function for automatic tsvector column update|CREATE TRIGGER ... tsvector\_update\_trigger\(tsvcol, 'pg\_catalog.swedish', title, body\)| |
|`tsvector_update_trigger_column()`|trigger|trigger function for automatic tsvector column update|CREATE TRIGGER ... tsvector\_update\_trigger\_column\(tsvcol, configcol, title, body\)| |

**Note:** All the text search functions that accept an optional `regconfig` argument will use the configuration specified by [default\_text\_search\_config](config_params/guc-list.html) when that argument is omitted.

The functions in the following table are listed separately because they are not usually used in everyday text searching operations. They are helpful for development and debugging of new text search configurations.

|Function|Return Type|Description|Example|Result|
|--------|-----------|-----------|-------|------|
|`ts_debug([ *config* regconfig, ] *document* text, OUT *alias* text, OUT *description* text, OUT *token* text, OUT *dictionaries* regdictionary[], OUT *dictionary* regdictionary, OUT *lexemes* text[])`|`setof record`|test a configuration|`ts_debug('english', 'The Brightest supernovaes')`|`(asciiword,"Word, all ASCII",The,{english_stem},english_stem,{}) ...`|
|`ts_lexize(*dict* regdictionary, *token* text)`|`text[]`|test a dictionary|`ts_lexize('english_stem', 'stars')`|`{star}`|
|`ts_parse(*parser\_name* text, *document* text, OUT *tokid* integer, OUT *token* text)`|`setof record`|test a parser|`ts_parse('default', 'foo - bar')`|\(``1,foo) ...``|
|`ts_parse(*parser\_oid* oid, *document* text, OUT *tokid* integer, OUT *token* text)`|`setof record`|test a parser|`ts_parse(3722, 'foo - bar')`|`(1,foo) ...`|
|`ts_token_type(*parser\_name* text, OUT *tokid* integer, OUT *alias* text, OUT description text)`|`setof record`|get token types defined by parser|`ts_token_type('default')`|`(1,asciiword,"Word, all ASCII") ...`|
|`ts_token_type(*parser\_oid* oid, OUT *tokid* integer, OUT *alias* text, OUT *description* text)`|`setof record`|get token types defined by parser|`ts_token_type(3722)`|`(1,asciiword,"Word, all ASCII") ...`|
|`ts_stat(*sqlquery* text, [ *weights* text, ] OUT *word* text, OUT *ndocinteger*, OUT *nentry* integer)`|`setof record`|get statistics of a tsvectorcolumn|`ts_stat('SELECT vector from apod')`|`(foo,10,15) ...`|

## <a id="functions-range"></a>Range Functions and Operators 

See [Range Types](datatype-range.html) for an overview of range types.

The following table shows the operators available for range types.

|Operator|Description|Example|Result|
|--------|-----------|-------|------|
|`=`|equal|`int4range(1,5) = '[1,4]'::int4range`|`t`|
|`<>`|not equal|`numrange(1.1,2.2) <> numrange(1.1,2.3)`|`t`|
|`<`|less than|`int4range(1,10) < int4range(2,3)`|`t`|
|`>`|greater than|`int4range(1,10) > int4range(1,5)`|`t`|
|`<=`|less than or equal|`numrange(1.1,2.2) <= numrange(1.1,2.2)`|`t`|
|`>=`|greater than or equal|`numrange(1.1,2.2) >= numrange(1.1,2.0)`|`t`|
|`@>`|contains range|`int4range(2,4) @> int4range(2,3)`|`t`|
|`@>`|contains element|`'[2011-01-01,2011-03-01)'::tsrange @> '2011-01-10'::timestamp`|`t`|
|`<@`|range is contained by|`int4range(2,4) <@ int4range(1,7)`|`t`|
|`<@`|element is contained by|`42 <@ int4range(1,7)`|`f`|
|`&&`|overlap \(have points in common\)|`int8range(3,7) && int8range(4,12)`|`t`|
|`<<`|strictly left of|`int8range(1,10) << int8range(100,110)`|`t`|
|`>>`|strictly right of|`int8range(50,60) >> int8range(20,30)`|`t`|
|`&<`|does not extend to the right of|`int8range(1,20) &< int8range(18,20)`|`t`|
|`&>`|does not extend to the left of|`int8range(7,20) &> int8range(5,10)`|`t`|
|`-|-`|is adjacent to|`numrange(1.1,2.2) -|- numrange(2.2,3.3)`|`t`|
|`+`|union|`numrange(5,15) + numrange(10,20)`|`[5,20)`|
|`*`|intersection|`int8range(5,15) * int8range(10,20)`|`[10,15)`|
|`-`|difference|`int8range(5,15) - int8range(10,20)`|`[5,10)`|

The simple comparison operators `<`, `>`, `<=`, and `>=` compare the lower bounds first, and only if those are equal, compare the upper bounds. These comparisons are not usually very useful for ranges, but are provided to allow B-tree indexes to be constructed on ranges.

The left-of/right-of/adjacent operators always return false when an empty range is involved; that is, an empty range is not considered to be either before or after any other range.

The union and difference operators will fail if the resulting range would need to contain two disjoint sub-ranges, as such a range cannot be represented.

The following table shows the functions available for use with range types.

|Function|Return Type|Description|Example|Result|
|--------|-----------|-----------|-------|------|
|``lower`(`anyrange`)`|range's element type|lower bound of range|`lower(numrange(1.1,2.2))`|`1.1`|
|``upper`(`anyrange`)`|range's element type|upper bound of range|`upper(numrange(1.1,2.2))`|`2.2`|
|``isempty`(`anyrange`)`|`boolean`|is the range empty?|`isempty(numrange(1.1,2.2))`|`false`|
|``lower_inc`(`anyrange`)`|`boolean`|is the lower bound inclusive?|`lower_inc(numrange(1.1,2.2))`|`true`|
|``upper_inc`(`anyrange`)`|`boolean`|is the upper bound inclusive?|`upper_inc(numrange(1.1,2.2))`|`false`|
|``lower_inf`(`anyrange`)`|`boolean`|is the lower bound infinite?|`lower_inf('(,)'::daterange)`|`true`|
|``upper_inf`(`anyrange`)`|`boolean`|is the upper bound infinite?|`upper_inf('(,)'::daterange)`|`true`|
|``range_merge`(`anyrange`, `anyrange`)`|`anyrange`|the smallest range which includes both of the given ranges|`range_merge('[1,2)'::int4range, '[3,4)'::int4range)`|`[1,4)`|

The `lower` and `upper` functions return null if the range is empty or the requested bound is infinite. The `lower_inc`, `upper_inc`, `lower_inf`, and `upper_inf` functions all return false for an empty range.

