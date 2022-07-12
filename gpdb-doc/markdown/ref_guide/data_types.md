# Greenplum Database Data Types 

Greenplum Database has a rich set of native data types available to users. Users may also define new data types using the `CREATE TYPE` command. This reference shows all of the built-in data types. In addition to the types listed here, there are also some internally used data types, such as *oid* \(object identifier\), but those are not documented in this guide.

Optional modules in the contrib directory may also install new data types. The `hstore` module, for example, introduces a new data type and associated functions for working with key-value pairs. See [hstore Functions](../utility_guide/hstore.html). The `citext` module adds a case-insensitive text data type. See [citext Data Type](../utility_guide/citext.html).

The following data types are specified by SQL: *bit*, *bit varying*, *boolean*, *character varying, varchar*, *character, char*, *date*, *double precision*, *integer*, *interval*, *numeric*, *decimal*, *real*, *smallint*, *time* \(with or without time zone\), and *timestamp* \(with or without time zone\).

Each data type has an external representation determined by its input and output functions. Many of the built-in types have obvious external formats. However, several types are either unique to PostgreSQL \(and Greenplum Database\), such as geometric paths, or have several possibilities for formats, such as the date and time types. Some of the input and output functions are not invertible. That is, the result of an output function may lose accuracy when compared to the original input.

|Name|Alias|Size|Range|Description|
|----|-----|----|-----|-----------|
|bigint|int8|8 bytes|-922337203​6854775808 to 922337203​6854775807|large range integer|
|bigserial|serial8|8 bytes|1 to 922337203​6854775807|large autoincrementing integer|
|bit \[ \(n\) \]| |*n* bits|[bit string constant](https://www.postgresql.org/docs/8.3/static/sql-syntax.html#SQL-SYNTAX-BIT-STRINGS)|fixed-length bit string|
|bit varying \[ \(n\) \][1](#if139219)|varbit|actual number of bits|[bit string constant](https://www.postgresql.org/docs/8.3/static/sql-syntax.html#SQL-SYNTAX-BIT-STRINGS)|variable-length bit string|
|boolean|bool|1 byte|true/false, t/f, yes/no, y/n, 1/0|logical boolean \(true/false\)|
|box| |32 bytes|\(\(x1,y1\),\(x2,y2\)\)|rectangular box in the plane - not allowed in distribution key columns.|
|bytea[1](#if139219)| |1 byte + *binary string*|sequence of [octets](https://www.postgresql.org/docs/8.3/static/datatype-binary.html#DATATYPE-BINARY-SQLESC)|variable-length binary string|
|character \[ \(n\) \][1](#if139219)|char \[ \(n\) \]|1 byte + *n*|strings up to *n* characters in length|fixed-length, blank padded|
|character varying \[ \(n\) \][1](#if139219)|varchar \[ \(n\) \]|1 byte + *string size*|strings up to *n* characters in length|variable-length with limit|
|cidr| |12 or 24 bytes| |IPv4 and IPv6 networks|
|circle| |24 bytes|<\(x,y\),r\> \(center and radius\)|circle in the plane - not allowed in distribution key columns.|
|date| |4 bytes|4713 BC - 294,277 AD|calendar date \(year, month, day\)|
|decimal \[ \(p, s\) \][1](#if139219)|numeric \[ \(p, s\) \]|variable|no limit|user-specified precision, exact|
|double precision|float8<br/>float<br/>|8 bytes|15 decimal digits precision|variable-precision, inexact|
|inet| |12 or 24 bytes| |IPv4 and IPv6 hosts and networks|
|integer|int, int4|4 bytes|-2147483648 to +2147483647|usual choice for integer|
|interval \[ \(p\) \]| |12 bytes|-178000000 years - 178000000 years|time span|
|json| |1 byte + json size|json of any length|variable unlimited length|
|lseg| |32 bytes|\(\(x1,y1\),\(x2,y2\)\)|line segment in the plane - not allowed in distribution key columns.|
|macaddr| |6 bytes| |MAC addresses|
|money| |8 bytes|-92233720368547758.08 to +92233720368547758.07|currency amount|
|path[<sup>1](#if139219)| |16+16n bytes|\[\(x1,y1\),...\]|geometric path in the plane - not allowed in distribution key columns.|
|point| |16 bytes|\(x,y\)|geometric point in the plane - not allowed in distribution key columns.|
|polygon| |40+16n bytes|\(\(x1,y1\),...\)|closed geometric path in the plane - not allowed in distribution key columns.|
|real|float4|4 bytes|6 decimal digits precision|variable-precision, inexact|
|serial|serial4|4 bytes|1 to 2147483647|autoincrementing integer|
|smallint|int2|2 bytes|-32768 to +32767|small range integer|
|text[<sup>1</sup>](#if139219)| |1 byte + *string size*|strings of any length|variable unlimited length|
|time \[ \(p\) \] \[ without time zone \]| |8 bytes|00:00:00\[.000000\] - 24:00:00\[.000000\]|time of day only|
|time \[ \(p\) \] with time zone|timetz|12 bytes|00:00:00+1359 - 24:00:00-1359|time of day only, with time zone|
|timestamp \[ \(p\) \] \[ without time zone \]| |8 bytes|4713 BC - 294,277 AD|both date and time|
|timestamp \[ \(p\) \] with time zone|timestamptz|8 bytes|4713 BC - 294,277 AD|both date and time, with time zone|
|uuid| |16 bytes| |Universally Unique Identifiers according to RFC 4122, ISO/IEC 9834-8:2005|
|xml[<sup>1</sup>](#if139219)| |1 byte + *xml size*|xml of any length|variable unlimited length|
|txid\_snapshot| | | |user-level transaction ID snapshot|

## Pseudo-Types 

Greenplum Database supports special-purpose data type entries that are collectively called *pseudo-types*. A pseudo-type cannot be used as a column data type, but it can be used to declare a function's argument or result type. Each of the available pseudo-types is useful in situations where a function's behavior does not correspond to simply taking or returning a value of a specific SQL data type.

Functions coded in procedural languages can use pseudo-types only as allowed by their implementation languages. The procedural languages all forbid use of a pseudo-type as an argument type, and allow only *void* and *record* as a result type.

A function with the pseudo-type *record* as a return data type returns an unspecified row type. The *record* represents an array of possibly-anonymous composite types. Since composite datums carry their own type identification, no extra knowledge is needed at the array level.

The pseudo-type *void* indicates that a function returns no value.

**Note:** Greenplum Database does not support triggers and the pseudo-type *trigger*.

The types *anyelement*, *anyarray*, *anynonarray*, and *anyenum* are pseudo-types called polymorphic types. Some procedural languages also support polymorphic functions using the types *anyarray*, *anyelement*, *anyenum*, and *anynonarray*.

The pseudo-type *anytable* is a Greenplum Database type that specifies a table expression—an expression that computes a table. Greenplum Database allows this type only as an argument to a user-defined function. See [Table Value Expressions](#topic22) for more about the *anytable* pseudo-type.

For more information about pseudo-types, see the Postgres documentation about [Pseudo-Types](https://www.postgresql.org/docs/8.3/static/datatype-pseudo.html).

## Polymorphic Types 

Four pseudo-types of special interest are *anyelement*, *anyarray*, *anynonarray*, and *anyenum*, which are collectively called *polymorphic* types. Any function declared using these types is said to be a polymorphic function. A polymorphic function can operate on many different data types, with the specific data types being determined by the data types actually passed to it at runtime.

Polymorphic arguments and results are tied to each other and are resolved to a specific data type when a query calling a polymorphic function is parsed. Each position \(either argument or return value\) declared as *anyelement* is allowed to have any specific actual data type, but in any given call they must all be the same actual type. Each position declared as *anyarray* can have any array data type, but similarly they must all be the same type. If there are positions declared *anyarray* and others declared *anyelement*, the actual array type in the *anyarray* positions must be an array whose elements are the same type appearing in the *anyelement* positions. *anynonarray* is treated exactly the same as *anyelement*, but adds the additional constraint that the actual type must not be an array type. *anyenum* is treated exactly the same as *anyelement*, but adds the additional constraint that the actual type must be an `enum` type.

When more than one argument position is declared with a polymorphic type, the net effect is that only certain combinations of actual argument types are allowed. For example, a function declared as equal\(*anyelement*, *anyelement*\) takes any two input values, so long as they are of the same data type.

When the return value of a function is declared as a polymorphic type, there must be at least one argument position that is also polymorphic, and the actual data type supplied as the argument determines the actual result type for that call. For example, if there were not already an array subscripting mechanism, one could define a function that implements subscripting as subscript\(*anyarray*, integer\) returns *anyelement*. This declaration constrains the actual first argument to be an array type, and allows the parser to infer the correct result type from the actual first argument's type. Another example is that a function declared as myfunc\(*anyarray*\) returns *anyenum* will only accept arrays of `enum` types.

Note that *anynonarray* and *anyenum* do not represent separate type variables; they are the same type as *anyelement*, just with an additional constraint. For example, declaring a function as myfunc\(*anyelement*, *anyenum*\) is equivalent to declaring it as myfunc\(*anyenum*, *anyenum*\): both actual arguments must be the same `enum` type.

A variadic function \(one taking a variable number of arguments\) is polymorphic when its last parameter is declared as VARIADIC *anyarray*. For purposes of argument matching and determining the actual result type, such a function behaves the same as if you had declared the appropriate number of *anynonarray* parameters.

For more information about polymorphic types, see the Postgres documentation about [Polymorphic Arguments and Return Types](https://www.postgresql.org/docs/8.3/static/xfunc-c.html#AEN41553).

## Table Value Expressions 

The *anytable* pseudo-type declares a function argument that is a table value expression. The notation for a table value expression is a `SELECT` statement enclosed in a `TABLE()` function. You can specify a distribution policy for the table by adding `SCATTER RANDOMLY`, or a `SCATTER BY` clause with a column list to specify the distribution key.

The `SELECT` statement is executed when the function is called and the result rows are distributed to segments so that each segment executes the function with a subset of the result table.

For example, this table expression selects three columns from a table named `customer` and sets the distribution key to the first column:

```
TABLE(SELECT cust_key, name, address FROM customer SCATTER BY 1)
```

The `SELECT` statement may include joins on multiple base tables, `WHERE` clauses, aggregates, and any other valid query syntax.

The *anytable* type is only permitted in functions implemented in the C or C++ languages. The body of the function can access the table using the Greenplum Database Server Programming Interface \(SPI\) or the Greenplum Partner Connector \(GPPC\) API.

The *anytable* type is used in some user-defined functions in the VMware Tanzu Greenplum Text API. The following GPText example uses the `TABLE` function with the `SCATTER BY` clause in the GPText function `gptext.index()` to populate the index `mydb.mytest.articles` with data from the messages table:

```
SELECT * FROM gptext.index(TABLE(SELECT * FROM mytest.messages 
          SCATTER BY distrib_id), 'mydb.mytest.messages');
        
```

For information about the function `gptext.index()`, see the VMware Tanzu Greenplum Text documentation.

**Parent topic:** [Greenplum Database Reference Guide](ref_guide.html)

<sup>1</sup> For variable length data types, if the data is greater than or equal to 127 bytes, the storage overhead is 4 bytes instead of 1.

