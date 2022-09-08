---
title: gp_percentile_agg 
---

The `gp_percentile_agg` module introduces improved Tanzu Greenplum Query Optimizer \(GPORCA\) performance for ordered-set aggregate functions including `percentile_cont()`, `percentile_disc()`, and `median()`. These improvements particularly benefit MADlib, which internally invokes these functions.

GPORCA generates a more performant query plan when:

- The sort expression does not include any computed columns.
- The `<fraction>` provided to the function is a `const` and not an `ARRAY`.
- The query does not contain a `GROUP BY` clause.

The `gp_percentile_agg` module is a Greenplum Database extension.

## <a id="topic_reg"></a>Installing and Registering the Module 

The `gp_percentile_agg` module is installed when you install Greenplum Database. You must register the `gp_percentile_agg` extension in each database where you want to use the module:

```
CREATE EXTENSION gp_percentile_agg;
```

Refer to [Installing Additional Supplied Modules](../../install_guide/install_modules.html) for more information.


## <a id="topic_upgrade"></a>Upgrading the Module

If you upgraded from and used the `gp_percentile_agg` module in Greenplum Database version 6.21.x, you must upgrade the module to obtain the bug fix and improvements introduced in Greenplum Database version 6.22.0.

To upgrade, drop and recreate the `gp_percentile_agg` extension in each database in which you are using the module:

```
DROP EXTENSION gp_percentile_agg;
CREATE EXTENSION gp_percentile_agg;
```

## <a id="topic_use"></a>About Using the Module 

To realize the GPORCA performance benefits when using ordered-set aggregate functions, in addition to registering the extension you must also enable the [optimizer_enable_orderedagg](../config_params/guc-list.html#optimizer_enable_orderedagg) server configuration parameter before you run the query. For example, to enable this parameter in a `psql` session:

``` sql
SET optimizer_enable_orderedagg = on;
```

When the extension is registered, `optimizer_enable_orderedagg` is enabled, and you invoke the `percentile_cont()`, `percentile_disc()`, or `median()` functions, GPORCA generates the more performant query plan.

## <a id="topic_info"></a>Additional Module Documentation 

Refer to [Ordered-Set Aggregate Functions](https://www.postgresql.org/docs/9.4/functions-aggregate.html#FUNCTIONS-ORDEREDSET-TABLE) in the PostgreSQL documentation for more information about using ordered-set aggregates.

