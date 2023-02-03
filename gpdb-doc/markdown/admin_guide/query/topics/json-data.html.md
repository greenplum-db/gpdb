---
title: Working with JSON Data 
---

Greenplum Database supports JSON as specified in the [RFC 7159](https://tools.ietf.org/html/rfc7159) document and enforces data validity according to the JSON rules. There are also JSON-specific functions and operators available for the `json` and `jsonb` data types. See [JSON Functions and Operators](../../../ref_guide/function-summary.html#topic_gn4_x3w_mq).

Greenplum Database offers two types for storing JSON \(JavaScript Object Notation\) data: `json` and `jsonb`. To implement efficient query mechanisms for these data types, Greenplum Database also provides the `jsonpath` data type described in [jsonpath Type](#topic_jsonpath) later in this topic.

This section contains the following topics:

-   [About JSON Data](#topic_upc_tcs_fz)
-   [JSON Input and Output Syntax](#topic_isn_ltw_mq)
-   [Designing JSON documents](#topic_eyt_3tw_mq)
-   [jsonb Containment and Existence](#topic_isx_2tw_mq)
-   [jsonb Indexing](#topic_aqt_1tw_mq)
-   [Transforms](#topic_transforms)
-   [jsonpath Type](#topic_jsonpath)

**Parent topic:** [Querying Data](../../query/topics/query.html)

## <a id="topic_upc_tcs_fz"></a>About JSON Data 

The `json` and `jsonb` data types accept *almost* identical sets of values as input. The major difference is one of efficiency.

-   The `json` data type stores an exact copy of the input text. This requires JSON processing functions to reparse `json` data on each execution. The `json` data type does not alter the input text.
    -   Semantically-insignificant white space between tokens is preserved, as well as the order of keys within JSON objects.
    -   All key/value pairs are kept even if a JSON object contains duplicate keys. For duplicate keys, JSON processing functions consider the last value as the operative one.
-   The `jsonb` data type stores a decomposed binary format of the input text. The conversion overhead makes data input slightly slower than the `json` data type. However, The JSON processing functions are significantly faster because reparsing `jsonb` data is not required. The `jsonb` data type alters the input text.

    -   White space is not preserved.
    -   The order of object keys is not preserved.
    -   Duplicate object keys are not kept. If the input includes duplicate keys, only the last value is kept.
-   The `jsonb` data type supports indexing. See [jsonb Indexing](#topic_aqt_1tw_mq).

In general, JSON data should be stored as the `jsonb` data type unless there are specialized needs, such as legacy assumptions about ordering of object keys.

Greenplum Database allows only one character set encoding per database. It is therefore not possible for the JSON types to conform rigidly to the JSON specification unless the database encoding is UTF8. Attempts to directly include characters that cannot be represented in the database encoding will fail; conversely, characters that can be represented in the database encoding but not in UTF8 are allowed.

### <a id="abuni"></a>About Unicode Characters in JSON Data 

The [RFC 7159](https://tools.ietf.org/html/rfc7159) document permits JSON strings to contain Unicode escape sequences denoted by `\uXXXX`.

-   The Greenplum Database input function for the `json` data type allows Unicode escapes regardless of the database encoding and checks Unicode escapes only for syntactic correctness \(a `\u` followed by four hex digits\).
-   The Greenplum Database input function for the `jsonb` data type is more strict. It does not allow Unicode escapes for non-ASCII characters \(those above `U+007F`\) unless the database encoding is UTF8. The `jsonb` type also rejects `\u0000`, which cannot be represented in the Greenplum Database `text` type, and it requires that any use of Unicode surrogate pairs to designate characters outside the Unicode Basic Multilingual Plane be correct. Valid Unicode escapes, except for `\u0000`, are converted to the equivalent ASCII or UTF8 character for storage; this includes folding surrogate pairs into a single character.

> **Note** Many of the JSON processing functions described in [JSON Functions and Operators](../../../ref_guide/function-summary.html#topic_gn4_x3w_mq) convert Unicode escapes to regular characters and will therefore throw the same types of errors just described even if their input is of type `json` not `jsonb`. The fact that the `json` input function does not make these checks may be considered a historical artifact, although it does allow for simple storage \(without processing\) of JSON Unicode escapes in a non-UTF8 database encoding. In general, it is best to avoid mixing Unicode escapes in JSON with a non-UTF8 database encoding, if possible.

### <a id="mapjson"></a>Mapping JSON Data Types to Greenplum Data Types 

When converting JSON text input into `jsonb` data, the primitive data types described by RFC 7159 are effectively mapped onto native Greenplum Database data types, as shown in the following table.

|JSON primitive data type|Greenplum Database data type|Notes|
|------------------------|----------------------------|-----|
|`string`|`text`|`\u0000` is not allowed. Non-ASCII Unicode escapes are allowed only if database encoding is UTF8|
|`number`|`numeric`|`NaN` and `infinity` values are disallowed|
|`boolean`|`boolean`|Only lowercase `true` and `false` spellings are accepted|
|`null`|\(none\)|The JSON `null` primitive type is different than the SQL `NULL`|

There are some minor constraints on what constitutes valid `jsonb` data that do not apply to the `json` data type, nor to JSON in the abstract, corresponding to limits on what can be represented by the underlying data type. Notably, when converting data to the `jsonb` data type, numbers that are outside the range of the Greenplum Database `numeric` data type are rejected, while the `json` data type does not reject such numbers.

Such implementation-defined restrictions are permitted by RFC 7159. However, in practice such problems might occur in other implementations, as it is common to represent the JSON `number` primitive type as IEEE 754 double precision floating point \(which RFC 7159 explicitly anticipates and allows for\).

When using JSON as an interchange format with other systems, be aware of the possibility of losing numeric precision compared to data originally stored by Greenplum Database.

Also, as noted in the previous table, there are some minor restrictions on the input format of JSON primitive types that do not apply to the corresponding Greenplum Database data types.

## <a id="topic_isn_ltw_mq"></a>JSON Input and Output Syntax 

The input and output syntax for the `json` data type is as specified in RFC 7159.

The following are all valid `json` \(or `jsonb`\) expressions:

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

One semantically-insignificant detail worth noting is that with the `jsonb` data type, numbers will be printed according to the behavior of the underlying `numeric` type. In practice, this means that numbers entered with `E` notation will be printed without it, for example:

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

Testing *containment* is an important capability of `jsonb`. There is no parallel set of facilities for the `json` type. Containment tests whether one `jsonb` document has contained within it another one. These examples return `true` except as noted:

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
SELECT '{"foo": {"bar": "baz"}}'::jsonb @> '{"bar": "baz"}'::jsonb; -- yields false

-- But with a layer of nesting, it is contained:
SELECT '{"foo": {"bar": "baz"}}'::jsonb @> '{"foo": {}}'::jsonb;
```

The general principle is that the contained object must match the containing object as to structure and data contents, possibly after discarding some non-matching array elements or object key/value pairs from the containing object. For containment, the order of array elements is not significant when doing a containment match, and duplicate array elements are effectively considered only once.

As an exception to the general principle that the structures must match, an array may contain a primitive value:

```
-- This array contains the primitive string value:
SELECT '["foo", "bar"]'::jsonb @> '"bar"'::jsonb;

-- This exception is not reciprocal -- non-containment is reported here:
SELECT '"bar"'::jsonb @> '["bar"]'::jsonb;  -- yields false
```

`jsonb` also has an *existence* operator, which is a variation on the theme of containment: it tests whether a string \(given as a `text` value\) appears as an object key or array element at the top level of the `jsonb` value. These examples return `true` except as noted:

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

Because JSON containment is nested, an appropriate query can skip explicit selection of sub-objects. As an example, suppose that we have a `doc` column containing objects at the top level, with most objects containing `tags` fields that contain arrays of sub-objects. This query finds entries in which sub-objects containing both `"term":"paris"` and `"term":"food"` appear, while ignoring any such keys outside the `tags` array:

```
SELECT doc->'site_name' FROM websites
  WHERE doc @> '{"tags":[{"term":"paris"}, {"term":"food"}]}';
```

The query with this predicate could accomplish the same thing:

```
SELECT doc->'site_name' FROM websites
  WHERE doc->'tags' @> '[{"term":"paris"}, {"term":"food"}]';
```

However, the second approach is less flexible and is often less efficient as well.

On the other hand, the JSON existence operator is not nested: it will only look for the specified key or array element at top level of the JSON value.

The various containment and existence operators, along with all other JSON operators and functions are documented in [JSON Functions and Operators](../../../ref_guide/function-summary.html#topic_gn4_x3w_mq).

## <a id="topic_aqt_1tw_mq"></a>jsonb Indexing 

The Greenplum Database `jsonb` data type, supports GIN, btree, and hash indexes.

-   [GIN Indexes on jsonb Data](#topic_yjx_dq2_rfb)
-   [Btree and Hash Indexes on jsonb Data](#topic_ahb_5ly_wq)

### <a id="topic_yjx_dq2_rfb"></a>GIN Indexes on jsonb Data 

GIN indexes can be used to efficiently search for keys or key/value pairs occurring within a large number of `jsonb` documents \(datums\). Two GIN operator classes are provided, offering different performance and flexibility trade-offs.

The default GIN operator class for jsonb supports queries with the `@>`, `?`, `?&` and `?|` operators. \(For details of the semantics that these operators implement, see [JSON Operators](../../../ref_guide/function-summary.html#topic_o5y_14w_2z).\) An example of creating an index with this operator class is:

```
CREATE INDEX idxgin ON api USING GIN (jdoc);
```

The non-default GIN operator class `jsonb_path_ops` does not support the key-exists operators, but it does support `@>`, `@?`, and `@@.` An example of creating an index with this operator class is:

```
CREATE INDEX idxginp ON api USING GIN (jdoc jsonb_path_ops);
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
CREATE INDEX idxgintags ON api USING GIN ((jdoc -> 'tags'));
```

Now, the `WHERE` clause `jdoc -> 'tags' ? 'qui'` is recognized as an application of the indexable operator `?` to the indexed expression `jdoc -> 'tags'`. For information about expression indexes, see [Indexes on Expressions](../../ddl/ddl-index.html).

Another approach to querying JSON documents is to exploit containment, for example:

```
-- Find documents in which the key "tags" contains array element "qui"
SELECT jdoc->'guid', jdoc->'name' FROM api WHERE jdoc @> '{"tags": ["qui"]}';

```

A simple GIN index on the `jdoc` column can support this query. However, the index will store copies of every key and value in the `jdoc` column, whereas the expression index of the previous example stores only data found under the `tags` key. While the simple-index approach is far more flexible \(since it supports queries about any key\), targeted expression indexes are likely to be smaller and faster to search than a simple index.

GIN indexes also support the `@?` and `@@` operators, which perform `jsonpath` matching. Examples are:

```
SELECT jdoc->'guid', jdoc->'name' FROM api WHERE jdoc @? '$.tags[*] ? (@ == "qui")';
```

```
SELECT jdoc->'guid', jdoc->'name' FROM api WHERE jdoc @@ '$.tags[*] == "qui"';
```

For these operators, a GIN index extracts clauses of the form `accessors_chain = constant` out of the `jsonpath` pattern, and does the index search based on the keys and values mentioned in these clauses. The accessors chain may include `.key`, `[*],` and `[index]` accessors. The `jsonb_ops` operator class also supports `.*` and `.**` accessors, but the `jsonb_path_ops` operator class does not.

Although the `jsonb_path_ops` operator class supports only queries with the `@>`, `@?` and `@@` operators, it has notable performance advantages over the default operator class `jsonb_ops`. A `jsonb_path_ops` index is usually much smaller than a `jsonb_ops` index over the same data, and the specificity of searches is better, particularly when queries contain keys that appear frequently in the data. Therefore search operations typically perform better than with the default operator class.

The technical difference between a `jsonb_ops` and a `jsonb_path_ops` GIN index is that the former creates independent index items for each key and value in the data, while the latter creates index items only for each value in the data.

> **Note** For this discussion, the term *value* includes array elements, though JSON terminology sometimes considers array elements distinct from values within objects.

Basically, each `jsonb_path_ops` index item is a hash of the value and the key\(s\) leading to it; for example to index `{"foo": {"bar": "baz"}}`, a single index item would be created incorporating all three of `foo`, `bar`, and `baz` into the hash value. Thus a containment query looking for this structure would result in an extremely specific index search; but there is no way at all to find out whether `foo` appears as a key. On the other hand, a `jsonb_ops` index would create three index items representing `foo`, `bar`, and `baz` separately; then to do the containment query, it would look for rows containing all three of these items. While GIN indexes can perform such an `AND` search fairly efficiently, it will still be less specific and slower than the equivalent `jsonb_path_ops` search, especially if there are a very large number of rows containing any single one of the three index items.

A disadvantage of the `jsonb_path_ops` approach is that it produces no index entries for JSON structures not containing any values, such as `{"a": {}}`. If a search for documents containing such a structure is requested, it will require a full-index scan, which is quite slow. `jsonb_path_ops` is ill-suited for applications that often perform such searches.

### <a id="topic_ahb_5ly_wq"></a>Btree and Hash Indexes on jsonb Data 

`jsonb` also supports `btree` and `hash` indexes. These are usually useful only when it is important to check the equality of complete JSON documents.

For completeness, the `btree` ordering for `jsonb` datums is:

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

## <a id="topic_transforms"></a>Transforms

Additional extensions are available that implement transforms for the `jsonb` type for different procedural languages.

The extensions for PL/Perl are called `jsonb_plperl` and j`sonb_plperlu`. If you use them, `jsonb` values are mapped to Perl arrays, hashes, and scalars, as appropriate.

The extensions for PL/Python are called `jsonb_plpythonu`, `jsonb_plpython2u`, and `jsonb_plpython3u`. If you use them, `jsonb` values are mapped to Python dictionaries, lists, and scalars, as appropriate.

## <a id="topic_jsonpath"></a>jsonpath Type

The `jsonpath` type implements support for the SQL/JSON path language in Greenplum Database to efficiently query JSON data. It provides a binary representation of the parsed SQL/JSON path expression that specifies the items to be retrieved by the path engine from the JSON data for further processing with the SQL/JSON query functions.

The semantics of SQL/JSON path predicates and operators generally follow SQL. At the same time, to provide a most natural way of working with JSON data, SQL/JSON path syntax uses some of the JavaScript conventions:

- Dot (`.`) is used for member access.

- Square brackets (`[]`) are used for array access.

- SQL/JSON arrays are 0-relative, unlike regular SQL arrays that start from 1.

An SQL/JSON path expression is typically written in an SQL query as an SQL character string literal, so it must be enclosed in single quotes, and any single quotes desired within the value must be doubled. Some forms of path expressions require string literals within them. These embedded string literals follow JavaScript/ECMAScript conventions: they must be surrounded by double quotes, and backslash escapes may be used within them to represent otherwise-hard-to-type characters. In particular, the way to write a double quote within an embedded string literal is `\"`, and to write a backslash itself, you must write `\\`. Other special backslash sequences include those recognized in JSON strings: `\b`, `\f`, `\n`, `\r`, `\t`, `\v` for various ASCII control characters, and `\uNNNN` for a Unicode character identified by its 4-hex-digit code point. The backslash syntax also includes two cases not allowed by JSON: `\xNN` for a character code written with only two hex digits, and `\u{N...}` for a character code written with 1 to 6 hex digits.

A path expression consists of a sequence of path elements, which can be the following:

- Path literals of JSON primitive types: Unicode text, numeric, true, false, or null.

- Path variables listed in the **jsonpath Variables** table below.

- Accessor operators listed in the **jsonpath Accessors** table below.

- `jsonpath` operators and methods listed in 
[SQL/JSON Path Operators and Methods](../../../ref_guide/function-summary.html#topic_jsonpath_opsmeth).

- Parentheses, which can be used to provide filter expressions or define the order of path evaluation.

For details on using `jsonpath` expressions with SQL/JSON query functions, see [SQL/JSON Filter Expression Elements](../../../ref_guide/function-summary.html#topic_jsonpath_filtexp).

 <div class="table" id="TYPE-JSONPATH-VARIABLES">
      <p class="title"><strong><code class="type">jsonpath</code> Variables</strong></p>
      <div class="table-contents">
        <table class="table" summary="jsonpath Variables" border="1">
          <colgroup>
            <col />
            <col />
          </colgroup>
          <thead>
            <tr class="row">
              <th class="entry nocellnorowborder">Variable</th>
              <th class="entry nocellnorowborder">Description</th>
            </tr>
          </thead>
          <tbody>
            <tr class="row">
              <td class="entry nocellnorowborder"><code class="literal">$</code></td>
              <td class="entry nocellnorowborder">A variable representing the JSON text to be queried (the <em class="firstterm">context item</em>).</td>
            </tr>
            <tr class="row">
              <td class="entry nocellnorowborder"><code class="literal">$varname</code></td>
              <td class="entry nocellnorowborder">A named variable. Its value can be set by the parameter <em class="parameter"><code>vars</code></em> of several JSON processing functions. See <a href="../../../ref_guide/function-summary.html#topic_z5d_snw_2z">JSON Processing Functions</a> and its notes for details.</td>
            </tr>
            <tr>
              <td class="entry nocellnorowborder"><code class="literal">@</code></td>
              <td class="entry nocellnorowborder">A variable representing the result of path evaluation in filter expressions.</td>
            </tr>
          </tbody>
        </table>
      </div>
    </div>

<div class="table" id="TYPE-JSONPATH-ACCESSORS">
      <p class="title"><strong><code class="type">jsonpath</code> Accessors</strong></p>
      <div class="table-contents">
        <table class="table" summary="jsonpath Accessors" border="1">
          <colgroup>
            <col />
            <col />
          </colgroup>
          <thead>
            <tr class="row">
              <th class="entry nocellnorowborder">Accessor Operator</th>
              <th class="entry nocellnorowborder">Description</th>
            </tr>
          </thead>
          <tbody>
            <tr class="row">
              <td class="entry nocellnorowborder">
                <p><code class="literal">.<em class="replaceable"><code>key</code></em></code></p>
                <p><code class="literal">."$<em class="replaceable"><code>varname</code></em>"</code></p>
              </td>
              <td class="entry nocellnorowborder">
                <p>Member accessor that returns an object member with the specified key. If the key name is a named variable starting with <code class="literal">$</code> or does not meet the JavaScript rules of an identifier, it must be enclosed in double quotes as a character string literal.</p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry nocellnorowborder">
                <p><code class="literal">.*</code></p>
              </td>
              <td class="entry nocellnorowborder">
                <p>Wildcard member accessor that returns the values of all members located at the top level of the current object.</p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry nocellnorowborder">
                <p><code class="literal">.**</code></p>
              </td>
              <td class="entry nocellnorowborder">
                <p>Recursive wildcard member accessor that processes all levels of the JSON hierarchy of the current object and returns all the member values, regardless of their nesting level. This is a <span class="productname">Greenplum Database</span> extension of the SQL/JSON standard.</p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry nocellnorowborder">
                <p><code class="literal">.**{<em class="replaceable"><code>level</code></em>}</code></p>
                <p><code class="literal">.**{<em class="replaceable"><code>start_level</code></em> to <em class="replaceable"><code>end_level</code></em>}</code></p>
              </td>
              <td class="entry nocellnorowborder">
                <p>Same as <code class="literal">.**</code>, but with a filter over nesting levels of JSON hierarchy. Nesting levels are specified as integers. Zero level corresponds to the current object. To access the lowest nesting level, you can use the <code class="literal">last</code> keyword. This is a <span class="productname">Greenplum Database</span> extension of the SQL/JSON standard.</p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry nocellnorowborder">
                <p><code class="literal">[<em class="replaceable"><code>subscript</code></em>, ...]</code></p>
              </td>
              <td class="entry nocellnorowborder">
                <p>Array element accessor. <code class="literal"><em class="replaceable"><code>subscript</code></em></code> can be given in two forms: <code class="literal"><em class="replaceable"><code>index</code></em></code> or <code class="literal"><em class="replaceable"><code>start_index</code></em> to <em class="replaceable"><code>end_index</code></em></code>. The first form returns a single array element by its index. The second form returns an array slice by the range of indexes, including the elements that correspond to the provided <em class="replaceable"><code>start_index</code></em> and <em class="replaceable"><code>end_index</code></em>.</p>
                <p>The specified <em class="replaceable"><code>index</code></em> can be an integer, as well as an expression returning a single numeric value, which is automatically cast to integer. Zero index corresponds to the first array element. You can also use the <code class="literal">last</code> keyword to denote the last array element, which is useful for handling arrays of unknown length.</p>
              </td>
            </tr>
            <tr class="row">
              <td class="entry nocellnorowborder">
                <p><code class="literal">[*]</code></p>
              </td>
              <td class="entry nocellnorowborder">
                <p>Wildcard array element accessor that returns all array elements.</p>
              </td>
            </tr>
          </tbody>
        </table>
      </div>
    </div>

