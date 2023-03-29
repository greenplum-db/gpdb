# timestamp9

The `timestamp9` module provides an efficient, nanosecond-precision timestamp data type and related functions and operators.

The Greenplum Database `timestamp9` module is based on version 1.2.0 of the `timestamp9` module used with PostgreSQL.

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

The `TIMESTAMP9` data type is similar to the `TIMESTAMP9_LTZ` data type. Please see the next section for details.

### More about `TIMESTAMP9_LTZ`

`LTZ` is an abbreviation for "Local Time Zone." Greenplum Database stores `TIMESTAMP9_LTZ` internally in UTC (Universal Coordinated Time, traditionally known as Greenwich Mean Time or GMT) time. An input value that has an explicit time zone specified is converted to UTC using the appropriate offset for that time zone. 

If no time zone is specified in the input string, then it is presumed to be in the time zone indicated by the system's [`TIMEZONE` server configuration parameter](https://docs.vmware.com/en/VMware-Tanzu-Greenplum/6/greenplum-database/GUID-ref_guide-config_params-guc-list.html#timezone) and is converted to UTC using the offset for the time zone.

See [TIMESTAMP9_LTZ Examples](#timestamp9_ltz-examples) for examples using this data type.

### More about `TIIMESTAMP9_NTZ`

"NTZ" is an abbreviation of ‘No Time Zone’. Greenplum Database stores UTC time internally without considering any time zone information. If a time zone is embedded in the timestamp string, Greenplum Database will simply ignore it. 

See [TIMESTAMP9_NTZ Examples](#timestamp9_ntz-examples) for examples using this data type.

## Supported Type Conversions

The following table summarizes the `timestamp9` module's supported type conversions.

|From|To|Description|
|--------|-------|----|-----------|---------|---------|----------|
|`BIGINT`|`TIMESTAMP9_LTZ`|Greenplum Database treats the `BIGINT` value as the number of nanoseconds started from ‘1970-01-01 00:00:00 +0000’.|[Example](#the-timezone-configuration-parameter-and-timestamp9-1|
|`DATE`|`TIMESTAMP9_LTZ`|Greenplum Database treats the `DATE` value as in the current session time zone. This behavior is identical to converting from from `DATE` to `TIMESTAMPTZ`.|Example|
|`TIMESTAMP9_NTZ`|8 bytes|Timestamp without time zone. |2261-12-31 23:59:59.999999999 +0000 |1700-01-01 00:00:00.000000000 +0000 | 1 nanosecond|Example|
|`TIMESTAMP WITHOUT TIME ZONE/TIMESTAMP`|`TIMESTAMP9_LTZ`|Greenplum Database treats the `TIMESTAMP` value as in the current session time zone.  This behavior is identical to converting from `TIMESTAMP` to `TIMESTAMPTZ`|Example|
|`TIMESTAMP WITH TIME ZONE/TIMESTAMPTZ` |`TIMESTAMP9_LTZ`|For this conversion, Greenplum Database only extends the fractional part to nanosecond precision.|Example|
|`TIMESTAMP9_LTZ`|`BIGINT`|The result of this conversion is the nanoseconds since ‘1970-01-01 00:00:00.000000000 +0000’ to the given TIMESTAMP9_LTZ value.  If the given TIMESTAMP9_LTZ value is before ‘1970-01-01 00:00:00.000000000 +0000’, the result is negative.|Example|
|`TIMESTAMP9_LTZ`|`DATE`|The result of this conversion depends on the date of the given TIMESTAMP9_LTZ value in the time zone of the current session. The behavior is like doing conversion from TIMESTAMPTZ to DATE.|Example|
|`TIMESTAMP9_LTZ`|`TIMESTAMP WITHOUT TIME ZONE/TIMESTAMP/TIMESTAMP`|The result of this conversion is a timestamp without time zone.  The result timestamp’s value is determined by the value of TIMESTAMP9_LTZ in the current session time zone.  Note that the fractional part of TIMESTAMP type has 6 digits, while TIMESTAMP9_LTZ has 9 digits in its fractional part. When converting TIMESTAMP9_LTZ to TIMESTAMP, the fractional part is truncated instead of being rounded off.|Example|
|`TIMESTAMP9_LTZ`|`TIMESTAMP WITHOUT TIME ZONE/TIMESTAMP/TIMESTAMP`|When performing this conversion, Greenplum Database truncates the fractional part to only 6 digits.|Example|

### Type Conversion Examples

#### <a id="1"></a>`BIGINT` => `TIMESTAMP9_LTZ`




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

## <a id="topic_info"></a>Module Documentation 

Refer to the [timestamp9 github documentation](https://github.com/fvannee/timestamp9) for detailed information about using the module.

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

#### Distribute a table by `TIMESTAMP9_LTZ`

This example distributes a table by a column of type `TIMESTAMP9_LTZ`:

``` 
The system’s TIMEZONE parameter is ‘Asia/Shanghai’ 

CREATE TABLE t_dist_by_ts9ltz(ts9ltz TIMESTAMP9_LTZ) DISTRIBUTED BY (ts9ltz); 

INSERT INTO t_dist_by_ts9ltz VALUES 

            ('2023-02-14 01:01:01.123456789'), 

            ('2023-02-14 01:01:01.123456789 +0100'), 

            ('2023-02-14 01:01:01.123456789 +0200'); 

SELECT gp_segment_id, * FROM t_dist_by_ts9ltz; 

 gp_segment_id |               ts9ltz 

---------------+------------------------------------- 

             0 | 2023-02-14 08:01:01.123456789 +0800 

             1 | 2023-02-14 07:01:01.123456789 +0800 

             2 | 2023-02-14 01:01:01.123456789 +0800 
(3 rows)
``` 

#### Partition a table by `TIMESTAMP9_LTZ`

This example partitions a table by a column of type `TIMESTAMP9_LTZ`:

``` 
The system’s TIMEZONE parameter is ‘Asia/Shanghai’ 

CREATE TABLE t_part_by_ts9ltz(ts9ltz TIMESTAMP9_LTZ) 

  PARTITION BY RANGE (ts9ltz) 

    (PARTITION p1 START ('2023-01-01') END ('2023-02-01'), 

     PARTITION p2 START ('2023-02-01') END ('2023-03-01')); 

INSERT INTO t_part_by_ts9ltz VALUES 

            ('2023-01-14 01:01:01.123456789'), 

            ('2023-02-14 01:01:01.123456789 +0100'), 

            ('2023-02-14 01:01:01.123456789 +0200'); 

SELECT * FROM t_part_by_ts9ltz 

    WHERE ts9ltz BETWEEN '2023-02-01' AND '2023-02-15'; 

               ts9ltz 
------------------------------------- 
2023-02-14 08:01:01.123456789 +0800 
2023-02-14 07:01:01.123456789 +0800 
(2 rows) 
``` 

### `TIMESTAMP9_NTZ` Examples

#### Valid input for `TIMESTAMP9_NTZ`

As with `TIMESTAMP9_LTZ`, valid input for the `TIMESTAMP9_NTZ` data type consists of the concatenation of a date and a time, followed by an optional time zone. Users can specify the fractional part of second up to 9 digits (in nanosecond precision). The difference is that, if the user specifies time zone in the input string, `TIMESTAMP9_NTZ` will ignore it and store the remaining timestamp as UTC time without applying any time zone offset. 

``` 
The current system’s TIMEZONE parameter is ‘Asia/Shanghai’ 

SELECT '2023-02-20 00:00:00.123456789 +0200'::TIMESTAMP9_NTZ; 

        timestamp9_ntz 
------------------------------- 
2023-02-20 00:00:00.123456789 
(1 row) 

SELECT '2023-02-20 00:00:00.123456789'::TIMESTAMP9_NTZ; 

        timestamp9_ntz 
------------------------------- 
2023-02-20 00:00:00.123456789 
(1 row) 
```

#### Distribute a table by `TIMESTAMP9_NTZ`

This example distributes a table by a column of type `TIMESTAMP9_NTZ`:

``` 
The system’s TIMEZONE parameter is ‘Asia/Shanghai’ 

CREATE TABLE t_dist_by_ts9ntz(ts9ntz TIMESTAMP9_NTZ) DISTRIBUTED BY (ts9ntz); 

INSERT INTO t_dist_by_ts9ntz VALUES 

            ('2023-02-14 01:01:01.123456789'), 

            ('2023-02-14 01:01:01.123456789 +0100'), 

            ('2023-02-14 01:01:01.123456789 +0200'); 

Since the inserted tuples are identical, they are located on the same segment. 

SELECT gp_segment_id, * FROM t_dist_by_ts9ntz; 

 gp_segment_id |            ts9ntz 
---------------+------------------------------- 

             1 | 2023-02-14 01:01:01.123456789 

             1 | 2023-02-14 01:01:01.123456789 

             1 | 2023-02-14 01:01:01.123456789 
(3 rows) 
``` 

#### Partition a table by `TIMESTAMP9_NTZ`

This example partitions a table by a column of type `TIMESTAMP9_NTZ`:

``` 
The system’s TIMEZONE parameter is ‘Asia/Shanghai’ 

CREATE TABLE t_part_by_ts9ntz(ts9ntz TIMESTAMP9_NTZ) 

  PARTITION BY RANGE (ts9ntz) 

    (PARTITION p1 START ('2023-01-01') END ('2023-02-01'), 

     PARTITION p2 START ('2023-02-01') END ('2023-03-01')); 

INSERT INTO t_part_by_ts9ntz VALUES 

            ('2023-01-14 01:01:01.123456789'), 

            ('2023-02-14 01:01:01.123456789 +0100'), 

            ('2023-02-14 01:01:01.123456789 +0200'); 

SELECT * FROM t_part_by_ts9ntz 

    WHERE ts9ntz BETWEEN '2023-02-01' AND '2023-02-15'; 

            ts9ntz 
-------------------------------
2023-02-14 01:01:01.123456789 

2023-02-14 01:01:01.123456789 
(2 rows) 
``` 

## <a id="topic_limit"></a>Limitations

The `timestamp9` data type does not support arithmetic calculations with nanoseconds.



