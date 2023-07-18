# Summary of Built-in Functions 

Greenplum Database supports built-in functions and operators including analytic functions and window functions that can be used in window expressions. For information about using built-in Greenplum Database functions see, "Using Functions and Operators" in the *Greenplum Database Administrator Guide*.

-   [Greenplum Database Function Types](#topic27)
-   [Built-in Functions and Operators](#topic29)
-   [JSON Functions and Operators](#topic_gn4_x3w_mq)
-   [Window Functions](#topic30)
-   [Advanced Aggregate Functions](#topic31)
-   [Text Search Functions and Operators](#topic_vpj_ss1_lfb)
-   [Range Functions and Operators](#functions-range)

**Parent topic:** [Greenplum Database Reference Guide](ref_guide.html)

## <a id="topic27"></a>Greenplum Database Function Types 

Greenplum Database evaluates functions and operators used in SQL expressions. Some functions and operators are only allowed to run on the coordinator since they could lead to inconsistencies in Greenplum Database segment instances. This table describes the Greenplum Database Function Types.

|Function Type|Greenplum Support|Description|Comments|
|-------------|-----------------|-----------|--------|
|IMMUTABLE|Yes|Relies only on information directly in its argument list. Given the same argument values, always returns the same result.| |
|STABLE|Yes, in most cases|Within a single table scan, returns the same result for same argument values, but results change across SQL statements.|Results depend on database lookups or parameter values. `current_timestamp` family of functions is `STABLE`; values do not change within an execution.|
|VOLATILE|Restricted|Function values can change within a single table scan. For example: `random()`, `timeofday()`.|Any function with side effects is volatile, even if its result is predictable. For example: `setval()`.|

In Greenplum Database, data is divided up across segments — each segment is a distinct PostgreSQL database. To prevent inconsistent or unexpected results, do not run functions classified as `VOLATILE` at the segment level if they contain SQL commands or modify the database in any way. For example, functions such as `setval()` are not allowed to run on distributed data in Greenplum Database because they can cause inconsistent data between segment instances.

To ensure data consistency, you can safely use `VOLATILE` and `STABLE` functions in statements that are evaluated on and run from the coordinator. For example, the following statements run on the coordinator \(statements without a `FROM` clause\):

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

The following table lists the categories of built-in functions and operators supported by PostgreSQL. All functions and operators are supported in Greenplum Database as in PostgreSQL with the exception of `STABLE` and `VOLATILE` functions, which are subject to the restrictions noted in [Greenplum Database Function Types](#topic27). See the [Functions and Operators](https://www.postgresql.org/docs/12/functions.html) section of the PostgreSQL documentation for more information about these built-in functions and operators.

|Operator/Function Category|VOLATILE Functions|STABLE Functions|Restrictions|
|--------------------------|------------------|----------------|------------|
|[Logical Operators](https://www.postgresql.org/docs/12/functions-logical.html)| | | |
|[Comparison Operators](https://www.postgresql.org/docs/12/functions-comparison.html)| | | |
|[Mathematical Functions and Operators](https://www.postgresql.org/docs/12/functions-math.html)|random<br/><br/>setseed| | |
|[String Functions and Operators](https://www.postgresql.org/docs/12/functions-string.html)|*All built-in conversion functions*|convert<br/><br/>pg\_client\_encoding| |
|[Binary String Functions and Operators](https://www.postgresql.org/docs/12/functions-binarystring.html)| | | |
|[Bit String Functions and Operators](https://www.postgresql.org/docs/12/functions-bitstring.html)| | | |
|[Pattern Matching](https://www.postgresql.org/docs/12/functions-matching.html)| | | |
|[Data Type Formatting Functions](https://www.postgresql.org/docs/12/functions-formatting.html)| |to\_char<br/><br/>to\_timestamp| |
|[Date/Time Functions and Operators](https://www.postgresql.org/docs/12/functions-datetime.html)|timeofday|age<br/><br/>current\_date<br/><br/>current\_time<br/><br/>current\_timestamp<br/><br/>localtime<br/><br/>localtimestamp<br/><br/>now| |
|[Enum Support Functions](https://www.postgresql.org/docs/12/functions-enum.html)| | | |
|[Geometric Functions and Operators](https://www.postgresql.org/docs/12/functions-geometry.html)| | | |
|[Network Address Functions and Operators](https://www.postgresql.org/docs/12/functions-net.html)| | | |
|[Sequence Manipulation Functions](https://www.postgresql.org/docs/12/functions-sequence.html)|nextval\(\)<br/><br/>setval\(\)| | |
|[Conditional Expressions](https://www.postgresql.org/docs/12/functions-conditional.html)| | | |
|[Array Functions and Operators](https://www.postgresql.org/docs/12/functions-array.html)| |*All array functions*| |
|[Aggregate Functions](https://www.postgresql.org/docs/12/functions-aggregate.html)| | | |
|[Subquery Expressions](https://www.postgresql.org/docs/12/functions-subquery.html)| | | |
|[Row and Array Comparisons](https://www.postgresql.org/docs/12/functions-comparisons.html)| | | |
|[Set Returning Functions](https://www.postgresql.org/docs/12/functions-srf.html)|generate\_series| | |
|[System Information Functions](https://www.postgresql.org/docs/12/functions-info.html)| |*All session information functions*<br/><br/>*All access privilege inquiry functions*<br/><br/>*All schema visibility inquiry functions*<br/><br/>*All system catalog information functions*<br/><br/>*All comment information functions*<br/><br/>*All transaction ids and snapshots*| |
|[System Administration Functions](https://www.postgresql.org/docs/12/functions-admin.html)|set\_config<br/><br/>pg\_cancel\_backend<br/><br/>pg\_reload\_conf<br/><br/>pg\_rotate\_logfile<br/><br/>pg\_start\_backup<br/><br/>pg\_stop\_backup<br/><br/>pg\_size\_pretty<br/><br/>pg\_ls\_dir<br/><br/>pg\_read\_file<br/><br/>pg\_stat\_file<br/><br/>|current\_setting<br/><br/>*All database object size functions*|> **Note** The function `pg_column_size` displays bytes required to store the value, possibly with TOAST compression.|
|[XML Functions](https://www.postgresql.org/docs/12/functions-xml.html) and function-like expressions| |cursor\_to\_xml\(cursor refcursor, count int, nulls boolean, tableforest boolean, targetns text\)<br/><br/> cursor\_to\_xmlschema\(cursor refcursor, nulls boolean, tableforest boolean, targetns text\)<br/><br/>database\_to\_xml\(nulls boolean, tableforest boolean, targetns text\)<br/><br/>database\_to\_xmlschema\(nulls boolean, tableforest boolean, targetns text\)<br/><br/>database\_to\_xml\_and\_xmlschema\(nulls boolean, tableforest boolean, targetns text\)<br/><br/>query\_to\_xml\(query text, nulls boolean, tableforest boolean, targetns text\)<br/><br/>query\_to\_xmlschema\(query text, nulls boolean, tableforest boolean, targetns text\)<br/><br/>query\_to\_xml\_and\_xmlschema\(query text, nulls boolean, tableforest boolean, targetns text\)<br/><br/> schema\_to\_xml\(schema name, nulls boolean, tableforest boolean, targetns text\)<br/><br/>schema\_to\_xmlschema\(schema name, nulls boolean, tableforest boolean, targetns text\)<br/><br/>schema\_to\_xml\_and\_xmlschema\(schema name, nulls boolean, tableforest boolean, targetns text\)<br/><br/>table\_to\_xml\(tbl regclass, nulls boolean, tableforest boolean, targetns text\)<br/><br/>table\_to\_xmlschema\(tbl regclass, nulls boolean, tableforest boolean, targetns text\)<br/><br/>table\_to\_xml\_and\_xmlschema\(tbl regclass, nulls boolean, tableforest boolean, targetns text\)<br/><br/>xmlagg\(xml\)<br/><br/>xmlconcat\(xml\[, ...\]\)<br/><br/>xmlelement\(name name \[, xmlattributes\(value \[AS attname\] \[, ... \]\)\] \[, content, ...\]\)<br/><br/>xmlexists\(text, xml\)<br/><br/>xmlforest\(content \[AS name\] \[, ...\]\)<br/><br/>xml\_is\_well\_formed\(text\)<br/><br/>xml\_is\_well\_formed\_document\(text\)<br/><br/>xml\_is\_well\_formed\_content\(text\)<br/><br/>xmlparse \( \{ DOCUMENT \| CONTENT \} value\)<br/><br/>xpath\(text, xml\)<br/><br/>xpath\(text, xml, text\[\]\)<br/><br/>xpath\_exists\(text, xml\)<br/><br/>xpath\_exists\(text, xml, text\[\]\)<br/><br/>xmlpi\(name target \[, content\]\)<br/><br/>xmlroot\(xml, version text \| no value \[, standalone yes\|no\|no value\]\)<br/><br/>xmlserialize \( \{ DOCUMENT \| CONTENT \} value AS type \)<br/><br/>xml\(text\)<br/><br/>text\(xml\)<br/><br/>xmlcomment\(xml\)<br/><br/>xmlconcat2\(xml, xml\)<br/><br/>| |

## <a id="topic_gn4_x3w_mq"></a>JSON Functions and Operators 

This section describes:

- functions and operators for processing and creating JSON data
- the SQL/JSON path language

### <a id="topic_json_funcop"></a>Processing and Creating JSON Data

Greenplum Database includes built-in functions and operators that create and manipulate JSON data:

-   [JSON Operators](#topic_o5y_14w_2z)
-   [JSON Creation Functions](#topic_u4s_wnw_2z)
-   [JSON Aggregate Functions](#topic_rvp_lk3_sfb)
-   [JSON Processing Functions](#topic_z5d_snw_2z)


#### <a id="topic_o5y_14w_2z"></a>JSON Operators 

This table describes the operators that are available for use with the `json` and `jsonb` data types.

|Operator|Right Operand Type|Return Type|Description|Example|Example Result|
|--------|------------------|-----------|-----------|-------|--------------|
|`->`|`int`| `json` or `jsonb` | Get the JSON array element \(indexed from zero, negative integers count from the end\).|`'[{"a":"foo"},{"b":"bar"},{"c":"baz"}]'::json->2`|`{"c":"baz"}`|
|`->`|`text`| `json` or `jsonb` | Get the JSON object field by key.|`'{"a": {"b":"foo"}}'::json->'a'`|`{"b":"foo"}`|
|`->>`|`int`| `text` | Get the JSON array element as `text`.|`'[1,2,3]'::json->>2`|`3`|
|`->>`|`text`| `text` | Get the JSON object field as `text`.|`'{"a":1,"b":2}'::json->>'b'`|`2`|
|`#>`|`text[]`| `json` or `jsonb` | Get the JSON object at the specified path.|`'{"a": {"b":{"c": "foo"}}}'::json#>'{a,b}`'|`{"c": "foo"}`|
|`#>>`|`text[]`| `text` | Get the JSON object at the specified path as `text`.|`'{"a":[1,2,3],"b":[4,5,6]}'::json#>>'{a,2}'`|`3`|

> **Note** There are parallel variants of these operators for both the `json` and `jsonb` data types. The field/element/path extraction operators return the same data type as their left-hand input \(either `json` or `jsonb`\), except for those specified as returning `text`, which coerce the value to `text`. The field/element/path extraction operators return `NULL`, rather than failing, if the JSON input does not have the right structure to match the request; for example if no such element exists. The field/element/path extraction operators that accept integer JSON array subscripts all support negative subscripting from the end of arrays.

These standard comparison operators are available for `jsonb`, but not for `json.` They follow the ordering rules for B-tree operations outlined at [jsonb Indexing](../admin_guide/query/topics/json-data.html#topic_aqt_1tw_mq).

|Operator|Description|
|--------|-----------|
|`<`|less than|
|`>`|greater than|
|`<=`|less than or equal to|
|`>=`|greater than or equal to|
|`=`|equal|
|`<>` or `!=`|not equal|

> **Note** The `!=` operator is converted to `<>` in the parser stage. It is not possible to implement `!=` and `<>` operators that do different things.

Operators that require the `jsonb` data type as the left operand are described in the following table. Many of these operators can be indexed by `jsonb` operator classes. For a full description of `jsonb` containment and existence semantics, refer to [jsonb Containment and Existence](../admin_guide/query/topics/json-data.html#topic_isx_2tw_mq). [jsonb Indexing](../admin_guide/query/topics/json-data.html#topic_aqt_1tw_mq) describes how these operators can be used to effectively index `jsonb` data.

|Operator|Right Operand Type|Description|Example|
|--------|------------------|-----------|-------|
|`@>`|`jsonb`|Does the left JSON value contain the right JSON path/value entries at the top level?|`'{"a":1, "b":2}'::jsonb @> '{"b":2}'::jsonb`|
|`<@`|`jsonb`|Are the left JSON path/value enries contained at the top level within the right JSON value?|`'{"b":2}'::jsonb <@ '{"a":1, "b":2}'::jsonb`|
|`?`|`text`|Does the *string* exist as a top-level key within the JSON value?|`'{"a":1, "b":2}'::jsonb ? 'b'`|
|`?|`|`text[]`|Do any of these array *strings* exist as a top-level key?|`'{"a":1, "b":2, "c":3}'::jsonb ?| array['b', 'c']`|
|`?&`|`text[]`|Do all of these array *strings* exist as top-level keys?|`'["a", "b"]'::jsonb ?& array['a', 'b']`|
| `||` | `jsonb` | Concatenate two `jsonb` values into a new `jsonb` value. | `'["a", "b"]'::jsonb || '["c", "d"]'::jsonb` |
| `-` | `text` | Delete key/value pair or *string* elements from left operand. Key/value pairs are matched based on their key value.| `'{"a": "b"}'::jsonb - 'a'` |
| `-` | `text[]` | Delete multiple key/value pairs or *string* elements from left operand. Key/value pairs are matched based on their key value.| `'{"a": "b", "c": "d"}'::jsonb - '{a,c}'::text[]` |
| `-` | `integer` | Delete the array element with specified index (Negative integers count from the end). Throws an error if top level container is not an array.| `'["a", "b"]'::jsonb - 1` |
| `#-` | `text[]` | Delete the field or element with specified path (for JSON arrays, negative integers count from the end)	| `'["a", {"b":1}]'::jsonb #- '{1,b}'` |
| `@?` | `jsonpath` | Does JSON path return any item for the specified JSON value?| `'{"a":[1,2,3,4,5]}'::jsonb @? '$.a[*] ? (@ > 2)'` |
| `@@` | `jsonpath` | Returns the result of JSON path predicate check for the specified JSON value. Only the first item of the result is taken into account. If the result is not Boolean, then `null` is returned.| `'{"a":[1,2,3,4,5]}'::jsonb @@ '$.a[*] > 2'` |

> **Note**  The `||` operator concatenates two JSON objects by generating an object containing the union of their keys, taking the second object's value when there are duplicate keys. All other cases produce a JSON array: first, any non-array input is converted into a single-element array, and then the two arrays are concatenated. It does not operate recursively; only the top-level array or object structure is merged.

> **Note**  The `@?` and `@@` operators suppress the following errors: lacking object field or array element, unexpected JSON item type, and numeric errors. This behavior might be helpful while searching over JSON document collections of varying structure.


#### <a id="topic_u4s_wnw_2z"></a>JSON Creation Functions 

This table describes the functions that create `json` and `jsonb` data type values. \(There are no equivalent functions for `jsonb` for `row_to_json()` and `array_to_json()`. However, the `to_jsonb()` function supplies much the same functionality as these functions would.\)

|Function|Description|Example|Example Result|
|--------|-----------|-------|--------------|
|`to_json(anyelement)`<br>`to_jsonb(anyelement)`|Returns the value as a `json` or `jsonb` object. Arrays and composites are converted \(recursively\) to arrays and objects; otherwise, if the input contains a cast from the type to `json`, the cast function is used to perform the conversion; otherwise, a scalar value is produced. For any scalar type other than a number, a Boolean, or a null value, the text representation will be used, in such a fashion that it is a valid `json` or `jsonb` value.|`to_json('Fred said "Hi."'::text)`|`"Fred said \"Hi.\""`|
|`array_to_json(anyarray [, pretty_bool])`|Returns the array as a JSON array. A multidimensional array becomes a JSON array of arrays. Line feeds will be added between dimension-1 elements if `pretty_bool` is true.|`array_to_json('{{1,5},{99,100}}'::int[])`|`[[1,5],[99,100]]`|
|`row_to_json(record [, pretty_bool])`|Returns the row as a JSON object. Line feeds will be added between level-1 elements if `pretty_bool` is true.|`row_to_json(row(1,'foo'))`|`{"f1":1,"f2":"foo"}`|
|`json_build_array(VARIADIC "any")`<br>`jsonb_build_array(VARIADIC "any")`|Builds a possibly-heterogeneously-typed JSON array out of a `VARIADIC` argument list.|`json_build_array(1,2,'3',4,5)`|`[1, 2, "3", 4, 5]`|
|`json_build_object(VARIADIC "any")`<br>`jsonb_build_object(VARIADIC "any")`|Builds a JSON object out of a `VARIADIC` argument list. The argument list is taken in order and converted to a set of key/value pairs.|`json_build_object('foo',1,'bar',2)`|`{"foo": 1, "bar": 2}`|
|`json_object(text[])`<br>`jsonb_object(text[])`| Builds a JSON object out of a text array. The array must have either exactly one dimension with an even number of members, in which case they are taken as alternating key/value pairs, or two dimensions such that each inner array has exactly two elements, which are taken as a key/value pair. |`json_object('{a, 1, b, "def", c, 3.5}')`<br/><br/>`json_object('{{a, 1},{b, "def"},{c, 3.5}}')`|`{"a": "1", "b": "def", "c": "3.5"}`|
|`json_object(keys text[], values text[])`<br>`jsonb_object(keys text[], values text[])`|Builds a JSON object out of a text array. This form of `json_object` takes keys and values pairwise from two separate arrays. In all other respects it is identical to the one-argument form.|`json_object('{a, b}', '{1,2}')`|`{"a": "1", "b": "2"}`|

> **Note** `array_to_json()` and `row_to_json()` have the same behavior as `to_json()` except for offering a pretty-printing option. The behavior described for `to_json()` likewise applies to each individual value converted by the other JSON creation functions.

> **Note** The [hstore](modules/hstore.html) extension has a cast from `hstore` to `json`, so that `hstore` values converted via the JSON creation functions will be represented as JSON objects, not as primitive string values.

#### <a id="topic_rvp_lk3_sfb"></a>JSON Aggregate Functions 

This table shows the functions that aggregate records to an array of JSON objects and pairs of values to a JSON object

|Function|Argument Types|Return Type|Description|
|--------|--------------|-----------|-----------|
|`json_agg(record)`<br>`jsonb_agg(record)`|`record`|`json`|Aggregates records as a JSON array of objects.|
|`json_object_agg(name, value)`<br>`jsonb_object_agg(name, value)`|`("any", "any")`|`json`|Aggregates name/value pairs as a JSON object.|

#### <a id="topic_z5d_snw_2z"></a>JSON Processing Functions 

This table shows the functions that are available for processing `json` and `jsonb` values.

Many of these processing functions and operators convert Unicode escapes in JSON strings to the appropriate single character. This is a not an issue if the input data type is `jsonb`, because the conversion was already done. However, for `json` data type input, this might result in an error being thrown as described in [About JSON Data](../admin_guide/query/topics/json-data.html#topic_upc_tcs_fz).

<div class="tablenoborder"><table cellpadding="4" cellspacing="0" summary="" id="topic_z5d_snw_2z__table_wfc_y3w_mb" class="table" frame="border" border="1" rules="all"><caption><span class="tablecap"><span class="table--title-label">Table 8. </span>JSON Processing Functions</span></caption><colgroup><col style="width:20.224719101123597%" /><col style="width:18.726591760299627%" /><col style="width:18.913857677902623%" /><col style="width:23.220973782771537%" /><col style="width:18.913857677902623%" /></colgroup><thead class="thead" style="text-align:left;">
<tr class="row">
<th class="entry nocellnorowborder" style="vertical-align:top;" id="d233567e2033">Function</th>
<th class="entry nocellnorowborder" style="vertical-align:top;" id="d233567e2036">Return Type</th>
<th class="entry nocellnorowborder" style="vertical-align:top;" id="d233567e2039">Description</th>
<th class="entry nocellnorowborder" style="vertical-align:top;" id="d233567e2042">Example</th>
<th class="entry nocellnorowborder" style="vertical-align:top;" id="d233567e2045">Example Result</th>
</tr>
</thead>
<tbody class="tbody">
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2033 ">
<code class="ph codeph">json_array_length(json)</code>
<p class="p">
<code class="ph codeph">jsonb_array_length(jsonb)</code>
</p>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2036 "><code class="ph codeph">int</code>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2039 ">Returns the number of elements in the outermost JSON array.</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2042 "><code class="ph codeph">json_array_length('[1,2,3,{"f1":1,"f2":[5,6]},4]')</code>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2045 ">
<code class="ph codeph">5</code>
</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2033 "><code class="ph codeph">json_each(json)</code>
<p class="p"><code class="ph codeph">jsonb_each(jsonb)</code>
</p>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2036 "><code class="ph codeph">setof key text, value json</code>
<p class="p"><code class="ph codeph">setof key text, value jsonb</code>
</p>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2039 ">Expands the outermost JSON object into a set of key/value pairs.</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2042 "><code class="ph codeph">select * from json_each('{"a":"foo", "b":"bar"}')</code>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2045 ">
<pre class="pre"> key | value
-----+-------
 a   | "foo"
 b   | "bar"
</pre>
</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2033 "><code class="ph codeph">json_each_text(json)</code>
<p class="p"><code class="ph codeph">jsonb_each_text(jsonb)</code>
</p>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2036 "><code class="ph codeph">setof key text, value text</code>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2039 ">Expands the outermost JSON object into a set of key/value pairs. The returned
                  values will be of type <code class="ph codeph">text</code>.</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2042 "><code class="ph codeph">select * from json_each_text('{"a":"foo", "b":"bar"}')</code>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2045 ">
<pre class="pre"> key | value
-----+-------
 a   | foo
 b   | bar
</pre>
</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2033 "><code class="ph codeph">json_extract_path(from_json json, VARIADIC path_elems
                    text[])</code>
<p class="p"><code class="ph codeph">jsonb_extract_path(from_json jsonb, VARIADIC path_elems
                      text[])</code>
</p>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2036 ">
<p class="p"><code class="ph codeph">json</code>
</p>
<p class="p"><code class="ph codeph">jsonb</code>
</p>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2039 ">Returns the JSON value pointed to by <code class="ph codeph">path_elems</code> (equivalent
                  to <code class="ph codeph">#&gt;</code> operator).</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2042 "><code class="ph codeph">json_extract_path('{"f2":{"f3":1},"f4":{"f5":99,"f6":"foo"}}','f4')</code>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2045 ">
<code class="ph codeph">{"f5":99,"f6":"foo"}</code>
</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2033 "><code class="ph codeph">json_extract_path_text(from_json json, VARIADIC path_elems
                    text[])</code>
<p class="p"><code class="ph codeph">jsonb_extract_path_text(from_json jsonb, VARIADIC path_elems
                      text[])</code>
</p>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2036 "><code class="ph codeph">text</code>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2039 ">Returns the JSON value pointed to by <code class="ph codeph">path_elems</code> as text
                  (equivalent to <code class="ph codeph">#&gt;&gt;</code> operator).</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2042 "><code class="ph codeph">json_extract_path_text('{"f2":{"f3":1},"f4":{"f5":99,"f6":"foo"}}','f4',
                    'f6')</code>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2045 ">
<code class="ph codeph">foo</code>
</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2033 "><code class="ph codeph">json_object_keys(json)</code>
<p class="p"><code class="ph codeph">jsonb_object_keys(jsonb)</code>
</p>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2036 "><code class="ph codeph">setof text</code>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2039 ">Returns set of keys in the outermost JSON object.</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2042 "><code class="ph codeph">json_object_keys('{"f1":"abc","f2":{"f3":"a", "f4":"b"}}')</code>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2045 ">
<pre class="pre"> json_object_keys
------------------
 f1
 f2
</pre>
</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2033 "><code class="ph codeph">json_populate_record(base anyelement, from_json
                      json)</code><p class="p"><code class="ph codeph">jsonb_populate_record(base anyelement, from_json
                      jsonb)</code>
</p>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2036 "><code class="ph codeph">anyelement</code>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2039 ">Expands the object in <code class="ph codeph">from_json</code> to a row whose columns match
                  the record type defined by <code>base</code>. See the <a class="xref" href="#topic_z5d_snw_2z__json_proc_1">Notes</a>.</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2042 "><code class="ph codeph">select * from json_populate_record(null::myrowtype, '{"a": 1, 
                    "b": ["2", "a b"], "c": {"d": 4, "e": "a b c"}}')</code>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2045 ">
<pre class="pre"> a |   b       |      c
---+-----------+-------------
 1 | {2,"a b"} | (4,"a b c")
</pre>
</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2033 "><code class="ph codeph">json_populate_recordset(base anyelement, from_json json)</code>
<p class="p"><code class="ph codeph">jsonb_populate_recordset(base anyelement, from_json jsonb)</code>
</p>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2036 "><code class="ph codeph">setof anyelement</code>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2039 ">Expands the outermost array of objects in <code class="ph codeph">from_json</code> to a set
                  of rows whose columns match the record type defined by <code>base</code>. See the <a class="xref" href="#topic_z5d_snw_2z__json_proc_1">Notes</a>.</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2042 "><code class="ph codeph">select * from json_populate_recordset(null::myrowtype,
                    '[{"a":1,"b":2},{"a":3,"b":4}]')</code>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2045 ">
<pre class="pre"> a | b
---+---
 1 | 2
 3 | 4
</pre>
</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2033 "><code class="ph codeph">json_array_elements(json)</code>
<p class="p"><code class="ph codeph">jsonb_array_elements(jsonb</code>)</p>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2036 ">
<p class="p"><code class="ph codeph">setof json</code>
</p>
<p class="p"><code class="ph codeph">setof jsonb</code>
</p>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2039 ">Expands a JSON array to a set of JSON values.</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2042 "><code class="ph codeph">select * from json_array_elements('[1,true, [2,false]]')</code>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2045 ">
<pre class="pre">   value
-----------
 1
 true
 [2,false]
</pre>
</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2033 "><code class="ph codeph">json_array_elements_text(json)</code>
<p class="p"><code class="ph codeph">jsonb_array_elements_text(jsonb)</code>
</p>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2036 "><code class="ph codeph">setof text</code>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2039 ">Expands a JSON array to a set of <code class="ph codeph">text</code> values.</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2042 "><code class="ph codeph">select * from json_array_elements_text('["foo", "bar"]')</code>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2045 ">
<pre class="pre">   value
-----------
 foo
 bar
</pre>
</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2033 "><code class="ph codeph">json_typeof(json)</code><p class="p"><code class="ph codeph">jsonb_typeof(jsonb)</code>
</p>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2036 "><code class="ph codeph">text</code>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2039 ">Returns the type of the outermost JSON value as a text string. Possible types
                  are <code class="ph codeph">object</code>, <code class="ph codeph">array</code>, <code class="ph codeph">string</code>,
<code class="ph codeph">number</code>, <code class="ph codeph">boolean</code>, and <code class="ph codeph">null</code>.
                  </td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2042 "><code class="ph codeph">json_typeof('-123.4')</code>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2045 ">
<code class="ph codeph">number</code>
</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2033 "><code class="ph codeph">json_to_record(json)</code><p class="p"><code class="ph codeph">jsonb_to_record(jsonb)</code>
</p>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2036 "><code class="ph codeph">record</code>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2039 ">Builds an arbitrary record from a JSON object. See the <a class="xref" href="#topic_z5d_snw_2z__json_proc_1">Notes</a>. <p class="p">As with all
                    functions returning record, the caller must explicitly define the structure of
                    the record with an <code class="ph codeph">AS</code> clause.</p>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2042 "><code class="ph codeph">select * from json_to_record('{"a":1,"b":[1,2,3],
                    "c":[1,2,3],"e":"bar","r": {"a": 123, "b": "a b c"}}')
                    as x(a int, b text, c int[], d text, r myrowtype)</code>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2045 ">
<pre class="pre"> a |    b    |    c    | d |       r
---+---------+---------+---+---------------
 1 | [1,2,3] | {1,2,3} |   | (123,"a b c")
</pre>
</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2033 "><code class="ph codeph">json_to_recordset(json)</code>
<p class="p"><code class="ph codeph">jsonb_to_recordset(jsonb)</code>
</p>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2036 "><code class="ph codeph">setof record</code>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2039 ">Builds an arbitrary set of records from a JSON array of objects See the <a class="xref" href="#topic_z5d_snw_2z__json_proc_1">Notes</a>. <p class="p">As with all
                    functions returning record, the caller must explicitly define the structure of
                    the record with an <code class="ph codeph">AS</code> clause.</p>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e2042 "><code class="ph codeph">select * from
                    json_to_recordset('[{"a":1,"b":"foo"},{"a":"2","c":"bar"}]') as x(a int, b
                    text);</code>
</td>
<td class="entry nocellnorowborder="vertical-align:top;" headers="d233567e2045 ">
<pre class="pre"> a |  b
---+-----
 1 | foo
 2 |
</pre>
</td>
</tr>
<tr class="row">
              <td class="entry nocellnorowborder">
                <p><code class="literal">json_strip_nulls(from_json json)</code></p>
                <p><code class="literal">jsonb_strip_nulls(from_json jsonb)</code></p>
              </td>
              <td class="entry nocellnorowborder">
                <p><code class="type">json</code></p>
                <p><code class="type">jsonb</code></p>
              </td>
              <td class="entry nocellnorowborder">Returns <em class="replaceable"><code>from_json</code></em> with all object fields that have null values omitted. Other null values are untouched.</td>
              <td class="entry nocellnorowborder"><code class="literal">json_strip_nulls('[{"f1":1,"f2":null},2,null,3]')</code></td>
              <td class="entry nocellnorowborder"><code class="literal">[{"f1":1},2,null,3]</code></td>
            </tr>
            <tr>
              <td class="entry nocellnorowborder">
                <p><code class="literal">jsonb_set(target jsonb, path text[], new_value jsonb [<span class="optional">, create_missing boolean</span>])</code></p>
              </td>
              <td class="entry nocellnorowborder">
                <p><code class="type">jsonb</code></p>
              </td>
              <td class="entry nocellnorowborder">Returns <em class="replaceable"><code>target</code></em> with the section designated by <em class="replaceable"><code>path</code></em> replaced by <em class="replaceable"><code>new_value</code></em>, or with <em class="replaceable"><code>new_value</code></em> added if <em class="replaceable"><code>create_missing</code></em> is true (default is <code class="literal">true</code>) and the item designated by <em class="replaceable"><code>path</code></em> does not exist. As with the path oriented operators, negative integers that appear in <em class="replaceable"><code>path</code></em> count from the end of JSON arrays.</td>
              <td class="entry nocellnorowborder">
                <p><code class="literal">jsonb_set('[{"f1":1,"f2":null},2,null,3]', '{0,f1}','[2,3,4]', false)</code></p>
                <p><code class="literal">jsonb_set('[{"f1":1,"f2":null},2]', '{0,f3}','[2,3,4]')</code></p>
              </td>
              <td class="entry nocellnorowborder">
                <p><code class="literal">[{"f1":[2,3,4],"f2":null},2,null,3]</code></p>
                <p><code class="literal">[{"f1": 1, "f2": null, "f3": [2, 3, 4]}, 2]</code></p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry nocellnorowborder">
                <p><code class="literal">jsonb_insert(target jsonb, path text[], new_value jsonb [<span class="optional">, insert_after boolean</span>])</code></p>
              </td>
              <td class="entry nocellnorowborder">
                <p><code class="type">jsonb</code></p>
              </td>
              <td class="entry nocellnorowborder">Returns <em class="replaceable"><code>target</code></em> with <em class="replaceable"><code>new_value</code></em> inserted. If <em class="replaceable"><code>target</code></em> section designated by <em class="replaceable"><code>path</code></em> is in a JSONB array, <em class="replaceable"><code>new_value</code></em> will be inserted before target or after if <em class="replaceable"><code>insert_after</code></em> is true (default is <code class="literal">false</code>). If <em class="replaceable"><code>target</code></em> section designated by <em class="replaceable"><code>path</code></em> is in JSONB object, <em class="replaceable"><code>new_value</code></em> will be inserted only if <em class="replaceable"><code>target</code></em> does not exist. As with the path oriented operators, negative integers that appear in <em class="replaceable"><code>path</code></em> count from the end of JSON arrays.</td>
              <td class="entry nocellnorowborder">
                <p><code class="literal">jsonb_insert('{"a": [0,1,2]}', '{a, 1}', '"new_value"')</code></p>
                <p><code class="literal">jsonb_insert('{"a": [0,1,2]}', '{a, 1}', '"new_value"', true)</code></p>
              </td>
              <td class="entry nocellnorowborder">
                <p><code class="literal">{"a": [0, "new_value", 1, 2]}</code></p>
                <p><code class="literal">{"a": [0, 1, "new_value", 2]}</code></p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry nocellnorowborder">
                <p><code class="literal">jsonb_pretty(from_json jsonb)</code></p>
              </td>
              <td class="entry nocellnorowborder">
                <p><code class="type">text</code></p>
              </td>
              <td class="entry nocellnorowborder">Returns <em class="replaceable"><code>from_json</code></em> as indented JSON text.</td>
              <td class="entry nocellnorowborder"><code class="literal">jsonb_pretty('[{"f1":1,"f2":null},2,null,3]')</code></td>
              <td class="entry nocellnorowborder">
                <pre class="programlisting">
[
    {
        "f1": 1,
        "f2": null
    },
    2,
    null,
    3
]
</pre>
              </td>
            </tr>
            <tr class="row">
              <td class="entry nocellnorowborder">
                <p><code class="literal">jsonb_path_exists(target jsonb, path jsonpath [<span class="optional">, vars jsonb [<span class="optional">, silent bool</span>]</span>])</code></p>
              </td>
              <td class="entry nocellnorowborder"><code class="type">boolean</code></td>
              <td class="entry nocellnorowborder">Checks whether JSON path returns any item for the specified JSON value.</td>
              <td class="entry nocellnorowborder">
                <p><code class="literal">jsonb_path_exists('{"a":[1,2,3,4,5]}', '$.a[*] ? (@ &gt;= $min &amp;&amp; @ &lt;= $max)', '{"min":2,"max":4}')</code></p>
              </td>
              <td class="entry nocellnorowborder">
                <p><code class="literal">true</code></p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry nocellnorowborder">
                <p><code class="literal">jsonb_path_match(target jsonb, path jsonpath [<span class="optional">, vars jsonb [<span class="optional">, silent bool</span>]</span>])</code></p>
              </td>
              <td class="entry nocellnorowborder"><code class="type">boolean</code></td>
              <td class="entry nocellnorowborder">Returns the result of JSON path predicate check for the specified JSON value. Only the first item of the result is taken into account. If the result is not Boolean, then <code class="literal">null</code> is returned.</td>
              <td class="entry nocellnorowborder">
                <p><code class="literal">jsonb_path_match('{"a":[1,2,3,4,5]}', 'exists($.a[*] ? (@ &gt;= $min &amp;&amp; @ &lt;= $max))', '{"min":2,"max":4}')</code></p>
              </td>
              <td class="entry nocellnorowborder">
                <p><code class="literal">true</code></p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry nocellnorowborder">
                <p><code class="literal">jsonb_path_query(target jsonb, path jsonpath [<span class="optional">, vars jsonb [<span class="optional">, silent bool</span>]</span>])</code></p>
              </td>
              <td class="entry nocellnorowborder"><code class="type">setof jsonb</code></td>
              <td class="entry nocellnorowborder">Gets all JSON items returned by JSON path for the specified JSON value.</td>
              <td class="entry nocellnorowborder">
                <p><code class="literal">select * from jsonb_path_query('{"a":[1,2,3,4,5]}', '$.a[*] ? (@ &gt;= $min &amp;&amp; @ &lt;= $max)', '{"min":2,"max":4}');</code></p>
              </td>
              <td class="entry nocellnorowborder">
                <pre class="programlisting">
 jsonb_path_query
------------------
 2
 3
 4
</pre>
              </td>
            </tr>
            <tr class="row">
              <td class="entry nocellnorowborder">
                <p><code class="literal">jsonb_path_query_array(target jsonb, path jsonpath [<span class="optional">, vars jsonb [<span class="optional">, silent bool</span>]</span>])</code></p>
              </td>
              <td class="entry nocellnorowborder"><code class="type">jsonb</code></td>
              <td class="entry nocellnorowborder">Gets all JSON items returned by JSON path for the specified JSON value and wraps result into an array.</td>
              <td class="entry nocellnorowborder">
                <p><code class="literal">jsonb_path_query_array('{"a":[1,2,3,4,5]}', '$.a[*] ? (@ &gt;= $min &amp;&amp; @ &lt;= $max)', '{"min":2,"max":4}')</code></p>
              </td>
              <td class="entry nocellnorowborder">
                <p><code class="literal">[2, 3, 4]</code></p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry nocellnorowborder">
                <p><code class="literal">jsonb_path_query_first(target jsonb, path jsonpath [<span class="optional">, vars jsonb [<span class="optional">, silent bool</span>]</span>])</code></p>
              </td>
              <td class="entry nocellnorowborder"><code class="type">jsonb</code></td>
              <td class="entry nocellnorowborder">Gets the first JSON item returned by JSON path for the specified JSON value. Returns <code class="literal">NULL</code> on no results.</td>
              <td class="entry nocellnorowborder">
                <p><code class="literal">jsonb_path_query_first('{"a":[1,2,3,4,5]}', '$.a[*] ? (@ &gt;= $min &amp;&amp; @ &lt;= $max)', '{"min":2,"max":4}')</code></p>
              </td>
              <td class="entry nocellnorowborder">
                <p><code class="literal">2</code></p>
              </td>
            </tr>

</tbody>
</table>
</div>

**Notes:**

1. The functions `json[b]_populate_record()`, `json[b]_populate_recordset()`, `json[b]_to_record()` and `json[b]_to_recordset()` operate on a JSON object, or array of objects, and extract the values associated with keys whose names match column names of the output row type. Object fields that do not correspond to any output column name are ignored, and output columns that do not match any object field will be filled with nulls. To convert a JSON value to the SQL type of an output column, the following rules are applied in sequence:

    - A JSON null value is converted to a SQL null in all cases.
    - If the output column is of type `json` or `jsonb`, the JSON value is just reproduced exactly.
    - If the output column is a composite (row) type, and the JSON value is a JSON object, the fields of the object are converted to columns of the output row type by recursive application of these rules.
    - Likewise, if the output column is an array type and the JSON value is a JSON array, the elements of the JSON array are converted to elements of the output array by recursive application of these rules.
    - Otherwise, if the JSON value is a string literal, the contents of the string are fed to the input conversion function for the column's data type.
    - Otherwise, the ordinary text representation of the JSON value is fed to the input conversion function for the column's data type.

    While the examples for these functions use constants, the typical use would be to reference a table in the `FROM` clause and use one of its `json` or `jsonb` columns as an argument to the function. Extracted key values can then be referenced in other parts of the query, like `WHERE` clauses and target lists. Extracting multiple values in this way can improve performance over extracting them separately with per-key operators.

1.  All the items of the `path` parameter of `jsonb_set()` as well as `jsonb_insert()` except the last item must be present in the target. If `create_missing` is false, all items of the `path` parameter of `jsonb_set()` must be present. If these conditions are not met the target is returned unchanged.

    If the last path item is an object key, it will be created if it is absent and given the new value. If the last path item is an array index, if it is positive the item to set is found by counting from the left, and if negative by counting from the right - `-1` designates the rightmost element, and so on. If the item is out of the range -array_length .. array_length -1, and create_missing is true, the new value is added at the beginning of the array if the item is negative, and at the end of the array if it is positive.


1.  The `json_typeof` function's `null` return value should not be confused with a SQL NULL. While calling `json_typeof('null'::json)` will return `null`, calling `json_typeof(NULL::json)` will return a SQL NULL.


1.  If the argument to `json_strip_nulls()` contains duplicate field names in any object, the result could be semantically somewhat different, depending on the order in which they occur. This is not an issue for `jsonb_strip_nulls()` since `jsonb` values never have duplicate object field names.

1.  The `jsonb_path_exists()`, `jsonb_path_match()`, `jsonb_path_query()`, `jsonb_path_query_array()`, and `jsonb_path_query_first()` functions have optional `vars` and `silent` arguments.

    If the `vars` argument is specified, it provides an object containing named variables to be substituted into a `jsonpath` expression.

    If the `silent` argument is specified and has the `true` value, these functions suppress the same errors as the `@?` and `@@` operators.


### <a id="topic_jsonpath"></a>The SQL/JSON Path Language

SQL/JSON path expressions specify the items to be retrieved from the JSON data, similar to XPath expressions used for SQL access to XML. In Greenplum Database, path expressions are implemented as the `jsonpath` data type and can use any elements described in [jsonpath Type](../admin_guide/query/topics/json-data.html#topic_jsonpath).

JSON query functions and operators pass the provided path expression to the *path engine* for evaluation. If the expression matches the queried JSON data, the corresponding SQL/JSON item is returned. Path expressions are written in the SQL/JSON path language and can also include arithmetic expressions and functions. Query functions treat the provided expression as a text string, so it must be enclosed in single quotes.

A path expression consists of a sequence of elements allowed by the `jsonpath` data type. The path expression is evaluated from left to right, but you can use parentheses to change the order of operations. If the evaluation is successful, a sequence of SQL/JSON items \(*SQL/JSON sequence*\) is produced, and the evaluation result is returned to the JSON query function that completes the specified computation.

To refer to the JSON data to be queried \(the *context item*\), use the `$` sign in the path expression. It can be followed by one or more [accessor operators](../admin_guide/query/topics/json-data.html#topic_jsonpath), which go down the JSON structure level by level to retrieve the content of context item. Each operator that follows deals with the result of the previous evaluation step.

For example, suppose you have some JSON data from a GPS tracker that you would like to parse, such as:

```
{
  "track": {
    "segments": [
      {
        "location":   [ 47.763, 13.4034 ],
        "start time": "2018-10-14 10:05:14",
        "HR": 73
      },
      {
        "location":   [ 47.706, 13.2635 ],
        "start time": "2018-10-14 10:39:21",
        "HR": 135
      }
    ]
  }
}
```

To retrieve the available track segments, you need to use the `.<key>` accessor operator for all the preceding JSON objects:

```
'$.track.segments'
```

If the item to retrieve is an element of an array, you have to unnest this array using the `[*]` operator. For example, the following path will return location coordinates for all the available track segments:

```
'$.track.segments[*].location'
```

To return the coordinates of the first segment only, you can specify the corresponding subscript in the `[]` accessor operator. Note that the SQL/JSON arrays are 0-relative:

```
'$.track.segments[0].location'
```

The result of each path evaluation step can be processed by one or more `jsonpath` operators and methods listed in [SQL/JSON Path Operators and Methods](#topic_jsonpath_opsmeth) below. Each method name must be preceded by a dot. For example, you can get an array size:

```
'$.track.segments.size()'
```

For more examples of using `jsonpath` operators and methods within path expressions, see [SQL/JSON Path Operators and Methods](#topic_jsonpath_opsmeth) below.

When defining the path, you can also use one or more *filter expressions* that work similar to the `WHERE` clause in SQL. A filter expression begins with a question mark and provides a condition in parentheses:

```
? (condition)
```

Filter expressions must be specified right after the path evaluation step to which they are applied. The result of this step is filtered to include only those items that satisfy the provided condition. SQL/JSON defines three-valued logic, so the condition can be `true`, `false`, or `unknown`. The `unknown` value plays the same role as SQL `NULL` and can be tested for with the is `unknown` predicate. Further path evaluation steps use only those items for which filter expressions return `true`.

Functions and operators that can be used in filter expressions are listed in [jsonpath Filter Expression Elements](#topic_jsonpath_filtexp). The path evaluation result to be filtered is denoted by the `@` variable. To refer to a JSON element stored at a lower nesting level, add one or more accessor operators after `@`.

Suppose you would like to retrieve all heart rate values higher than 130. You can achieve this using the following expression:

```
'$.track.segments[*].HR ? (@ > 130)'
```

To get the start time of segments with such values instead, you have to filter out irrelevant segments before returning the start time, so the filter expression is applied to the previous step, and the path used in the condition is different:

```
'$.track.segments[*] ? (@.HR > 130)."start time"'
```

You can use several filter expressions on the same nesting level, if required. For example, the following expression selects all segments that contain locations with relevant coordinates and high heart rate values:

```
'$.track.segments[*] ? (@.location[1] < 13.4) ? (@.HR > 130)."start time"'
```

Using filter expressions at different nesting levels is also allowed. The following example first filters all segments by location, and then returns high heart rate values for these segments, if available:

```
'$.track.segments[*] ? (@.location[1] < 13.4).HR ? (@ > 130)'
```

You can also nest filter expressions within each other:

```
'$.track ? (exists(@.segments[*] ? (@.HR > 130))).segments.size()'
```
This expression returns the size of the track if it contains any segments with high heart rate values, or an empty sequence otherwise.

#### <a id="topic_jsonpath_deviation"></a>Deviations from Standard

Greenplum Database's implementation of SQL/JSON path language has the following deviations from the SQL/JSON standard:

- `.datetime()` item method is not implemented yet mainly because immutable `jsonpath` functions and operators cannot reference session timezone, which is used in some datetime operations. Datetime support will be added to `jsonpath` in future versions of Greenplum Database.
- A path expression can be a Boolean predicate, although the SQL/JSON standard allows predicates only in filters. This is necessary for implementation of the `@@` operator. For example, the following `jsonpath` expression is valid in Greenplum Database:

    ```
    '$.track.segments[*].HR < 70'
    ```
- There are minor differences in the interpretation of regular expression patterns used in `like_regex` filters as described in [Regular Expressions](#topic_jsonpath_regexp).

#### <a id="topic_jsonpath_strictlax"></a>Strict And Lax Modes

When you query JSON data, the path expression may not match the actual JSON data structure. An attempt to access a non-existent member of an object or element of an array results in a structural error. SQL/JSON path expressions have two modes of handling structural errors:

- lax (default) — the path engine implicitly adapts the queried data to the specified path. Any remaining structural errors are suppressed and converted to empty SQL/JSON sequences.

- strict — if a structural error occurs, an error is raised.

The lax mode facilitates matching of a JSON document structure and path expression if the JSON data does not conform to the expected schema. If an operand does not match the requirements of a particular operation, it can be automatically wrapped as an SQL/JSON array or unwrapped by converting its elements into an SQL/JSON sequence before performing this operation. Besides, comparison operators automatically unwrap their operands in the lax mode, so you can compare SQL/JSON arrays out-of-the-box. An array of size 1 is considered equal to its sole element. Automatic unwrapping is not performed only when:

- The path expression contains `type()` or `size()` methods that return the type and the number of elements in the array, respectively.

- The queried JSON data contain nested arrays. In this case, only the outermost array is unwrapped, while all the inner arrays remain unchanged. Thus, implicit unwrapping can only go one level down within each path evaluation step.

For example, when querying the GPS data listed above, you can abstract from the fact that it stores an array of segments when using the lax mode:

```
'lax $.track.segments.location'
```

In the strict mode, the specified path must exactly match the structure of the queried JSON document to return an SQL/JSON item, so using this path expression will cause an error. To get the same result as in the lax mode, you have to explicitly unwrap the segments array:

```
'strict $.track.segments[*].location'
```

The `.**` accessor can lead to surprising results when using the lax mode. For instance, the following query selects every HR value twice:

```
lax $.**.HR
```

This happens because the `.**` accessor selects both the segments array and each of its elements, while the `.HR` accessor automatically unwraps arrays when using the lax mode. To avoid surprising results, we recommend using the `.**` accessor only in the strict mode. The following query selects each HR value just once:

```
strict $.**.HR
```

#### <a id="topic_jsonpath_regexp"></a>Regular Expressions

SQL/JSON path expressions allow matching text to a regular expression with the `like_regex` filter. For example, the following SQL/JSON path query would case-insensitively match all strings in an array that starts with an English vowel:

```
'$[*] ? (@ like_regex "^[aeiou]" flag "i")'
```

The optional flag string may include one or more of the characters `i` for case-insensitive match, `m` to allow `^` and `$` to match at newlines, `s` to allow `.` to match a newline, and `q` to quote the whole pattern \(reducing the behavior to a simple substring match\).

The SQL/JSON standard borrows its definition for regular expressions from the `LIKE_REGEX` operator, which in turn uses the XQuery standard. Greenplum Database does not currently support the `LIKE_REGEX` operator. Therefore, the `like_regex` filter is implemented using the POSIX regular expression engine as described in [POSIX Regular Expressions](https://www.postgresql.org/docs/12/functions-matching.html#FUNCTIONS-POSIX-REGEXP). This leads to various minor discrepancies from standard SQL/JSON behavior which are catalogued in [Differences From XQuery (LIKE_REGEX)](https://www.postgresql.org/docs/12/functions-matching.html#POSIX-VS-XQUERY). Note, however, that the flag-letter incompatibilities described there do not apply to SQL/JSON, as it translates the XQuery flag letters to match what the POSIX engine expects.

Keep in mind that the pattern argument of `like_regex` is a JSON path string literal, written according to the rules given in [jsonpath Type](../admin_guide/query/topics/json-data.html#topic_jsonpath). This means in particular that any backslashes you want to use in the regular expression must be doubled. For example, to match string values of the root document that contain only digits:

```
$.* ? (@ like_regex "^\\d+$")
```

#### <a id="topic_jsonpath_opsmeth"></a>SQL/JSON Path Operators and Methods

The following table describes the operators and methods available in `jsonpath`:

<div>
<table class="table" summary="jsonpath Operators and Methods" border="1">
            <colgroup>
              <col />
              <col />
              <col />
              <col />
              <col />
            </colgroup>
            <thead>
              <tr class="row">
                <th class="entry nocellnorowborder">Operator/Method</th>
                <th class="entry nocellnorowborder">Description</th>
                <th class="entry nocellnorowborder">Example JSON</th>
                <th class="entry nocellnorowborder">Example Query</th>
                <th class="entry nocellnorowborder">Result</th>
              </tr>
            </thead>
            <tbody>
              <tr class="row">
                <td class="entry nocellnorowborder"><code class="literal">+</code> (unary)</td>
                <td class="entry nocellnorowborder">Plus operator that iterates over the SQL/JSON sequence</td>
                <td class="entry nocellnorowborder"><code class="literal">{"x": [2.85, -14.7, -9.4]}</code></td>
                <td class="entry nocellnorowborder"><code class="literal">+ $.x.floor()</code></td>
                <td class="entry nocellnorowborder"><code class="literal">2, -15, -10</code></td>
              </tr>
              <tr class="row">
                <td class="entry nocellnorowborder"><code class="literal">-</code> (unary)</td>
                <td class="entry nocellnorowborder">Minus operator that iterates over the SQL/JSON sequence</td>
                <td class="entry nocellnorowborder"><code class="literal">{"x": [2.85, -14.7, -9.4]}</code></td>
                <td class="entry nocellnorowborder"><code class="literal">- $.x.floor()</code></td>
                <td class="entry nocellnorowborder"><code class="literal">-2, 15, 10</code></td>
              </tr>
              <tr class="row">
                <td class="entry nocellnorowborder"><code class="literal">+</code> (binary)</td>
                <td class="entry nocellnorowborder">Addition</td>
                <td class="entry nocellnorowborder"><code class="literal">[2]</code></td>
                <td class="entry nocellnorowborder"><code class="literal">2 + $[0]</code></td>
                <td class="entry nocellnorowborder"><code class="literal">4</code></td>
              </tr>
              <tr class="row">
                <td class="entry nocellnorowborder"><code class="literal">-</code> (binary)</td>
                <td class="entry nocellnorowborder">Subtraction</td>
                <td class="entry nocellnorowborder"><code class="literal">[2]</code></td>
                <td class="entry nocellnorowborder"><code class="literal">4 - $[0]</code></td>
                <td class="entry nocellnorowborder"><code class="literal">2</code></td>
              </tr>
              <tr class="row">
                <td class="entry nocellnorowborder"><code class="literal">*</code></td>
                <td class="entry nocellnorowborder">Multiplication</td>
                <td class="entry nocellnorowborder"><code class="literal">[4]</code></td>
                <td class="entry nocellnorowborder"><code class="literal">2 * $[0]</code></td>
                <td class="entry nocellnorowborder"><code class="literal">8</code></td>
              </tr>
              <tr class="row">
                <td class="entry nocellnorowborder"><code class="literal">/</code></td>
                <td class="entry nocellnorowborder">Division</td>
                <td class="entry nocellnorowborder"><code class="literal">[8]</code></td>
                <td class="entry nocellnorowborder"><code class="literal">$[0] / 2</code></td>
                <td class="entry nocellnorowborder"><code class="literal">4</code></td>
              </tr>
              <tr class="row">
                <td class="entry nocellnorowborder"><code class="literal">%</code></td>
                <td class="entry nocellnorowborder">Modulus</td>
                <td class="entry nocellnorowborder"><code class="literal">[32]</code></td>
                <td class="entry nocellnorowborder"><code class="literal">$[0] % 10</code></td>
                <td class="entry nocellnorowborder"><code class="literal">2</code></td>
              </tr>
              <tr class="row">
                <td class="entry nocellnorowborder"><code class="literal">type()</code></td>
                <td class="entry nocellnorowborder">Type of the SQL/JSON item</td>
                <td class="entry nocellnorowborder"><code class="literal">[1, "2", {}]</code></td>
                <td class="entry nocellnorowborder"><code class="literal">$[*].type()</code></td>
                <td class="entry nocellnorowborder"><code class="literal">"number", "string", "object"</code></td>
              </tr>
              <tr class="row">
                <td class="entry nocellnorowborder"><code class="literal">size()</code></td>
                <td class="entry nocellnorowborder">Size of the SQL/JSON item</td>
                <td class="entry nocellnorowborder"><code class="literal">{"m": [11, 15]}</code></td>
                <td class="entry nocellnorowborder"><code class="literal">$.m.size()</code></td>
                <td class="entry nocellnorowborder"><code class="literal">2</code></td>
              </tr>
              <tr class="row">
                <td class="entry nocellnorowborder"><code class="literal">double()</code></td>
                <td class="entry nocellnorowborder">Approximate floating-point number converted from an SQL/JSON number or a string</td>
                <td class="entry nocellnorowborder"><code class="literal">{"len": "1.9"}</code></td>
                <td class="entry nocellnorowborder"><code class="literal">$.len.double() * 2</code></td>
                <td class="entry nocellnorowborder"><code class="literal">3.8</code></td>
              </tr>
              <tr class="row">
                <td class="entry nocellnorowborder"><code class="literal">ceiling()</code></td>
                <td class="entry nocellnorowborder">Nearest integer greater than or equal to the SQL/JSON number</td>
                <td class="entry nocellnorowborder"><code class="literal">{"h": 1.3}</code></td>
                <td class="entry nocellnorowborder"><code class="literal">$.h.ceiling()</code></td>
                <td class="entry nocellnorowborder"><code class="literal">2</code></td>
              </tr>
              <tr class="row">
                <td class="entry nocellnorowborder"><code class="literal">floor()</code></td>
                <td class="entry nocellnorowborder">Nearest integer less than or equal to the SQL/JSON number</td>
                <td class="entry nocellnorowborder"><code class="literal">{"h": 1.3}</code></td>
                <td class="entry nocellnorowborder"><code class="literal">$.h.floor()</code></td>
                <td class="entry nocellnorowborder"><code class="literal">1</code></td>
              </tr>
              <tr class="row">
                <td class="entry nocellnorowborder"><code class="literal">abs()</code></td>
                <td class="entry nocellnorowborder">Absolute value of the SQL/JSON number</td>
                <td class="entry nocellnorowborder"><code class="literal">{"z": -0.3}</code></td>
                <td class="entry nocellnorowborder"><code class="literal">$.z.abs()</code></td>
                <td class="entry nocellnorowborder"><code class="literal">0.3</code></td>
              </tr>
              <tr class="row">
                <td class="entry nocellnorowborder"><code class="literal">keyvalue()</code></td>
                <td class="entry nocellnorowborder">Sequence of object's key-value pairs represented as array of items containing three fields (<code class="literal">"key"</code>, <code class="literal">"value"</code>, and <code class="literal">"id"</code>). <code class="literal">"id"</code> is a unique identifier of the object key-value pair belongs to.</td>
                <td class="entry nocellnorowborder"><code class="literal">{"x": "20", "y": 32}</code></td>
                <td class="entry nocellnorowborder"><code class="literal">$.keyvalue()</code></td>
                <td class="entry nocellnorowborder"><code class="literal">{"key": "x", "value": "20", "id": 0}, {"key": "y", "value": 32, "id": 0}</code></td>
              </tr>
            </tbody>
          </table>
</div>

#### <a id="topic_jsonpath_filtexp"></a>SQL/JSON Filter Expression Elements

The following table describes the available filter expressions elements for `jsonpath`:

<div class="table-contents">
          <table class="table" summary="jsonpath Filter Expression Elements" border="1">
            <colgroup>
              <col />
              <col />
              <col />
              <col />
              <col />
            </colgroup>
            <thead>
              <tr class="row">
                <th class="entry nocellnorowborder">Value/Predicate</th>
                <th class="entry nocellnorowborder">Description</th>
                <th class="entry nocellnorowborder">Example JSON</th>
                <th class="entry nocellnorowborder">Example Query</th>
                <th class="entry nocellnorowborder">Result</th>
              </tr>
            </thead>
            <tbody>
              <tr class="row">
                <td class="entry nocellnorowborder"><code class="literal">==</code></td>
                <td class="entry nocellnorowborder">Equality operator</td>
                <td class="entry nocellnorowborder"><code class="literal">[1, 2, 1, 3]</code></td>
                <td class="entry nocellnorowborder"><code class="literal">$[*] ? (@ == 1)</code></td>
                <td class="entry nocellnorowborder"><code class="literal">1, 1</code></td>
              </tr>
              <tr class="row">
                <td class="entry nocellnorowborder"><code class="literal">!=</code></td>
                <td class="entry nocellnorowborder">Non-equality operator</td>
                <td class="entry nocellnorowborder"><code class="literal">[1, 2, 1, 3]</code></td>
                <td class="entry nocellnorowborder"><code class="literal">$[*] ? (@ != 1)</code></td>
                <td class="entry nocellnorowborder"><code class="literal">2, 3</code></td>
              </tr>
              <tr class="row">
                <td class="entry nocellnorowborder"><code class="literal">&lt;&gt;</code></td>
                <td class="entry nocellnorowborder">Non-equality operator (same as <code class="literal">!=</code>)</td>
                <td class="entry nocellnorowborder"><code class="literal">[1, 2, 1, 3]</code></td>
                <td class="entry nocellnorowborder"><code class="literal">$[*] ? (@ &lt;&gt; 1)</code></td>
                <td class="entry nocellnorowborder"><code class="literal">2, 3</code></td>
              </tr>
              <tr class="row">
                <td class="entry nocellnorowborder"><code class="literal">&lt;</code></td>
                <td class="entry nocellnorowborder">Less-than operator</td>
                <td class="entry nocellnorowborder"><code class="literal">[1, 2, 3]</code></td>
                <td class="entry nocellnorowborder"><code class="literal">$[*] ? (@ &lt; 2)</code></td>
                <td class="entry nocellnorowborder"><code class="literal">1</code></td>
              </tr>
              <tr class="row">
                <td class="entry nocellnorowborder"><code class="literal">&lt;=</code></td>
                <td class="entry nocellnorowborder">Less-than-or-equal-to operator</td>
                <td class="entry nocellnorowborder"><code class="literal">[1, 2, 3]</code></td>
                <td class="entry nocellnorowborder"><code class="literal">$[*] ? (@ &lt;= 2)</code></td>
                <td class="entry nocellnorowborder"><code class="literal">1, 2</code></td>
              </tr>
              <tr class="row">
                <td class="entry nocellnorowborder"><code class="literal">&gt;</code></td>
                <td class="entry nocellnorowborder">Greater-than operator</td>
                <td class="entry nocellnorowborder"><code class="literal">[1, 2, 3]</code></td>
                <td class="entry nocellnorowborder"><code class="literal">$[*] ? (@ &gt; 2)</code></td>
                <td class="entry nocellnorowborder"><code class="literal">3</code></td>
              </tr>
              <tr class="row">
                <td class="entry nocellnorowborder"><code class="literal">&gt;=</code></td>
                <td class="entry nocellnorowborder">Greater-than-or-equal-to operator</td>
                <td class="entry nocellnorowborder"><code class="literal">[1, 2, 3]</code></td>
                <td class="entry nocellnorowborder"><code class="literal">$[*] ? (@ &gt;= 2)</code></td>
                <td class="entry nocellnorowborder"><code class="literal">2, 3</code></td>
              </tr>
              <tr class="row">
                <td class="entry nocellnorowborder"><code class="literal">true</code></td>
                <td class="entry nocellnorowborder">Value used to perform comparison with JSON <code class="literal">true</code> literal</td>
                <td class="entry nocellnorowborder"><code class="literal">[{"name": "John", "parent": false}, {"name": "Chris", "parent": true}]</code></td>
                <td class="entry nocellnorowborder"><code class="literal">$[*] ? (@.parent == true)</code></td>
                <td class="entry nocellnorowborder"><code class="literal">{"name": "Chris", "parent": true}</code></td>
              </tr>
              <tr class="row">
                <td class="entry nocellnorowborder"><code class="literal">false</code></td>
                <td class="entry nocellnorowborder">Value used to perform comparison with JSON <code class="literal">false</code> literal</td>
                <td class="entry nocellnorowborder"><code class="literal">[{"name": "John", "parent": false}, {"name": "Chris", "parent": true}]</code></td>
                <td class="entry nocellnorowborder"><code class="literal">$[*] ? (@.parent == false)</code></td>
                <td class="entry nocellnorowborder"><code class="literal">{"name": "John", "parent": false}</code></td>
              </tr>
              <tr class="row">
                <td class="entry nocellnorowborder"><code class="literal">null</code></td>
                <td class="entry nocellnorowborder">Value used to perform comparison with JSON <code class="literal">null</code> value</td>
                <td class="entry nocellnorowborder"><code class="literal">[{"name": "Mary", "job": null}, {"name": "Michael", "job": "driver"}]</code></td>
                <td class="entry nocellnorowborder"><code class="literal">$[*] ? (@.job == null) .name</code></td>
                <td class="entry nocellnorowborder"><code class="literal">"Mary"</code></td>
              </tr>
              <tr class="row">
                <td class="entry nocellnorowborder"><code class="literal">&amp;&amp;</code></td>
                <td class="entry nocellnorowborder">Boolean AND</td>
                <td class="entry nocellnorowborder"><code class="literal">[1, 3, 7]</code></td>
                <td class="entry nocellnorowborder"><code class="literal">$[*] ? (@ &gt; 1 &amp;&amp; @ &lt; 5)</code></td>
                <td class="entry nocellnorowborder"><code class="literal">3</code></td>
              </tr>
              <tr class="row">
                <td class="entry nocellnorowborder"><code class="literal">||</code></td>
                <td class="entry nocellnorowborder">Boolean OR</td>
                <td class="entry nocellnorowborder"><code class="literal">[1, 3, 7]</code></td>
                <td class="entry nocellnorowborder"><code class="literal">$[*] ? (@ &lt; 1 || @ &gt; 5)</code></td>
                <td class="entry nocellnorowborder"><code class="literal">7</code></td>
              </tr>
              <tr class="row">
                <td class="entry nocellnorowborder"><code class="literal">!</code></td>
                <td class="entry nocellnorowborder">Boolean NOT</td>
                <td class="entry nocellnorowborder"><code class="literal">[1, 3, 7]</code></td>
                <td class="entry nocellnorowborder"><code class="literal">$[*] ? (!(@ &lt; 5))</code></td>
                <td class="entry nocellnorowborder"><code class="literal">7</code></td>
              </tr>
              <tr class="row">
                <td class="entry nocellnorowborder"><code class="literal">like_regex</code></td>
                <td class="entry nocellnorowborder">Tests whether the first operand matches the regular expression given by the second operand, optionally with modifications described by a string of <code class="literal">flag</code> characters.</td>
                <td class="entry nocellnorowborder"><code class="literal">["abc", "abd", "aBdC", "abdacb", "babc"]</code></td>
                <td class="entry nocellnorowborder"><code class="literal">$[*] ? (@ like_regex "^ab.*c" flag "i")</code></td>
                <td class="entry nocellnorowborder"><code class="literal">"abc", "aBdC", "abdacb"</code></td>
              </tr>
              <tr class="row">
                <td class="entry nocellnorowborder"><code class="literal">starts with</code></td>
                <td class="entry nocellnorowborder">Tests whether the second operand is an initial substring of the first operand</td>
                <td class="entry nocellnorowborder"><code class="literal">["John Smith", "Mary Stone", "Bob Johnson"]</code></td>
                <td class="entry nocellnorowborder"><code class="literal">$[*] ? (@ starts with "John")</code></td>
                <td class="entry nocellnorowborder"><code class="literal">"John Smith"</code></td>
              </tr>
              <tr class="row">
                <td class="entry nocellnorowborder"><code class="literal">exists</code></td>
                <td class="entry nocellnorowborder">Tests whether a path expression matches at least one SQL/JSON item</td>
                <td class="entry nocellnorowborder"><code class="literal">{"x": [1, 2], "y": [2, 4]}</code></td>
                <td class="entry nocellnorowborder"><code class="literal">strict $.* ? (exists (@ ? (@[*] &gt; 2)))</code></td>
                <td class="entry nocellnorowborder"><code class="literal">2, 4</code></td>
              </tr>
              <tr class="row">
                <td class="entry nocellnorowborder"><code class="literal">is unknown</code></td>
                <td class="entry nocellnorowborder">Tests whether a Boolean condition is <code class="literal">unknown</code></td>
                <td class="entry nocellnorowborder"><code class="literal">[-1, 2, 7, "infinity"]</code></td>
                <td class="entry nocellnorowborder"><code class="literal">$[*] ? ((@ &gt; 0) is unknown)</code></td>
                <td class="entry nocellnorowborder"><code class="literal">"infinity"</code></td>
              </tr>
            </tbody>
          </table>
        </div>

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

> **Note** The Greenplum MADlib Extension for Analytics provides additional advanced functions to perform statistical analysis and machine learning with Greenplum Database data. See [MADlib Extension for Analytics](../analytics/madlib.html#topic1).

<div class="tablenoborder"><table cellpadding="4" cellspacing="0" summary="" id="topic31__in2073121" class="table" frame="border" border="1" rules="all"><caption><span class="tablecap"><span class="table--title-label">Table 10. </span>Advanced Aggregate Functions</span></caption><colgroup><col style="width:20.845288240441164%" /><col style="width:12.005779052967869%" /><col style="width:41.10249679506745%" /><col style="width:26.046435911523513%" /></colgroup><thead class="thead" style="text-align:left;">
<tr class="row">
<th class="entry nocellnorowborder" style="vertical-align:top;" id="d233567e3144">Function</th>
<th class="entry nocellnorowborder" style="vertical-align:top;" id="d233567e3147">Return Type</th>
<th class="entry nocellnorowborder" style="vertical-align:top;" id="d233567e3150">Full Syntax</th>
<th class="entry nocellnorowborder" style="vertical-align:top;" id="d233567e3153">Description</th>
</tr>
</thead>
<tbody class="tbody">
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e3144 ">
<code class="ph codeph">MEDIAN (<em class="ph i">expr</em>)</code>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e3147 ">
<code class="ph codeph">timestamp, timestamptz, interval, float</code>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e3150 ">
<code class="ph codeph">MEDIAN (<em class="ph i">expression</em>)</code>
<p class="p">
<em class="ph i">Example:</em>
</p>
<pre class="pre codeblock"><code>SELECT department_id, MEDIAN(salary) 
FROM employees 
GROUP BY department_id; </code></pre>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e3153 ">Can take a two-dimensional array as input. Treats such arrays as
                matrices.</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e3144 ">
<code class="ph codeph">PERCENTILE_CONT (<em class="ph i">expr</em>) WITHIN GROUP (ORDER BY <em class="ph i">expr</em>
                  [DESC/ASC])</code>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e3147 ">
<code class="ph codeph">timestamp, timestamptz, interval, float</code>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e3150 ">
<code class="ph codeph">PERCENTILE_CONT(<em class="ph i">percentage</em>) WITHIN GROUP (ORDER BY
<em class="ph i">expression</em>)</code>
<p class="p">
<em class="ph i">Example:</em>
</p>
<pre class="pre codeblock"><code>SELECT department_id,
PERCENTILE_CONT (0.5) WITHIN GROUP (ORDER BY salary DESC)
"Median_cont"; 
FROM employees GROUP BY department_id;</code></pre>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e3153 ">Performs an inverse distribution function that assumes a
                continuous distribution model. It takes a percentile value and a sort specification
                and returns the same datatype as the numeric datatype of the argument. This returned
                value is a computed result after performing linear interpolation. Null are ignored
                in this calculation.</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e3144 "><code class="ph codeph">PERCENTILE_DISC (<em class="ph i">expr</em>) WITHIN GROUP (ORDER BY
<em class="ph i">expr</em> [DESC/ASC])</code></td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e3147 ">
<code class="ph codeph">timestamp, timestamptz, interval, float</code>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e3150 ">
<code class="ph codeph">PERCENTILE_DISC(<em class="ph i">percentage</em>) WITHIN GROUP (ORDER BY
<em class="ph i">expression</em>)</code>
<p class="p">
<em class="ph i">Example:</em>
</p>
<pre class="pre codeblock"><code>SELECT department_id, 
PERCENTILE_DISC (0.5) WITHIN GROUP (ORDER BY salary DESC)
"Median_desc"; 
FROM employees GROUP BY department_id;</code></pre>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e3153 ">Performs an inverse distribution function that assumes a
                discrete distribution model. It takes a percentile value and a sort specification.
                This returned value is an element from the set. Null are ignored in this
                calculation.</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e3144 ">
<code class="ph codeph">sum(array[])</code>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e3147 ">
<code class="ph codeph">smallint[]int[], bigint[], float[]</code>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e3150 ">
<code class="ph codeph">sum(array[[1,2],[3,4]])</code>
<p class="p">
<em class="ph i">Example:</em>
</p>
<pre class="pre codeblock"><code>CREATE TABLE mymatrix (myvalue int[]);
INSERT INTO mymatrix VALUES (array[[1,2],[3,4]]);
INSERT INTO mymatrix VALUES (array[[0,1],[1,0]]);
SELECT sum(myvalue) FROM mymatrix;
 sum 
---------------
 {{1,3},{4,4}}</code></pre>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e3153 ">Performs matrix summation. Can take as input a two-dimensional
                array that is treated as a matrix.</td>
</tr>
<tr class="row">
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e3144 ">
<code class="ph codeph">pivot_sum (label[], label, expr)</code>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e3147 ">
<code class="ph codeph">int[], bigint[], float[]</code>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e3150 ">
<code class="ph codeph">pivot_sum( array['A1','A2'], attr, value)</code>
</td>
<td class="entry nocellnorowborder" style="vertical-align:top;" headers="d233567e3153 ">A pivot aggregation using sum to resolve duplicate
                entries.</td>
</tr>
<tr class="row">
<td class="entry row-nocellborder" style="vertical-align:top;" headers="d233567e3144 ">
<code class="ph codeph">unnest (array[])</code>
</td>
<td class="entry row-nocellborder" style="vertical-align:top;" headers="d233567e3147 ">set of <code class="ph codeph">anyelement</code></td>
<td class="entry row-nocellborder" style="vertical-align:top;" headers="d233567e3150 ">
<code class="ph codeph">unnest( array['one', 'row', 'per', 'item'])</code>
</td>
<td class="entry cellrowborder" style="vertical-align:top;" headers="d233567e3153 ">Transforms a one dimensional array into rows. Returns a set of
<code class="ph codeph">anyelement</code>, a polymorphic <a class="xref" href="https://www.postgresql.org/docs/12/datatype-pseudo.html" target="_blank"><span class="ph">pseudotype in PostgreSQL</span></a>.</td>
</tr>
</tbody>
</table>
</div>

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

> **Note** The `tsquery` containment operators consider only the lexemes listed in the two queries, ignoring the combining operators.

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

> **Note** All the text search functions that accept an optional `regconfig` argument will use the configuration specified by [default\_text\_search\_config](config_params/guc-list.html) when that argument is omitted.

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
|`lower(anyrange)`|range's element type|lower bound of range|`lower(numrange(1.1,2.2))`|`1.1`|
|`upper(anyrange)`|range's element type|upper bound of range|`upper(numrange(1.1,2.2))`|`2.2`|
|`isempty(anyrange)`|`boolean`|is the range empty?|`isempty(numrange(1.1,2.2))`|`false`|
|`lower_inc(anyrange)`|`boolean`|is the lower bound inclusive?|`lower_inc(numrange(1.1,2.2))`|`true`|
|`upper_inc(anyrange)`|`boolean`|is the upper bound inclusive?|`upper_inc(numrange(1.1,2.2))`|`false`|
|`lower_inf(anyrange)`|`boolean`|is the lower bound infinite?|`lower_inf('(,)'::daterange)`|`true`|
|`upper_inf(anyrange)`|`boolean`|is the upper bound infinite?|`upper_inf('(,)'::daterange)`|`true`|
|`range_merge(anyrange, anyrange)`|`anyrange`|the smallest range which includes both of the given ranges|`range_merge('[1,2)'::int4range, '[3,4)'::int4range)`|`[1,4)`|

The `lower` and `upper` functions return null if the range is empty or the requested bound is infinite. The `lower_inc`, `upper_inc`, `lower_inf`, and `upper_inf` functions all return false for an empty range.

