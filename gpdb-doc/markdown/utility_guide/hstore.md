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

|Function|Return Type|Description|Example|Result|
|--------|-----------|-----------|-------|------|
|`hstore(text, text)`|`hstore`|make single-item `hstore`|`hstore('a', 'b')`|`"a"=>"b"`|
|`akeys(hstore)`|`text[]`|get `hstore`'s keys as array|`akeys('a=>1,b=>2')`|`{a,b}`|
|`skeys(hstore)`|`setof text`|get `hstore`'s keys as set|`skeys('a=>1,b=>2')`|```
a
b
```

|
|`avals(hstore)`|`text[]`|get `hstore`'s values as array|`avals('a=>1,b=>2')`|`{1,2}`|
|`svals(hstore)`|`setof text`|get `hstore`'s values as set|`svals('a=>1,b=>2')`|```
1
2
```

|
|`each(hstore)`|`setof (key text, value text)`|get `hstore`'s keys and values as set|`select * from each('a=>1,b=>2')`|```
 key | value
-----+-------
 a   | 1
 b   | 2
```

|
|`exist(hstore,text)`|`boolean`|does `hstore` contain key?|`exist('a=>1','a')`|`t`|
|`defined(hstore,text)`|`boolean`|does `hstore` contain non-null value for key?|`defined('a=>NULL','a')`|`f`|
|`delete(hstore,text)`|`hstore`|delete any item matching key|`delete('a=>1,b=>2','b')`|`"a"=>"1"`|

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

**Parent topic:**[Additional Supplied Modules](contrib-modules.html)

