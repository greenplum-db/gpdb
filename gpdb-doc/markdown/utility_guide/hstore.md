# hstore Functions 

The `hstore` module implements a data type for storing sets of \(key,value\) pairs within a single Greenplum Database data field. This can be useful in various scenarios, such as rows with many attributes that are rarely examined, or semi-structured data.

In the current implementation, neither the key nor the value string can exceed 65535 bytes in length; an error will be thrown if this limit is exceeded. These maximum lengths may change in future releases.

## Installing hstore 

Before you can use `hstore` data type and functions, run the installation script `$GPHOME/share/postgresql/contrib/hstore.sql` in each database where you want the ability to query other databases:

```
$ psql -d testdb -f $GPHOME/share/postgresql/contrib/hstore.sql
```

## hstore External Representation 

The text representation of an `hstore` value includes zero or more key `=>` value items, separated by commas. For example:

```
k => v
foo => bar, baz => whatever
"1-a" => "anything at all"
```

The order of the items is not considered significant \(and may not be reproduced on output\). Whitespace between items or around the `=>` sign is ignored. Use double quotes if a key or value includes whitespace, comma, `=` or `>`. To include a double quote or a backslash in a key or value, precede it with another backslash. \(Keep in mind that depending on the setting of standard\_conforming\_strings, you may need to double backslashes in SQL literal strings.\)

A value \(but not a key\) can be a SQL NULL. This is represented as

```
key => NULL
```

The `NULL` keyword is not case-sensitive. Again, use double quotes if you want the string `null` to be treated as an ordinary data value.

Currently, double quotes are always used to surround key and value strings on output, even when this is not strictly necessary.

## hstore Operators and Functions 

|Operator|Description|Example|Result|
|--------|-----------|-------|------|
|`hstore` `->` `text`|get value for key \(null if not present\)|`'a=>x, b=>y'::hstore -> 'a'`|`x`|
|`text` `=>` `text`|make single-item `hstore`|`'a' => 'b'`|`"a"=>"b"`|
|`hstore` `||` `hstore`|concatenation|`'a=>b, c=>d'::hstore || 'c=>x, d=>q'::hstore`|`"a"=>"b", "c"=>"x", "d"=>"q"`|
|`hstore` `?` `text`|does `hstore` contain key?|`'a=>1'::hstore ? 'a'`|`t`|
|`hstore` `@>` `hstore`|does left operand contain right?|`'a=>b, b=>1, c=>NULL'::hstore @> 'b=>1'`|`t`|
|`hstore` `<@` `hstore`|is left operand contained in right?|`'a=>c'::hstore <@ 'a=>b, b=>1, c=>NULL'`|`f`|

**Note:** The `=>` operator is deprecated and may be removed in a future release. Use the `hstore(text, text)` function instead.

<div class="tablenoborder">
<table cellpadding="4" cellspacing="0" summary="" id="topic_vcn_jkq_1bb__hstore-func-table" class="table" frame="border" border="1" rules="all">
<caption><span class="tablecap">Table 2. hstore Functions</span></caption>
     <thead class="thead" align="left">
      <tr class="row">
       <th class="entry" valign="top" id="d69496e294">Function</th>
       <th class="entry" valign="top" id="d69496e297">Return Type</th>
       <th class="entry" valign="top" id="d69496e300">Description</th>
       <th class="entry" valign="top" id="d69496e303">Example</th>
       <th class="entry" valign="top" id="d69496e306">Result</th>
      </tr>
     </thead>
     <tbody class="tbody">
      <tr class="row">
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 "><samp class="ph codeph">hstore(text, text)</samp></td>
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 "><samp class="ph codeph">hstore</samp></td>
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 ">make single-item <samp class="ph codeph">hstore</samp>
</td>
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 "><samp class="ph codeph">hstore('a', 'b')</samp></td>
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 "><samp class="ph codeph">"a"=&gt;"b"</samp></td>
      </tr>
      <tr class="row">
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 "><samp class="ph codeph">akeys(hstore)</samp></td>
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 "><samp class="ph codeph">text[]</samp></td>
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 ">get <samp class="ph codeph">hstore</samp>'s keys as array</td>
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 "><samp class="ph codeph">akeys('a=&gt;1,b=&gt;2')</samp></td>
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 "><samp class="ph codeph">{a,b}</samp></td>
      </tr>
      <tr class="row">
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 "><samp class="ph codeph">skeys(hstore)</samp></td>
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 "><samp class="ph codeph">setof text</samp></td>
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 ">get <samp class="ph codeph">hstore</samp>'s keys as set</td>
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 "><samp class="ph codeph">skeys('a=&gt;1,b=&gt;2')</samp></td>
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 ">
        <pre class="pre">a
b</pre>
       </td>
      </tr>
      <tr class="row">
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 "><samp class="ph codeph">avals(hstore)</samp></td>
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 "><samp class="ph codeph">text[]</samp></td>
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 ">get <samp class="ph codeph">hstore</samp>'s values as array</td>
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 "><samp class="ph codeph">avals('a=&gt;1,b=&gt;2')</samp></td>
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 "><samp class="ph codeph">{1,2}</samp></td>
      </tr>
      <tr class="row">
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 "><samp class="ph codeph">svals(hstore)</samp></td>
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 "><samp class="ph codeph">setof text</samp></td>
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 ">get <samp class="ph codeph">hstore</samp>'s values as set</td>
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 "><samp class="ph codeph">svals('a=&gt;1,b=&gt;2')</samp></td>
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 ">
        <pre class="pre">1
2</pre>
       </td>
      </tr>
      <tr class="row">
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 "><samp class="ph codeph">each(hstore)</samp></td>
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 "><samp class="ph codeph">setof (key text, value text)</samp></td>
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 ">get <samp class="ph codeph">hstore</samp>'s keys and values as set</td>
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 "><samp class="ph codeph">select * from each('a=&gt;1,b=&gt;2')</samp></td>
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 ">
        <code>&nbsp;key|value<br/>&nbsp;---------+-------------<br/> &nbsp;  a  |  1<br/> &nbsp;  b  |  2</code>
       </td>
      </tr>
      <tr class="row">
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 "><samp class="ph codeph">exist(hstore,text)</samp></td>
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 "><samp class="ph codeph">boolean</samp></td>
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 ">does <samp class="ph codeph">hstore</samp> contain key?</td>
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 "><samp class="ph codeph">exist('a=&gt;1','a')</samp></td>
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 "><samp class="ph codeph">t</samp></td>
      </tr>
      <tr class="row">
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 "><samp class="ph codeph">defined(hstore,text)</samp></td>
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 "><samp class="ph codeph">boolean</samp></td>
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 ">does <samp class="ph codeph">hstore</samp> contain non-null value for key?</td>
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 "><samp class="ph codeph">defined('a=&gt;NULL','a')</samp></td>
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 "><samp class="ph codeph">f</samp></td>
      </tr>
      <tr class="row">
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 "><samp class="ph codeph">delete(hstore,text)</samp></td>
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 "><samp class="ph codeph">hstore</samp></td>
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 ">delete any item matching key</td>
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 "><samp class="ph codeph">delete('a=&gt;1,b=&gt;2','b')</samp></td>
       <td class="entry" valign="top" headers="d69496e294 d69496e297 d69496e300 d69496e303 d69496e306 "><samp class="ph codeph">"a"=&gt;"1"</samp></td>
      </tr>
     </tbody>
    </table>
</div>

## Indexes 

`hstore` has index support for `@>` and `?` operators. You can use the GiST index type. For example:

```
CREATE INDEX hidx ON testhstore USING GIST(h);
```

## Examples 

Add a key, or update an existing key with a new value:

```
UPDATE tab SET h = h || ('c' => '3');
```

Delete a key:

```
UPDATE tab SET h = delete(h, 'k1');
```

## Statistics 

The `hstore` type, because of its intrinsic liberality, could contain a lot of different keys. Checking for valid keys is the task of the application. Examples below demonstrate several techniques for checking keys and obtaining statistics.

Simple example:

```
SELECT * FROM each('aaa=>bq, b=>NULL, ""=>1');
```

Using a table:

```
SELECT (each(h)).key, (each(h)).value INTO stat FROM testhstore;
```

Online statistics:

```
SELECT key, count(*) FROM
  (SELECT (each(h)).key FROM testhstore) AS stat
  GROUP BY key
  ORDER BY count DESC, key;
    key    | count
-----------+-------
 line      |   883
 query     |   207
 pos       |   203
 node      |   202
 space     |   197
 status    |   195
 public    |   194
 title     |   190
 org       |   189
...................
```

**Parent topic:** [Additional Supplied Modules](contrib-modules.html)

