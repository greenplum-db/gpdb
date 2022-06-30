# Working with JSON Data 

Greenplum Database supports the `json` data type that stores JSON \(JavaScript Object Notation\) data.

Greenplum Database supports JSON as specified in the [RFC 7159](https://tools.ietf.org/html/rfc7159) document and enforces data validity according to the JSON rules. There are also JSON-specific functions and operators available for `json` data. See [JSON Functions and Operators](#topic_gn4_x3w_mq).

This section contains the following topics:

-   [About JSON Data](#topic_upc_tcs_fz)
-   [JSON Input and Output Syntax](#topic_isn_ltw_mq)
-   [Designing JSON documents](#topic_eyt_3tw_mq)
-   [JSON Functions and Operators](#topic_gn4_x3w_mq)

**Parent topic:** [Querying Data](../../query/topics/query.html)

## About JSON Data

When Greenplum Database stores data as `json` data type, an exact copy of the input text is stored and the JSON processing functions reparse the data on each execution.

-   Semantically-insignificant white space between tokens is retained, as well as the order of keys within JSON objects.
-   All key/value pairs are kept even if a JSON object contains duplicate keys. For duplicate keys, JSON processing functions consider the last value as the operative one.

Greenplum Database allows only one character set encoding per database. It is not possible for the `json` type to conform rigidly to the JSON specification unless the database encoding is UTF8. Attempts to include characters that cannot be represented in the database encoding will fail. Characters that can be represented in the database encoding but not in UTF8 are allowed.

The RFC 7159 document permits JSON strings to contain Unicode escape sequences denoted by `\uXXXX`. For the `json` type, the Greenplum Database input function allows Unicode escapes regardless of the database encoding and checks Unicode escapes only for syntactic correctness \(a `\u` followed by four hex digits\).

**Note:** Many of the JSON processing functions described in [JSON Functions and Operators](#topic_gn4_x3w_mq) convert Unicode escapes to regular characters. The functions throw an error for characters that cannot be represented in the database encoding. You should avoid mixing Unicode escapes in JSON with a non-UTF8 database encoding, if possible.

## JSON Input and Output Syntax 

The input and output syntax for the `json` data type is as specified in RFC 7159.

The following are all valid `json` expressions:

```
-- Simple scalar/primitive value
-- Primitive values can be numbers, quoted strings, true, false, or null
SELECT '5'::json;

-- Array of zero or more elements (elements need not be of same type)
SELECT '[1, 2, "foo", null]'::json;

-- Object containing pairs of keys and values
-- Note that object keys must always be quoted strings
SELECT '{"bar": "baz", "balance": 7.77, "active": false}'::json;

-- Arrays and objects can be nested arbitrarily
SELECT '{"foo": [true, "bar"], "tags": {"a": 1, "b": null}}'::json;
```

## Designing JSON documents 

Representing data as JSON can be considerably more flexible than the traditional relational data model, which is compelling in environments where requirements are fluid. It is quite possible for both approaches to co-exist and complement each other within the same application. However, even for applications where maximal flexibility is desired, it is still recommended that JSON documents have a somewhat fixed structure. The structure is typically unenforced \(though enforcing some business rules declaratively is possible\), but having a predictable structure makes it easier to write queries that usefully summarize a set of "documents" \(datums\) in a table.

JSON data is subject to the same concurrency-control considerations as any other data type when stored in a table. Although storing large documents is practicable, keep in mind that any update acquires a row-level lock on the whole row. Consider limiting JSON documents to a manageable size in order to decrease lock contention among updating transactions. Ideally, JSON documents should each represent an atomic datum that business rules dictate cannot reasonably be further subdivided into smaller datums that could be modified independently.

## JSON Functions and Operators 

Built-in functions and operators that create and manipulate JSON data.

-   [JSON Operators](#topic_o5y_14w_2z)
-   [JSON Creation Functions](#topic_u4s_wnw_2z)
-   [JSON Processing Functions](#topic_z5d_snw_2z)

**Note:** For `json` values, all key/value pairs are kept even if a JSON object contains duplicate keys. For duplicate keys, JSON processing functions consider the last value as the operative one.

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
|`array_to_json(anyarray [, pretty_bool])`|Returns the array as a JSON array. A Greenplum Database multidimensional array becomes a JSON array of arrays. Line feeds are added between dimension 1 elements if pretty\_bool is `true`.|`array_to_json('{{1,5},{99,100}}'::int[])`|`[[1,5],[99,100]]`|
|`row_to_json(record [, pretty_bool])`|Returns the row as a JSON object. Line feeds are added between level 1 elements if `pretty_bool` is `true`.|`row_to_json(row(1,'foo'))`|`{"f1":1,"f2":"foo"}`|

### JSON Processing Functions 

This table describes the functions that process `json` values.

<div class="tablenoborder">
<table cellpadding="4" cellspacing="0" summary="" id="topic_z5d_snw_2z__table_wfc_y3w_mq" class="table" frame="border" border="1" rules="all">
<caption><span class="tablecap"></span></caption>         
            <thead class="thead" align="left">
              <tr class="row">
                <th class="entry" valign="top" width="20.224719101123597%" id="d104919e582">Function</th>
                <th class="entry" valign="top" width="18.726591760299627%" id="d104919e585">Return Type</th>
                <th class="entry" valign="top" width="18.913857677902623%" id="d104919e588">Description</th>
                <th class="entry" valign="top" width="23.220973782771537%" id="d104919e591">Example</th>
                <th class="entry" valign="top" width="18.913857677902623%" id="d104919e594">Example Result</th>
              </tr>
            </thead>
            <tbody class="tbody">
              <tr class="row">
                <td class="entry" valign="top" width="20.224719101123597%" headers="d104919e582 "><samp class="ph codeph">json_each(json)</samp></td>
                <td class="entry" valign="top" width="18.726591760299627%" headers="d104919e585 ">
                  <samp class="ph codeph">setof key text, value json</samp>
                  <p class="p">
                    <samp class="ph codeph">setof key text, value jsonb</samp>
                  </p>
                </td>
                <td class="entry" valign="top" width="18.913857677902623%" headers="d104919e588 ">Expands the outermost JSON object into a set of key/value pairs.</td>
                <td class="entry" valign="top" width="23.220973782771537%" headers="d104919e591 ">
<samp class="ph codeph">select * from json_each('{"a":"foo", "b":"bar"}')</samp>
                </td>
                <td class="entry" valign="top" width="18.913857677902623%" headers="d104919e594 ">
                  <pre class="pre"> key | value
-----+-------
 a   | "foo"
 b   | "bar"
</pre>
                </td>
              </tr>
              <tr class="row">
                <td class="entry" valign="top" width="20.224719101123597%" headers="d104919e582 "><samp class="ph codeph">json_each_text(json)</samp></td>
                <td class="entry" valign="top" width="18.726591760299627%" headers="d104919e585 ">
<samp class="ph codeph">setof key text, value text</samp>
                </td>
                <td class="entry" valign="top" width="18.913857677902623%" headers="d104919e588 ">Expands the outermost JSON object into a set of key/value pairs. The returned
                  values are of type <samp class="ph codeph">text</samp>.</td>
                <td class="entry" valign="top" width="23.220973782771537%" headers="d104919e591 ">
<samp class="ph codeph">select * from json_each_text('{"a":"foo", "b":"bar"}')</samp>
                </td>
                <td class="entry" valign="top" width="18.913857677902623%" headers="d104919e594 ">
                  <pre class="pre"> key | value
-----+-------
 a   | foo
 b   | bar
</pre>
                </td>
              </tr>
              <tr class="row">
                <td class="entry" valign="top" width="20.224719101123597%" headers="d104919e582 "><samp class="ph codeph">json_extract_path(from_json json, VARIADIC path_elems
                    text[])</samp></td>
                <td class="entry" valign="top" width="18.726591760299627%" headers="d104919e585 ">
<samp class="ph codeph">json</samp>
                </td>
                <td class="entry" valign="top" width="18.913857677902623%" headers="d104919e588 ">Returns the JSON value specified to by <samp class="ph codeph">path_elems</samp>.
                  Equivalent to <samp class="ph codeph">#&gt;</samp> operator.</td>
                <td class="entry" valign="top" width="23.220973782771537%" headers="d104919e591 ">
                  <samp class="ph codeph">json_extract_path('{"f2":{"f3":1},"f4":{"f5":99,"f6":"foo"}}','f4')</samp>
                </td>
                <td class="entry" valign="top" width="18.913857677902623%" headers="d104919e594 ">
                  <samp class="ph codeph">{"f5":99,"f6":"foo"}</samp>
                </td>
              </tr>
              <tr class="row">
                <td class="entry" valign="top" width="20.224719101123597%" headers="d104919e582 ">
                  <p class="p"><samp class="ph codeph">json_extract_path_text(from_json json, VARIADIC path_elems
                      text[])</samp></p>
                </td>
                <td class="entry" valign="top" width="18.726591760299627%" headers="d104919e585 ">
<samp class="ph codeph">text</samp>
                </td>
                <td class="entry" valign="top" width="18.913857677902623%" headers="d104919e588 ">Returns the JSON value specified to by <samp class="ph codeph">path_elems</samp> as text.
                  Equivalent to <samp class="ph codeph">#&gt;&gt;</samp> operator.</td>
                <td class="entry" valign="top" width="23.220973782771537%" headers="d104919e591 ">
                  <samp class="ph codeph">json_extract_path_text('{"f2":{"f3":1},"f4":{"f5":99,"f6":"foo"}}','f4',
                    'f6')</samp>
                </td>
                <td class="entry" valign="top" width="18.913857677902623%" headers="d104919e594 ">
                  <samp class="ph codeph">foo</samp>
                </td>
              </tr>
              <tr class="row">
                <td class="entry" valign="top" width="20.224719101123597%" headers="d104919e582 "><samp class="ph codeph">json_object_keys(json)</samp></td>
                <td class="entry" valign="top" width="18.726591760299627%" headers="d104919e585 ">
<samp class="ph codeph">setof text</samp>
                </td>
                <td class="entry" valign="top" width="18.913857677902623%" headers="d104919e588 ">Returns set of keys in the outermost JSON object.</td>
                <td class="entry" valign="top" width="23.220973782771537%" headers="d104919e591 ">
                  <samp class="ph codeph">json_object_keys('{"f1":"abc","f2":{"f3":"a", "f4":"b"}}')</samp>
                </td>
                <td class="entry" valign="top" width="18.913857677902623%" headers="d104919e594 ">
                  <pre class="pre"> json_object_keys
------------------
 f1
 f2
</pre>
                </td>
              </tr>
              <tr class="row">
                <td class="entry" valign="top" width="20.224719101123597%" headers="d104919e582 "><samp class="ph codeph">json_populate_record(base anyelement, from_json
                  json)</samp></td>
                <td class="entry" valign="top" width="18.726591760299627%" headers="d104919e585 ">
<samp class="ph codeph">anyelement</samp>
                </td>
                <td class="entry" valign="top" width="18.913857677902623%" headers="d104919e588 ">Expands the object in <samp class="ph codeph">from_json</samp> to a row whose columns match
                  the record type defined by base. See <a class="xref" href="#topic_z5d_snw_2z__json-note">Note</a>.</td>
                <td class="entry" valign="top" width="23.220973782771537%" headers="d104919e591 ">
                  <samp class="ph codeph">select * from json_populate_record(null::myrowtype,
                    '{"a":1,"b":2}')</samp>
                </td>
                <td class="entry" valign="top" width="18.913857677902623%" headers="d104919e594 ">
                  <pre class="pre"> a | b
---+---
 1 | 2
</pre>
                </td>
              </tr>
              <tr class="row">
                <td class="entry" valign="top" width="20.224719101123597%" headers="d104919e582 ">
<samp class="ph codeph">json_populate_recordset(base anyelement, from_json json)</samp>
                </td>
                <td class="entry" valign="top" width="18.726591760299627%" headers="d104919e585 ">
<samp class="ph codeph">setof anyelement</samp>
                </td>
                <td class="entry" valign="top" width="18.913857677902623%" headers="d104919e588 ">Expands the outermost array of objects in <samp class="ph codeph">from_json</samp> to a set
                  of rows whose columns match the record type defined by <samp class="ph codeph">base</samp>. See
                    <a class="xref" href="#topic_z5d_snw_2z__json-note">Note</a>.</td>
                <td class="entry" valign="top" width="23.220973782771537%" headers="d104919e591 ">
                  <samp class="ph codeph">select * from json_populate_recordset(null::myrowtype,
                    '[{"a":1,"b":2},{"a":3,"b":4}]')</samp>
                </td>
                <td class="entry" valign="top" width="18.913857677902623%" headers="d104919e594 ">
                  <pre class="pre"> a | b
---+---
 1 | 2
 3 | 4
</pre>
                </td>
              </tr>
              <tr class="row">
                <td class="entry" valign="top" width="20.224719101123597%" headers="d104919e582 "><samp class="ph codeph">json_array_elements(json)</samp></td>
                <td class="entry" valign="top" width="18.726591760299627%" headers="d104919e585 "><samp class="ph codeph">setof json</samp></td>
                <td class="entry" valign="top" width="18.913857677902623%" headers="d104919e588 ">Expands a JSON array to a set of JSON values.</td>
                <td class="entry" valign="top" width="23.220973782771537%" headers="d104919e591 ">
<samp class="ph codeph">select * from json_array_elements('[1,true, [2,false]]')</samp>
                </td>
                <td class="entry" valign="top" width="18.913857677902623%" headers="d104919e594 ">
                  <pre class="pre">   value
-----------
 1
 true
 [2,false]
</pre>
                </td>
              </tr>
            </tbody>
          </table>
</div>

**Note:** Many of these functions and operators convert Unicode escapes in JSON strings to regular characters. The functions throw an error for characters that cannot be represented in the database encoding.

For `json_populate_record` and `json_populate_recordset`, type coercion from JSON is best effort and might not result in desired values for some types. JSON keys are matched to identical column names in the target row type. JSON fields that do not appear in the target row type are omitted from the output, and target columns that do not match any JSON field return `NULL`.
