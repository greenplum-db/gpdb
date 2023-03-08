# timestamp9

The `timestamp9` module provides an efficient, nanosecond-precision timestamp data type and related functions and operators.

The Greenplum Database `timestamp9` module is based on version 1.1.0 of the `timestamp9` module used with PostgreSQL.

## <a id="topic_reg"></a>Installing and Registering the Module 

The `timestamp9` module is installed when you install Greenplum Database. Before you can use the data type defined in the module, you must register the `timestamp9` extension in each database in which you want to use the type:

```
CREATE EXTENSION timestamp9;
```

Refer to [Installing Additional Supplied Modules](../../install_guide/install_modules.html) for more information.

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

