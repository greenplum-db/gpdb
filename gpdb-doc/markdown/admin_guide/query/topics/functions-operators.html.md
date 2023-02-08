---
title: Using Functions and Operators 
---

Description of user-defined and built-in functions and operators in Greenplum Database.

-   [Using Functions in Greenplum Database](#topic27)
-   [User-Defined Functions](#topic28)
-   [User-Defined Procedures](#topic28a)
-   [Built-in Functions and Operators](#topic29)
-   [Window Functions](#topic30)
-   [Advanced Aggregate Functions](#topic31)

**Parent topic:** [Querying Data](../../query/topics/query.html)

## <a id="topic27"></a>Using Functions in Greenplum Database 

When you invoke a function in Greenplum Database, function attributes control the execution of the function. The volatility attributes \(`IMMUTABLE`, `STABLE`, `VOLATILE`\) and the `EXECUTE ON` attributes control two different aspects of function execution. In general, volatility indicates when the function is run, and `EXECUTE ON` indicates where it is run. The volatility attributes are PostgreSQL based attributes, the `EXECUTE ON` attributes are Greenplum Database attributes.

For example, a function defined with the `IMMUTABLE` attribute can be run at query planning time, while a function with the `VOLATILE` attribute must be run for every row in the query. A function with the `EXECUTE ON MASTER` attribute runs only on the coordinator instance, and a function with the `EXECUTE ON ALL SEGMENTS` attribute runs on all primary segment instances \(not the coordinator\).

These tables summarize what Greenplum Database assumes about function execution based on the attribute.

|Function Attribute|Greenplum Support|Description|Comments|
|------------------|-----------------|-----------|--------|
|IMMUTABLE|Yes|Relies only on information directly in its argument list. Given the same argument values, always returns the same result.| |
|STABLE|Yes, in most cases|Within a single table scan, returns the same result for same argument values, but results change across SQL statements.|Results depend on database lookups or parameter values. `current_timestamp` family of functions is `STABLE`; values do not change within an execution.|
|VOLATILE|Restricted|Function values can change within a single table scan. For example: `random()`, `timeofday()`. This is the default attribute.|Any function with side effects is volatile, even if its result is predictable. For example: `setval()`.|

|Function Attribute|Description|Comments|
|------------------|-----------|--------|
|EXECUTE ON ANY|Indicates that the function can be run on the coordinator, or any segment instance, and it returns the same result regardless of where it runs. This is the default attribute.|Greenplum Database determines where the function runs.|
|EXECUTE ON MASTER|Indicates that the function must be run on the coordinator instance.|Specify this attribute if the user-defined function runs queries to access tables.|
|EXECUTE ON ALL SEGMENTS|Indicates that for each invocation, the function must be run on all primary segment instances, but not the coordinator.| |
|EXECUTE ON INITPLAN|Indicates that the function contains an SQL command that dispatches queries to the segment instances and requires special processing on the coordinator instance by Greenplum Database when possible.| |

You can display the function volatility and `EXECUTE ON` attribute information with the psql `\df+ function` command.

Refer to the PostgreSQL [Function Volatility Categories](https://www.postgresql.org/docs/12/xfunc-volatility.html) documentation for additional information about the Greenplum Database function volatility classifications.

For more information about `EXECUTE ON` attributes, see [CREATE FUNCTION](../../../ref_guide/sql_commands/CREATE_FUNCTION.html).

In Greenplum Database, data is divided up across segments — each segment is a distinct PostgreSQL database. To prevent inconsistent or unexpected results, do not run functions classified as `VOLATILE` at the segment level if they contain SQL commands or modify the database in any way. For example, functions such as `setval()` are not allowed to run on distributed data in Greenplum Database because they can cause inconsistent data between segment instances.

A function can run read-only queries on replicated tables \(`DISTRIBUTED REPLICATED`\) on the segments, but any SQL command that modifies data must run on the coordinator instance.

> **Note** The hidden system columns \(`ctid`, `cmin`, `cmax`, `xmin`, `xmax`, and `gp_segment_id`\) cannot be referenced in user queries on replicated tables because they have no single, unambiguous value. Greenplum Database returns a `column does not exist` error for the query.

To ensure data consistency, you can safely use `VOLATILE` and `STABLE` functions in statements that are evaluated on and run from the coordinator. For example, the following statements run on the coordinator \(statements without a `FROM` clause\):

```
SELECT setval('myseq', 201);
SELECT foo();
```

If a statement has a `FROM` clause containing a distributed table *and* the function in the `FROM` clause returns a set of rows, the statement can run on the segments:

```
SELECT * from foo();
```

Greenplum Database does not support functions that return a table reference \(`rangeFuncs`\) or functions that use the `refCursor` data type.

### <a id="topic281"></a>Function Volatility and Plan Caching 

There is relatively little difference between the `STABLE` and `IMMUTABLE` function volatility categories for simple interactive queries that are planned and immediately run. It does not matter much whether a function is run once during planning or once during query execution start up. But there is a big difference when you save the plan and reuse it later. If you mislabel a function `IMMUTABLE`, Greenplum Database may prematurely fold it to a constant during planning, possibly reusing a stale value during subsequent execution of the plan. You may run into this hazard when using `PREPARE`d statements, or when using languages such as PL/pgSQL that cache plans.

## <a id="topic28"></a>User-Defined Functions 

Greenplum Database supports user-defined functions. See [Extending SQL](https://www.postgresql.org/docs/12/extend.html) in the PostgreSQL documentation for more information.

Use the `CREATE FUNCTION` statement to register user-defined functions that are used as described in [Using Functions in Greenplum Database](#topic27). By default, user-defined functions are declared as `VOLATILE`, so if your user-defined function is `IMMUTABLE` or `STABLE`, you must specify the correct volatility level when you register your function.

By default, user-defined functions are declared as `EXECUTE ON ANY`. A function that runs queries to access tables is supported only when the function runs on the coordinator instance, except that a function can run `SELECT` commands that access only replicated tables on the segment instances. A function that accesses hash-distributed or randomly distributed tables must be defined with the `EXECUTE ON MASTER` attribute. Otherwise, the function might return incorrect results when the function is used in a complicated query. Without the attribute, planner optimization might determine it would be beneficial to push the function invocation to segment instances.

When you create user-defined functions, avoid using fatal errors or destructive calls. Greenplum Database may respond to such errors with a sudden shutdown or restart.

In Greenplum Database, the shared library files for user-created functions must reside in the same library path location on every host in the Greenplum Database array \(coordinators, segments, and mirrors\).

You can also create and run anonymous code blocks that are written in a Greenplum Database procedural language such as PL/pgSQL. The anonymous blocks run as transient anonymous functions. For information about creating and running anonymous blocks, see the [`DO`](../../../ref_guide/sql_commands/DO.html) command.

## <a id="topic28a"></a>User-Defined Procedures

A procedure is a database object similar to a function. The key differences are:

- You define a procedure with the [CREATE PROCEDURE](../../../ref_guide/sql_commands/CREATE_PROCEDURE.html) command, not `CREATE FUNCTION`.
- Procedures do not return a function value; hence `CREATE PROCEDURE` lacks a `RETURNS` clause. However, procedures can instead return data to their callers via output parameters.
- While a function is called as part of a query or DML command, you call a procedure in isolation using the [CALL](../../../ref_guide/sql_commands/CALL.html) command.
- A procedure can commit or roll back transactions during its execution \(then automatically beginning a new transaction\), so long as the invoking `CALL` command is not part of an explicit transaction block. A function cannot do that.
- Certain function attributes, such as strictness, don't apply to procedures. Those attributes control how the function is used in a query, which isn't relevant to procedures.

Collectively, functions and procedures are also known as routines. There are commands such as [ALTER ROUTINE](../../../ref_guide/sql_commands/ALTER_ROUTINE.html) and [DROP ROUTINE](../../../ref_guide/sql_commands/DROP_ROUTINE.html) that can operate on functions and procedures without having to know which kind it is. Note, however, that there is no `CREATE ROUTINE` command.

## <a id="topic29"></a>Built-in Functions and Operators 

The following table lists the categories of built-in functions and operators supported by PostgreSQL. All functions and operators are supported in Greenplum Database as in PostgreSQL with the exception of `STABLE` and `VOLATILE` functions, which are subject to the restrictions noted in [Using Functions in Greenplum Database](#topic27). See the [Functions and Operators](https://www.postgresql.org/docs/12/functions.html) section of the PostgreSQL documentation for more information about these built-in functions and operators.

Greenplum Database includes JSON processing functions that manipulate values the `json` data type. For information about JSON data, see [Working with JSON Data](json-data.html).

<table class="table" id="topic29__in204913"><caption><span class="table--title-label">Table 3. </span><span class="title">Built-in functions and operators</span></caption><colgroup><col style="width:27.67527675276753%"><col style="width:18.45018450184502%"><col style="width:22.14022140221402%"><col style="width:31.73431734317343%"></colgroup><thead class="thead">
                        <tr class="row">
                            <th class="entry" id="topic29__in204913__entry__1">Operator/Function Category</th>
                            <th class="entry" id="topic29__in204913__entry__2">VOLATILE Functions</th>
                            <th class="entry" id="topic29__in204913__entry__3">STABLE Functions</th>
                            <th class="entry" id="topic29__in204913__entry__4">Restrictions</th>
                        </tr>
                    </thead><tbody class="tbody">
                        <tr class="row">
                            <td class="entry" headers="topic29__in204913__entry__1">
                                <a class="xref" href="https://www.postgresql.org/docs/12/functions-logical.html" target="_blank" rel="external noopener">Logical Operators</a>
                            </td>
                            <td class="entry" headers="topic29__in204913__entry__2"></td>
                            <td class="entry" headers="topic29__in204913__entry__3"></td>
                            <td class="entry" headers="topic29__in204913__entry__4"></td>
                        </tr>
                        <tr class="row">
                            <td class="entry" headers="topic29__in204913__entry__1">
                                <a class="xref" href="https://www.postgresql.org/docs/12/functions-comparison.html" target="_blank" rel="external noopener">Comparison Operators</a>
                            </td>
                            <td class="entry" headers="topic29__in204913__entry__2"></td>
                            <td class="entry" headers="topic29__in204913__entry__3"></td>
                            <td class="entry" headers="topic29__in204913__entry__4"></td>
                        </tr>
                        <tr class="row">
                            <td class="entry" headers="topic29__in204913__entry__1">
                                <a class="xref" href="https://www.postgresql.org/docs/12/functions-math.html" target="_blank" rel="external noopener">
                                    <span class="ph">Mathematical Functions and Operators</span>
                                </a>
                            </td>
                            <td class="entry" headers="topic29__in204913__entry__2">random<p class="p">setseed</p></td>
                            <td class="entry" headers="topic29__in204913__entry__3"></td>
                            <td class="entry" headers="topic29__in204913__entry__4"></td>
                        </tr>
                        <tr class="row">
                            <td class="entry" headers="topic29__in204913__entry__1">
                                <a class="xref" href="https://www.postgresql.org/docs/12/functions-string.html" target="_blank" rel="external noopener">
                                    <span class="ph">String Functions and Operators</span>
                                </a>
                            </td>
                            <td class="entry" headers="topic29__in204913__entry__2">
                                <em class="ph i">All built-in conversion functions</em>
                            </td>
                            <td class="entry" headers="topic29__in204913__entry__3">convert<p class="p">pg_client_encoding</p></td>
                            <td class="entry" headers="topic29__in204913__entry__4"></td>
                        </tr>
                        <tr class="row">
                            <td class="entry" headers="topic29__in204913__entry__1">
                                <a class="xref" href="https://www.postgresql.org/docs/12/functions-binarystring.html" target="_blank" rel="external noopener">
                                    <span class="ph">Binary String Functions and Operators</span>
                                </a>
                            </td>
                            <td class="entry" headers="topic29__in204913__entry__2"></td>
                            <td class="entry" headers="topic29__in204913__entry__3"></td>
                            <td class="entry" headers="topic29__in204913__entry__4"></td>
                        </tr>
                        <tr class="row">
                            <td class="entry" headers="topic29__in204913__entry__1">
                                <a class="xref" href="https://www.postgresql.org/docs/12/functions-bitstring.html" target="_blank" rel="external noopener">
                                    <span class="ph">Bit String Functions and Operators</span>
                                </a>
                            </td>
                            <td class="entry" headers="topic29__in204913__entry__2"></td>
                            <td class="entry" headers="topic29__in204913__entry__3"></td>
                            <td class="entry" headers="topic29__in204913__entry__4"></td>
                        </tr>
                        <tr class="row">
                            <td class="entry" headers="topic29__in204913__entry__1">
                                <a class="xref" href="https://www.postgresql.org/docs/12/functions-matching.html" target="_blank" rel="external noopener">
                                    <span class="ph">Pattern Matching</span>
                                </a>
                            </td>
                            <td class="entry" headers="topic29__in204913__entry__2"></td>
                            <td class="entry" headers="topic29__in204913__entry__3"></td>
                            <td class="entry" headers="topic29__in204913__entry__4"></td>
                        </tr>
                        <tr class="row">
                            <td class="entry" headers="topic29__in204913__entry__1">
                                <a class="xref" href="https://www.postgresql.org/docs/12/functions-formatting.html" target="_blank" rel="external noopener">
                                    <span class="ph">Data Type Formatting Functions</span>
                                </a>
                            </td>
                            <td class="entry" headers="topic29__in204913__entry__2"></td>
                            <td class="entry" headers="topic29__in204913__entry__3">to_char<p class="p">to_timestamp</p></td>
                            <td class="entry" headers="topic29__in204913__entry__4"></td>
                        </tr>
                        <tr class="row">
                            <td class="entry" headers="topic29__in204913__entry__1">
                                <a class="xref" href="https://www.postgresql.org/docs/12/functions-datetime.html" target="_blank" rel="external noopener"> Date/Time Functions and Operators</a>
                            </td>
                            <td class="entry" headers="topic29__in204913__entry__2">timeofday</td>
                            <td class="entry" headers="topic29__in204913__entry__3">age<p class="p">current_date</p><p class="p">current_time</p><p class="p">current_timestamp</p><p class="p">localtime</p><p class="p">localtimestamp</p><p class="p">now</p></td>
                            <td class="entry" headers="topic29__in204913__entry__4"></td>
                        </tr>
                        <tr class="row">
                            <td class="entry" headers="topic29__in204913__entry__1">
                                <a class="xref" href="https://www.postgresql.org/docs/12/functions-enum.html" target="_blank" rel="external noopener"> Enum Support Functions </a>
                            </td>
                            <td class="entry" headers="topic29__in204913__entry__2"></td>
                            <td class="entry" headers="topic29__in204913__entry__3"></td>
                            <td class="entry" headers="topic29__in204913__entry__4"></td>
                        </tr>
                        <tr class="row">
                            <td class="entry" headers="topic29__in204913__entry__1">
                                <a class="xref" href="https://www.postgresql.org/docs/12/functions-geometry.html" target="_blank" rel="external noopener">
                                    <span class="ph">Geometric Functions and Operators</span>
                                </a>
                            </td>
                            <td class="entry" headers="topic29__in204913__entry__2"></td>
                            <td class="entry" headers="topic29__in204913__entry__3"></td>
                            <td class="entry" headers="topic29__in204913__entry__4"></td>
                        </tr>
                        <tr class="row">
                            <td class="entry" headers="topic29__in204913__entry__1">
                                <a class="xref" href="https://www.postgresql.org/docs/12/functions-net.html" target="_blank" rel="external noopener">
                                    <span class="ph">Network Address Functions and Operators</span>
                                </a>
                            </td>
                            <td class="entry" headers="topic29__in204913__entry__2"></td>
                            <td class="entry" headers="topic29__in204913__entry__3"></td>
                            <td class="entry" headers="topic29__in204913__entry__4"></td>
                        </tr>
                        <tr class="row">
                            <td class="entry" headers="topic29__in204913__entry__1">
                                <a class="xref" href="https://www.postgresql.org/docs/12/functions-sequence.html" target="_blank" rel="external noopener">
                                    <span class="ph">Sequence Manipulation Functions</span>
                                </a>
                            </td>
                            <td class="entry" headers="topic29__in204913__entry__2">nextval()<p class="p">setval()</p></td>
                            <td class="entry" headers="topic29__in204913__entry__3"></td>
                            <td class="entry" headers="topic29__in204913__entry__4"></td>
                        </tr>
                        <tr class="row">
                            <td class="entry" headers="topic29__in204913__entry__1">
                                <a class="xref" href="https://www.postgresql.org/docs/12/functions-conditional.html" target="_blank" rel="external noopener">
                                    <span class="ph">Conditional Expressions</span>
                                </a>
                            </td>
                            <td class="entry" headers="topic29__in204913__entry__2"></td>
                            <td class="entry" headers="topic29__in204913__entry__3"></td>
                            <td class="entry" headers="topic29__in204913__entry__4"></td>
                        </tr>
                        <tr class="row">
                            <td class="entry" headers="topic29__in204913__entry__1">
                                <a class="xref" href="https://www.postgresql.org/docs/12/functions-array.html" target="_blank" rel="external noopener">
                                    <span class="ph">Array Functions and Operators</span>
                                </a>
                            </td>
                            <td class="entry" headers="topic29__in204913__entry__2"></td>
                            <td class="entry" headers="topic29__in204913__entry__3">
                                <em class="ph i">All array functions</em>
                            </td>
                            <td class="entry" headers="topic29__in204913__entry__4"></td>
                        </tr>
                        <tr class="row">
                            <td class="entry" headers="topic29__in204913__entry__1">
                                <a class="xref" href="https://www.postgresql.org/docs/12/functions-aggregate.html" target="_blank" rel="external noopener">
                                    <span class="ph">Aggregate Functions</span>
                                </a>
                            </td>
                            <td class="entry" headers="topic29__in204913__entry__2"></td>
                            <td class="entry" headers="topic29__in204913__entry__3"></td>
                            <td class="entry" headers="topic29__in204913__entry__4"></td>
                        </tr>
                        <tr class="row">
                            <td class="entry" headers="topic29__in204913__entry__1">
                                <a class="xref" href="https://www.postgresql.org/docs/12/functions-subquery.html" target="_blank" rel="external noopener">
                                    <span class="ph">Subquery Expressions</span>
                                </a>
                            </td>
                            <td class="entry" headers="topic29__in204913__entry__2"></td>
                            <td class="entry" headers="topic29__in204913__entry__3"></td>
                            <td class="entry" headers="topic29__in204913__entry__4"></td>
                        </tr>
                        <tr class="row">
                            <td class="entry" headers="topic29__in204913__entry__1">
                                <a class="xref" href="https://www.postgresql.org/docs/12/functions-comparisons.html" target="_blank" rel="external noopener">
                                    <span class="ph">Row and Array Comparisons</span>
                                </a>
                            </td>
                            <td class="entry" headers="topic29__in204913__entry__2"></td>
                            <td class="entry" headers="topic29__in204913__entry__3"></td>
                            <td class="entry" headers="topic29__in204913__entry__4"></td>
                        </tr>
                        <tr class="row">
                            <td class="entry" headers="topic29__in204913__entry__1">
                                <a class="xref" href="https://www.postgresql.org/docs/12/functions-srf.html" target="_blank" rel="external noopener">
                                    <span class="ph">Set Returning Functions</span>
                                </a>
                            </td>
                            <td class="entry" headers="topic29__in204913__entry__2">generate_series</td>
                            <td class="entry" headers="topic29__in204913__entry__3"></td>
                            <td class="entry" headers="topic29__in204913__entry__4"></td>
                        </tr>
                        <tr class="row">
                            <td class="entry" headers="topic29__in204913__entry__1">
                                <a class="xref" href="https://www.postgresql.org/docs/12/functions-info.html" target="_blank" rel="external noopener">
                                    <span class="ph">System Information Functions</span>
                                </a>
                            </td>
                            <td class="entry" headers="topic29__in204913__entry__2"></td>
                            <td class="entry" headers="topic29__in204913__entry__3">
                                <em class="ph i">All session information functions</em>
                                <p class="p">
                                    <em class="ph i">All access privilege inquiry functions</em>
                                </p><p class="p">
                                    <em class="ph i">All schema visibility inquiry functions</em>
                                </p><p class="p">
                                    <em class="ph i">All system catalog information functions</em>
                                </p><p class="p">
                                    <em class="ph i">All comment information functions</em>
                                </p><p class="p">
                                    <em class="ph i">All transaction ids and snapshots</em>
                                </p></td>
                            <td class="entry" headers="topic29__in204913__entry__4"></td>
                        </tr>
                        <tr class="row">
                            <td class="entry" headers="topic29__in204913__entry__1">
                                <a class="xref" href="https://www.postgresql.org/docs/12/functions-admin.html" target="_blank" rel="external noopener">
                                    <span class="ph">System Administration Functions</span>
                                </a>
                            </td>
                            <td class="entry" headers="topic29__in204913__entry__2">set_config<p class="p">pg_cancel_backend</p><p class="p">pg_terminate_backend</p><p class="p">pg_reload_conf</p><p class="p">pg_rotate_logfile</p><p class="p">pg_start_backup</p><p class="p">pg_stop_backup</p><p class="p">pg_size_pretty</p><p class="p">pg_ls_dir</p><p class="p">pg_read_file</p><p class="p">pg_stat_file</p></td>
                            <td class="entry" headers="topic29__in204913__entry__3">current_setting<p class="p"><em class="ph i">All database object size
                                        functions</em></p></td>
                            <td class="entry" headers="topic29__in204913__entry__4"><strong class="ph b">Note:</strong> The function
                                    <code class="ph codeph">pg_column_size</code> displays bytes required to store
                                the value, possibly with TOAST compression.</td>
                        </tr>
                        <tr class="row">
                            <td class="entry" headers="topic29__in204913__entry__1">
                                <a class="xref" href="https://www.postgresql.org/docs/12/functions-xml.html" target="_blank" rel="external noopener">XML Functions</a> and function-like
                                expressions </td>
                            <td class="entry" headers="topic29__in204913__entry__2"></td>
                            <td class="entry" headers="topic29__in204913__entry__3">
                                <p class="p">cursor_to_xml(cursor refcursor, count int, nulls boolean,
                                    tableforest boolean, targetns text)</p>
                                <p class="p">cursor_to_xmlschema(cursor refcursor, nulls boolean, tableforest
                                    boolean, targetns text)</p>
                                <p class="p">database_to_xml(nulls boolean, tableforest boolean, targetns
                                    text)</p>
                                <p class="p">database_to_xmlschema(nulls boolean, tableforest boolean,
                                    targetns text)</p>
                                <p class="p">database_to_xml_and_xmlschema( nulls boolean, tableforest
                                    boolean, targetns text)</p>
                                <p class="p">query_to_xml(query text, nulls boolean, tableforest boolean,
                                    targetns text)</p>
                                <p class="p">query_to_xmlschema(query text, nulls boolean, tableforest
                                    boolean, targetns text)</p>
                                <p class="p">query_to_xml_and_xmlschema( query text, nulls boolean,
                                    tableforest boolean, targetns text)</p>
                                <p class="p">schema_to_xml(schema name, nulls boolean, tableforest boolean,
                                    targetns text)</p>
                                <p class="p">schema_to_xmlschema( schema name, nulls boolean, tableforest
                                    boolean, targetns text)</p>
                                <p class="p">schema_to_xml_and_xmlschema( schema name, nulls boolean,
                                    tableforest boolean, targetns text)</p>
                                <p class="p">table_to_xml(tbl regclass, nulls boolean, tableforest boolean,
                                    targetns text)</p>
                                <p class="p">table_to_xmlschema( tbl regclass, nulls boolean, tableforest
                                    boolean, targetns text)</p>
                                <p class="p">table_to_xml_and_xmlschema( tbl regclass, nulls boolean,
                                    tableforest boolean, targetns text)</p>
                                <p class="p">xmlagg(xml)</p>
                                <p class="p">xmlconcat(xml[, ...])</p>
                                <p class="p">xmlelement(name name [, xmlattributes(value [AS attname] [, ...
                                    ])] [, content, ...])</p>
                                <p class="p">xmlexists(text, xml)</p>
                                <p class="p">xmlforest(content [AS name] [, ...])</p>
                                <p class="p">xml_is_well_formed(text)</p>
                                <p class="p">xml_is_well_formed_document(text)</p>
                                <p class="p">xml_is_well_formed_content(text)</p>
                                <p class="p">xmlparse ( { DOCUMENT | CONTENT } value)</p>
                                <p class="p">xpath(text, xml)</p>
                                <p class="p">xpath(text, xml, text[])</p>
                                <p class="p">xpath_exists(text, xml)</p>
                                <p class="p">xpath_exists(text, xml, text[])</p>
                                <p class="p">xmlpi(name target [, content])</p>
                                <p class="p">xmlroot(xml, version text | no value [, standalone yes|no|no
                                    value])</p>
                                <p class="p">xmlserialize ( { DOCUMENT | CONTENT } value AS type )</p>
                                <p class="p">xml(text)</p>
                                <p class="p">text(xml)</p>
                                <p class="p">xmlcomment(xml)</p>
                                <p class="p">xmlconcat2(xml, xml)</p>
                            </td>
                            <td class="entry" headers="topic29__in204913__entry__4"></td>
                        </tr>
                    </tbody></table>

## <a id="topic30"></a>Window Functions 

The following built-in window functions are Greenplum extensions to the PostgreSQL database. All window functions are *immutable*. For more information about window functions, see [Window Expressions](defining-queries.html).

<table class="table" id="topic30__in164369"><caption><span class="table--title-label">Table 4. </span><span class="title">Window functions</span></caption><colgroup><col style="width:22.86775087590859%"><col style="width:15.426449824818283%"><col style="width:39.21978769021597%"><col style="width:22.486011609057158%"></colgroup><thead class="thead">
                        <tr class="row">
                            <th class="entry" id="topic30__in164369__entry__1">Function</th>
                            <th class="entry" id="topic30__in164369__entry__2">Return Type</th>
                            <th class="entry" id="topic30__in164369__entry__3">Full Syntax</th>
                            <th class="entry" id="topic30__in164369__entry__4">Description</th>
                        </tr>
                    </thead><tbody class="tbody">
                        <tr class="row">
                            <td class="entry" headers="topic30__in164369__entry__1">
                                <code class="ph codeph">cume_dist()</code>
                            </td>
                            <td class="entry" headers="topic30__in164369__entry__2">
                                <code class="ph codeph">double precision</code>
                            </td>
                            <td class="entry" headers="topic30__in164369__entry__3">
                                <code class="ph codeph">CUME_DIST() OVER ( [PARTITION BY </code>
                                <span class="ph">expr</span>
                                <code class="ph codeph">] ORDER BY </code>
                                <span class="ph">expr</span>
                                <code class="ph codeph"> )</code>
                            </td>
                            <td class="entry" headers="topic30__in164369__entry__4">Calculates the cumulative distribution of a value
                                in a group of values. Rows with equal values always evaluate to the
                                same cumulative distribution value.</td>
                        </tr>
                        <tr class="row">
                            <td class="entry" headers="topic30__in164369__entry__1">
                                <code class="ph codeph">dense_rank()</code>
                            </td>
                            <td class="entry" headers="topic30__in164369__entry__2">
                                <code class="ph codeph">bigint</code>
                            </td>
                            <td class="entry" headers="topic30__in164369__entry__3">
                                <code class="ph codeph">DENSE_RANK () OVER ( [PARTITION BY </code>
                                <span class="ph">expr</span>
                                <code class="ph codeph">] ORDER BY </code>
                                <span class="ph">expr</span>
                                <code class="ph codeph">)</code>
                            </td>
                            <td class="entry" headers="topic30__in164369__entry__4">Computes the rank of a row in an ordered group of
                                rows without skipping rank values. Rows with equal values are given
                                the same rank value.</td>
                        </tr>
                        <tr class="row">
                            <td class="entry" headers="topic30__in164369__entry__1">
                                <code class="ph codeph">first_value(<em class="ph i">expr</em>)</code>
                            </td>
                            <td class="entry" headers="topic30__in164369__entry__2">same as input <span class="ph">expr</span> type</td>
                            <td class="entry" headers="topic30__in164369__entry__3">
                                <code class="ph codeph">FIRST_VALUE(</code>
                                <span class="ph">expr</span>
                                <code class="ph codeph">) OVER ( [PARTITION BY </code>
                                <span class="ph">expr</span>
                                <code class="ph codeph">] ORDER BY </code>
                                <span class="ph">expr</span>
                                <code class="ph codeph"> [ROWS|RANGE </code>
                                <span class="ph">frame_expr</span>
                                <code class="ph codeph">] )</code>
                            </td>
                            <td class="entry" headers="topic30__in164369__entry__4">Returns the first value in an ordered set of
                                values.</td>
                        </tr>
                        <tr class="row">
                            <td class="entry" headers="topic30__in164369__entry__1">
                                <code class="ph codeph">lag(<em class="ph i">expr</em> [,<em class="ph i">offset</em>] [,<em class="ph i">default</em>])</code>
                            </td>
                            <td class="entry" headers="topic30__in164369__entry__2">same as input <em class="ph i">expr</em> type</td>
                            <td class="entry" headers="topic30__in164369__entry__3">
                                <code class="ph codeph">LAG(</code>
                                <em class="ph i">expr</em>
                                <code class="ph codeph"> [,</code>
                                <em class="ph i">offset</em>
                                <code class="ph codeph">] [,</code>
                                <em class="ph i">default</em>
                                <code class="ph codeph">]) OVER ( [PARTITION BY </code>
                                <em class="ph i">expr</em>
                                <code class="ph codeph">] ORDER BY </code>
                                <em class="ph i">expr</em>
                                <code class="ph codeph"> )</code>
                            </td>
                            <td class="entry" headers="topic30__in164369__entry__4">Provides access to more than one row of the same
                                table without doing a self join. Given a series of rows returned
                                from a query and a position of the cursor, <code class="ph codeph">LAG</code>
                                provides access to a row at a given physical offset prior to that
                                position. The default <code class="ph codeph">offset</code> is 1. <em class="ph i">default</em>
                                sets the value that is returned if the offset goes beyond the scope
                                of the window. If <em class="ph i">default</em> is not specified, the default value
                                is null.</td>
                        </tr>
                        <tr class="row">
                            <td class="entry" headers="topic30__in164369__entry__1">
                                <code class="ph codeph">last_value(<em class="ph i">expr</em></code>)</td>
                            <td class="entry" headers="topic30__in164369__entry__2">same as input <em class="ph i">expr</em> type</td>
                            <td class="entry" headers="topic30__in164369__entry__3">
                                <code class="ph codeph">LAST_VALUE(<em class="ph i">expr</em>) OVER ( [PARTITION BY <em class="ph i">expr</em>]
                                    ORDER BY <em class="ph i">expr</em> [ROWS|RANGE <em class="ph i">frame_expr</em>] )</code>
                            </td>
                            <td class="entry" headers="topic30__in164369__entry__4">Returns the last value in an ordered set of
                                values.</td>
                        </tr>
                        <tr class="row">
                            <td class="entry" headers="topic30__in164369__entry__1">
                                <code class="ph codeph">
                                    <code class="ph codeph">lead(<em class="ph i">expr</em> [,<em class="ph i">offset</em>]
                                        [,<em class="ph i">default</em>])</code>
                                </code>
                            </td>
                            <td class="entry" headers="topic30__in164369__entry__2">same as input <em class="ph i">expr</em> type</td>
                            <td class="entry" headers="topic30__in164369__entry__3">
                                <code class="ph codeph">LEAD(<em class="ph i">expr </em>[,<em class="ph i">offset</em>]
                                        [,<em class="ph i">expr</em><em class="ph i">default</em>]) OVER ( [PARTITION BY
                                        <em class="ph i">expr</em>] ORDER BY <em class="ph i">expr</em> )</code>
                            </td>
                            <td class="entry" headers="topic30__in164369__entry__4">Provides access to more than one row of the same
                                table without doing a self join. Given a series of rows returned
                                from a query and a position of the cursor, <code class="ph codeph">lead</code>
                                provides access to a row at a given physical offset after that
                                position. If <em class="ph i">offset</em> is not specified, the default offset is
                                1. <em class="ph i">default</em> sets the value that is returned if the offset goes
                                beyond the scope of the window. If <em class="ph i">default</em> is not specified,
                                the default value is null.</td>
                        </tr>
                        <tr class="row">
                            <td class="entry" headers="topic30__in164369__entry__1">
                                <code class="ph codeph">ntile(<em class="ph i">expr</em>)</code>
                            </td>
                            <td class="entry" headers="topic30__in164369__entry__2"><code class="ph codeph">bigint</code></td>
                            <td class="entry" headers="topic30__in164369__entry__3">
                                <code class="ph codeph">NTILE(<em class="ph i">expr</em>) OVER ( [PARTITION BY <em class="ph i">expr</em>] ORDER
                                    BY <em class="ph i">expr</em> )</code>
                            </td>
                            <td class="entry" headers="topic30__in164369__entry__4">Divides an ordered data set into a number of
                                buckets (as defined by <em class="ph i">expr</em>) and assigns a bucket number to
                                each row.</td>
                        </tr>
                        <tr class="row">
                            <td class="entry" headers="topic30__in164369__entry__1"><code class="ph codeph">percent_rank()</code></td>
                            <td class="entry" headers="topic30__in164369__entry__2">
                                <code class="ph codeph">double precision</code>
                            </td>
                            <td class="entry" headers="topic30__in164369__entry__3">
                                <code class="ph codeph">PERCENT_RANK () OVER ( [PARTITION BY <em class="ph i">expr</em>] ORDER BY
                                        <em class="ph i">expr </em>)</code>
                            </td>
                            <td class="entry" headers="topic30__in164369__entry__4">Calculates the rank of a hypothetical row
                                    <code class="ph codeph">R</code> minus 1, divided by 1 less than the number of
                                rows being evaluated (within a window partition).</td>
                        </tr>
                        <tr class="row">
                            <td class="entry" headers="topic30__in164369__entry__1">
                                <code class="ph codeph">rank()</code>
                            </td>
                            <td class="entry" headers="topic30__in164369__entry__2"><code class="ph codeph">bigint</code></td>
                            <td class="entry" headers="topic30__in164369__entry__3">
                                <code class="ph codeph">RANK () OVER ( [PARTITION BY <em class="ph i">expr</em>] ORDER BY <em class="ph i">expr
                                    </em>)</code>
                            </td>
                            <td class="entry" headers="topic30__in164369__entry__4">Calculates the rank of a row in an ordered group
                                of values. Rows with equal values for the ranking criteria receive
                                the same rank. The number of tied rows are added to the rank number
                                to calculate the next rank value. Ranks may not be consecutive
                                numbers in this case.</td>
                        </tr>
                        <tr class="row">
                            <td class="entry" headers="topic30__in164369__entry__1"><code class="ph codeph">row_number()</code></td>
                            <td class="entry" headers="topic30__in164369__entry__2">
                                <code class="ph codeph">bigint</code>
                            </td>
                            <td class="entry" headers="topic30__in164369__entry__3">
                                <code class="ph codeph">ROW_NUMBER () OVER ( [PARTITION BY <em class="ph i">expr</em>] ORDER BY
                                        <em class="ph i">expr </em>)</code>
                            </td>
                            <td class="entry" headers="topic30__in164369__entry__4">Assigns a unique number to each row to which it is
                                applied (either each row in a window partition or each row of the
                                query).</td>
                        </tr>
                    </tbody></table>

## <a id="topic31"></a>Advanced Aggregate Functions 

The following built-in advanced aggregate functions are Greenplum extensions of the PostgreSQL database. These functions are *immutable*.

> **Note** The Greenplum MADlib Extension for Analytics provides additional advanced functions to perform statistical analysis and machine learning with Greenplum Database data. See [Greenplum MADlib Extension for Analytics](../../../analytics/madlib.html) in the *Greenplum Database Reference Guide*.

<table class="table" id="topic31__in2073121"><caption><span class="table--title-label">Table 5. </span><span class="title">Advanced Aggregate Functions</span></caption><colgroup><col style="width:23.00684070063499%"><col style="width:18.098714684499523%"><col style="width:31.2862357741035%"><col style="width:27.60820884076199%"></colgroup><thead class="thead">
                        <tr class="row">
                            <th class="entry" id="topic31__in2073121__entry__1">Function</th>
                            <th class="entry" id="topic31__in2073121__entry__2">Return Type</th>
                            <th class="entry" id="topic31__in2073121__entry__3">Full Syntax</th>
                            <th class="entry" id="topic31__in2073121__entry__4">Description</th>
                        </tr>
                    </thead><tbody class="tbody">
                        <tr class="row">
                            <td class="entry" headers="topic31__in2073121__entry__1">
                                <code class="ph codeph">MEDIAN (<em class="ph i">expr</em>)</code>
                            </td>
                            <td class="entry" headers="topic31__in2073121__entry__2">
                                <code class="ph codeph">timestamp, timestamptz, interval, float</code>
                            </td>
                            <td class="entry" headers="topic31__in2073121__entry__3">
                                <code class="ph codeph">MEDIAN (<em class="ph i">expression</em>)</code>
                                <p class="p">
                                    <em class="ph i">Example:</em>
                                </p><pre class="pre codeblock"><code>SELECT departmzent_id, MEDIAN(salary) 
  FROM employees 
GROUP BY department_id; </code></pre>
                            </td>
                            <td class="entry" headers="topic31__in2073121__entry__4">Can take a two-dimensional array as input. Treats
                                such arrays as matrices.</td>
                        </tr>
                        <tr class="row">
                            <td class="entry" headers="topic31__in2073121__entry__1">
                                <code class="ph codeph">sum(array[])</code>
                            </td>
                            <td class="entry" headers="topic31__in2073121__entry__2">
                                <code class="ph codeph">smallint[], int[], bigint[], float[]</code>
                            </td>
                            <td class="entry" headers="topic31__in2073121__entry__3">
                                <code class="ph codeph">sum(array[[1,2],[3,4]])</code>
                                <p class="p">
                                    <em class="ph i">Example:</em>
                                </p><pre class="pre codeblock"><code>CREATE TABLE mymatrix (myvalue int[]);
INSERT INTO mymatrix 
   VALUES (array[[1,2],[3,4]]);
INSERT INTO mymatrix 
   VALUES (array[[0,1],[1,0]]);
SELECT sum(myvalue) FROM mymatrix;
 sum 
\-\-\-\-\-\-\-\-\-\-\-\-\-\-\-
 {{1,3},{4,4}}</code></pre>
                            </td>
                            <td class="entry" headers="topic31__in2073121__entry__4">Performs matrix summation. Can take as input a
                                two-dimensional array that is treated as a matrix.</td>
                        </tr>
                        <tr class="row">
                            <td class="entry" headers="topic31__in2073121__entry__1">
                                <code class="ph codeph">pivot_sum (label[], label, expr)</code>
                            </td>
                            <td class="entry" headers="topic31__in2073121__entry__2">
                                <code class="ph codeph">int[], bigint[], float[]</code>
                            </td>
                            <td class="entry" headers="topic31__in2073121__entry__3">
                                <code class="ph codeph">pivot_sum( array['A1','A2'], attr, value)</code>
                            </td>
                            <td class="entry" headers="topic31__in2073121__entry__4">A pivot aggregation using sum to resolve duplicate
                                entries.</td>
                        </tr>
                        <tr class="row">
                            <td class="entry" headers="topic31__in2073121__entry__1">
                                <code class="ph codeph">unnest (array[])</code>
                            </td>
                            <td class="entry" headers="topic31__in2073121__entry__2">set of <code class="ph codeph">anyelement</code></td>
                            <td class="entry" headers="topic31__in2073121__entry__3">
                                <code class="ph codeph">unnest( array['one', 'row', 'per', 'item'])</code>
                            </td>
                            <td class="entry" headers="topic31__in2073121__entry__4">Transforms a one dimensional array into rows.
                                Returns a set of <code class="ph codeph">anyelement</code>, a polymorphic <a class="xref" href="https://www.postgresql.org/docs/12/datatype-pseudo.html" target="_blank" rel="external noopener"><span class="ph">pseudo-type</span></a> in
                                PostgreSQL.</td>
                        </tr>
                    </tbody></table>

