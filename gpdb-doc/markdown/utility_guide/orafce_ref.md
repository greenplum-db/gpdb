# Oracle Compatibility Functions 

Describes the Oracle Compatibility SQL functions in Greenplum Database. These functions target PostgreSQL.

**Parent topic:** [Additional Supplied Modules](contrib-modules.html)

## Installing Oracle Compatibility Functions 

Before using any Oracle Compatibility Functions, run the installation script `$GPHOME/share/postgresql/contrib/orafunc.sql` once for each database. For example, to install the functions in database `testdb`, use the following command:

```
$ psql -d testdb -f $GPHOME/share/postgresql/contrib/orafunc.sql
```

To uninstall Oracle Compatibility Functions, run the `uinstall_orafunc.sql` script:

```
$GPHOME/share/postgresql/contrib/uninstall_orafunc.sql
```

The following functions are available by default and do not require running the Oracle Compatibility installer:

-   [sinh](#topic28)
-   [tanh](#topic29)
-   [cosh](#topic8)
-   [decode](#topic9)

**Note:** The Oracle Compatibility Functions reside in the `oracompat` schema. To access them, prefix the schema name \(`oracompat`\) or alter the database search path to include the schema name. For example:

```
ALTER DATABASE <db_name> SET <search_path> = $<user>, public, oracompat; 
```

If you alter the database search path, you must restart the database.

## Oracle and Greenplum Implementation Differences 

There are some differences in the implementation of these compatibility functions in the Greenplum Database from the Oracle implementation. If you use validation scripts, the output may not be exactly the same as in Oracle. Some of the differences are as follows:

-   Oracle performs a decimal round off, Greenplum Database does not:
    -   2.00 becomes 2 in Oracle.
    -   2.0.0 remains 2.00 in Greenplum Database.
-   The provided Oracle Compatibility functions handle implicit type conversions differently. For example, using the `decode` function:

    ```
    decode(<expression>, <value>, <return> [,<value>, <return>]...
                [, default])
    ```

    Oracle automatically converts expression and each value to the datatype of the first value before comparing. Oracle automatically converts return to the same datatype as the first result.

    The Greenplum implementation restricts return and `default` to be of the same data type. The expression and value can be different types if the data type of value can be converted into the data type of the expression. This is done implicitly. Otherwise, `decode` fails with an `invalid input syntax` error. For example:

    ```
    SELECT decode('M',true,false);
    CASE
    ------
     f
    (1 row)
    SELECT decode(1,'M',true,false);
    ERROR: Invalid input syntax for integer:*"M" 
    *LINE 1: SELECT decode(1,'M',true,false);
    ```

-   Numbers in `bigint` format are displayed in scientific notation in Oracle, but not in Greenplum Database:
    -   9223372036854775 displays as 9.2234E+15 in Oracle.
    -   9223372036854775 remains 9223372036854775 in Greenplum Database.
-   The default date and timestamp format in Oracle is different than the default format in Greenplum Database. If the following code is executed:

    ```
    CREATE TABLE TEST(date1 date, time1 timestamp, time2 
                      timestamp with timezone);
    INSERT INTO TEST VALUES ('2001-11-11','2001-12-13 
                     01:51:15','2001-12-13 01:51:15 -08:00');
    SELECT DECODE(date1, '2001-11-11', '2001-01-01') FROM TEST;
    ```

    Greenplum Database returns the row, but Oracle does not return any rows.

    **Note:** The correct syntax in Oracle to return the row is:

    ```
    SELECT DECODE(to_char(date1, 'YYYY-MM-DD'), '2001-11-11', 
                  '2001-01-01') FROM TEST
    ```


## Oracle Compatibility Functions Reference 

The following are the Oracle Compatibility Functions.

-   [add\_months](#topic5)

-   [bitand](#topic6)

-   [concat](#topic7)

-   [cosh](#topic8)

-   [decode](#topic9)

-   [dump](#topic12)

-   [instr](#topic13)

-   [last\_day](#topic14)

-   [listagg](#topic15)

-   [listagg \(2\)](#topic16)

-   [lnnvl](#topic17)

-   [months\_between](#topic18)

-   [nanvl](#topic19)

-   [next\_day](#topic20)

-   [next\_day \(2\)](#topic21)

-   [nlssort](#topic22)

-   [nvl](#topic23)

-   [nvl2](#topic24)

-   [oracle.substr](#topic25)

-   [reverse](#topic26)

-   [round](#topic27)

-   [sinh](#topic28)

-   [tanh](#topic29)

-   [trunc](#topic30)

## add\_months 

Oracle-compliant function to add a given number of months to a given date.

### Synopsis 

```
add_months(<date_expression>, <months_to_add>)
```

This Oracle-compatible function adds months\_to\_add to a date\_expression and returns a `DATE`.

If the date\_expression specifies the last day of the month, or if the resulting month has fewer days than the date\_expression, then the returned value is the last day of the resulting month. Otherwise, the returned value has the same day of the month as the date\_expression.

date\_expression
:   The starting date. This can be any expression that can be implicitly converted to `DATE`.

months\_to\_add
:   The number of months to add to the date\_expression. This is an integer or any value that can be implicitly converted to an integer. This parameter can be positive or negative.

```
SELECT name, phone, nextcalldate FROM clientdb
WHERE nextcalldate >= add_months(CURRENT_DATE,6);
```

Returns `name`, `phone`, and `nextcalldate` for all records where `nextcalldate` is at least six months in the future.

This command is compatible with Oracle syntax and is provided for convenience.

## bitand 

Oracle-compliant function that computes a logical `AND` operation on the bits of two non-negative values.

### Synopsis 

```
bitand(<expr1>, <expr2>)
```

This Oracle-compatible function returns an integer representing an `AND` operation on the bits of two non-negative values \(expr1 and expr2\). 1 is returned when the values are the same. 0 is returned when the values are different. Only significant bits are compared. For example, an `AND` operation on the integers 5 \(binary 101\) and 1 \(binary 001 or 1\) compares only the rightmost bit, and results in a value of 1 \(binary 1\).

The types of expr1 and expr2 are `NUMBER`, and the result is of type `NUMBER`. If either argument is `NULL`, the result is `NULL`.

The arguments must be in the range `-(2(n-1)) .. ((2(n-1))-1)`. If an argument is out of this range, the result is undefined.

**Note:**

-   The current implementation of `BITAND` defines n = 128.
-   PL/SQL supports an overload of `BITAND` for which the types of the inputs and of the result are all `BINARY_INTEGER` and for which n = 32.

expr1
:   A non-negative integer expression.

expr2
:   A non-negative integer expression.

```
SELECT bitand(<expr1>, <expr2>)
FROM ClientDB;
```

This command is compatible with Oracle syntax and is provided for convenience.

## concat 

Oracle-compliant function to concatenate two strings together.

### Synopsis 

```
concat (<string1>, <string2>)
```

This Oracle-compatible function concatenates two strings \(string1 and string2\) together.

The string returned is in the same character set as string1. Its datatype depends on the datatypes of the arguments.

In concatenations of two different datatypes, the datatype returned is the one that results in a lossless conversion. Therefore, if one of the arguments is a `LOB`, then the returned value is a `LOB`. If one of the arguments is a national datatype, then the returned value is a national datatype. For example:

```
concat(CLOB, NCLOB) returns NCLOB
concat(NCLOB, NCHAR) returns NCLOB
concat(NCLOB, CHAR) returns NCLOB
concat(NCHAR, CLOB) returns NCLOB
```

This function is equivalent to the concatenation operator \(`||`\).

string1/string2
:   The two strings to concatenate together.

:   Both string1 and string2 can be any of the datatypes `CHAR`, `VARCHAR2`, `NCHAR`, `NVARCHAR2`, `CLOB`, or `NCLOB`.

```
SELECT concat(concat(last_name, '''s job category is '),
     job_id)
FROM employees 
```

Returns `'Smith's job category is 4B'`

This command is compatible with Oracle syntax and is provided for convenience.

## cosh 

Oracle-compliant function to return the hyperbolic cosine of a given number.

### Synopsis 

```
cosh(<float8>)
```

This Oracle-compatible function returns the hyperbolic cosine of the floating 8 input number \(float8\).

**Note:** This function is available by default and can be accessed without running the Oracle Compatibility installer.

float8
:   The input number.

```
SELECT cosh(0.2)
FROM ClientDB;
```

Returns `'1.02006675561908'`' \(hyperbolic cosine of 0.2\)

This command is compatible with Oracle syntax and is provided for convenience.

## decode 

Oracle-compliant function to transform a data value to a specified return value. This function is a way to implement a set of `CASE` statements.

**Note:** `decode` is converted into a reserved word in Greenplum Database. If you want to use the Postgres two-argument `decode` function that decodes binary strings previously encoded to ASCII-only representation, you must invoke it by using the full schema-qualified syntax, `pg_catalog.decode()`, or by enclosing the function name in quotes `"decode" ()`.

**Note:** Greenplum's implementation of this function transforms `decode` into `case`.

This results in the following type of output:

```
gptest=# select decode(a, 1, 'A', 2, 'B', 'C') from 
decodetest;
 case 
------
 C
 A
 C
 B
 C
(5 rows)
```

This also means that if you deparse your view with `decode`, you will see `case` expression instead.

You should use the `case` function instead of `decode`.

### Synopsis 

```
decode(<expression>, <value>, <return> [,<value>, <return>]...
       [, default])
```

The Oracle-compatible function decode searches for a value in an expression. If the value is found, the function returns the specified value.

**Note:** This function is available by default and can be accessed without running the Oracle Compatibility installer.

expression
:   The expression to search.

value
:   The value to find in the expression.

return
:   What to return if expression matches value.

default
:   What to return if expression does not match any of the values.

Only one `expression` is passed to the function. Multiple `value`/`return` pairs can be passed.

The `default` parameter is optional. If `default` is not specified and if `expression` does not match any of the passed `value` parameters, decode returns `null`. The Greenplum implementation restricts `return` and `default` to be of the same data type. The `expression` and `value` can be different types if the data type of `value` can be converted into the data type of the `expression`. This is done implicitly. Otherwise, `decode` fails with an `invalid input syntax` error.

In the following code, `decode` searches for a value for `company_id` and returns a specified value for that company. If `company_id` not one of the listed values, the default value `Other` is returned.

```
SELECT decode(company_id, 1, 'EMC',
                          2, 'Greenplum',
                          'Other')
FROM suppliers;
```

The following code using `CASE` statements to produce the same result as the example using `decode`.

```
SELECT CASE company_id
WHEN IS NOT DISTINCT FROM 1 THEN 'EMC'
WHEN IS NOT DISTINCT FROM 2 THEN 'Greenplum'
ELSE 'Other'
END
FROM suppliers;
```

To assign a range of values to a single return value, either pass an expression for each value in the range, or pass an expression that evaluates identically for all values in the range. For example, if a fiscal year begins on August 1, the quarters are shown in the following table.

|Range \(Alpha\)|Range \(Numeric\)|Quarter|
|---------------|-----------------|-------|
|August — October|8 — 10|Q1|
|November — January|11 — 1|Q2|
|February — April|2 — 4|Q3|
|May — July|5 — 7|Q4|

The table contains a numeric field `curr_month` that holds the numeric value of a month, 1 – 12. There are two ways to use `decode` to get the quarter:

-   Method 1 - Include 12 values in the `decode` function:

    ```
    SELECT decode(curr_month, 1, 'Q2',
                              2, 'Q3',
                              3, 'Q3',
                              4, 'Q3',
                              5, 'Q4',
                              6, 'Q4',
                              7, 'Q4',
                              8, 'Q1',
                              9, 'Q1',
                             10, 'Q1',
                             11, 'Q2',
                             12, 'Q2')
    FROM suppliers;
    ```


-   Method 2 - Use an expression that defines a unique value to decode:

    ```
    SELECT decode((1+MOD(curr_month+4,12)/3)::int, 1, 'Q1',
                                                   2, 'Q2',
                                                   3, 'Q3',
                                                   4, 'Q4',
    FROM suppliers;
    ```


This command is compatible with Oracle syntax and is provided for convenience.

PostgreSQL [decode](https://www.postgresql.org/docs/8.3/static/functions-binarystring.html) \(not compatible with Oracle\)

## dump 

Oracle-compliant function that returns a text value that includes the datatype code,the length in bytes, and the internal representation of the expression.

### Synopsis 

```
dump(<expression> [,<integer>]) 
```

This Oracle-compatible function returns a text value that includes the datatype code, the length in bytes, and the internal representation of the expression.

expression
:   Any expression

integer
:   The number of characters to return

```
dump('Tech') returns 'Typ=96 Len=4: 84,101,99,104'

dump ('tech') returns 'Typ-96 Len=4: 84,101,99,104'

dump('Tech', 10) returns 'Typ=96 Len=4: 84,101,99,104'

dump('Tech', 16) returns 'Typ=96 Len=4: 54,65,63,68'

dump('Tech', 1016) returns 'Typ=96 Len=4 CharacterSet=US7ASCII:     54,65,63,68'

dump('Tech', 1017) returns 'Typ=96 Len=4 CharacterSet=US7ASCII:     T,e,c,h'
```

This command is compatible with Oracle syntax and is provided for convenience.

## instr 

Oracle-compliant function to return the location of a substring in a string.

### Synopsis 

```
instr(<string>, <substring>, [<position>[,<occurrence>]])
```

This Oracle-compatible function searches for a substring in a string. If found, it returns an integer indicating the position of the substring in the string, if not found, the function returns 0.

Optionally you can specify that the search starts at a given position in the string, and only return the nth occurrence of the substring in the string.

`instr`calculates strings using characters as defined by the input character set.

The value returned is of `NUMBER` datatype.

string
:   The string to search.

substring
:   The substring to search for in string.

:   Both string and substring can be any of the datatypes `CHAR`, `VARCHAR2`, `NCHAR`, `NVARCHAR2`, `CLOB`, or `NCLOB`.

position
:   The position is a nonzero integer in string where the search will start. If not specified, this defaults to 1. If this value is negative, the function counts backwards from the end of string then searches towards to beginning from the resulting position.

occurrence
:   Occurrence is an integer indicating which occurrence of the substring should be searched for. The value of occurrence must be positive.

:   Both position and occurrence must be of datatype `NUMBER`, or any datatype that can be implicitly converted to `NUMBER`, and must resolve to an integer. The default values of both position and occurrence are 1, meaning that the search begins at the first character of string for the first occurrence of substring. The return value is relative to the beginning of string, regardless of the value of position, and is expressed in characters.

```
SELECT instr('Greenplum', 'e') 
FROM ClientDB;
```

Returns 3; the first occurrence of 'e'

```
SELECT instr('Greenplum', 'e',1,2)
FROM ClientDB;
```

Returns 4; the second occurrence of 'e'

This command is compatible with Oracle syntax and is provided for convenience.

## last\_day 

Oracle-compliant function to return the last day in a given month.

### Synopsis 

```
last_day(<date_expression>) 
```

This Oracle-compatible function returns the last day of the month specified by a date\_expression.

The return type is always `DATE`, regardless of the datatype of date\_expression.

date\_expression
:   The date value used to calculate the last day of the month. This can be any expression that can be implicitly converted to `DATE`.

```
SELECT name, hiredate, last_day(hiredate) "Option Date"
FROM employees;
```

Returns the `name`, `hiredate`, and `last_day` of the month of `hiredate` labeled " `Option Date`."

This command is compatible with Oracle syntax and is provided for convenience.

## listagg 

Oracle-compliant function that aggregates text values into a string.

**Note:** This function is an overloaded function. There are two Oracle-compliant `listagg` functions, one that takes one argument, the text to be aggregated \(see below\), and one that takes two arguments, the text to be aggregated and a delimiter \(see next page\).

### Synopsis 

```
listagg(<text>) 
```

This Oracle-compatible function aggregates text values into a string.

text
:   The text value to be aggregated into a string.

```
SELECT listagg(t) FROM (VALUES('abc'), ('def')) as l(t) 
```

Returns: `abcdef`

This command is compatible with Oracle syntax and is provided for convenience.

## listagg \(2\) 

Oracle-compliant function that aggregates text values into a string, separating each by the separator specified in a second argument.

**Note:** This function is an overloaded function. There are two Oracle-compliant `listagg` functions, one that takes one argument, the text to be aggregated \(see previous page\), and one that takes two arguments, the text to be aggregated and a delimiter \(see below\).

### Synopsis 

```
listagg(<text>, <separator>) 
```

This Oracle-compatible function aggregates text values into a string, separating each by the separator specified in a second argument \(separator\).

text
:   The text value to be aggregated into a string.

separator
:   The separator by which to delimit the text values.

```
SELECT oracompat.listagg(t, '.') FROM (VALUES('abc'), 
('def')) as l(t)
```

Returns: `abc.def`

This command is compatible with Oracle syntax and is provided for convenience.

## lnnvl 

Oracle-compliant function that returns `true` if the argument is false or NULL, or `false`.

### Synopsis 

```
lnnvl(<condition>) 
```

This Oracle-compatible function takes as an argument a condition and returns `true` if the condition is false or NULL and `false` if the condition is true.

condition
:   Any condition that evaluates to `true`, `false`, or NULL.

```
SELECT lnnvl(true) 
```

Returns: `false`

```
SELECT lnnvl(NULL) 
```

Returns: `true`

```
SELECT lnnvl(false) 
```

Returns: `true`

```
SELECT (3=5)               
```

Returns: `true`

This command is compatible with Oracle syntax and is provided for convenience.

## months\_between 

Oracle-compliant function to evaluate the number of months between two given dates.

### Synopsis 

```
months_between(<date_expression1>, <date_expression2>)
```

This Oracle-compatible function returns the number of months between date\_expression1 and date\_expression2.

If date\_expression1 is later than date\_expression2, then the result is positive.

If date\_expression1 is earlier than date\_expression2, then the result is negative.

If date\_expression1 and date\_expression2 are either the same days of the month or both last days of months, then the result is always an integer. Otherwise the function calculates the fractional portion of the month based on a 31-day month.

date\_expression1, date\_expression2
:   The date values used to calculate the number of months. This can be any expression that can be implicitly converted to `DATE`.

```
SELECT months_between
    (to_date ('2003/07/01', 'yyyy/mm/dd'), 
    to_date ('2003/03/14', 'yyyy/mm/dd'));
```

Returns the number of months between July 1, 2003 and March 14, 2014.

```
SELECT * FROM employees 
    where months_between(hire_date, leave_date) <12;
```

Returns the number of months between `hire_date` and `leave_date`.

This command is compatible with Oracle syntax and is provided for convenience.

## nanvl 

Oracle-compliant function to substitute a value for a floating point number when a non-number value is encountered.

### Synopsis 

```
nanvl(<float1>, <float2>)
```

This Oracle-compatible function evaluates a floating point number \(float1\) such as `BINARY_FLOAT` or `BINARY_DOUBLE`. If it is a non-number \('not a number', NaN\), the function returns float2. This function is most commonly used to convert non-number values into either NULL or 0.

float1
:   The `BINARY_FLOAT` or `BINARY_NUMBER` to evaluate.

float2
:   The value to return if float1 is not a number.

:   float1 and float2 can be any numeric datatype or any nonnumeric datatype that can be implicitly converted to a numeric datatype. The function determines the argument with the highest numeric precedence, implicitly converts the remaining arguments to that datatype, and returns that datatype.

```
SELECT nanvl(binary1, 0)
FROM MyDB;
```

Returns 0 if the `binary1` field contained a non-number value. Otherwise, it would return the `binary1` value.

This command is compatible with Oracle syntax and is provided for convenience.

## next\_day 

Oracle-compliant function to return the date of the next specified weekday after a date.

This section describes using this function with a string argument; see the following page for details about using this function with an integer argument.

**Note:** This function is an overloaded function. There are two Oracle-compliant `next_day` functions, one that takes a date and a day of the week as its arguments \(see below\), and one that takes a date and an integer as its arguments \(see next page\).

### Synopsis 

```
next_day(<date_expression>, `day_of_the_week`)
```

This Oracle-compatible function returns the first `day_of_the_week` \(Tuesday, Wednesday, etc.\) to occur after a date\_expression.

The weekday must be specified in English.

The case of the weekday is irrelevant.

The return type is always `DATE`, regardless of the datatype of date\_expression.

date\_expression
:   The starting date. This can be any expression that can be implicitly converted to `DATE`.

day\_of\_the\_week
:   A string containing the name of a day, in English; for example 'Tuesday'. day\_of\_the\_week is case-insensitive.

```
SELECT name, next_day(hiredate,"MONDAY") "Second Week Start"
FROM employees;
```

Returns the `name` and the `date` of the next Monday after `hiredate` labeled "`Second Week Start`".

This command is compatible with Oracle syntax and is provided for convenience.

## next\_day \(2\) 

Oracle-compliant function to add a given number of days to a date and returns the date of the following day.

**Note:** This function is an overloaded function. There are two Oracle `next_day` functions, one that takes a date and a day of the week as its arguments \(see previous page\), and one that takes a date and an integer as its arguments \(see below\).

### Synopsis 

```
next_day(<date_expression>, <days_to_add>) 
```

This Oracle-compatible function adds the number of days\_to\_add to a date\_expression and returns the date of the day after the result.

The return type is always `DATE`, regardless of the datatype of date\_expression.

date\_expression
:   The starting date. This can be any expression that can be implicitly converted to `DATE`.

days\_to\_add
:   The number of days to be add to the date\_expression. This is an integer or any value that can be implicitly converted to an integer. This parameter can be positive or negative.

```
SELECT name, next_day(hiredate,90) "Benefits Eligibility 
Date"
FROM EMPLOYEES;
```

Returns the `name` and the `date` that is 90 days after `hiredate` labeled "`Benefits Eligibility Date`".

This command is compatible with Oracle syntax and is provided for convenience.

## nlssort 

Oracle-compliant function that sorts data according to a specific collation.

### Synopsis 

```
nlssort (<variable>, <collation>)
```

This Oracle-compatible function sorts data according to a specific collation.

variable
:   The data to sort.

collation
:   The collation type by which to sort.

```
CREATE TABLE test (name text);
INSERT INTO test VALUES('Anne'), ('anne'), ('Bob'), ('bob');
SELECT * FROM test ORDER BY nlssort(name, 'en_US.UTF-8');
 anne
 Anne
 bob
 Bob

SELECT * FROM test ORDER BY nlssort(name, 'C');
 Anne
 Bob
 anne
 bob 
```

In the first example, the UTF-8 collation rules are specified. This groups characters together regardless of case.

In the second example, ASCII \(C\) collation is specified. This sorts according to ASCII order. The result is that upper case characters are sorted ahead of lower case ones.

This command is compatible with Oracle syntax and is provided for convenience.

## nvl 

Oracle-compliant function to substitute a specified value when an expression evaluates to `null`.

**Note:** This function is analogous to the PostgreSQL `coalesce` function.

### Synopsis 

```
nvl(<expression_to_evaluate>, <null_replacement_value>)
```

This Oracle-compatible function evaluates expression\_to\_evaluate. If it is `null`, the function returns null\_replacement\_value; otherwise, it returns expression\_to\_evaluate.

expression\_to\_evaluate
:   The expression to evaluate for a null value.

null\_replacement\_value
:   The value to return if expression\_to\_evaluate is `null`.

Both expression\_to\_evaluate and null\_replacement\_value must be the same data type.

```
SELECT nvl(contact_name,'None') 
FROM clients;
SELECT nvl(amount_past_due,0) 
FROM txns;
SELECT nvl(nickname, firstname) 
FROM contacts;
```

This command is compatible with Oracle syntax and is provided for convenience.

## nvl2 

Oracle-compliant function that returns alternate values for both null and non-null values.

### Synopsis 

```
nvl2(<expression_to_evaluate>, <non_null_replacement_value>,
     <null_replacement_value>)
```

This Oracle-compatible function evaluates expression\_to\_evaluate. If it is not `null`, the function returns non\_null\_replacement\_value; otherwise, it returns null\_replacement\_value.

expression\_to\_evaluate
:   The expression to evaluate for a null value.

non\_null\_replacement\_value
:   The value to return if expression\_to\_evaluate is not `null`.

null\_replacement\_value
:   The value to return if expression\_to\_evaluate is `null`.

```
select nvl2(unit_number,'Multi Unit','Single Unit') 
from clients;
```

This command is compatible with Oracle syntax and is provided for convenience.

[decode](#topic9)

## oracle.substr 

This Oracle-compliant function extracts a portion of a string.

### Synopsis 

```
oracle.substr(<string>, [<start> [,<char_count>]])
```

This Oracle-compatible function extract a portion of a string.

If start is 0, it is evaluated as 1.

If start is negative, the starting position is negative, the starting position is start characters moving backwards from the end of string.

If char\_count is not passed to the function, all characters from start to the end of string are returned.

If char\_count is less than 1, null is returned.

If start or char\_count is a number, but not an integer, the values are resolved to integers.

string
:   The string from which to extract.

start
:   An integer specifying the starting position in the string.

char\_count
:   An integer specifying the number of characters to extract.

```
oracle.substr(name,1,15) 
```

Returns the first 15 characters of `name`.

```
oracle.substr("Greenplum",-4,4) 
```

Returns "`plum`".

```
oracle.substr(name,2) 
```

Returns all characters of `name`, beginning with the second character.

PostgreSQL [substr](https://www.postgresql.org/docs/8.3/static/functions-string.html) \(not compatible with Oracle\)

## reverse 

Oracle-compliant function to return the input string in reverse order.

### Synopsis 

```
reverse (<string>)
```

This Oracle-compatible function returns the input string \(string\) in reverse order.

string
:   The input string.

```
SELECT reverse('gnirts') 
FROM ClientDB;
```

Returns `'string'`'

This command is compatible with Oracle syntax and is provided for convenience.

## round 

Oracle-compliant function to round a date to a specific unit of measure \(day, week, etc.\).

**Note:** This function is an overloaded function. It shares the same name with the Postgres `round` mathematical function that rounds numeric input to the nearest integer or optionally to the nearest x number of decimal places.

### Synopsis 

```
round (<date_time_expression>, [<unit_of_measure>]) 
```

This Oracle-compatible function rounds a date\_time\_expression to the nearest unit\_of\_measure \(day, week, etc.\). If a unit\_of\_measure is not specified, the date\_time\_expression is rounded to the nearest day. It operates according to the rules of the Gregorian calendar.

If the date\_time\_expression datatype is `TIMESTAMP`, the value returned is always of datatype `TIMESTAMP`.

If the date\_time\_expression datatype is `DATE`, the value returned is always of datatype `DATE`.

date\_time\_expression
:   The date to round. This can be any expression that can be implicitly converted to `DATE` or `TIMESTAMP`.

unit\_of\_measure
:   The unit of measure to apply for rounding. If not specified, then the date\_time\_expression is rounded to the nearest day. Valid parameters are:

|Unit|Valid parameters|Rounding Rule|
|----|----------------|-------------|
|Year|SYYYY, YYYY, YEAR, SYEAR, YYY, YY, Y|Rounds up on July 1st|
|ISO Year|IYYY, IY, I| |
|Quarter|Q|Rounds up on the 16th day of the second month of the quarter|
|Month|MONTH, MON, MM, RM|Rounds up on the 16th day of the month|
|Week|WW|Same day of the week as the first day of the year|
|IW|IW|Same day of the week as the first day of the ISO year|
|W|W|Same day of the week as the first day of the month|
|Day|DDD, DD, J|Rounds to the nearest day|
|Start day of the week|DAY, DY, D|Rounds to the nearest start \(sunday\) day of the week|
|Hour|HH, HH12, HH24|Rounds to the next hour|
|Minute|MI|Rounds to the next minute|

```
SELECT round(TO_DATE('27-OCT-00','DD-MON-YY'), 'YEAR')
FROM ClientDB;
```

Returns '01-JAN-01' \(27 Oct 00 rounded to the first day of the following year \(`YEAR`\)\)

```
SELECT round('startdate','Q')
FROM ClientDB;
```

Returns '01-JUL-92' \(the `startdate` rounded to the first day of the quarter \(`Q`\)\)

This command is compatible with Oracle syntax and is provided for convenience.

PostgreSQL [round](https://www.postgresql.org/docs/8.3/static/functions-math.html) \(not compatible with Oracle\)

## sinh 

Oracle-compliant function to return the hyperbolic sine of a given number.

### Synopsis 

```
sinh(<float8>)
```

This Oracle-compatible function returns the hyperbolic sine of the floating 8 input number \(float8\).

**Note:** This function is available by default and can be accessed without running the Oracle Compatibility installer.

float8
:   The input number.

```
SELECT sinh(3) 
FROM ClientDB;
```

Returns `'10.0178749274099'`'\(hyperbolic sine of 3\)

This command is compatible with Oracle syntax and is provided for convenience.

## tanh 

Oracle-compliant function to return the hyperbolic tangent of a given number.

### Synopsis 

```
tanh(<float8>)
```

This Oracle-compatible function returns the hyperbolic tangent of the floating 8 input number \(float8\).

**Note:**

This function is available by default and can be accessed without running the Oracle Compatibility installer.

float8
:   The input number.

```
SELECT tanh(3)
FROM ClientDB;
```

Returns `'0.99505475368673'`' \(hyperbolic tangent of 3\)

This command is compatible with Oracle syntax and is provided for convenience.

## trunc 

Oracle-compliant function to truncate a date to a specific unit of measure \(day, week, hour, etc.\).

**Note:**

This function is an overloaded function. It shares the same name with the Postgres `trunc` and the Oracle `trunc` mathematical functions. Both of these truncate numeric input to the nearest integer or optionally to the nearest x number of decimal places.

### Synopsis 

```
trunc(<date_time_expression>, [<unit_of_measure>]) 
```

This Oracle-compatible function truncates a date\_time\_expression to the nearest unit\_of\_measure \(day, week, etc.\). If a unit\_of\_measure is not specified, the date\_time\_expression is truncated to the nearest day. It operates according to the rules of the Gregorian calendar.

If the date\_time\_expression datatype is `TIMESTAMP`, the value returned is always of datatype `TIMESTAMP`, truncated to the hour/min level.

If the date\_time\_expression datatype is `DATE`, the value returned is always of datatype `DATE`.

date\_time\_expression
:   The date to truncate. This can be any expression that can be implicitly converted to `DATE` or `TIMESTAMP`.

unit\_of\_measure
:   The unit of measure to apply for truncating. If not specified, then date\_\_time\_expression is truncated to the nearest day. Valid formats are:

|Unit|Valid parameters|
|----|----------------|
|Year|SYYYY, YYYY, YEAR, SYEAR, YYY, YY, Y|
|ISO Year|IYYY, IY, I|
|Quarter|Q|
|Month|MONTH, MON, MM, RM|
|Week|WW|
|IW|IW|
|W|W|
|Day|DDD, DD, J|
|Start day of the week|DAY, DY, D|
|Hour|HH, HH12, HH24|
|Minute|MI|

```
SELECT TRUNC(TO_DATE('27-OCT-92','DD-MON-YY'), 'YEAR')
FROM ClientDB;
```

Returns '01-JAN-92' \(27 Oct 92 truncated to the first day of the year \(`YEAR`\)\)

```
SELECT TRUNC(startdate,'Q')
FROM ClientDB;
```

Returns '1992-07-01' \(the `startdate` truncated to the first day of the quarter \(`Q`\), depending on the date\_style setting\)

This command is compatible with Oracle syntax and is provided for convenience.

PostgreSQL [trunc](https://www.postgresql.org/docs/8.3/static/functions-math.html) \(not compatible with Oracle\)

