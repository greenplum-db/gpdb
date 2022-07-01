# Summary of Built-in Functions 

Greenplum Database supports built-in functions and operators including analytic functions and window functions that can be used in window expressions. For information about using built-in Greenplum Database functions see, "Using Functions and Operators" in the *Greenplum Database Administrator Guide*.

-   [Greenplum Database Function Types](#topic27)
-   [Built-in Functions and Operators](#topic29)
-   [JSON Functions and Operators](#topic_gn4_x3w_mq)
-   [Window Functions](#topic30)
-   [Advanced Aggregate Functions](#topic31)

**Parent topic:** [Greenplum Database Reference Guide](ref_guide.html)

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
|[Mathematical Functions and Operators](https://www.postgresql.org/docs/8.3/static/functions-math.html)|random<br/>setseed| | |
|[String Functions and Operators](https://www.postgresql.org/docs/8.3/static/functions-string.html)|*All built-in<br/>conversion<br/>functions*|convert<br/>pg\_client\_encoding| |
|[Binary String Functions and Operators](https://www.postgresql.org/docs/8.3/static/functions-binarystring.html)| | | |
|[Bit String Functions and Operators](https://www.postgresql.org/docs/8.3/static/functions-bitstring.html)| | | |
|[Pattern Matching](https://www.postgresql.org/docs/8.3/static/functions-matching.html)| | | |
|[Data Type Formatting Functions](https://www.postgresql.org/docs/8.3/static/functions-formatting.html)| |to\_char<br/>to\_timestamp| |
|[Date/Time Functions and Operators](https://www.postgresql.org/docs/8.3/static/functions-datetime.html)|timeofday|age<br/>current\_date<br/>current\_time<br/>current\_timestamp<br/>localtime<br/>localtimestamp<br/>now| 
|[Enum Support Functions](https://www.postgresql.org/docs/8.3/static/functions-enum.html)| | | |
|[Geometric Functions and Operators](https://www.postgresql.org/docs/8.3/static/functions-geometry.html)| | | |
|[Network Address Functions and Operators](https://www.postgresql.org/docs/8.3/static/functions-net.html)| | | |
|[Sequence Manipulation Functions](https://www.postgresql.org/docs/8.3/static/functions-sequence.html)|nextval\(\)<br/>setval\(\)| | |
|[Conditional Expressions](https://www.postgresql.org/docs/8.3/static/functions-conditional.html)| | | |
|[Array Functions and Operators](https://www.postgresql.org/docs/8.3/static/functions-array.html)| |*All array functions*| |
|[Aggregate Functions](https://www.postgresql.org/docs/8.3/static/functions-aggregate.html)| | | |
|[Subquery Expressions](https://www.postgresql.org/docs/8.3/static/functions-subquery.html)| | | |
|[Row and Array Comparisons](https://www.postgresql.org/docs/8.3/static/functions-comparisons.html)| | | |
|[Set Returning Functions](https://www.postgresql.org/docs/8.3/static/functions-srf.html)|generate\_series| | |
|[System Information Functions](https://www.postgresql.org/docs/8.3/static/functions-info.html)| |*All session information functions*<br/>*All access privilege inquiry functions*<br/>*All schema visibility inquiry functions*<br/>*All system catalog information functions*<br/>*All comment information functions*<br/>*All transaction ids and snapshots*| 
|[System Administration Functions](https://www.postgresql.org/docs/8.3/static/functions-admin.html)|set\_configpg\_cancel\_backend<br/>pg\_reload\_conf<br/>pg\_rotate\_logfile<br/>pg\_start\_backup<br/>pg\_stop\_backup<br/>pg\_size\_pretty<br/>pg\_ls\_dir<br/>pg\_read\_file<br/>pg\_stat\_file|current\_setting<br/>*All database object size functions*|**Note:** The function `pg_column_size` displays bytes required to store the value, possibly with TOAST compression.|
|[XML Functions](https://www.postgresql.org/docs/9.1/static/functions-xml.html)<br/>and function-like expressions| |cursor\_to\_xml\(cursor refcursor, count int, nulls boolean, tableforest boolean, targetns text\)<br/><br/>cursor\_to\_xmlschema\(cursor refcursor, nulls boolean, tableforest boolean, targetns text\)<br/><br/>database\_to\_xml\(nulls boolean, tableforest boolean, targetns text\)<br/><br/>database\_to\_xmlschema\(nulls boolean, tableforest boolean, targetns text\)<br/><br/>database\_to\_xml\_and\_xmlschema\(nulls boolean, tableforest boolean, targetns text\)<br/><br/>query\_to\_xml\(query text, nulls boolean, tableforest boolean, targetns text\)<br/><br/>query\_to\_xmlschema\(query text, nulls boolean, tableforest boolean, targetns text\)<br/><br/>query\_to\_xml\_and\_xmlschema\(query text, nulls boolean, tableforest boolean, targetns text\)<br/><br/>schema\_to\_xml\(schema name, nulls boolean, tableforest boolean, targetns text\)<br/><br/>schema\_to\_xmlschema\(schema name, nulls boolean, tableforest boolean, targetns text\<br/><br/><br/>schema\_to\_xml\_and\_xmlschema\(schema name, nulls boolean, tableforest boolean, targetns text\)<br/><br/>table\_to\_xml\(tbl regclass, nulls boolean, tableforest boolean, targetns text\)<br/><br/>table\_to\_xmlschema\(tbl regclass, nulls boolean, tableforest boolean, targetns text<br/><br/>table\_to\_xml\_and\_xmlschema\(tbl regclass, nulls boolean, tableforest boolean, targetns text\)<br/><br/>xmlagg\(xml\)<br/><br/>xmlconcat\(xml\[, ...\]\)<br/><br/>xmlelement\(name name \[, xmlattributes\(value \[AS attname\] \[, ... \]\)\] \[, content, ...\]\)<br/><br/>xmlexists\(text, xml\)<br/><br/>xmlforest\(content \[AS name\] \[, ...\]<br/><br/>xml\_is\_well\_formed\(text\)<br/><br/>xml\_is\_well\_formed\_document\(text\)<br/><br/>xml\_is\_well\_formed\_content\(text\)<br/><br/>xmlparse \( \{ DOCUMENT \| CONTENT \} value\)<br/><br/>xpath\(text, xml\)<br/><br/>xpath\(text, xml, text\[\]\)<br/><br/>xpath\_exists\(text, xml\)<br/><br/>xpath\_exists\(text, xml, text\[\]\)<br/><br/>xmlpi\(name target \[, content\]\)<br/><br/>xmlroot\(xml, version text \| no value \[, standalone yes\|no\|no value\]\)<br/><br/>xmlserialize \( \{ DOCUMENT \| CONTENT \} value AS type \)<br/><br/>xml\(text\)<br/><br/>text\(xml\)<br/><br/>xmlcomment\(xml\)<br/><br/>xmlconcat2\(xml, xml\)| |

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
|`array_to_json(anyarray [, pretty_bool])`|Returns the array as a JSON array. A Greenplum Database multidimensional array becomes a JSON array of arrays.<br/>Line feeds are added between dimension 1 elements if pretty\_bool is `true`.<br/>|`array_to_json('{{1,5},{99,100}}'::int[])`|`[[1,5],[99,100]]`|
`row_to_json(record [, pretty_bool])`|Returns the row as a JSON object. Line feeds are added between level 1 elements if `pretty_bool` is `true`.|`row_to_json(row(1,'foo'))`|`{"f1":1,"f2":"foo"}`|

### JSON Processing Functions 

This table describes the functions that process `json` values.

<div class="tablenoborder">
<table cellpadding="4" cellspacing="0" summary="" id="topic_z5d_snw_2z__table_wfc_y3w_mq" class="table" frame="border" border="1" rules="all">
<caption><span class="tablecap"></span></caption>
            <thead class="thead" align="left">
              <tr class="row">
                <th class="entry" valign="top" width="10.351966873706003%" id="d142067e981">Operator</th>
                <th class="entry" valign="top" width="12.111801242236025%" id="d142067e984">Right Operand Type</th>
                <th class="entry" valign="top" width="28.88198757763975%" id="d142067e987">Description</th>
                <th class="entry" valign="top" width="28.778467908902687%" id="d142067e990">Example</th>
                <th class="entry" valign="top" width="19.875776397515526%" id="d142067e993">Example Result</th>
              </tr>
            </thead>
            <tbody class="tbody">
              <tr class="row">
                <td class="entry" valign="top" width="10.351966873706003%" headers="d142067e981 ">
                  <samp class="ph codeph">-&gt;</samp>
                </td>
                <td class="entry" valign="top" width="12.111801242236025%" headers="d142067e984 ">
                  <samp class="ph codeph">int</samp>
                </td>
                <td class="entry" valign="top" width="28.88198757763975%" headers="d142067e987 ">Get JSON array element (indexed from zero).</td>
                <td class="entry" valign="top" width="28.778467908902687%" headers="d142067e990 ">
                  <samp class="ph codeph">'[{"a":"foo"},{"b":"bar"},{"c":"baz"}]'::json-&gt;2</samp>
                </td>
                <td class="entry" valign="top" width="19.875776397515526%" headers="d142067e993 ">
                  <samp class="ph codeph">{"c":"baz"}</samp>
                </td>
              </tr>
              <tr class="row">
                <td class="entry" valign="top" width="10.351966873706003%" headers="d142067e981 ">
                  <samp class="ph codeph">-&gt;</samp>
                </td>
                <td class="entry" valign="top" width="12.111801242236025%" headers="d142067e984 ">
                  <samp class="ph codeph">text</samp>
                </td>
                <td class="entry" valign="top" width="28.88198757763975%" headers="d142067e987 ">Get JSON object field by key.</td>
                <td class="entry" valign="top" width="28.778467908902687%" headers="d142067e990 ">
                  <samp class="ph codeph">'{"a": {"b":"foo"}}'::json-&gt;'a'</samp>
                </td>
                <td class="entry" valign="top" width="19.875776397515526%" headers="d142067e993 ">
                  <samp class="ph codeph">{"b":"foo"}</samp>
                </td>
              </tr>
              <tr class="row">
                <td class="entry" valign="top" width="10.351966873706003%" headers="d142067e981 ">
                  <samp class="ph codeph">-&gt;&gt;</samp>
                </td>
                <td class="entry" valign="top" width="12.111801242236025%" headers="d142067e984 ">
                  <samp class="ph codeph">int</samp>
                </td>
                <td class="entry" valign="top" width="28.88198757763975%" headers="d142067e987 ">Get JSON array element as text.</td>
                <td class="entry" valign="top" width="28.778467908902687%" headers="d142067e990 ">
                  <samp class="ph codeph">'[1,2,3]'::json-&gt;&gt;2</samp>
                </td>
                <td class="entry" valign="top" width="19.875776397515526%" headers="d142067e993 ">
                  <samp class="ph codeph">3</samp>
                </td>
              </tr>
              <tr class="row">
                <td class="entry" valign="top" width="10.351966873706003%" headers="d142067e981 ">
                  <samp class="ph codeph">-&gt;&gt;</samp>
                </td>
                <td class="entry" valign="top" width="12.111801242236025%" headers="d142067e984 ">
                  <samp class="ph codeph">text</samp>
                </td>
                <td class="entry" valign="top" width="28.88198757763975%" headers="d142067e987 ">Get JSON object field as text.</td>
                <td class="entry" valign="top" width="28.778467908902687%" headers="d142067e990 ">
                  <samp class="ph codeph">'{"a":1,"b":2}'::json-&gt;&gt;'b'</samp>
                </td>
                <td class="entry" valign="top" width="19.875776397515526%" headers="d142067e993 ">
                  <samp class="ph codeph">2</samp>
                </td>
              </tr>
              <tr class="row">
                <td class="entry" valign="top" width="10.351966873706003%" headers="d142067e981 ">
                  <samp class="ph codeph">#&gt;</samp>
                </td>
                <td class="entry" valign="top" width="12.111801242236025%" headers="d142067e984 ">
                  <samp class="ph codeph">text[]</samp>
                </td>0
                <td class="entry" valign="top" width="28.88198757763975%" headers="d142067e987 ">Get JSON object at specified path.</td>
                <td class="entry" valign="top" width="28.778467908902687%" headers="d142067e990 ">
<samp class="ph codeph">'{"a": {"b":{"c": "foo"}}}'::json#&gt;'{a,b}</samp>'</td>
                <td class="entry" valign="top" width="19.875776397515526%" headers="d142067e993 ">
                  <samp class="ph codeph">{"c": "foo"}</samp>
                </td>
              </tr>
              <tr class="row">
                <td class="entry" valign="top" width="10.351966873706003%" headers="d142067e981 ">
                  <samp class="ph codeph">#&gt;&gt;</samp>
                </td>
                <td class="entry" valign="top" width="12.111801242236025%" headers="d142067e984 ">
                  <samp class="ph codeph">text[]</samp>
                </td>
                <td class="entry" valign="top" width="28.88198757763975%" headers="d142067e987 ">Get JSON object at specified path as text.</td>
                <td class="entry" valign="top" width="28.778467908902687%" headers="d142067e990 ">
                  <samp class="ph codeph">'{"a":[1,2,3],"b":[4,5,6]}'::json#&gt;&gt;'{a,2}'</samp>
                </td>
                <td class="entry" valign="top" width="19.875776397515526%" headers="d142067e993 ">
                  <samp class="ph codeph">3</samp>
                </td>
              </tr>
            </tbody>
          </table>
</div>

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
<div class="tablenoborder">
<table cellpadding="4" cellspacing="0" summary="" id="topic31__in2073121" class="table" frame="border" border="1" rules="all">
          <thead class="thead" align="left">
            <tr class="row">
              <th class="entry" valign="top" width="20.845288240441164%" id="d142067e2153">Function</th>
              <th class="entry" valign="top" width="12.005779052967869%" id="d142067e2156">Return Type</th>
              <th class="entry" valign="top" width="41.10249679506745%" id="d142067e2159">Full Syntax</th>
              <th class="entry" valign="top" width="26.046435911523513%" id="d142067e2162">Description</th>
            </tr>
          </thead>
          <tbody class="tbody">
            <tr class="row">
              <td class="entry" valign="top" width="20.845288240441164%" headers="d142067e2153 ">
                <samp class="ph codeph">MEDIAN (<em class="ph i">expr</em>)</samp>
              </td>
              <td class="entry" valign="top" width="12.005779052967869%" headers="d142067e2156 ">
                <samp class="ph codeph">timestamp, timestamptz, interval, float</samp>
              </td>
              <td class="entry" valign="top" width="41.10249679506745%" headers="d142067e2159 ">
                <samp class="ph codeph">MEDIAN (<em class="ph i">expression</em>)</samp>
                <p class="p">
                  <em class="ph i">Example:</em>
                </p>
                <pre class="pre codeblock">SELECT department_id, MEDIAN(salary) FROM employees 
GROUP BY department_id; </pre>
              </td>
              <td class="entry" valign="top" width="26.046435911523513%" headers="d142067e2162 ">Can take a two-dimensional array as input. Treats such arrays as
                matrices.</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="20.845288240441164%" headers="d142067e2153 ">
                <samp class="ph codeph">PERCENTILE_CONT (<em class="ph i">expr</em>) WITHIN GROUP (ORDER BY <em class="ph i">expr</em>
                  [DESC/ASC])</samp>
              </td>
              <td class="entry" valign="top" width="12.005779052967869%" headers="d142067e2156 ">
                <samp class="ph codeph">timestamp, timestamptz, interval, float</samp>
              </td>
              <td class="entry" valign="top" width="41.10249679506745%" headers="d142067e2159 ">
                <samp class="ph codeph">PERCENTILE_CONT(<em class="ph i">percentage</em>) WITHIN GROUP (ORDER BY
                  <em class="ph i">expression</em>)</samp>
                <p class="p">
                  <em class="ph i">Example:</em>
                </p>
                <pre class="pre codeblock">SELECT department_id,
PERCENTILE_CONT (0.5) WITHIN GROUP (ORDER BY salary DESC)
"Median_cont"; 
FROM employees GROUP BY department_id;</pre>
              </td>
              <td class="entry" valign="top" width="26.046435911523513%" headers="d142067e2162 ">Performs an inverse distribution function that assumes a
                continuous distribution model. It takes a percentile value and a sort specification
                and returns the same datatype as the numeric datatype of the argument. This returned
                value is a computed result after performing linear interpolation. Null are ignored
                in this calculation.</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="20.845288240441164%" headers="d142067e2153 "><samp class="ph codeph">PERCENTILE_DISC (<em class="ph i">expr</em>) WITHIN GROUP (ORDER BY
                    <em class="ph i">expr</em> [DESC/ASC])</samp></td>
              <td class="entry" valign="top" width="12.005779052967869%" headers="d142067e2156 ">
                <samp class="ph codeph">timestamp, timestamptz, interval, float</samp>
              </td>
              <td class="entry" valign="top" width="41.10249679506745%" headers="d142067e2159 ">
                <samp class="ph codeph">PERCENTILE_DISC(<em class="ph i">percentage</em>) WITHIN GROUP (ORDER BY
                  <em class="ph i">expression</em>)</samp>
                <p class="p">
                  <em class="ph i">Example:</em>
                </p>
                <pre class="pre codeblock">SELECT department_id, 
PERCENTILE_DISC (0.5) WITHIN GROUP (ORDER BY salary DESC)
"Median_desc"; 
FROM employees GROUP BY department_id;</pre>
              </td>
              <td class="entry" valign="top" width="26.046435911523513%" headers="d142067e2162 ">Performs an inverse distribution function that assumes a
                discrete distribution model. It takes a percentile value and a sort specification.
                This returned value is an element from the set. Null are ignored in this
                calculation.</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="20.845288240441164%" headers="d142067e2153 ">
                <samp class="ph codeph">sum(array[])</samp>
              </td>
              <td class="entry" valign="top" width="12.005779052967869%" headers="d142067e2156 ">
                <samp class="ph codeph">smallint[]int[], bigint[], float[]</samp>
              </td>
              <td class="entry" valign="top" width="41.10249679506745%" headers="d142067e2159 ">
                <samp class="ph codeph">sum(array[[1,2],[3,4]])</samp>
                <p class="p">
                  <em class="ph i">Example:</em>
                </p>
                <pre class="pre codeblock">CREATE TABLE mymatrix (myvalue int[]);
INSERT INTO mymatrix VALUES (array[[1,2],[3,4]]);
INSERT INTO mymatrix VALUES (array[[0,1],[1,0]]);
SELECT sum(myvalue) FROM mymatrix;
 sum 
---------------
 {{1,3},{4,4}}</pre>
              </td>
              <td class="entry" valign="top" width="26.046435911523513%" headers="d142067e2162 ">Performs matrix summation. Can take as input a two-dimensional
                array that is treated as a matrix.</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="20.845288240441164%" headers="d142067e2153 ">
                <samp class="ph codeph">pivot_sum (label[], label, expr)</samp>
              </td>
              <td class="entry" valign="top" width="12.005779052967869%" headers="d142067e2156 ">
                <samp class="ph codeph">int[], bigint[], float[]</samp>
              </td>
              <td class="entry" valign="top" width="41.10249679506745%" headers="d142067e2159 ">
                <samp class="ph codeph">pivot_sum( array['A1','A2'], attr, value)</samp>
              </td>
              <td class="entry" valign="top" width="26.046435911523513%" headers="d142067e2162 ">A pivot aggregation using sum to resolve duplicate
                entries.</td>
            </tr>
            <tr class="row">
              <td class="entry" valign="top" width="20.845288240441164%" headers="d142067e2153 ">
                <samp class="ph codeph">unnest (array[])</samp>
              </td>
              <td class="entry" valign="top" width="12.005779052967869%" headers="d142067e2156 ">set of <samp class="ph codeph">anyelement</samp>
</td>
              <td class="entry" valign="top" width="41.10249679506745%" headers="d142067e2159 ">
                <samp class="ph codeph">unnest( array['one', 'row', 'per', 'item'])</samp>
              </td>
              <td class="entry" valign="top" width="26.046435911523513%" headers="d142067e2162 ">Transforms a one dimensional array into rows. Returns a set of
                  <samp class="ph codeph">anyelement</samp>, a polymorphic <a class="xref" href="https://www.postgresql.org/docs/8.3/static/datatype-pseudo.html" target="_blank"><span class="ph">pseudotype in PostgreSQL</span></a>.</td>
            </tr>
          </tbody>
        </table>
</div>
    </div>
  </div>