# Greenplum Fuzzy String Match Extension 

The Greenplum Database Fuzzy String Match extension provides functions to determine similarities and distance between strings based on various algorithms.

-   [Soundex Functions](#topic_soundex)
-   [Levenshtein Functions](#topic_levenshtein)
-   [Metaphone Functions](#topic_metaphone)
-   [Double Metaphone Functions](#d_metaphone)
-   [Installing and Uninstalling the Fuzzy String Match Functions](#topic_install)

The Greenplum Database installation contains the files required for the functions in this extension module and SQL scripts to define the extension functions in a database and remove the functions from a database.

**Warning:** The functions soundex, metaphone, dmetaphone, and dmetaphone\_alt do not work well with multibyte encodings \(such as UTF-8\).

The Greenplum Database Fuzzy String Match extension is based on the PostgreSQL fuzzystrmatch module.

**Parent topic:** [Greenplum Database Reference Guide](../ref_guide.html)

## Soundex Functions 

The Soundex system is a method of matching similar-sounding \(similar phonemes\) names by converting them to the same code.

**Note:** Soundex is most useful for English names.

These functions work with Soundex codes:

```
soundex(text <string1>) returns text
difference(text <string1>, text <string2>) returns int
```

The soundex function converts a string to its Soundex code. Soundex codes consist of four characters.

The difference function converts two strings to their Soundex codes and then reports the number of matching code positions. The result ranges from zero to four, zero being no match and four being an exact match. These are some examples:

```
SELECT soundex('hello world!');
SELECT soundex('Anne'), soundex('Ann'), difference('Anne', 'Ann');
SELECT soundex('Anne'), soundex('Andrew'), difference('Anne', 'Andrew');
SELECT soundex('Anne'), soundex('Margaret'), difference('Anne', 'Margaret');

CREATE TABLE s (nm text);

INSERT INTO s VALUES ('john');
INSERT INTO s VALUES ('joan');
INSERT INTO s VALUES ('wobbly');
INSERT INTO s VALUES ('jack');

SELECT * FROM s WHERE soundex(nm) = soundex('john');

SELECT * FROM s WHERE difference(s.nm, 'john') > 2;
```

For information about the Soundex indexing system see [https://www.archives.gov/research/census/soundex.html](https://www.archives.gov/research/census/soundex.html).

## Levenshtein Functions 

These functions calculate the Levenshtein distance between two strings:

```
levenshtein(text <source>, text <target>, int <ins_cost>, int <del_cost>, int <sub_cost>) returns int
levenshtein(text <source>, text <target>) returns int
levenshtein_less_equal(text <source>, text <target>, int <ins_cost>, int <del_cost>, int <sub_cost>, int <max_d>) returns int
levenshtein_less_equal(text <source>, text <target>, int max_d) returns int
```

Both the `source` and `target` parameters can be any non-null string, with a maximum of 255 bytes. The cost parameters `ins_cost`, `del_cost`, and `sub_cost` specify cost of a character insertion, deletion, or substitution, respectively. You can omit the cost parameters, as in the second version of the function; in that case the cost parameters default to 1.

levenshtein\_less\_equal is accelerated version of levenshtein function for low values of distance. If actual distance is less or equal then `max_d`, then levenshtein\_less\_equal returns an accurate value of the distance. Otherwise, this function returns value which is greater than `max_d`. Examples:

```
test=# SELECT levenshtein('GUMBO', 'GAMBOL');
 levenshtein
-------------
           2
(1 row)

test=# SELECT levenshtein('GUMBO', 'GAMBOL', 2,1,1);
 levenshtein
-------------
           3
(1 row)

test=# SELECT levenshtein_less_equal('extensive', 'exhaustive',2);
 levenshtein_less_equal
------------------------
                      3
(1 row)

test=# SELECT levenshtein_less_equal('extensive', 'exhaustive',4);
 levenshtein_less_equal
------------------------
                      4
(1 row)
```

For information about the Levenshtein algorithm, see [http://www.levenshtein.net/](http://www.levenshtein.net/).

## Metaphone Functions 

Metaphone, like Soundex, is based on the idea of constructing a representative code for an input string. Two strings are then deemed similar if they have the same codes. This function calculates the metaphone code of an input string :

```
metaphone(text <source>, int <max_output_length>) returns text
```

The `source` parameter must be a non-null string with a maximum of 255 characters. The `max_output_length` parameter sets the maximum length of the output metaphone code; if longer, the output is truncated to this length. Example:

```
test=# SELECT metaphone('GUMBO', 4);
 metaphone
-----------
 KM
(1 row)
```

For information about the Metaphone algorithm, see [https://en.wikipedia.org/wiki/Metaphone](https://en.wikipedia.org/wiki/Metaphone).

## Double Metaphone Functions 

The Double Metaphone system computes two "sounds like" strings for a given input string - a "primary" and an "alternate". In most cases they are the same, but for non-English names especially they can be a bit different, depending on pronunciation. These functions compute the primary and alternate codes:

```
dmetaphone(text <source>) returns text
dmetaphone_alt(text <source>) returns text
```

There is no length limit on the input strings. Example:

```
test=# select dmetaphone('gumbo');
 dmetaphone
------------
 KMP
(1 row)
```

For information about the Double Metaphone algorithm, see [https://en.wikipedia.org/wiki/Metaphone\#Double\_Metaphone](https://en.wikipedia.org/wiki/Metaphone#Double_Metaphone).

## Installing and Uninstalling the Fuzzy String Match Functions 

Greenplum Database supplies SQL scripts to install and uninstall the Fuzzy String Match extension functions.

To install the functions in a database, run the following SQL script:

```
psql -f $GPHOME/share/postgresql/contrib/fuzzystrmatch.sql
```

To uninstall the functions, run the following SQL script:

```
psql -f $GPHOME/share/postgresql/contrib/uninstall_fuzzystrmatch.sql
```

**Note:** When you uninstall the Fuzzy String Match functions from a database, routines that you created in the database that use the functions will no longer work.

