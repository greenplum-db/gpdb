---
title: Working with JSON Data 
---

Greenplum Database supports the `json` and `jsonb` data types that store JSON \(JavaScript Object Notation\) data.

Greenplum Database supports JSON as specified in the [RFC 7159](https://tools.ietf.org/html/rfc7159) document and enforces data validity according to the JSON rules. There are also JSON-specific functions and operators available for the `json` and `jsonb` data types. See [JSON Functions and Operators](#topic_gn4_x3w_mq).

This section contains the following topics:

-   [About JSON Data](#topic_upc_tcs_fz)
-   [JSON Input and Output Syntax](#topic_isn_ltw_mq)
-   [Designing JSON documents](#topic_eyt_3tw_mq)
-   [jsonb Containment and Existence](#topic_isx_2tw_mq)
-   [jsonb Indexing](#topic_aqt_1tw_mq)
-   [JSON Functions and Operators](#topic_gn4_x3w_mq)

**Parent topic:**[Querying Data](../../query/topics/query.html)

## <a id="topic_upc_tcs_fz"></a>About JSON Data 

Greenplum Database supports two JSON data types: `json` and `jsonb`. They accept almost identical sets of values as input. The major difference is one of efficiency.

-   The `json` data type stores an exact copy of the input text. This requires JSON processing functions to reparse `json` data on each execution. The `json` data type does not alter the input text.
    -   Semantically-insignificant white space between tokens is retained, as well as the order of keys within JSON objects.
    -   All key/value pairs are kept even if a JSON object contains duplicate keys. For duplicate keys, JSON processing functions consider the last value as the operative one.
-   The `jsonb` data type stores a decomposed binary format of the input text. The conversion overhead makes data input slightly slower than the `json` data type. However, The JSON processing functions are significantly faster because reparsing `jsonb` data is not required. The `jsonb` data type alters the input text.

    -   White space is not preserved.
    -   The order of object keys is not preserved.
    -   Duplicate object keys are not kept. If the input includes duplicate keys, only the last value is kept.
    The `jsonb` data type supports indexing. See [jsonb Indexing](#topic_aqt_1tw_mq).


In general, JSON data should be stored as the `jsonb` data type unless there are specialized needs, such as legacy assumptions about ordering of object keys.

### <a id="abuni"></a>About Unicode Characters in JSON Data 

The [RFC 7159](https://tools.ietf.org/html/rfc7159) document permits JSON strings to contain Unicode escape sequences denoted by `\uXXXX`. However, Greenplum Database allows only one character set encoding per database. It is not possible for the `json` data type to conform rigidly to the JSON specification unless the database encoding is UTF8. Attempts to include characters that cannot be represented in the database encoding will fail. Characters that can be represented in the database encoding, but not in UTF8, are allowed.

-   The Greenplum Database input function for the `json` data type allows Unicode escapes regardless of the database encoding and checks Unicode escapes only for syntactic correctness \(a `\u` followed by four hex digits\).
-   The Greenplum Database input function for the `jsonb` data type is more strict. It does not allow Unicode escapes for non-ASCII characters \(those above `U+007F`\) unless the database encoding is UTF8. It also rejects `\u0000`, which cannot be represented in the Greenplum Database `text` type, and it requires that any use of Unicode surrogate pairs to designate characters outside the Unicode Basic Multilingual Plane be correct. Valid Unicode escapes, except for `\u0000`, are converted to the equivalent ASCII or UTF8 character for storage; this includes folding surrogate pairs into a single character.

**Note:** Many of the JSON processing functions described in [JSON Functions and Operators](#topic_gn4_x3w_mq) convert Unicode escapes to regular characters. The functions throw an error for characters that cannot be represented in the database encoding. You should avoid mixing Unicode escapes in JSON with a non-UTF8 database encoding, if possible.

### <a id="mapjson"></a>Mapping JSON Data Types to Greenplum Data Types 

When converting JSON text input into `jsonb` data, the primitive data types described by RFC 7159 are effectively mapped onto native Greenplum Database data types, as shown in the following table.

|JSON primitive data type|Greenplum Database data type|Notes|
|------------------------|----------------------------|-----|
|`string`|`text`|`\u0000` is not allowed. Non-ASCII Unicode escapes are allowed only if database encoding is UTF8|
|`number`|`numeric`|`NaN` and `infinity` values are disallowed|
|`boolean`|`boolean`|Only lowercase `true` and `false` spellings are accepted|
|`null`|\(none\)|The JSON `null` primitive type is different than the SQL `NULL`.|

There are some minor constraints on what constitutes valid `jsonb` data that do not apply to the `json` data type, nor to JSON in the abstract, corresponding to limits on what can be represented by the underlying data type. Notably, when converting data to the `jsonb` data type, numbers that are outside the range of the Greenplum Database `numeric` data type are rejected, while the `json` data type does not reject such numbers.

Such implementation-defined restrictions are permitted by RFC 7159. However, in practice such problems might occur in other implementations, as it is common to represent the JSON `number` primitive type as IEEE 754 double precision floating point \(which RFC 7159 explicitly anticipates and allows for\).

When using JSON as an interchange format with other systems, be aware of the possibility of losing numeric precision compared to data originally stored by Greenplum Database.

Also, as noted in the previous table, there are some minor restrictions on the input format of JSON primitive types that do not apply to the corresponding Greenplum Database data types.

## <a id="topic_isn_ltw_mq"></a>JSON Input and Output Syntax 

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

As previously stated, when a JSON value is input and then printed without any additional processing, the `json` data type outputs the same text that was input, while the `jsonb` data type does not preserve semantically-insignificant details such as whitespace. For example, note the differences here:

```
SELECT '{"bar": "baz", "balance": 7.77, "active":false}'::json;
                      json                       
-------------------------------------------------
 {"bar": "baz", "balance": 7.77, "active":false}
(1 row)

SELECT '{"bar": "baz", "balance": 7.77, "active":false}'::jsonb;
                      jsonb                       
--------------------------------------------------
 {"bar": "baz", "active": false, "balance": 7.77}
(1 row)
```

One semantically-insignificant detail worth noting is that with the `jsonb` data type, numbers will be printed according to the behavior of the underlying numeric type. In practice, this means that numbers entered with E notation will be printed without it, for example:

```
SELECT '{"reading": 1.230e-5}'::json, '{"reading": 1.230e-5}'::jsonb;
         json          |          jsonb          
-----------------------+-------------------------
 {"reading": 1.230e-5} | {"reading": 0.00001230}
(1 row)
```

However, the `jsonb` data type preserves trailing fractional zeroes, as seen in previous example, even though those are semantically insignificant for purposes such as equality checks.

## <a id="topic_eyt_3tw_mq"></a>Designing JSON documents 

Representing data as JSON can be considerably more flexible than the traditional relational data model, which is compelling in environments where requirements are fluid. It is quite possible for both approaches to co-exist and complement each other within the same application. However, even for applications where maximal flexibility is desired, it is still recommended that JSON documents have a somewhat fixed structure. The structure is typically unenforced \(though enforcing some business rules declaratively is possible\), but having a predictable structure makes it easier to write queries that usefully summarize a set of JSON documents \(datums\) in a table.

JSON data is subject to the same concurrency-control considerations as any other data type when stored in a table. Although storing large documents is practicable, keep in mind that any update acquires a row-level lock on the whole row. Consider limiting JSON documents to a manageable size in order to decrease lock contention among updating transactions. Ideally, JSON documents should each represent an atomic datum that business rules dictate cannot reasonably be further subdivided into smaller datums that could be modified independently.

## <a id="topic_isx_2tw_mq"></a>jsonb Containment and Existence 

Testing *containment* is an important capability of `jsonb`. There is no parallel set of facilities for the `json` type. Containment tests whether one `jsonb` document has contained within it another one. These examples return true except as noted:

```
-- Simple scalar/primitive values contain only the identical value:
SELECT '"foo"'::jsonb @> '"foo"'::jsonb;

-- The array on the right side is contained within the one on the left:
SELECT '[1, 2, 3]'::jsonb @> '[1, 3]'::jsonb;

-- Order of array elements is not significant, so this is also true:
SELECT '[1, 2, 3]'::jsonb @> '[3, 1]'::jsonb;

-- Duplicate array elements don't matter either:
SELECT '[1, 2, 3]'::jsonb @> '[1, 2, 2]'::jsonb;

-- The object with a single pair on the right side is contained
-- within the object on the left side:
SELECT '{"product": "Greenplum", "version": "7.0.0", "jsonb":true}'::jsonb @> '{"version":"7.0.0"}'::jsonb;

-- The array on the right side is not considered contained within the
-- array on the left, even though a similar array is nested within it:
SELECT '[1, 2, [1, 3]]'::jsonb @> '[1, 3]'::jsonb;  -- yields false

-- But with a layer of nesting, it is contained:
SELECT '[1, 2, [1, 3]]'::jsonb @> '[[1, 3]]'::jsonb;

-- Similarly, containment is not reported here:
SELECT '{"foo": {"bar": "baz", "zig": "zag"}}'::jsonb @> '{"bar": "baz"}'::jsonb; -- yields false

-- But with a layer of nesting, it is contained:
SELECT '{"foo": {"bar": "baz", "zig": "zag"}}'::jsonb @> '{"foo": {"bar": "baz"}}'::jsonb;
```

The general principle is that the contained object must match the containing object as to structure and data contents, possibly after discarding some non-matching array elements or object key/value pairs from the containing object. For containment, the order of array elements is not significant when doing a containment match, and duplicate array elements are effectively considered only once.

As an exception to the general principle that the structures must match, an array may contain a primitive value:

```
-- This array contains the primitive string value:
SELECT '["foo", "bar"]'::jsonb @> '"bar"'::jsonb;

-- This exception is not reciprocal -- non-containment is reported here:
SELECT '"bar"'::jsonb @> '["bar"]'::jsonb;  -- yields false
```

`jsonb` also has an *existence* operator, which is a variation on the theme of containment: it tests whether a string \(given as a text value\) appears as an object key or array element at the top level of the `jsonb` value. These examples return true except as noted:

```
-- String exists as array element:
SELECT '["foo", "bar", "baz"]'::jsonb ? 'bar';

-- String exists as object key:
SELECT '{"foo": "bar"}'::jsonb ? 'foo';

-- Object values are not considered:
SELECT '{"foo": "bar"}'::jsonb ? 'bar';  -- yields false

-- As with containment, existence must match at the top level:
SELECT '{"foo": {"bar": "baz"}}'::jsonb ? 'bar'; -- yields false

-- A string is considered to exist if it matches a primitive JSON string:
SELECT '"foo"'::jsonb ? 'foo';
```

JSON objects are better suited than arrays for testing containment or existence when there are many keys or elements involved, because unlike arrays they are internally optimized for searching, and do not need to be searched linearly.

The various containment and existence operators, along with all other JSON operators and functions are documented in [JSON Functions and Operators](#topic_gn4_x3w_mq).

Because JSON containment is nested, an appropriate query can skip explicit selection of sub-objects. As an example, suppose that we have a doc column containing objects at the top level, with most objects containing tags fields that contain arrays of sub-objects. This query finds entries in which sub-objects containing both `"term":"paris"` and `"term":"food"` appear, while ignoring any such keys outside the tags array:

```
SELECT doc->'site_name' FROM websites
  WHERE doc @> '{"tags":[{"term":"paris"}, {"term":"food"}]}';
```

The query with this predicate could accomplish the same thing.

```
SELECT doc->'site_name' FROM websites
  WHERE doc->'tags' @> '[{"term":"paris"}, {"term":"food"}]';
```

However, the second approach is less flexible and is often less efficient as well.

On the other hand, the JSON existence operator is not nested: it will only look for the specified key or array element at top level of the JSON value.

## <a id="topic_aqt_1tw_mq"></a>jsonb Indexing 

The Greenplum Database `jsonb` data type, supports GIN, btree, and hash indexes.

-   [GIN Indexes on jsonb Data](#topic_yjx_dq2_rfb)
-   [Btree and Hash Indexes on jsonb Data](#topic_ahb_5ly_wq)

### <a id="topic_yjx_dq2_rfb"></a>GIN Indexes on jsonb Data 

GIN indexes can be used to efficiently search for keys or key/value pairs occurring within a large number of `jsonb` documents \(datums\). Two GIN operator classes are provided, offering different performance and flexibility trade-offs.

The default GIN operator class for jsonb supports queries with the `@>`, `?`, `?&` and `?|` operators. \(For details of the semantics that these operators implement, see the table [Table 3](#table_dcb_y3w_mq).\) An example of creating an index with this operator class is:

```
CREATE INDEX idxgin ON api USING gin (jdoc);
```

The non-default GIN operator class `jsonb_path_ops` supports indexing the `@>` operator only. An example of creating an index with this operator class is:

```
CREATE INDEX idxginp ON api USING gin (jdoc jsonb_path_ops);
```

Consider the example of a table that stores JSON documents retrieved from a third-party web service, with a documented schema definition. This is a typical document:

```
{
    "guid": "9c36adc1-7fb5-4d5b-83b4-90356a46061a",
    "name": "Angela Barton",
    "is_active": true,
    "company": "Magnafone",
    "address": "178 Howard Place, Gulf, Washington, 702",
    "registered": "2009-11-07T08:53:22 +08:00",
    "latitude": 19.793713,
    "longitude": 86.513373,
    "tags": [
        "enim",
        "aliquip",
        "qui"
    ]
}
```

The JSON documents are stored a table named `api`, in a `jsonb` column named `jdoc`. If a GIN index is created on this column, queries like the following can make use of the index:

```
-- Find documents in which the key "company" has value "Magnafone"
SELECT jdoc->'guid', jdoc->'name' FROM api WHERE jdoc @> '{"company": "Magnafone"}';
```

However, the index could not be used for queries like the following. The operator `?` is indexable, however, the comparison is not applied directly to the indexed column `jdoc`:

```
-- Find documents in which the key "tags" contains key or array element "qui"
SELECT jdoc->'guid', jdoc->'name' FROM api WHERE jdoc -> 'tags' ? 'qui';
```

With appropriate use of expression indexes, the above query can use an index. If querying for particular items within the `tags` key is common, defining an index like this might be worthwhile:

```
CREATE INDEX idxgintags ON api USING gin ((jdoc -> 'tags'));
```

Now, the `WHERE` clause `jdoc -> 'tags' ? 'qui'` is recognized as an application of the indexable operator `?` to the indexed expression `jdoc -> 'tags'`. For information about expression indexes, see [Indexes on Expressions](../../ddl/ddl-index.html).

Another approach to querying JSON documents is to exploit containment, for example:

```
-- Find documents in which the key "tags" contains array element "qui"
SELECT jdoc->'guid', jdoc->'name' FROM api WHERE jdoc @> '{"tags": ["qui"]}';

```

A simple GIN index on the `jdoc` column can support this query. However, the index will store copies of every key and value in the `jdoc` column, whereas the expression index of the previous example stores only data found under the tags key. While the simple-index approach is far more flexible \(since it supports queries about any key\), targeted expression indexes are likely to be smaller and faster to search than a simple index.

Although the `jsonb_path_ops` operator class supports only queries with the `@>` operator, it has performance advantages over the default operator class `jsonb_ops`. A `jsonb_path_ops` index is usually much smaller than a `jsonb_ops` index over the same data, and the specificity of searches is better, particularly when queries contain keys that appear frequently in the data. Therefore search operations typically perform better than with the default operator class.

The technical difference between a `jsonb_ops` and a `jsonb_path_ops` GIN index is that the former creates independent index items for each key and value in the data, while the latter creates index items only for each value in the data.

**Note:** For this discussion, the term *value* includes array elements, though JSON terminology sometimes considers array elements distinct from values within objects.

Basically, each `jsonb_path_ops` index item is a hash of the value and the key\(s\) leading to it; for example to index `{"foo": {"bar": "baz"}}`, a single index item would be created incorporating all three of `foo`, `bar`, and `baz` into the hash value. Thus a containment query looking for this structure would result in an extremely specific index search; but there is no way at all to find out whether `foo` appears as a key. On the other hand, a `jsonb_ops` index would create three index items representing `foo`, `bar`, and `baz` separately; then to do the containment query, it would look for rows containing all three of these items. While GIN indexes can perform such an `AND` search fairly efficiently, it will still be less specific and slower than the equivalent `jsonb_path_ops` search, especially if there are a very large number of rows containing any single one of the three index items.

A disadvantage of the `jsonb_path_ops` approach is that it produces no index entries for JSON structures not containing any values, such as `{"a": {}}`. If a search for documents containing such a structure is requested, it will require a full-index scan, which is quite slow. `jsonb_path_ops` is ill-suited for applications that often perform such searches.

### <a id="topic_ahb_5ly_wq"></a>Btree and Hash Indexes on jsonb Data 

`jsonb` also supports `btree` and `hash` indexes. These are usually useful only when it is important to check the equality of complete JSON documents.

For completeness the `btree` ordering for `jsonb` datums is:

```
Object > Array > Boolean > Number > String > Null

Object with n pairs > object with n - 1 pairs

Array with n elements > array with n - 1 elements
```

Objects with equal numbers of pairs are compared in the order:

```
key-1, value-1, key-2 ...
```

Object keys are compared in their storage order. In particular, since shorter keys are stored before longer keys, this can lead to orderings that might not be intuitive, such as:

```
{ "aa": 1, "c": 1} > {"b": 1, "d": 1}
```

Similarly, arrays with equal numbers of elements are compared in the order:

```
element-1, element-2 ...
```

Primitive JSON values are compared using the same comparison rules as for the underlying Greenplum Database data type. Strings are compared using the default database collation.

## <a id="topic_gn4_x3w_mq"></a>JSON Functions and Operators 

Greenplum Database includes built-in functions and operators that create and manipulate JSON data.

-   [JSON Operators](#topic_o5y_14w_2z)
-   [JSON Creation Functions](#topic_u4s_wnw_2z)
-   [JSON Aggregate Functions](#topic_rvp_lk3_sfb)
-   [JSON Processing Functions](#topic_z5d_snw_2z)

**Note:** For `json` data type values, all key/value pairs are kept even if a JSON object contains duplicate keys. For duplicate keys, JSON processing functions consider the last value as the operative one. For the `jsonb` data type, duplicate object keys are not kept. If the input includes duplicate keys, only the last value is kept. See [About JSON Data](#topic_upc_tcs_fz).

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

Operators that require the `jsonb` data type as the left operand are described in the following table. Many of these operators can be indexed by `jsonb` operator classes. For a full description of `jsonb` containment and existence semantics, see [jsonb Containment and Existence](#topic_isx_2tw_mq). For information about how these operators can be used to effectively index `jsonb` data, see [jsonb Indexing](#topic_aqt_1tw_mq).

<table class="table" id="topic_o5y_14w_2z__table_dcb_y3w_mq"><caption><span class="table--title-label">Table 3. </span><span class="title">jsonb Operators</span></caption><colgroup><col style="width:9.970089730807578%"><col style="width:19.3419740777667%"><col style="width:45.76271186440678%"><col style="width:24.925224327018945%"></colgroup><thead class="thead">
              <tr class="row">
                <th class="entry" id="topic_o5y_14w_2z__table_dcb_y3w_mq__entry__1">Operator</th>
                <th class="entry" id="topic_o5y_14w_2z__table_dcb_y3w_mq__entry__2">Right Operand Type</th>
                <th class="entry" id="topic_o5y_14w_2z__table_dcb_y3w_mq__entry__3">Description</th>
                <th class="entry" id="topic_o5y_14w_2z__table_dcb_y3w_mq__entry__4">Example</th>
              </tr>
            </thead><tbody class="tbody">
              <tr class="row">
                <td class="entry" headers="topic_o5y_14w_2z__table_dcb_y3w_mq__entry__1">
                  <code class="ph codeph">@&gt;</code>
                </td>
                <td class="entry" headers="topic_o5y_14w_2z__table_dcb_y3w_mq__entry__2">
                  <code class="ph codeph">jsonb</code>
                </td>
                <td class="entry" headers="topic_o5y_14w_2z__table_dcb_y3w_mq__entry__3">Does the left JSON value contain within it the right value?</td>
                <td class="entry" headers="topic_o5y_14w_2z__table_dcb_y3w_mq__entry__4">
                  <code class="ph codeph">'{"a":1, "b":2}'::jsonb @&gt; '{"b":2}'::jsonb</code>
                </td>
              </tr>
              <tr class="row">
                <td class="entry" headers="topic_o5y_14w_2z__table_dcb_y3w_mq__entry__1">
                  <code class="ph codeph">&lt;@</code>
                </td>
                <td class="entry" headers="topic_o5y_14w_2z__table_dcb_y3w_mq__entry__2">
                  <code class="ph codeph">jsonb</code>
                </td>
                <td class="entry" headers="topic_o5y_14w_2z__table_dcb_y3w_mq__entry__3">Is the left JSON value contained within the right value?</td>
                <td class="entry" headers="topic_o5y_14w_2z__table_dcb_y3w_mq__entry__4">
                  <code class="ph codeph">'{"b":2}'::jsonb &lt;@ '{"a":1, "b":2}'::jsonb</code>
                </td>
              </tr>
              <tr class="row">
                <td class="entry" headers="topic_o5y_14w_2z__table_dcb_y3w_mq__entry__1">
                  <code class="ph codeph">?</code>
                </td>
                <td class="entry" headers="topic_o5y_14w_2z__table_dcb_y3w_mq__entry__2">
                  <code class="ph codeph">text</code>
                </td>
                <td class="entry" headers="topic_o5y_14w_2z__table_dcb_y3w_mq__entry__3">Does the key/element string exist within the JSON value?</td>
                <td class="entry" headers="topic_o5y_14w_2z__table_dcb_y3w_mq__entry__4">
                  <code class="ph codeph">'{"a":1, "b":2}'::jsonb ? 'b'</code>
                </td>
              </tr>
              <tr class="row">
                <td class="entry" headers="topic_o5y_14w_2z__table_dcb_y3w_mq__entry__1">
                  <code class="ph codeph">?|</code>
                </td>
                <td class="entry" headers="topic_o5y_14w_2z__table_dcb_y3w_mq__entry__2">
                  <code class="ph codeph">text[]</code>
                </td>
                <td class="entry" headers="topic_o5y_14w_2z__table_dcb_y3w_mq__entry__3">Do any of these key/element strings exist?</td>
                <td class="entry" headers="topic_o5y_14w_2z__table_dcb_y3w_mq__entry__4">
                  <code class="ph codeph">'{"a":1, "b":2, "c":3}'::jsonb ?| array['b', 'c']</code>
                </td>
              </tr>
              <tr class="row">
                <td class="entry" headers="topic_o5y_14w_2z__table_dcb_y3w_mq__entry__1">
                  <code class="ph codeph">?&amp;</code>
                </td>
                <td class="entry" headers="topic_o5y_14w_2z__table_dcb_y3w_mq__entry__2">
                  <code class="ph codeph">text[]</code>
                </td>
                <td class="entry" headers="topic_o5y_14w_2z__table_dcb_y3w_mq__entry__3">Do all of these key/element strings exist?</td>
                <td class="entry" headers="topic_o5y_14w_2z__table_dcb_y3w_mq__entry__4">
                  <code class="ph codeph">'["a", "b"]'::jsonb ?&amp; array['a', 'b']</code>
                </td>
              </tr>
            </tbody></table>

The standard comparison operators in the following table are available only for the `jsonb` data type, not for the `json` data type. They follow the ordering rules for B-tree operations described in [jsonb Indexing](#topic_aqt_1tw_mq).

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

<table class="table" id="topic_u4s_wnw_2z__table_sqb_y3w_mb"><caption><span class="table--title-label">Table 5. </span><span class="title">JSON Creation Functions </span></caption><colgroup><col><col><col><col></colgroup><thead class="thead">
              <tr class="row">
                <th class="entry" id="topic_u4s_wnw_2z__table_sqb_y3w_mb__entry__1">Function</th>
                <th class="entry" id="topic_u4s_wnw_2z__table_sqb_y3w_mb__entry__2">Description</th>
                <th class="entry" id="topic_u4s_wnw_2z__table_sqb_y3w_mb__entry__3">Example</th>
                <th class="entry" id="topic_u4s_wnw_2z__table_sqb_y3w_mb__entry__4">Example Result</th>
              </tr>
            </thead><tbody class="tbody">
              <tr class="row">
                <td class="entry" headers="topic_u4s_wnw_2z__table_sqb_y3w_mb__entry__1">
                  <code class="ph codeph">to_json(anyelement)</code>
                </td>
                <td class="entry" headers="topic_u4s_wnw_2z__table_sqb_y3w_mb__entry__2">Returns the value as a JSON object. Arrays and composites are processed
                  recursively and are converted to arrays and objects. If the input contains a cast
                  from the type to <code class="ph codeph">json</code>, the cast function is used to perform the
                  conversion; otherwise, a JSON scalar value is produced. For any scalar type other
                  than a number, a Boolean, or a null value, the text representation will be used,
                  properly quoted and escaped so that it is a valid JSON string.</td>
                <td class="entry" headers="topic_u4s_wnw_2z__table_sqb_y3w_mb__entry__3">
                  <code class="ph codeph">to_json('Fred said "Hi."'::text)</code>
                </td>
                <td class="entry" headers="topic_u4s_wnw_2z__table_sqb_y3w_mb__entry__4">
                  <code class="ph codeph">"Fred said \"Hi.\""</code>
                </td>
              </tr>
              <tr class="row">
                <td class="entry" headers="topic_u4s_wnw_2z__table_sqb_y3w_mb__entry__1">
                  <code class="ph codeph">array_to_json(anyarray [, pretty_bool])</code>
                </td>
                <td class="entry" headers="topic_u4s_wnw_2z__table_sqb_y3w_mb__entry__2">Returns the array as a JSON array. A multidimensional array becomes a JSON
                  array of arrays. <p class="p">Line feeds will be added between dimension-1 elements if
                      <code class="ph codeph">pretty_bool</code> is true.</p></td>
                <td class="entry" headers="topic_u4s_wnw_2z__table_sqb_y3w_mb__entry__3">
                  <code class="ph codeph">array_to_json('{{1,5},{99,100}}'::int[])</code>
                </td>
                <td class="entry" headers="topic_u4s_wnw_2z__table_sqb_y3w_mb__entry__4">
                  <code class="ph codeph">[[1,5],[99,100]]</code>
                </td>
              </tr>
              <tr class="row">
                <td class="entry" headers="topic_u4s_wnw_2z__table_sqb_y3w_mb__entry__1">
                  <code class="ph codeph">row_to_json(record [, pretty_bool])</code>
                </td>
                <td class="entry" headers="topic_u4s_wnw_2z__table_sqb_y3w_mb__entry__2">Returns the row as a JSON object. <p class="p">Line feeds will be added between level-1
                    elements if <code class="ph codeph">pretty_bool</code> is true.</p></td>
                <td class="entry" headers="topic_u4s_wnw_2z__table_sqb_y3w_mb__entry__3">
                  <code class="ph codeph">row_to_json(row(1,'foo'))</code>
                </td>
                <td class="entry" headers="topic_u4s_wnw_2z__table_sqb_y3w_mb__entry__4">
                  <code class="ph codeph">{"f1":1,"f2":"foo"}</code>
                </td>
              </tr>
              <tr class="row">
                <td class="entry" headers="topic_u4s_wnw_2z__table_sqb_y3w_mb__entry__1"><code class="ph codeph">json_build_array(VARIADIC "any"</code>)</td>
                <td class="entry" headers="topic_u4s_wnw_2z__table_sqb_y3w_mb__entry__2">Builds a possibly-heterogeneously-typed JSON array out of a
                    <code class="ph codeph">VARIADIC</code> argument list.</td>
                <td class="entry" headers="topic_u4s_wnw_2z__table_sqb_y3w_mb__entry__3">
                  <code class="ph codeph">json_build_array(1,2,'3',4,5)</code>
                </td>
                <td class="entry" headers="topic_u4s_wnw_2z__table_sqb_y3w_mb__entry__4">
                  <code class="ph codeph">[1, 2, "3", 4, 5]</code>
                </td>
              </tr>
              <tr class="row">
                <td class="entry" headers="topic_u4s_wnw_2z__table_sqb_y3w_mb__entry__1">
                  <code class="ph codeph">json_build_object(VARIADIC "any")</code>
                </td>
                <td class="entry" headers="topic_u4s_wnw_2z__table_sqb_y3w_mb__entry__2">Builds a JSON object out of a <code class="ph codeph">VARIADIC</code> argument list. The
                  argument list is taken in order and converted to a set of key/value pairs.</td>
                <td class="entry" headers="topic_u4s_wnw_2z__table_sqb_y3w_mb__entry__3">
                  <code class="ph codeph">json_build_object('foo',1,'bar',2)</code>
                </td>
                <td class="entry" headers="topic_u4s_wnw_2z__table_sqb_y3w_mb__entry__4">
                  <code class="ph codeph">{"foo": 1, "bar": 2}</code>
                </td>
              </tr>
              <tr class="row">
                <td class="entry" headers="topic_u4s_wnw_2z__table_sqb_y3w_mb__entry__1">
                  <code class="ph codeph">json_object(text[])</code>
                </td>
                <td class="entry" headers="topic_u4s_wnw_2z__table_sqb_y3w_mb__entry__2">Builds a JSON object out of a text array. The array must be either a one or a
                  two dimensional array.<p class="p">The one dimensional array must have an even number of
                    elements. The elements are taken as key/value pairs. </p><p class="p">For a two
                    dimensional array, each inner array must have exactly two elements, which are
                    taken as a key/value pair.</p></td>
                <td class="entry" headers="topic_u4s_wnw_2z__table_sqb_y3w_mb__entry__3">
                  <p class="p">
                    <code class="ph codeph">json_object('{a, 1, b, "def", c, 3.5}')</code>
                  </p>
                  <p class="p">
                    <code class="ph codeph">json_object('{{a, 1},{b, "def"},{c, 3.5}}')</code>
                  </p>
                </td>
                <td class="entry" headers="topic_u4s_wnw_2z__table_sqb_y3w_mb__entry__4">
                  <code class="ph codeph">{"a": "1", "b": "def", "c": "3.5"}</code>
                </td>
              </tr>
              <tr class="row">
                <td class="entry" headers="topic_u4s_wnw_2z__table_sqb_y3w_mb__entry__1">
                  <code class="ph codeph">json_object(keys text[], values text[])</code>
                </td>
                <td class="entry" headers="topic_u4s_wnw_2z__table_sqb_y3w_mb__entry__2">Builds a JSON object out of a text array. This form of
                    <code class="ph codeph">json_object</code> takes keys and values pairwise from two separate
                  arrays. In all other respects it is identical to the one-argument form.</td>
                <td class="entry" headers="topic_u4s_wnw_2z__table_sqb_y3w_mb__entry__3">
                  <code class="ph codeph">json_object('{a, b}', '{1,2}')</code>
                </td>
                <td class="entry" headers="topic_u4s_wnw_2z__table_sqb_y3w_mb__entry__4">
                  <code class="ph codeph">{"a": "1", "b": "2"}</code>
                </td>
              </tr>
            </tbody></table>

**Note:** `array_to_json` and `row_to_json` have the same behavior as `to_json` except for offering a pretty-printing option. The behavior described for `to_json` likewise applies to each individual value converted by the other JSON creation functions.

**Note:** The [hstore module](../../../ref_guide/modules/hstore.html) contains functions that cast from `hstore` to `json`, so that `hstore` values converted via the JSON creation functions will be represented as JSON objects, not as primitive string values.

### <a id="topic_rvp_lk3_sfb"></a>JSON Aggregate Functions 

This table shows the functions aggregate records to an array of JSON objects and pairs of values to a JSON object

|Function|Argument Types|Return Type|Description|
|--------|--------------|-----------|-----------|
|`json_agg(record)`|`record`|`json`|Aggregates records as a JSON array of objects.|
|`json_object_agg(name, value)`|`("any", "any")`|`json`|Aggregates name/value pairs as a JSON object.|

### <a id="topic_z5d_snw_2z"></a>JSON Processing Functions 

This table shows the functions that are available for processing `json` and `jsonb` values.

Many of these processing functions and operators convert Unicode escapes in JSON strings to the appropriate single character. This is a not an issue if the input data type is `jsonb`, because the conversion was already done. However, for `json` data type input, this might result in an error being thrown. See [About JSON Data](#topic_upc_tcs_fz).

<table class="table" id="topic_z5d_snw_2z__table_wfc_y3w_mb"><caption><span class="table--title-label">Table 7. </span><span class="title">JSON Processing Functions</span></caption><colgroup><col style="width:20.224719101123597%"><col style="width:18.726591760299627%"><col style="width:18.913857677902623%"><col style="width:23.220973782771537%"><col style="width:18.913857677902623%"></colgroup><thead class="thead">
              <tr class="row">
                <th class="entry" id="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__1">Function</th>
                <th class="entry" id="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__2">Return Type</th>
                <th class="entry" id="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__3">Description</th>
                <th class="entry" id="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__4">Example</th>
                <th class="entry" id="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__5">Example Result</th>
              </tr>
            </thead><tbody class="tbody">
              <tr class="row">
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__1">
                  <code class="ph codeph">json_array_length(json)</code>
                  <p class="p">
                    <code class="ph codeph">jsonb_array_length(jsonb)</code>
                  </p></td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__2"><code class="ph codeph">int</code>
                </td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__3">Returns the number of elements in the outermost JSON array.</td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__4"><code class="ph codeph">json_array_length('[1,2,3,{"f1":1,"f2":[5,6]},4]')</code>
                </td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__5">
                  <code class="ph codeph">5</code>
                </td>
              </tr>
              <tr class="row">
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__1"><code class="ph codeph">json_each(json)</code>
                  <p class="p"><code class="ph codeph">jsonb_each(jsonb)</code>
                  </p></td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__2"><code class="ph codeph">setof key text, value json</code>
                  <p class="p"><code class="ph codeph">setof key text, value jsonb</code>
                  </p>
                </td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__3">Expands the outermost JSON object into a set of key/value pairs.</td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__4"><code class="ph codeph">select * from json_each('{"a":"foo", "b":"bar"}')</code>
                </td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__5">
                  <pre class="pre"> key | value
-----+-------
 a   | "foo"
 b   | "bar"
</pre>
                </td>
              </tr>
              <tr class="row">
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__1"><code class="ph codeph">json_each_text(json)</code>
                  <p class="p"><code class="ph codeph">jsonb_each_text(jsonb)</code>
                  </p>
                </td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__2"><code class="ph codeph">setof key text, value text</code>
                </td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__3">Expands the outermost JSON object into a set of key/value pairs. The returned
                  values will be of type <code class="ph codeph">text</code>.</td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__4"><code class="ph codeph">select * from json_each_text('{"a":"foo", "b":"bar"}')</code>
                </td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__5">
                  <pre class="pre"> key | value
-----+-------
 a   | foo
 b   | bar
</pre>
                </td>
              </tr>
              <tr class="row">
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__1"><code class="ph codeph">json_extract_path(from_json json, VARIADIC path_elems
                    text[])</code>
                  <p class="p"><code class="ph codeph">jsonb_extract_path(from_json jsonb, VARIADIC path_elems
                      text[])</code>
                  </p></td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__2">
                  <p class="p"><code class="ph codeph">json</code>
                  </p>
                  <p class="p"><code class="ph codeph">jsonb</code>
                  </p>
                </td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__3">Returns the JSON value pointed to by <code class="ph codeph">path_elems</code> (equivalent
                  to <code class="ph codeph">#&gt;</code> operator).</td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__4"><code class="ph codeph">json_extract_path('{"f2":{"f3":1},"f4":{"f5":99,"f6":"foo"}}','f4')</code>
                </td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__5">
                  <code class="ph codeph">{"f5":99,"f6":"foo"}</code>
                </td>
              </tr>
              <tr class="row">
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__1"><code class="ph codeph">json_extract_path_text(from_json json, VARIADIC path_elems
                    text[])</code>
                  <p class="p"><code class="ph codeph">jsonb_extract_path_text(from_json jsonb, VARIADIC path_elems
                      text[])</code>
                  </p></td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__2"><code class="ph codeph">text</code>
                </td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__3">Returns the JSON value pointed to by <code class="ph codeph">path_elems</code> as text.
                  Equivalent to <code class="ph codeph">#&gt;&gt;</code> operator.</td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__4"><code class="ph codeph">json_extract_path_text('{"f2":{"f3":1},"f4":{"f5":99,"f6":"foo"}}','f4',
                    'f6')</code>
                </td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__5">
                  <code class="ph codeph">foo</code>
                </td>
              </tr>
              <tr class="row">
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__1"><code class="ph codeph">json_object_keys(json)</code>
                  <p class="p"><code class="ph codeph">jsonb_object_keys(jsonb)</code>
                  </p></td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__2"><code class="ph codeph">setof text</code>
                </td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__3">Returns set of keys in the outermost JSON object.</td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__4"><code class="ph codeph">json_object_keys('{"f1":"abc","f2":{"f3":"a", "f4":"b"}}')</code>
                </td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__5">
                  <pre class="pre"> json_object_keys
------------------
 f1
 f2
</pre>
                </td>
              </tr>
              <tr class="row">
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__1"><code class="ph codeph">json_populate_record(base anyelement, from_json
                      json)</code><p class="p"><code class="ph codeph">jsonb_populate_record(base anyelement, from_json
                      jsonb)</code>
                  </p></td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__2"><code class="ph codeph">anyelement</code>
                </td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__3">Expands the object in <code class="ph codeph">from_json</code> to a row whose columns match
                  the record type defined by base. See <a class="xref" href="#topic_z5d_snw_2z__json_proc_1">Note 1</a>.</td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__4"><code class="ph codeph">select * from json_populate_record(null::myrowtype,
                    '{"a":1,"b":2}')</code>
                </td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__5">
                  <pre class="pre"> a | b
---+---
 1 | 2
</pre>
                </td>
              </tr>
              <tr class="row">
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__1"><code class="ph codeph">json_populate_recordset(base anyelement, from_json json)</code>
                  <p class="p"><code class="ph codeph">jsonb_populate_recordset(base anyelement, from_json jsonb)</code>
                  </p></td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__2"><code class="ph codeph">setof anyelement</code>
                </td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__3">Expands the outermost array of objects in <code class="ph codeph">from_json</code> to a set
                  of rows whose columns match the record type defined by base. See <a class="xref" href="#topic_z5d_snw_2z__json_proc_1">Note 1</a>.</td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__4"><code class="ph codeph">select * from json_populate_recordset(null::myrowtype,
                    '[{"a":1,"b":2},{"a":3,"b":4}]')</code>
                </td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__5">
                  <pre class="pre"> a | b
---+---
 1 | 2
 3 | 4
</pre>
                </td>
              </tr>
              <tr class="row">
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__1"><code class="ph codeph">json_array_elements(json)</code>
                  <p class="p"><code class="ph codeph">jsonb_array_elements(jsonb</code>)</p></td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__2">
                  <p class="p"><code class="ph codeph">setof json</code>
                  </p>
                  <p class="p"><code class="ph codeph">setof jsonb</code>
                  </p>
                </td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__3">Expands a JSON array to a set of JSON values.</td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__4"><code class="ph codeph">select * from json_array_elements('[1,true, [2,false]]')</code>
                </td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__5">
                  <pre class="pre">   value
-----------
 1
 true
 [2,false]
</pre>
                </td>
              </tr>
              <tr class="row">
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__1"><code class="ph codeph">json_array_elements_text(json)</code>
                  <p class="p"><code class="ph codeph">jsonb_array_elements_text(jsonb)</code>
                  </p></td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__2"><code class="ph codeph">setof text</code>
                </td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__3">Expands a JSON array to a set of <code class="ph codeph">text</code> values.</td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__4"><code class="ph codeph">select * from json_array_elements_text('["foo", "bar"]')</code>
                </td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__5">
                  <pre class="pre">   value
-----------
 foo
 bar
</pre>
                </td>
              </tr>
              <tr class="row">
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__1"><code class="ph codeph">json_typeof(json)</code><p class="p"><code class="ph codeph">jsonb_typeof(jsonb)</code>
                  </p></td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__2"><code class="ph codeph">text</code>
                </td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__3">Returns the type of the outermost JSON value as a text string. Possible types
                  are <code class="ph codeph">object</code>, <code class="ph codeph">array</code>, <code class="ph codeph">string</code>,
                    <code class="ph codeph">number</code>, <code class="ph codeph">boolean</code>, and <code class="ph codeph">null</code>.
                  See <a class="xref" href="#topic_z5d_snw_2z__json_proc_2">Note
                  2</a>.</td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__4"><code class="ph codeph">json_typeof('-123.4')</code>
                </td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__5">
                  <code class="ph codeph">number</code>
                </td>
              </tr>
              <tr class="row">
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__1"><code class="ph codeph">json_to_record(json)</code><p class="p"><code class="ph codeph">jsonb_to_record(jsonb)</code>
                  </p></td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__2"><code class="ph codeph">record</code>
                </td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__3">Builds an arbitrary record from a JSON object. See <a class="xref" href="#topic_z5d_snw_2z__json_proc_1">Note 1</a>. <p class="p">As with all
                    functions returning record, the caller must explicitly define the structure of
                    the record with an <code class="ph codeph">AS</code> clause.</p></td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__4"><code class="ph codeph">select * from json_to_record('{"a":1,"b":[1,2,3],"c":"bar"}') as x(a
                    int, b text, d text)</code>
                </td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__5">
                  <pre class="pre"> a |    b    | d
---+---------+---
 1 | [1,2,3] |
</pre>
                </td>
              </tr>
              <tr class="row">
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__1"><code class="ph codeph">json_to_recordset(json)</code>
                  <p class="p"><code class="ph codeph">jsonb_to_recordset(jsonb)</code>
                  </p></td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__2"><code class="ph codeph">setof record</code>
                </td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__3">Builds an arbitrary set of records from a JSON array of objects See <a class="xref" href="#topic_z5d_snw_2z__json_proc_1">Note 1</a>. <p class="p">As with all
                    functions returning record, the caller must explicitly define the structure of
                    the record with an <code class="ph codeph">AS</code> clause.</p></td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__4"><code class="ph codeph">select * from
                    json_to_recordset('[{"a":1,"b":"foo"},{"a":"2","c":"bar"}]') as x(a int, b
                    text);</code>
                </td>
                <td class="entry" headers="topic_z5d_snw_2z__table_wfc_y3w_mb__entry__5">
                  <pre class="pre"> a |  b
---+-----
 1 | foo
 2 |
</pre>
                </td>
              </tr>
            </tbody></table>

**Note:**

1.  The examples for the functions `json_populate_record()`, `json_populate_recordset()`, `json_to_record()` and `json_to_recordset()` use constants. However, the typical use would be to reference a table in the `FROM` clause and use one of its `json` or `jsonb` columns as an argument to the function. The extracted key values can then be referenced in other parts of the query. For example the value can be referenced in `WHERE` clauses and target lists. Extracting multiple values in this way can improve performance over extracting them separately with per-key operators.

    JSON keys are matched to identical column names in the target row type. JSON type coercion for these functions might not result in desired values for some types. JSON fields that do not appear in the target row type will be omitted from the output, and target columns that do not match any JSON field will be `NULL`.

2.  The `json_typeof` function null return value of `null` should not be confused with a SQL `NULL`. While calling `json_typeof('null'::json)` will return `null`, calling `json_typeof(NULL::json)` will return a SQL `NULL`.

