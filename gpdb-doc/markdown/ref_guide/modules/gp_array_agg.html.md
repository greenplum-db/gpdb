---
title: gp\_array\_agg 
---

The `gp_array_agg` module introduces a parallel `array_agg()` aggregate function that you can use in Greenplum Database.

The `gp_array_agg` module is a Greenplum Database extension.

## <a id="topic_reg"></a>Installing and Registering the Module 

The `gp_array_agg` module is installed when you install Greenplum Database. Before you can use the aggregate function defined in the module, you must register the `gp_array_agg` extension in each database where you want to use the function:

```
CREATE EXTENSION gp_array_agg;
```

Refer to [Installing Additional Supplied Modules](../../install_guide/install_modules.html) for more information.

## <a id="topic_use"></a>Using the Module 

The `gp_array_agg()` function has the following signature:

```
gp_array_agg( anyelement )
```

You can use the function to create an array from input values, including nulls. For example:

```
SELECT gp_array_agg(a) FROM t1;
   gp_array_agg   
------------------
 {2,1,3,NULL,1,2}
(1 row)
```

`gp_array_agg()` assigns each input value to an array element, and then returns the array. The function returns null rather than an empty array when there are no input rows.

`gp_array_agg()` produces results that depend on the ordering of the input rows. The ordering is unspecified by default; you can control the ordering by specifying an `ORDER BY` clause within the aggregate. For example:

```
CREATE TABLE table1(a int4, b int4);
INSERT INTO table1 VALUES (4,5), (2,1), (1,3), (3,null), (3,7);
SELECT gp_array_agg(a ORDER BY b NULLS FIRST) FROM table1;
  gp_array_agg  
--------------
 {3,2,1,4,7}
(1 row)
```

## <a id="topic_info"></a>Additional Module Documentation 

Refer to [Aggregate Functions](https://www.postgresql.org/docs/9.4/functions-aggregate.html) in the PostgreSQL documentation for more information about aggregates.

