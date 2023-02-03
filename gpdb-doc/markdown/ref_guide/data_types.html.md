# Data Types 

Greenplum Database has a rich set of native data types available to users. Users may also define new data types using the `CREATE TYPE` command. This reference shows all of the built-in data types. In addition to the types listed here, there are also some internally used data types, such as *oid* \(object identifier\), but those are not documented in this guide.

Additional modules that you register may also install new data types. The `hstore` module, for example, introduces a new data type and associated functions for working with key-value pairs. See [hstore](modules/hstore.html). The `citext` module adds a case-insensitive text data type. See [citext](modules/citext.html).

The following data types are specified by SQL: *bit*, *bit varying*, *boolean*, *character varying, varchar*, *character, char*, *date*, *double precision*, *integer*, *interval*, *numeric*, *decimal*, *real*, *smallint*, *time* \(with or without time zone\), and *timestamp* \(with or without time zone\).

Each data type has an external representation determined by its input and output functions. Many of the built-in types have obvious external formats. However, several types are either unique to PostgreSQL \(and Greenplum Database\), such as geometric paths, or have several possibilities for formats, such as the date and time types. Some of the input and output functions are not invertible. That is, the result of an output function may lose accuracy when compared to the original input.

|Name|Alias|Size|Range|Description|
|----|-----|----|-----|-----------|
|bigint|int8|8 bytes|-922337203​6854775808 to 922337203​6854775807|large range integer|
|bigserial|serial8|8 bytes|1 to 922337203​6854775807|large autoincrementing integer|
|bit \[ \(n\) \]| |*n* bits|[bit string constant](https://www.postgresql.org/docs/12/sql-syntax.html#SQL-SYNTAX-BIT-STRINGS)|fixed-length bit string|
|bit varying \[ \(n\) \]<sup>1</sup>|varbit|actual number of bits|[bit string constant](https://www.postgresql.org/docs/12/sql-syntax.html#SQL-SYNTAX-BIT-STRINGS)|variable-length bit string|
|boolean|bool|1 byte|true/false, t/f, yes/no, y/n, 1/0|logical boolean \(true/false\)|
|box| |32 bytes|\(\(x1,y1\),\(x2,y2\)\)|rectangular box in the plane - not allowed in distribution key columns.|
|bytea<sup>1</sup>| |1 byte + *binary string*|sequence of [octets](https://www.postgresql.org/docs/12/datatype-binary.html#DATATYPE-BINARY-SQLESC)|variable-length binary string|
|character \[ \(n\) \]<sup>1</sup>|char \[ \(n\) \]|1 byte + *n*|strings up to *n* characters in length|fixed-length, blank padded|
|character varying \[ \(n\) \]<sup>1</sup>|varchar \[ \(n\) \]|1 byte + *string size*|strings up to *n* characters in length|variable-length with limit|
|cidr| |12 or 24 bytes| |IPv4 and IPv6 networks|
|circle| |24 bytes|<\(x,y\),r\> \(center and radius\)|circle in the plane - not allowed in distribution key columns.|
|date| |4 bytes|4713 BC - 294,277 AD|calendar date \(year, month, day\)|
|decimal \[ \(p, s\) \]<sup>1</sup>|numeric \[ \(p, s\) \]|variable|no limit|user-specified precision, exact|
|double precision|float8<br/><br/>float|8 bytes|15 decimal digits precision|variable-precision, inexact|
|inet| |12 or 24 bytes| |IPv4 and IPv6 hosts and networks|
|integer|int, int4|4 bytes|-2147483648 to +2147483647|usual choice for integer|
|interval \[ fields \] \[ \(p\) \]| |16 bytes|-178000000 years to 178000000 years|time span|
|json| |1 byte + json size|json of any length|variable unlimited length|
|jsonb| |1 byte + binary string|json of any length in a decomposed binary format|variable unlimited length|
|lseg| |32 bytes|\(\(x1,y1\),\(x2,y2\)\)|line segment in the plane - not allowed in distribution key columns.|
|macaddr| |6 bytes| |MAC addresses|
|money| |8 bytes|-92233720368547758.08 to +92233720368547758.07|currency amount|
|path<sup>1</sup>| |16+16n bytes|\[\(x1,y1\),...\]|geometric path in the plane - not allowed in distribution key columns.|
|point| |16 bytes|\(x,y\)|geometric point in the plane - not allowed in distribution key columns.|
|polygon| |40+16n bytes|\(\(x1,y1\),...\)|closed geometric path in the plane - not allowed in distribution key columns.|
|real|float4|4 bytes|6 decimal digits precision|variable-precision, inexact|
|serial|serial4|4 bytes|1 to 2147483647|autoincrementing integer|
|smallint|int2|2 bytes|-32768 to +32767|small range integer|
|text<sup>1</sup>| |1 byte + *string size*|strings of any length|variable unlimited length|
|time \[ \(p\) \] \[ without time zone \]| |8 bytes|00:00:00\[.000000\] - 24:00:00\[.000000\]|time of day only|
|time \[ \(p\) \] with time zone|timetz|12 bytes|00:00:00+1359 - 24:00:00-1359|time of day only, with time zone|
|timestamp \[ \(p\) \] \[ without time zone \]| |8 bytes|4713 BC - 294,277 AD|both date and time|
|timestamp \[ \(p\) \] with time zone|timestamptz|8 bytes|4713 BC - 294,277 AD|both date and time, with time zone|
|uuid| |16 bytes| |Universally Unique Identifiers according to RFC 4122, ISO/IEC 9834-8:2005|
|xml<sup>1</sup>| |1 byte + *xml size*|xml of any length|variable unlimited length|
|txid\_snapshot| | | |user-level transaction ID snapshot|

-   **[Date/Time Types](datatype-datetime.html)**  

-   **[Pseudo-Types](datatype-pseudo.html)**  

-   **[Text Search Data Types](datatype-textsearch.html)**  

-   **[Range Types](datatype-range.html)**  


**Parent topic:** [Greenplum Database Reference Guide](ref_guide.html)

<a id="if139219"></a><sup>1</sup> For variable length data types, if the data is greater than or equal to 127 bytes, the storage overhead is 4 bytes instead of 1.

