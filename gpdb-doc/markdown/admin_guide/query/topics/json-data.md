# Working with JSON Data 

Greenplum Database supports the `json` data type that stores JSON \(JavaScript Object Notation\) data.

Greenplum Database supports JSON as specified in the [RFC 7159](https://tools.ietf.org/html/rfc7159) document and enforces data validity according to the JSON rules. There are also JSON-specific functions and operators available for `json` data. See [JSON Functions and Operators](#topic_gn4_x3w_mq).

This section contains the following topics:

-   [About JSON Data](#topic_upc_tcs_fz)
-   [JSON Input and Output Syntax](#topic_isn_ltw_mq)
-   [Designing JSON documents](#topic_eyt_3tw_mq)
-   [JSON Functions and Operators](#topic_gn4_x3w_mq)

**Parent topic:**[Querying Data](../../query/topics/query.html)

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

