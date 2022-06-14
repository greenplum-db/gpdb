# citext Data Type 

The citext module provides a case-insensitive character string type, `citext`. Essentially, it internally calls the `lower` function when comparing values. Otherwise, it behaves almost exactly like the `text` data type.

The standard method to perform case-insensitive matches on text values is to use the `lower` function when comparing values, for example

```
SELECT * FROM tab WHERE lower(col) = LOWER(?);
```

This method works well, but has drawbacks:

-   It makes your SQL statements verbose, and you must remember to use `lower` on both the column and the query value.
-   It does not work with an index, unless you create a functional index using `lower`.

The `citext` data type allows you to eliminate calls to `lower` in SQL queries and you can create case-insensitive indexes on columns of type `citext`. `citext` is locale-aware, like the `text` type, which means comparing uppercase and lowercase characters depends on the rules of the LC\_CTYPE locale setting. This behavior is the same as using `lower` in queries, but it is done transparently by the data type, so you do not have to do anything special in your queries.

**Parent topic:** [Additional Supplied Modules](contrib-modules.html)

## Installing citext 

Before you can use the `citext` data type, run the installation script `$GPHOME/share/postgresql/contrib/citext.sql` in each database where you want to use the type:

```
$ psql -d testdb -f $GPHOME/share/postgresql/contrib/citext.sql
```

## Using the citext Type 

Here is a simple example defining a `citext` table column:

```
CREATE TABLE users (
    id bigint PRIMARY KEY,
    nick CITEXT NOT NULL,
    pass TEXT   NOT NULL
) DISTRIBUTED BY (id);

INSERT INTO users VALUES (1,  'larry',  md5(random()::text) );
INSERT INTO users VALUES (2,  'Tom',    md5(random()::text) );
INSERT INTO users VALUES (3,  'Damian', md5(random()::text) );
INSERT INTO users VALUES (4,  'NEAL',   md5(random()::text) );
INSERT INTO users VALUES (5,  'Bjørn',  md5(random()::text) );

SELECT * FROM users WHERE nick = 'Larry';

```

The `SELECT` statement returns one tuple, even though the `nick` column is set to `larry` and the query specified `Larry`.

## String Comparison Behavior 

`citext` performs comparisons by converting each string to lower case \(as though the `lower` function were called\) and then comparing the results normally. Two strings are considered equal if `lower` would produce identical results for them.

In order to emulate a case-insensitive collation as closely as possible, there are `citext`-specific versions of a number of string-processing operators and functions. So, for example, the regular expression operators `~` and `~*` exhibit the same behavior when applied to `citext`: they both match case-insensitively. The same is true for `!~` and `!~*`, as well as for the `LIKE` operators `~~` and `~~*`, and `!~~` and `!~~*`. If you want to match case-sensitively, you can cast the operator's arguments to `text`.

The following functions perform matching case-insensitively if their arguments are `citext`:

-   `regexp_match()`
-   `regexp_matches()`
-   `regexp_replace()`
-   `regexp_split_to_array()`
-   `regexp_split_to_table()`
-   `replace()`
-   `split_part()`
-   `strpos()`
-   `translate()`

For the regexp functions, if you want to match case-sensitively, you can specify the “c” flag to force a case-sensitive match. If you want case-sensitive behavior, you must cast to `text` before using one of these functions.

## Limitations 

-   A column of type `citext` cannot be part of a primary key or distribution key in a `CREATE TABLE` statement.
-   The `citext` type's case-folding behavior depends on the `LC_CTYPE` setting of your database. How it compares values is therefore determined when the database is created. It is not truly case-insensitive in the terms defined by the Unicode standard. Effectively, what this means is that, as long as you're happy with your collation, you should be happy with `citext`'s comparisons. But if you have data in different languages stored in your database, users of one language may find their query results are not as expected if the collation is for another language.

-   `citext` is not as efficient as `text` because the operator functions and the B-tree comparison functions must make copies of the data and convert them to lower case for comparisons. It is, however, slightly more efficient than using `lower` to perform case-insensitive matching.
-   `citext` may not be the best option if you need data to compare case-sensitively in some contexts and case-insensitively in other contexts. The standard recommendation is to use the `text` type and manually apply the `lower` function when you need to compare case-insensitively. This works if case-insensitive comparison is needed only infrequently. If you need case-insensitive behavior most of the time and case-sensitive infrequently, consider storing the data as `citext` and explicitly casting the column to `text` when you want case-sensitive comparison. In either situation, you will need two indexes if you want both types of searches to be fast.
-   The schema containing the `citext` operators must be in the current `search_path` \(typically `public`\); if it is not, the normal case-sensitive `text` operators will be invoked instead.

