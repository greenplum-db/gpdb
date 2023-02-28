# timestamp9

The `timestamp9` module provides an efficient, nanosecond-precision timestamp data type and related functions and operators.

The Greenplum Database `timestamp9` module is based on version 1.2.0 of the `timestamp9` module used with PostgreSQL.

## <a id="topic_reg"></a>Installing and Registering the Module 

The `timestamp9` module is installed when you install Greenplum Database. Before you can use the data type defined in the module, you must register the `timestamp9` extension in each database in which you want to use the type:

```
CREATE EXTENSION timestamp9;
```

Refer to [Installing Additional Supplied Modules](../../install_guide/install_modules.html) for more information.

## Supported Datatypes

The Greenplum Database `timestamp9` extension supports three kinds of datatatypes: `TIMESTAMP9`, `TIMESTAMP9_LTZ` and `TIMESTAMP9_NTZ`. (The `TIMESTAMP9_LTZ` data type is an alias for `TIMESTAMP9` datatype.) 

The following table summarizes key information about the `TIMESTAMP9_LTZ` and `TIMESTAMP9_NTZ` datatypes:

|Datatype|Storage Size|Description|Max Value|Min Value
|-------------|-----------------|-----------|--------|
|IMMUTABLE|Yes|Relies only on information directly in its argument list. Given the same argument values, always returns the same result.| |
|STABLE|Yes, in most cases|Within a single table scan, returns the same result for same argument values, but results change across SQL statements.|Results depend on database lookups or parameter values. `current_timestamp` family of functions is `STABLE`; values do not change within an execution.|
|VOLATILE|Restricted|Function values can change within a single table scan. For example: `random()`, `timeofday()`.|Any function with side effects is volatile, even if its result is predictable. For example: `setval()`.|



|Datatype|Storage Size|Description|Max Value|Min Value|Resolution|
|--------|-------|----|-----------|---------|---------|----------|
|`TIMESTAMP9`|8 bytes|Like TIMESTAMP9_LTZ. Timestamp with local time zone. |2262-04-11 00:00:00.000000000 +0000 |1970-01-01 00:00:00.000000000 +0000 | 1 nanosecond |


## <a id="topic_info"></a>Module Documentation 

Refer to the [timestamp9 github documentation](https://github.com/fvannee/timestamp9) for detailed information about using the module.

## <a id="topic_gp"></a>Additional Documentation

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

## <a id="topic_limit"></a>Limitations

The `timestamp9` data type does not support arithmetic calculations with nanoseconds.

