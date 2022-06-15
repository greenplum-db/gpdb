# CREATE AGGREGATE 

Defines a new aggregate function.

## Synopsis 

``` {#sql_command_synopsis}
CREATE [ORDERED] AGGREGATE <name> (<input_data_type> [ , ... ]) 
      ( SFUNC = <sfunc>,
        STYPE = <state_data_type>
        [, PREFUNC = <prefunc>]
        [, FINALFUNC = <ffunc>]
        [, INITCOND = <initial_condition>]
        [, SORTOP = <sort_operator>] )
```

## Description 

`CREATE AGGREGATE` defines a new aggregate function. Some basic and commonly-used aggregate functions such as `count`, `min`, `max`, `sum`, `avg` and so on are already provided in Greenplum Database. If one defines new types or needs an aggregate function not already provided, then `CREATE AGGREGATE` can be used to provide the desired features.

An aggregate function is identified by its name and input data types. Two aggregate functions in the same schema can have the same name if they operate on different input types. The name and input data types of an aggregate function must also be distinct from the name and input data types of every ordinary function in the same schema.

An aggregate function is made from one, two or three ordinary functions \(all of which must be `IMMUTABLE` functions\):

-   A state transition function sfunc
-   An optional preliminary segment-level calculation function prefunc
-   An optional final calculation function ffunc

These functions are used as follows:

```
sfunc( internal-state, next-data-values ) ---> next-internal-state
prefunc( internal-state, internal-state ) ---> next-internal-state
ffunc( internal-state ) ---> aggregate-value
```

You can specify `PREFUNC` as method for optimizing aggregate execution. By specifying `PREFUNC`, the aggregate can be executed in parallel on segments first and then on the master. When a two-level execution is performed, `SFUNC` is executed on the segments to generate partial aggregate results, and `PREFUNC` is executed on the master to aggregate the partial results from segments. If single-level aggregation is performed, all the rows are sent to the master and `sfunc` is applied to the rows.

Single-level aggregation and two-level aggregation are equivalent execution strategies. Either type of aggregation can be implemented in a query plan. When you implement the functions `prefunc` and `sfunc`, you must ensure that the invocation of `sfunc` on the segment instances followed by `prefunc` on the master produce the same result as single-level aggregation that sends all the rows to the master and then applies only the `sfunc` to the rows.

Greenplum Database creates a temporary variable of data type stype to hold the current internal state of the aggregate function. At each input row, the aggregate argument values are calculated and the state transition function is invoked with the current state value and the new argument values to calculate a new internal state value. After all the rows have been processed, the final function is invoked once to calculate the aggregate return value. If there is no final function then the ending state value is returned as-is.

An aggregate function can provide an optional initial condition, an initial value for the internal state value. This is specified and stored in the database as a value of type text, but it must be a valid external representation of a constant of the state value data type. If it is not supplied then the state value starts out `NULL`.

If the state transition function is declared `STRICT`, then it cannot be called with `NULL` inputs. With such a transition function, aggregate execution behaves as follows. Rows with any null input values are ignored \(the function is not called and the previous state value is retained\). If the initial state value is `NULL`, then at the first row with all non-null input values, the first argument value replaces the state value, and the transition function is invoked at subsequent rows with all non-null input values. This is useful for implementing aggregates like `max`. Note that this behavior is only available when state\_data\_type is the same as the first input\_data\_type. When these types are different, you must supply a non-null initial condition or use a nonstrict transition function.

If the state transition function is not declared `STRICT`, then it will be called unconditionally at each input row, and must deal with `NULL` inputs and `NULL` transition values for itself. This allows the aggregate author to have full control over the aggregate handling of `NULL` values.

If the final function is declared `STRICT`, then it will not be called when the ending state value is `NULL`; instead a `NULL` result will be returned automatically. \(This is the normal behavior of `STRICT` functions.\) In any case the final function has the option of returning a `NULL` value. For example, the final function for `avg` returns `NULL` when it sees there were zero input rows.

Single argument aggregate functions, such as min or max, can sometimes be optimized by looking into an index instead of scanning every input row. If this aggregate can be so optimized, indicate it by specifying a sort operator. The basic requirement is that the aggregate must yield the first element in the sort ordering induced by the operator; in other words:

```
SELECT <agg>(<col>) FROM <tab>; 
```

must be equivalent to:

```
SELECT <col> FROM <tab> ORDER BY <col> USING <sortop> LIMIT 1;
```

Further assumptions are that the aggregate function ignores `NULL` inputs, and that it delivers a `NULL` result if and only if there were no non-null inputs. Ordinarily, a data type's `<` operator is the proper sort operator for `MIN`, and `>` is the proper sort operator for `MAX`. Note that the optimization will never actually take effect unless the specified operator is the "less than" or "greater than" strategy member of a B-tree index operator class.

**Ordered Aggregates**

If the optional qualification `ORDERED` appears, the created aggregate function is an *ordered aggregate*. In this case, the preliminary aggregation function, `prefunc` cannot be specified.

An ordered aggregate is called with the following syntax.

```
name ( arg [ , ... ] [ORDER BY sortspec [ , ...]] )
```

If the optional `ORDER BY` is omitted, a system-defined ordering is used. The transition function `sfunc` of an ordered aggregate function is called on its input arguments in the specified order and on a single segment. There is a new column `aggordered` in the [pg\_aggregate](../system_catalogs/pg_aggregate.html) table to indicate the aggregate function is defined as an ordered aggregate.

## Parameters 

name
:   The name \(optionally schema-qualified\) of the aggregate function to create.

input\_data\_type
:   An input data type on which this aggregate function operates. To create a zero-argument aggregate function, write \* in place of the list of input data types. An example of such an aggregate is count\(\*\).

sfunc
:   The name of the state transition function to be called for each input row. For an N-argument aggregate function, the sfunc must take N+1 arguments, the first being of type state\_data\_type and the rest matching the declared input data types of the aggregate. The function must return a value of type state\_data\_type. This function takes the current state value and the current input data values, and returns the next state value.

state\_data\_type
:   The data type for the aggregate state value.

prefunc
:   The name of a preliminary aggregation function. This is a function of two arguments, both of type state\_data\_type. It must return a value of state\_data\_type. A preliminary function takes two transition state values and returns a new transition state value representing the combined aggregation. In Greenplum Database, if the result of the aggregate function is computed in a segmented fashion, the preliminary aggregation function is invoked on the individual internal states in order to combine them into an ending internal state.

:   Note that this function is also called in hash aggregate mode within a segment. Therefore, if you call this aggregate function without a preliminary function, hash aggregate is never chosen. Since hash aggregate is efficient, consider defining preliminary function whenever possible.

ffunc
:   The name of the final function called to compute the aggregate result after all input rows have been traversed. The function must take a single argument of type `state_data_type`. The return data type of the aggregate is defined as the return type of this function. If `ffunc` is not specified, then the ending state value is used as the aggregate result, and the return type is state\_data\_type.

initial\_condition
:   The initial setting for the state value. This must be a string constant in the form accepted for the data type state\_data\_type. If not specified, the state value starts out `NULL`.

sort\_operator
:   The associated sort operator for a MIN- or MAX-like aggregate function. This is just an operator name \(possibly schema-qualified\). The operator is assumed to have the same input data types as the aggregate function \(which must be a single-argument aggregate function\).

## Notes 

The ordinary functions used to define a new aggregate function must be defined first. Note that in this release of Greenplum Database, it is required that the sfunc, ffunc, and prefunc functions used to create the aggregate are defined as `IMMUTABLE`.

If a user-defined aggregate is used in a window expression, a `prefunc` function must be defined for the aggregate.

If the value of the Greenplum Database server configuration parameter `gp_enable_multiphase_agg` is `off`, only single-level aggregation is performed.

Any compiled code \(shared library files\) for custom functions must be placed in the same location on every host in your Greenplum Database array \(master and all segments\). This location must also be in the `LD_LIBRARY_PATH` so that the server can locate the files.

## Example 

The following simple example creates an aggregate function that computes the sum of two columns.

Before creating the aggregate function, create two functions that are used as the `SFUNC` and `PREFUNC` functions of the aggregate function.

This function is specified as the `SFUNC` function in the aggregate function.

```
CREATE FUNCTION mysfunc_accum(numeric, numeric, numeric) 
  RETURNS numeric
   AS 'select $1 + $2 + $3'
   LANGUAGE SQL
   IMMUTABLE
   RETURNS NULL ON NULL INPUT;
```

This function is specified as the `PREFUNC` function in the aggregate function.

```
CREATE FUNCTION mypre_accum(numeric, numeric )
  RETURNS numeric
   AS 'select $1 + $2'
   LANGUAGE SQL
   IMMUTABLE
   RETURNS NULL ON NULL INPUT;
```

This `CREATE AGGREGATE` command creates the aggregate function that adds two columns.

```
CREATE AGGREGATE agg_prefunc(numeric, numeric) (
   SFUNC = mysfunc_accum,
   STYPE = numeric,
   PREFUNC = mypre_accum,
   INITCOND = 0 );
```

The following commands create a table, adds some rows, and runs the aggregate function.

```
create table t1 (a int, b int) DISTRIBUTED BY (a);
insert into t1 values
   (10, 1),
   (20, 2),
   (30, 3);
select agg_prefunc(a, b) from t1;
```

This `EXPLAIN` command shows two phase aggregation.

```
explain select agg_prefunc(a, b) from t1;

QUERY PLAN
-------------------------------------------------------------------------- 
Aggregate (cost=1.10..1.11 rows=1 width=32)  
 -> Gather Motion 2:1 (slice1; segments: 2) (cost=1.04..1.08 rows=1
      width=32)
     -> Aggregate (cost=1.04..1.05 rows=1 width=32)
       -> Seq Scan on t1 (cost=0.00..1.03 rows=2 width=8)
 (4 rows)
```

## Compatibility 

`CREATE AGGREGATE` is a Greenplum Database language extension. The SQL standard does not provide for user-defined aggregate functions.

## See Also 

[ALTER AGGREGATE](ALTER_AGGREGATE.html), [DROP AGGREGATE](DROP_AGGREGATE.html), [CREATE FUNCTION](CREATE_FUNCTION.html)

**Parent topic:** [SQL Command Reference](../sql_commands/sql_ref.html)

