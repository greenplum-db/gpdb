---
title: CREATE AGGREGATE 
---

Defines a new aggregate function.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
CREATE AGGREGATE <name> ( [ <argmode> ] [ <argname> ] <arg_data_type> [ , ... ] ) (
    SFUNC = <statefunc>,
    STYPE = <state_data_type>
    [ , SSPACE = <state_data_size> ]
    [ , FINALFUNC = <ffunc> ]
    [ , FINALFUNC_EXTRA ]
    [ , COMBINEFUNC = <combinefunc> ]
    [ , SERIALFUNC = <serialfunc> ]
    [ , DESERIALFUNC = <deserialfunc> ]
    [ , INITCOND = <initial_condition> ]
    [ , MSFUNC = <msfunc> ]
    [ , MINVFUNC = <minvfunc> ]
    [ , MSTYPE = <mstate_data_type> ]
    [ , MSSPACE = <mstate_data_size> ]
    [ , MFINALFUNC = <mffunc> ]
    [ , MFINALFUNC_EXTRA ]
    [ , MINITCOND = <minitial_condition> ]
    [ , SORTOP = <sort_operator> ]
  )
  
  CREATE AGGREGATE <name> ( [ [ <argmode> ] [ <argname> ] <arg_data_type> [ , ... ] ]
      ORDER BY [ <argmode> ] [ <argname> ] <arg_data_type> [ , ... ] ) (
    SFUNC = <statefunc>,
    STYPE = <state_data_type>
    [ , SSPACE = <state_data_size> ]
    [ , FINALFUNC = <ffunc> ]
    [ , FINALFUNC_EXTRA ]
    [ , COMBINEFUNC = <combinefunc> ]
    [ , SERIALFUNC = <serialfunc> ]
    [ , DESERIALFUNC = <deserialfunc> ]
    [ , INITCOND = <initial_condition> ]
    [ , HYPOTHETICAL ]
  )
  
  or the old syntax
  
  CREATE AGGREGATE <name> (
    BASETYPE = <base_type>,
    SFUNC = <statefunc>,
    STYPE = <state_data_type>
    [ , SSPACE = <state_data_size> ]
    [ , FINALFUNC = <ffunc> ]
    [ , FINALFUNC_EXTRA ]
    [ , COMBINEFUNC = <combinefunc> ]
    [ , SERIALFUNC = <serialfunc> ]
    [ , DESERIALFUNC = <deserialfunc> ]
    [ , INITCOND = <initial_condition> ]
    [ , MSFUNC = <msfunc> ]
    [ , MINVFUNC = <minvfunc> ]
    [ , MSTYPE = <mstate_data_type> ]
    [ , MSSPACE = <mstate_data_size> ]
    [ , MFINALFUNC = <mffunc> ]
    [ , MFINALFUNC_EXTRA ]
    [ , MINITCOND = <minitial_condition> ]
    [ , SORTOP = <sort_operator> ]
  )
```

## <a id="section3"></a>Description 

`CREATE AGGREGATE` defines a new aggregate function. Some basic and commonly-used aggregate functions such as `count`, `min`, `max`, `sum`, `avg` and so on are already provided in Greenplum Database. If you define new types or need an aggregate function not already provided, you can use `CREATE AGGREGATE` to provide the desired features.

If a schema name is given \(for example, `CREATE AGGREGATE myschema.myagg ...`\) then the aggregate function is created in the specified schema. Otherwise it is created in the current schema.

An aggregate function is identified by its name and input data types. Two aggregate functions in the same schema can have the same name if they operate on different input types. The name and input data types of an aggregate function must also be distinct from the name and input data types of every ordinary function in the same schema. This behavior is identical to overloading of ordinary function names. See [CREATE FUNCTION](CREATE_FUNCTION.html).

A simple aggregate function is made from one, two, or three ordinary functions \(which must be `IMMUTABLE` functions\):

-   a state transition function statefunc
-   an optional final calculation function ffunc
-   an optional combine function combinefunc

These functions are used as follows:

```
<statefunc>( internal-state, next-data-values ) ---> next-internal-state
<ffunc>( internal-state ) ---> aggregate-value
<combinefunc>( internal-state, internal-state ) ---> next-internal-state
```

Greenplum Database creates a temporary variable of data type state\_data\_type to hold the current internal state of the aggregate function. At each input row, the aggregate argument values are calculated and the state transition function is invoked with the current state value and the new argument values to calculate a new internal state value. After all the rows have been processed, the final function is invoked once to calculate the aggregate return value. If there is no final function then the ending state value is returned as-is.

**Note:** If you write a user-defined aggregate in C, and you declare the state value \(state\_data\_type\) as type `internal`, there is a risk of an out-of-memory error occurring. If `internal` state values are not properly managed and a query acquires too much memory for state values, an out-of-memory error could occur. To prevent this, use `mpool_alloc(mpool, size)` to have Greenplum manage and allocate memory for non-temporary state values, that is, state values that have a lifespan for the entire aggregation. The argument `mpool` of the `mpool_alloc()` function is `aggstate->hhashtable->group_buf`. For an example, see the implementation of the numeric data type aggregates in `src/backend/utils/adt/numeric.c` in the Greenplum Database open source code.

You can specify `combinefunc` as a method for optimizing aggregate execution. By specifying `combinefunc`, the aggregate can be run in parallel on segments first and then on the master. When a two-level execution is performed, the `statefunc` is run on the segments to generate partial aggregate results, and `combinefunc` is run on the master to aggregate the partial results from segments. If single-level aggregation is performed, all the rows are sent to the master and the `statefunc` is applied to the rows.

Single-level aggregation and two-level aggregation are equivalent execution strategies. Either type of aggregation can be implemented in a query plan. When you implement the functions `combinefunc` and `statefunc`, you must ensure that the invocation of the `statefunc` on the segment instances followed by `combinefunc` on the master produce the same result as single-level aggregation that sends all the rows to the master and then applies only the `statefunc` to the rows.

An aggregate function can provide an optional initial condition, an initial value for the internal state value. This is specified and stored in the database as a value of type `text`, but it must be a valid external representation of a constant of the state value data type. If it is not supplied then the state value starts out `NULL`.

If `statefunc` is declared `STRICT`, then it cannot be called with `NULL` inputs. With such a transition function, aggregate execution behaves as follows. Rows with any null input values are ignored \(the function is not called and the previous state value is retained\). If the initial state value is `NULL`, then at the first row with all non-null input values, the first argument value replaces the state value, and the transition function is invoked at subsequent rows with all non-null input values. This is useful for implementing aggregates like `max`. Note that this behavior is only available when state\_data\_type is the same as the first arg\_data\_type. When these types are different, you must supply a non-null initial condition or use a nonstrict transition function.

If statefunc is not declared `STRICT`, then it will be called unconditionally at each input row, and must deal with `NULL` inputs and `NULL` state values for itself. This allows the aggregate author to have full control over the aggregate's handling of `NULL` values.

If the final function \(`ffunc`\) is declared `STRICT`, then it will not be called when the ending state value is `NULL`; instead a `NULL` result will be returned automatically. \(This is the normal behavior of `STRICT` functions.\) In any case the final function has the option of returning a `NULL` value. For example, the final function for `avg` returns `NULL` when it sees there were zero input rows.

Sometimes it is useful to declare the final function as taking not just the state value, but extra parameters corresponding to the aggregate's input values. The main reason for doing this is if the final function is polymorphic and the state value's data type would be inadequate to pin down the result type. These extra parameters are always passed as `NULL` \(and so the final function must not be strict when the `FINALFUNC_EXTRA` option is used\), but nonetheless they are valid parameters. The final function could for example make use of `get_fn_expr_argtype` to identify the actual argument type in the current call.

An aggregate can optionally support *moving-aggregate mode*, as described in [Moving-Aggregate Mode](https://www.postgresql.org/docs/9.4/xaggr.html#XAGGR-MOVING-AGGREGATES) in the PostgreSQL documentation. This requires specifying the `msfunc`, `minvfunc`, and `mstype` functions, and optionally the `mspace`, `mfinalfunc`, `mfinalfunc\_extra`, and `minitcond` functions. Except for `minvfunc`, these functions work like the corresponding simple-aggregate functions without `m`; they define a separate implementation of the aggregate that includes an inverse transition function.

The syntax with `ORDER BY` in the parameter list creates a special type of aggregate called an *ordered-set aggregate*; or if `HYPOTHETICAL` is specified, then a *hypothetical-set aggregate* is created. These aggregates operate over groups of sorted values in order-dependent ways, so that specification of an input sort order is an essential part of a call. Also, they can have *direct* arguments, which are arguments that are evaluated only once per aggregation rather than once per input row. Hypothetical-set aggregates are a subclass of ordered-set aggregates in which some of the direct arguments are required to match, in number and data types, the aggregated argument columns. This allows the values of those direct arguments to be added to the collection of aggregate-input rows as an additional "hypothetical" row.

Single argument aggregate functions, such as `min` or `max`, can sometimes be optimized by looking into an index instead of scanning every input row. If this aggregate can be so optimized, indicate it by specifying a *sort operator*. The basic requirement is that the aggregate must yield the first element in the sort ordering induced by the operator; in other words:

```
SELECT <agg>(<col>) FROM <tab>; 
```

must be equivalent to:

```
SELECT <col> FROM <tab> ORDER BY <col> USING <sortop> LIMIT 1;
```

Further assumptions are that the aggregate function ignores `NULL` inputs, and that it delivers a `NULL` result if and only if there were no non-null inputs. Ordinarily, a data type's `<` operator is the proper sort operator for `MIN`, and `>` is the proper sort operator for `MAX`. Note that the optimization will never actually take effect unless the specified operator is the "less than" or "greater than" strategy member of a B-tree index operator class.

To be able to create an aggregate function, you must have `USAGE` privilege on the argument types, the state type\(s\), and the return type, as well as `EXECUTE` privilege on the transition and final functions.

## <a id="section5"></a>Parameters 

name
:   The name \(optionally schema-qualified\) of the aggregate function to create.

argmode
:   The mode of an argument: `IN` or `VARIADIC`. \(Aggregate functions do not support `OUT` arguments.\) If omitted, the default is `IN`. Only the last argument can be marked `VARIADIC`.

argname
:   The name of an argument. This is currently only useful for documentation purposes. If omitted, the argument has no name.

arg\_data\_type
:   An input data type on which this aggregate function operates. To create a zero-argument aggregate function, write `*` in place of the list of argument specifications. \(An example of such an aggregate is `count(*)`.\)

base\_type
:   In the old syntax for `CREATE AGGREGATE`, the input data type is specified by a `basetype` parameter rather than being written next to the aggregate name. Note that this syntax allows only one input parameter. To define a zero-argument aggregate function with this syntax, specify the `basetype` as `"ANY"` \(not `*`\). Ordered-set aggregates cannot be defined with the old syntax.

statefunc
:   The name of the state transition function to be called for each input row. For a normal N-argument aggregate function, the state transition function `statefunc` must take N+1 arguments, the first being of type state\_data\_type and the rest matching the declared input data types of the aggregate. The function must return a value of type state\_data\_type. This function takes the current state value and the current input data values, and returns the next state value.

:   For ordered-set \(including hypothetical-set\) aggregates, the state transition function statefunc receives only the current state value and the aggregated arguments, not the direct arguments. Otherwise it is the same.

state\_data\_type
:   The data type for the aggregate's state value.

state\_data\_size
:   The approximate average size \(in bytes\) of the aggregate's state value. If this parameter is omitted or is zero, a default estimate is used based on the state\_data\_type. The planner uses this value to estimate the memory required for a grouped aggregate query. Large values of this parameter discourage use of hash aggregation.

ffunc
:   The name of the final function called to compute the aggregate result after all input rows have been traversed. The function must take a single argument of type state\_data\_type. The return data type of the aggregate is defined as the return type of this function. If `ffunc` is not specified, then the ending state value is used as the aggregate result, and the return type is state\_data\_type.

:   For ordered-set \(including hypothetical-set\) aggregates, the final function receives not only the final state value, but also the values of all the direct arguments.

:   If `FINALFUNC_EXTRA` is specified, then in addition to the final state value and any direct arguments, the final function receives extra NULL values corresponding to the aggregate's regular \(aggregated\) arguments. This is mainly useful to allow correct resolution of the aggregate result type when a polymorphic aggregate is being defined.

combinefunc
:   The name of a combine function. This is a function of two arguments, both of type state\_data\_type. It must return a value of state\_data\_type. A combine function takes two transition state values and returns a new transition state value representing the combined aggregation. In Greenplum Database, if the result of the aggregate function is computed in a segmented fashion, the combine function is invoked on the individual internal states in order to combine them into an ending internal state.

:   Note that this function is also called in hash aggregate mode within a segment. Therefore, if you call this aggregate function without a combine function, hash aggregate is never chosen. Since hash aggregate is efficient, consider defining a combine function whenever possible.

serialfunc
:   An aggregate function whose state\_data\_type is `internal` can participate in parallel aggregation only if it has a serialfunc function, which must serialize the aggregate state into a `bytea` value for transmission to another process. This function must take a single argument of type `internal` and return type `bytea`. A corresponding deserialfunc is also required.

deserialfunc
:   Deserialize a previously serialized aggregate state back into state\_data\_type. This function must take two arguments of types `bytea` and `internal`, and produce a result of type `internal`. \(Note: the second, `internal` argument is unused, but is required for type safety reasons.\)

initial\_condition
:   The initial setting for the state value. This must be a string constant in the form accepted for the data type state\_data\_type. If not specified, the state value starts out null.

msfunc
:   The name of the forward state transition function to be called for each input row in moving-aggregate mode. This is exactly like the regular transition function, except that its first argument and result are of type mstate\_data\_type, which might be different from state\_data\_type.

minvfunc
:   The name of the inverse state transition function to be used in moving-aggregate mode. This function has the same argument and result types as msfunc, but it is used to remove a value from the current aggregate state, rather than add a value to it. The inverse transition function must have the same strictness attribute as the forward state transition function.

mstate\_data\_type
:   The data type for the aggregate's state value, when using moving-aggregate mode.

mstate\_data\_size
:   The approximate average size \(in bytes\) of the aggregate's state value, when using moving-aggregate mode. This works the same as state\_data\_size.

mffunc
:   The name of the final function called to compute the aggregate's result after all input rows have been traversed, when using moving-aggregate mode. This works the same as ffunc, except that its first argument's type is mstate\_data\_type and extra dummy arguments are specified by writing `MFINALFUNC_EXTRA`. The aggregate result type determined by mffunc or mstate\_data\_type must match that determined by the aggregate's regular implementation.

minitial\_condition
:   The initial setting for the state value, when using moving-aggregate mode. This works the same as initial\_condition.

sort\_operator
:   The associated sort operator for a `MIN`- or `MAX`-like aggregate. This is just an operator name \(possibly schema-qualified\). The operator is assumed to have the same input data types as the aggregate \(which must be a single-argument normal aggregate\).

HYPOTHETICAL
:   For ordered-set aggregates only, this flag specifies that the aggregate arguments are to be processed according to the requirements for hypothetical-set aggregates: that is, the last few direct arguments must match the data types of the aggregated \(`WITHIN GROUP`\) arguments. The `HYPOTHETICAL` flag has no effect on run-time behavior, only on parse-time resolution of the data types and collations of the aggregate's arguments.

## <a id="section6"></a>Notes 

The ordinary functions used to define a new aggregate function must be defined first. Note that in this release of Greenplum Database, it is required that the statefunc, ffunc, and combinefunc functions used to create the aggregate are defined as `IMMUTABLE`.

If the value of the Greenplum Database server configuration parameter `gp_enable_multiphase_agg` is `off`, only single-level aggregation is performed.

Any compiled code \(shared library files\) for custom functions must be placed in the same location on every host in your Greenplum Database array \(master and all segments\). This location must also be in the `LD_LIBRARY_PATH` so that the server can locate the files.

In previous versions of Greenplum Database, there was a concept of ordered aggregates. Since version 6, any aggregate can be called as an ordered aggregate, using the syntax:

```
name ( arg [ , ... ] [ORDER BY sortspec [ , ...]] )
```

The `ORDERED` keyword is accepted for backwards compatibility, but is ignored.

In previous versions of Greenplum Database, the `COMBINEFUNC` option was called `PREFUNC`. It is still accepted for backwards compatibility, as a synonym for `COMBINEFUNC`.

## <a id="section7"></a>Example 

The following simple example creates an aggregate function that computes the sum of two columns.

Before creating the aggregate function, create two functions that are used as the `statefunc` and `combinefunc` functions of the aggregate function.

This function is specified as the `statefunc` function in the aggregate function.

```
CREATE FUNCTION mysfunc_accum(numeric, numeric, numeric) 
  RETURNS numeric
   AS 'select $1 + $2 + $3'
   LANGUAGE SQL
   IMMUTABLE
   RETURNS NULL ON NULL INPUT;
```

This function is specified as the ``combinefunc`` function in the aggregate function.

```
CREATE FUNCTION mycombine_accum(numeric, numeric )
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
   COMBINEFUNC = mycombine_accum,
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
 Optimizer: Pivotal Optimizer (GPORCA)
 (5 rows)
```

## <a id="section8"></a>Compatibility 

`CREATE AGGREGATE` is a Greenplum Database language extension. The SQL standard does not provide for user-defined aggregate functions.

## <a id="section9"></a>See Also 

[ALTER AGGREGATE](ALTER_AGGREGATE.html), [DROP AGGREGATE](DROP_AGGREGATE.html), [CREATE FUNCTION](CREATE_FUNCTION.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

