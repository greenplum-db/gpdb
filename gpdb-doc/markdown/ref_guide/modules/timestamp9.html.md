# timestamp9

The `timestamp9` module provides an efficient, nanosecond-precision timestamp data type and related functions and operators.

The Greenplum Database `timestamp9` module is based on version 1.2.0 of the [`timestamp9` module](https://github.com/fvannee/timestamp9) used with PostgreSQL.

## <a id="topic_reg"></a>Installing and Registering the Module 

The `timestamp9` module is installed when you install Greenplum Database. Before you can use the data type defined in the module, you must register the `timestamp9` extension in each database in which you want to use the type:

```
CREATE EXTENSION timestamp9;
```

Refer to [Installing Additional Supplied Modules](../../install_guide/install_modules.html) for more information.

## Supported Data Types

The Greenplum Database `timestamp9` extension supports three kinds of datatatypes: `TIMESTAMP9`, `TIMESTAMP9_LTZ` and `TIMESTAMP9_NTZ`. (The `TIMESTAMP9_LTZ` data type is an alias for `TIMESTAMP9` data type.) 

The following table summarizes key information about the `timestamp9` data types:

|Data Type|Storage Size|Description|Max Value|Min Value|Resolution|
|--------|-------|----|-----------|---------|---------|----------|
|`TIMESTAMP9`|8 bytes|Like TIMESTAMP9_LTZ. Timestamp with local time zone. |2261-12-31 23:59:59.999999999 +0000 |1700-01-01 00:00:00.000000000 +0000 | 1 nanosecond |
|`TIMESTAMP9_LTZ`|8 bytes|Timestamp with local time zone. |2261-12-31 23:59:59.999999999 +0000 |1700-01-01 00:00:00.000000000 +0000 | 1 nanosecond |
|`TIMESTAMP9_NTZ`|8 bytes|Timestamp without time zone. |2261-12-31 23:59:59.999999999 +0000 |1700-01-01 00:00:00.000000000 +0000 | 1 nanosecond|

### More about `TIMESTAMP9`

The `TIMESTAMP9` data type is identical to the `TIMESTAMP9_LTZ` data type. Please see the next section for details.

### More about `TIMESTAMP9_LTZ`

`LTZ` is an abbreviation for "Local Time Zone." Greenplum Database stores `TIMESTAMP9_LTZ` internally in UTC (Universal Coordinated Time, traditionally known as Greenwich Mean Time or GMT) time. An input value that has an explicit time zone specified is converted to UTC using the appropriate offset for that time zone. 

If no time zone is specified in the input string, then it is presumed to be in the time zone indicated by the system's [`TIMEZONE` server configuration parameter](https://docs.vmware.com/en/VMware-Greenplum/6/greenplum-database/ref_guide-config_params-guc-list.html#timezone) and is converted to UTC using the offset for the time zone.

See [TIMESTAMP9_LTZ Examples](#timestamp9_ltz-examples) for examples using this data type.

### More about `TIIMESTAMP9_NTZ`

`NTZ` is an abbreviation of ‘No Time Zone.’ Greenplum Database stores UTC time internally without considering any time zone information. If a time zone is embedded in the timestamp string, Greenplum Database will simply ignore it. 

See [TIMESTAMP9_NTZ Examples](#timestamp9_ntz-examples) for examples using this data type.

## Supported Type Conversions

The following table summarizes the `timestamp9` module's supported type conversions.

|From|To|Description|
|--------|-------|----|
|`BIGINT`|`TIMESTAMP9_LTZ`|Greenplum Database treats the `BIGINT` value as the number of nanoseconds started from ‘1970-01-01 00:00:00 +0000’.|
|`DATE`|`TIMESTAMP9_LTZ`|Greenplum Database treats the `DATE` value as in the current session time zone. This behavior is identical to converting from from `DATE` to `TIMESTAMPTZ`.|
|`TIMESTAMP WITHOUT TIME ZONE/TIMESTAMP`|`TIMESTAMP9_LTZ`|Greenplum Database treats the `TIMESTAMP` value as in the current session time zone.  This behavior is identical to converting from `TIMESTAMP` to `TIMESTAMPTZ`.|
|`TIMESTAMP WITH TIME ZONE/TIMESTAMPTZ` |`TIMESTAMP9_LTZ`|For this conversion, Greenplum Database only extends the fractional part to nanosecond precision.|
|`TIMESTAMP9_LTZ`|`BIGINT`|The result of this conversion is the nanoseconds since ‘1970-01-01 00:00:00.000000000 +0000’ to the given `TIMESTAMP9_LTZ` value.  If the given `TIMESTAMP9_LTZ` value is before ‘1970-01-01 00:00:00.000000000 +0000’, the result is negative.|
|`TIMESTAMP9_LTZ`|`DATE`|The result of this conversion depends on the date of the given `TIMESTAMP9_LTZ` value in the time zone of the current session. The behavior is like doing conversion from `TIMESTAMPTZ` to `DATE`.|
|`TIMESTAMP9_LTZ`|`TIMESTAMP WITHOUT TIME ZONE/TIMESTAMP`|The result of this conversion is a timestamp without time zone.  The resulting timestamp’s value is determined by the value of `TIMESTAMP9_LTZ` in the current session time zone.  Note that the fractional part of `TIMESTAMP` type has 6 digits, while `TIMESTAMP9_LTZ` has 9 digits in its fractional part. When converting `TIMESTAMP9_LTZ` to `TIMESTAMP`, the fractional part is truncated instead of being rounded off.|
|`TIMESTAMP9_LTZ`|`TIMESTAMP WITH TIME ZONE/TIMESTAMP`|When performing this conversion, Greenplum Database truncates the fractional part to only 6 digits.|
|`BIGINT`|`TIMESTAMP9_NTZ`|When performing this conversion, Greenplum Database treats the BIGINT value as the number of nanoseconds started from ‘1970-01-01 00:00:00’.|
|`DATE`|`TIMESTAMP9_NTZ`|When performing this conversion, the resulting timestamp is ‘00:00:00.000000000’ on the given date.|
|`TIMESTAMP WITHOUT TIME ZONE/TIMESTAMP`|`TIMESTAMP9_NTZ`|When peforming this conversion, Greenplum Database only extends the fractional part to nanosecond precision.|
|`TIMESTAMP WITH TIME ZONE/TIMESTAMP`|`TIMESTAMP9_NTZ`|The resulting timestamp’s value is determined by the value of `TIMESTAMPTZ` in the current session time zone.|
|`TIMESTAMP9_NTZ`|`BIGINT`|The result of this conversion is the nanoseconds since ‘1970-01-01 00:00:00.000000000’ to the given `TIMESTAMP9_NTZ` value.  If the given `TIMESTAMP9_NTZ` value is before ‘1970-01-01 00:00:00.000000000’, the result is negative.|
|`TIMESTAMP9_NTZ`|`DATE`|When performing this conversion, Greenplum Database truncatest the time portion and preserves the date portion.|
|`TIMESTAMP9_NTZ`|`TIMESTAMP WITHOUT TIME ZONE/TIMESTAMP`|When performing this conversion, Greenplum Database truncates only the fractional part to 6 digits.|
|`TIMESTAMP9_NTZ`|`TIMESTAMP WITH TIME ZONE/TIMESTAMP`|When performing this conversion, Greenplum Database only truncates the fractional part to 6 digits and add the time zone of the current session. 
|`TIMESTAMP9_LTZ`|`TIMESTAMP9_NTZ`|The resulting `TIMESTAMP9_NTZ` value is determined by the value of `TIMESTAMP9_LTZ` in the current session's time zone.|
|`TIMESTAMP9_NTZ`|`TIMESTAMP9_LTZ`|When performing this conversion, Greenplum Database adds the timezone of the current sesion.|

### Type Conversion Examples

#### <a id="conversion_1"></a>Convert `BIGINT` to `TIMESTAMP9_LTZ`

```
=# SHOW TIMEZONE; 

   TimeZone 
-------------- 
Asia/Shanghai 
(1 row) 
	
=# SELECT 0::BIGINT::TIMESTAMP9_LTZ; 

           timestamp9_ltz 
------------------------------------ 
1970-01-01 08:00:00.000000000 +0800 
(1 row) 
```

#### <a id="conversion_2"></a>Convert `DATE` to `TIMESTAMP9_LTZ`

```
=# SHOW TIMEZONE; 

   TimeZone 
-------------- 
Asia/Shanghai 
(1 row) 

 =# SELECT '2023-01-01'::DATE::TIMESTAMP9_LTZ; 

           timestamp9_ltz 
------------------------------------- 
2023-01-01 00:00:00.000000000 +0800 
(1 row) 

=# SELECT '2023-01-01'::DATE::TIMESTAMPTZ; 

      timestamptz 
------------------------ 
2023-01-01 00:00:00+08 
(1 row) 
```

#### <a id="conversion_3"></a>Convert `TIMESTAMP WITHOUT TIME ZONE/TIMESTAMP` to `TIMESTAMP9_LTZ`

```
=# SHOW TIMEZONE; 

   TimeZone 
--------------- 
Asia/Shanghai 
(1 row) 

Time: 0.411 ms 
=# SELECT '2023-01-01 00:00:00'::TIMESTAMP::TIMESTAMP9_LTZ; 

           timestamp9_ltz 
------------------------------------- 
2023-01-01 00:00:00.000000000 +0800 
(1 row) 

Time: 0.691 ms 
=# SELECT '2023-01-01 00:00:00'::TIMESTAMP::TIMESTAMPTZ; 

      timestamptz 
------------------------ 
2023-01-01 00:00:00+08 
(1 row) 
```

#### <a id="conversion_4"></a>Convert `TIMESTAMP WITH TIME ZONE/TIMESTAMP` to `TIMESTAMP9_LTZ`

```
=# SELECT '2023-01-01 00:00:00.123456'::TIMESTAMPTZ::TIMESTAMP9_LTZ; 

           timestamp9_ltz 
------------------------------------- 
2023-01-01 00:00:00.123456000 +0800 
(1 row) 
```

#### <a id="conversion_5"></a>Convert `TIMESTAMP9_LTZ` to `BIGINT`

```
=# SELECT '2023-01-01 00:00:00.123456 Asia/Shanghai'::TIMESTAMP9_LTZ::BIGINT; 

        int8 
--------------------- 
1672502400123456000 
(1 row) 

=# SELECT '1969-01-01 00:00:00.123456 Asia/Shanghai'::TIMESTAMP9_LTZ::BIGINT; 

        int8 
-------------------- 
-31564799876544000 
(1 row) 
```

#### <a id="conversion_6"></a>Convert `TIMESTAMP9_LTZ` to `DATE`

```
=# SET TIMEZONE TO 'Asia/Shanghai'; 
SET 
=# SELECT '2023-01-02 02:59:59 Asia/Shanghai'::TIMESTAMPTZ::DATE; 
    date 
------------ 
2023-01-02 
(1 row) 

=# SET TIMEZONE TO 'UTC+0'; 
SET 
=# SELECT '2023-01-02 02:59:59 Asia/Shanghai'::TIMESTAMPTZ::DATE; 
    date 
------------ 
2023-01-01 
(1 row) 
```

#### <a id="conversion_7"></a>Convert `TIMESTAMP9_LTZ` to `TIMESTAMP WITHOUT TIME ZONE/TIMESTAMP`

**Example 1**

```
=# SET TIMEZONE TO 'Asia/Shanghai'; 
SET 
=# SELECT '2023-01-02 02:59:59 Asia/Shanghai'::TIMESTAMP9_LTZ::TIMESTAMP; 

      timestamp 
--------------------- 
2023-01-02 02:59:59 
(1 row) 

=# SET TIMEZONE TO 'UTC+0'; 

SET 
=# SELECT '2023-01-02 02:59:59 Asia/Shanghai'::TIMESTAMP9_LTZ::TIMESTAMP; 

      timestamp 
--------------------- 
2023-01-01 18:59:59 
(1 row) 
```
 
**Example 2 -- Truncation of the fractional part** 

```
=# SET TIMEZONE TO 'Asia/Shanghai'; 
SET 
=# SELECT '2023-01-02 02:59:59.123456789 Asia/Shanghai'::TIMESTAMP9_LTZ::TIMESTAMP; 

         timestamp 
---------------------------- 
2023-01-02 02:59:59.123456 
(1 row) 
```

#### <a id="conversion_8"></a>Convert `TIMESTAMP9_LTZ` to `TIMESTAMP WITH TIME ZONE/TIMESTAMP`

```
=# SET TIMEZONE TO 'Asia/Shanghai'; 
=# SELECT '2023-01-02 02:59:59.123456789 Asia/Shanghai'::TIMESTAMP9_LTZ::TIMESTAMPTZ; 
          timestamptz 
------------------------------- 
2023-01-02 02:59:59.123456+08 
(1 row) 
```

#### <a id="conversion_9"></a>Convert `BIGINT` to `TIMESTAMP9_NTZ`

```
=# SELECT 0::BIGINT::TIMESTAMP9_NTZ; 
        timestamp9_ntz 
------------------------------- 

1970-01-01 00:00:00.000000000 
(1 row) 
```

#### <a id="conversion_10"></a>Convert `DATE` to `TIMESTAMP9_NTZ`

```
=# SELECT '2023-01-01'::DATE::TIMESTAMP9_NTZ; 
        timestamp9_ntz 
------------------------------- 
2023-01-01 00:00:00.000000000 
(1 row) 
```

#### <a id="conversion_11"></a>Convert `TIMESTAMP WITHOUT TIME ZONE/TIMESTAMP` to `TIMESTAMP9_NTZ` 

```
=# SELECT '2023-01-01 00:00:00.123456'::TIMESTAMP::TIMESTAMP9_NTZ; 
        timestamp9_ntz 

------------------------------ 
2023-01-01 00:00:00.123456000 
(1 row) 
```

#### <a id="conversion_12"></a>Convert `TIMESTAMP WITH TIME ZONE/TIMESTAMP` to `TIMESTAMP9_NTZ` 

```
=# SET TIMEZONE TO 'Asia/Shanghai'; 
SET 
=# SELECT '2023-01-01 00:00:00.123456 Asia/Shanghai'::TIMESTAMPTZ::TIMESTAMP9_NTZ; 
        timestamp9_ntz 
------------------------------- 
2023-01-01 00:00:00.123456000 
(1 row) 

=# SET TIMEZONE TO 'UTC+0'; 
SET 
=# SELECT '2023-01-01 00:00:00.123456 Asia/Shanghai'::TIMESTAMPTZ::TIMESTAMP9_NTZ; 
        timestamp9_ntz 
------------------------------- 
2022-12-31 16:00:00.123456000 
(1 row) 
```

#### <a id="conversion_13"></a>Convert `TIMESTAMP9_NTZ` to `BIGINT`

```
=# SELECT '2023-01-01 00:00:00.123456'::TIMESTAMP9_NTZ::BIGINT; 
        int8 
--------------------- 
1672531200123456000 
(1 row) 

=# SELECT '1969-01-01 00:00:00.123456'::TIMESTAMP9_NTZ::BIGINT; 
        int8 
-------------------- 
-31535999876544000 
(1 row) 
```

#### <a id="conversion_14"></a>Convert `TIMESTAMP9_NTZ` to `DATE`

```
=# SELECT '2023-01-01 00:00:00.123456'::TIMESTAMP9_NTZ::DATE; 
    date 
------------ 
2023-01-01 
(1 row) 
```

#### <a id="conversion_15"></a>Convert `TIMESTAMP9_NTZ` to `TIMESTAMP WITHOUT TIME ZONE/TIMESTAMP` 

```
=# SELECT '2023-01-01 00:00:00.123456789'::TIMESTAMP9_NTZ::TIMESTAMP; 
         timestamp 
---------------------------- 
2023-01-01 00:00:00.123456 
(1 row) 
```

#### <a id="conversion_16"></a>Convert `TIMESTAMP9_NTZ` to `TIMESTAMP WITH TIME ZONE/TIMESTAMP` 

```
=# SET TIMEZONE TO 'Asia/Shanghai'; 
SET 
=# SELECT '2023-01-01 23:00:00.123456789'::TIMESTAMP9_NTZ::TIMESTAMPTZ; 
          timestamptz 
------------------------------- 
2023-01-01 23:00:00.123456+08 
(1 row) 

Time: 0.793 ms 
=# SET TIMEZONE TO 'UTC+0'; 
SET 
=# SELECT '2023-01-01 23:00:00.123456789'::TIMESTAMP9_NTZ::TIMESTAMPTZ; 
          timestamptz 
------------------------------- 
2023-01-01 23:00:00.123456+00 
(1 row) 
```

#### <a id="conversion_17"></a>Convert `TIMESTAMP9_LTZ` to `TIMESTAMP9_NTZ`

```
=# SET TIMEZONE TO 'Asia/Shanghai'; 
SET 
=# SELECT '2023-01-01 23:00:00.123456789 Asia/Shanghai'::TIMESTAMP9_LTZ::TIMESTAMP9_NTZ; 
        timestamp9_ntz 
------------------------------- 
2023-01-01 23:00:00.123456789 
(1 row) 

=# SET TIMEZONE TO 'UTC+0'; 
SET 
=# SELECT '2023-01-01 23:00:00.123456789 Asia/Shanghai'::TIMESTAMP9_LTZ::TIMESTAMP9_NTZ; 
        timestamp9_ntz 
------------------------------- 
2023-01-01 15:00:00.123456789 
(1 row) 
```

#### <a id="conversion_18"></a>Convert `TIMESTAMP9_NTZ` to `TIMESTAMP9_LTZ`

```
=# SET TIMEZONE TO 'Asia/Shanghai'; 
SET 
=# SELECT '2023-01-01 23:00:00.123456789'::TIMESTAMP9_NTZ::TIMESTAMP9_LTZ; 
           timestamp9_ltz 
------------------------------------- 
2023-01-01 23:00:00.123456789 +0800 
(1 row) 

=# SET TIMEZONE TO 'UTC+0'; 
SET 
=# SELECT '2023-01-01 23:00:00.123456789'::TIMESTAMP9_NTZ::TIMESTAMP9_LTZ; 
           timestamp9_ltz 
------------------------------------- 
2023-01-01 23:00:00.123456789 +0000 
(1 row) 
```

## <a id="topic_gp"></a>The TimeZone Configuration Parameter and `timestamp9`

You can set the [TimeZone](../config_params/guc-list.html#TimeZone) server configuration parameter to specify the time zone that Greenplum Database uses when it prints a `timestamp9` timestamp. When you set this parameter, Greenplum Database displays the timestamp value in that time zone. For example:

```sql
testdb=# SELECT now()::timestamp9;
                 now
-------------------------------------
 2022-08-24 18:08:01.729360000 +0800
(1 row)

testdb=# SET timezone TO 'UTC+2';
SET
testdb=# SELECT now()::timestamp9;
                 now
-------------------------------------
 2022-08-24 08:08:12.995542000 -0200
(1 row)
```

## <a id="examples"></a>Examples

### `TIMESTAMP9_LTZ` Examples

#### Valid input for `TIMESTAMP9_LTZ`

Valid input for the `TIMESTAMP9_LTZ` consists of the concatenation of a date and a time, followed by an optional time zone. Users can specify the fractional part of second up to 9 digits (in nanosecond precision). 

```
The current system’s TIMEZONE parameter is ‘Asia/Shanghai’ 

SELECT '2023-02-20 00:00:00.123456789 +0200'::TIMESTAMP9_LTZ; 

           timestamp9_ltz 

------------------------------------- 
2023-02-20 06:00:00.123456789 +0800 
(1 row) 

 If the input string doesn’t have explicit time zone information, the timestamp is presumed to be in the time zone indicated by the system’s TIMEZONE parameter. 

SELECT '2023-02-20 00:00:00.123456789'::TIMESTAMP9_LTZ; 

           timestamp9_ltz 
------------------------------------- 
2023-02-20 00:00:00.123456789 +0800 
(1 row) 
```

TIMESTAMP9_LTZ also accepts numbers as valid input. It’s interpreted as the number of nanoseconds since the UTC time ‘1970-01-01 00:00:00.000000000’. 

``` 
SELECT '123456789'::TIMESTAMP9_LTZ; 

           timestamp9_ltz 
------------------------------------- 
1970-01-01 08:00:00.123456789 +0800 
(1 row) 
```

### `TIMESTAMP9_NTZ` Examples

#### Valid input for `TIMESTAMP9_NTZ`

As with `TIMESTAMP9_LTZ`, valid input for the `TIMESTAMP9_NTZ` data type consists of the concatenation of a date and a time, followed by an optional time zone. Users can specify the fractional part of second up to 9 digits (in nanosecond precision). The difference is that, if the user specifies time zone in the input string, `TIMESTAMP9_NTZ` will ignore it and store the remaining timestamp as UTC time without applying any time zone offset. 

The current system’s TIMEZONE parameter is ‘Asia/Shanghai’

```
=# SELECT '2023-02-20 00:00:00.123456789 +0200'::TIMESTAMP9_NTZ;
        timestamp9_ntz
-------------------------------
2023-02-20 00:00:00.123456789
(1 row)
=# SELECT '2023-02-20 00:00:00.123456789'::TIMESTAMP9_NTZ;
        timestamp9_ntz
-------------------------------
2023-02-20 00:00:00.123456789
(1 row)
```

## <a id="topic_limit"></a>Limitations

The `timestamp9` data type does not support arithmetic calculations with nanoseconds.



