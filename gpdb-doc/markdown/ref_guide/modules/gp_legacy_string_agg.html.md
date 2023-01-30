# gp\_legacy\_string\_agg 

The `gp_legacy_string_agg` module re-introduces the single-argument `string_agg()` function that was present in Greenplum Database 5.

The `gp_legacy_string_agg` module is a Greenplum Database extension.

> **Note** Use this module to aid migration from Greenplum Database 5 to the native, two-argument `string_agg()` function included in Greenplum 6.

## <a id="topic_reg"></a>Installing and Registering the Module 

The `gp_legacy_string_agg` module is installed when you install Greenplum Database. Before you can use the function defined in the module, you must register the `gp_legacy_string_agg` extension in each database where you want to use the function. Refer to [Installing Additional Supplied Modules](../../install_guide/install_modules.html) for more information about registering the module.

## <a id="topic_use"></a>Using the Module 

The single-argument `string_agg()` function has the following signature:

```
string_agg( text )
```

You can use the function to concatenate non-null input values into a string. For example:

```
SELECT string_agg(a) FROM (VALUES('aaaa'),('bbbb'),('cccc'),(NULL)) g(a);
WARNING:  Deprecated call to string_agg(text), use string_agg(text, text) instead
  string_agg  
--------------
 aaaabbbbcccc
(1 row)
```

The function concatenates each string value until it encounters a null value, and then returns the string. The function returns a null value when no rows are selected in the query.

`string_agg()` produces results that depend on the ordering of the input rows. The ordering is unspecified by default; you can control the ordering by specifying an `ORDER BY` clause within the aggregate. For example:

```
CREATE TABLE table1(a int, b text);
INSERT INTO table1 VALUES(4, 'aaaa'),(2, 'bbbb'),(1, 'cccc'), (3, NULL);
SELECT string_agg(b ORDER BY a) FROM table1;
WARNING:  Deprecated call to string_agg(text), use string_agg(text, text) instead
  string_agg  
--------------
 ccccbbbb
(1 row)
```

## <a id="topic_migrate"></a>Migrating to the Two-Argument string\_agg\(\) Function 

Greenplum Database 6 includes a native, two-argument, text input `string_agg()` function:

```
string_agg( text, text )
```

The following function invocation is equivalent to the single-argument `string_agg()` function that is provided in this module:

```
string_agg( text, '' )
```

You can use this conversion when you are ready to migrate from this contrib module.

