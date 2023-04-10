---
title: Just-in-Time Compilation (JIT)
---

This topic provides an explanation of what Just-in-Time compilation is and how to configure it in Greenplum Database.

**Parent topic:** [Querying Data](../../query/topics/query.html)

## <a id="topic2"></a>What is JIT compilation?

Just-in-Time (JIT) compilation is the process of turning some form of interpreted program evaluation into a native program, and doing so at run time. For example, instead of using general-purpose code that can evaluate arbitrary SQL expressiones to evaluate a particular SQL predicate like `WHERE a.col=3`, it is possible to generate a function that is specific to that expression and can be natively executed by the CPU, resulting in faster execution.

Greenplum Database uses LLVM for JIT compilation and it is enabled with all RPM distributions of Greenplum Database. If you build Greenplum Database from source, you must include the `--with-llvm` build option to include JIT compilation support.

It is possible to use JIT with both Postgres Planner and GPORCA. Since GPORCA and Postgres Planner use different algorithms and the values for the calculated costs are different, you must tune the JIT thresholds according to your usage. See [When to JIT?](#topic3) for more information.

### <a id="topic21"></a>JIT accelerated operations

Currently Greenplum Database's JIT implementation supports for accelerating expression evaluation and tuple deforming.

Expression evaluation is used to evaluate `WHERE` clauses, target lists, aggregates and projections. It can be accelerated by generating code specific to each case.

Tuple deforming is the process of transforming an on-disk tuple into its in-memory representation. It can be accelerated by creating a function specific to the table layout and the number of columns to be extracted.

### <a id="topic22"></a>In-line compilation (inlining)

Greenplum Database is very extensible and allows new data types, functions, operators and other database objects to be defined. In fact, the built-in objects are implemented using nearly the same mechanisms. This extensibility implies some overhead, for example due to function calls. To reduce that overhead, JIT can use in-line compilation to fit the bodies of small functions into the expressions using them. That allows a significant percentage of the overhead to be optimized away.

### <a id="topic23"></a>Optimization

LLVM has support for optimizing generated code. Some of the optimizations are cheap enough to be performed whenever JIT is used, while others are only beneficial for longer-running queries. See the [LLVM Documentation](https://llvm.org/docs/Passes.html#transform-passes) for more details about optimizations.

## <a id="topic3"></a>When to JIT? 

JIT compilation is beneficial primarily for long-running CPU-bound queries. Frequently these are analytical queries. For short queries the added overhead of performing JIT compilation will often be higher than the time it can save.

The internal workflow of JIT can be divided into three different stages:

1. Planner Stage
    
    This stage takes place in the Greenplum Database coordinator. The planner generates the plan tree of a query and its estimated cost. 

    The planner decides to trigger JIT compilation if:

    - The configuration parameter [jit](../../../ref_guide/config_params/guc-list.html#jit) is `true`.
    - The estimated cost of the query is higher than the value of the configuration parameter [jit_above_cost](../../../ref_guide/config_params/guc-list.html#jit_above_cost).  

    If the parameter [jit_expressions](../../../ref_guide/config_params/guc-list.html#jit_expressions) is enabled, the planner suggests to the executor to compile the expressions in JIT space. Additionally, the planner must make other decisions; if the estimated cost is more than the setting of [jit_inline_above_cost](../../../ref_guide/config_params/guc-list.html#jit_inline_above_cost), the planner compiles short functions and operators used in the query using in-line compilation. If the estimated cost is more than the setting of [jit_optimize_above_cost](../../../ref_guide/config_params/guc-list.html#jit_optimize_above_cost), it applies expensive optimizations to improve the generated code. If the configuration parameter [jit_tuple_deforming](../../../ref_guide/config_params/guc-list.html#jit_tuple_deforming) is enabled, it generates a custom function to deform the target table. Each of these options increases the JIT compilation overhead, but can reduce query execution time considerably.

    You should tune these configuration parameters when you enable or disable GPORCA, as the meaning of cost is different for GPORCA and Postgres Planner. Note that setting the JIT cost parameters to ‘0’ forces all queries to be JIT-compiled and, as a result, slows down queries. Setting them to a negative value will disable the feature the parameter provides.

    When the plan is ready, the planner provides the plan trees and JIT flags to the executor.

1. Executor Initialization Stage

    This stage takes place in the Greenplum segments. Greenplum creates the expression evaluation steps. If JIT is used, it re-writes the steps as functions in the JIT space. The decisions made at plan time determine whether or not JIT compilation is adviced to be triggered in execution stage, along with the JIT strategy to apply if it is triggered. However, it is at execution time when Greenplum makes the decision of using JIT if the configuration parameter `jit` is enabled and the JIT libraries are loaded successfully. The executor may ignore the cached decisions if `jit` or `jit_expression` has been changed to `false` between planner and execution stages or if it encounters an error.

    Additionally, the executor checks the following developer configuration parameters:

    - [jit_provider](../../../ref_guide/config_params/guc-list.html#jit_provider) : Specifies the name of the JIT provider library to be used.
    - [jit_dump_bitcode](../../../ref_guide/config_params/guc-list.html#jit_dump_bitcode): Writes the generated LLVM IR out to the file system, inside [data_directory](../../../install_guide/create_data_dirs.html).
    - [jit_profiling_support](../../../ref_guide/config_params/guc-list.html#jit_profiling_support): If LLVM has the required functionality, emits the data needed to allow `perf` command to profile functions generated by JIT.
    - [jit_debugging_support](../../../ref_guide/config_params/guc-list.html#jit_debugging_support): If LLVM has the required functionality, registers generated functions with GDB. 

1. Executor Run Stage

    This stage also occurs in the Greenplum segments which execute the steps provided by the initialization stage. The functions in JIT space are combined as a whole before the first call.

The JIT workflow can also handle executor fault tolerance: if JIT fails to load on the segments, the execution mode fails back to non-JIT.

## <a id="topic4"></a>Examples

In the examples below, the configuration parameter `jit_above_cost` was modified so it would trigger JIT compilation. The use of JIT affects the cost of the plan, which can or cannot be bigger than the potential savings. JIT was used, but inlining and expensive optimization were not. If `jit_inline_above_cost` or `jit_optimize_above_cost` were also lowered, they could be triggered.

You may enable the configuration parameter [gp_explain_jit](../../../ref_guide/config_params/guc-list.html#gp_explain_jit) to display summarized JIT information from all query executions when running the `EXPLAIN` command. You must turn disable it when running regression tests

Note that the output from `EXPLAIN` provides information on JIT such as the slice average timing spent in JIT, what segment the maximum vector comes from, or how many JIT functions are created and total time spent in JIT tasks. This information can be helpful when tuning JIT or debugging a timing problem. Set the configuration parameters `log_min_messages`, `client_min_messages` to `DEBUG 5` to view this information.

With Postgres Planner:

```
EXPLAIN (ANALYZE) SELECT * FROM jit_explain_output LIMIT 10;
QUERY PLAN
Limit  (cost=35.50..35.67 rows=10 width=4) (actual time=13.199..13.310 rows=10 loops=1)
  ->  Gather Motion 3:1  (slice1; segments: 3)  (cost=35.50..36.01 rows=30 width=4) (actual time=11.848..11.890 rows=10 loops=1)
        ->  Limit  (cost=35.50..35.61 rows=10 width=4) (actual time=0.861..0.971 rows=10 loops=1)
              ->  Seq Scan on jit_explain_output  (cost=0.00..355.00 rows=32100 width=4) (actual time=0.029..0.070 rows=10 loops=1)
Optimizer: Postgres query optimizer
Planning Time: 0.158 ms
  (slice0)    Executor memory: 37K bytes.
  (slice1)    Executor memory: 36K bytes avg x 3 workers, 36K bytes max (seg0).
Memory used:  128000kB
JIT:
  Options: Inlining false, Optimization false, Expressions true, Deforming true.
  (slice0): Functions: 2.00. Timing: 1.381 ms total.
  (slice1): Functions: 1.00 avg x 3 workers, 1.00 max (seg0). Timing: 0.830 ms avg x 3 workers, 0.854 ms max (seg1).
Execution Time: 24.023 ms
(14 rows)
```

With GPORCA:

```
EXPLAIN (ANALYZE) SELECT * FROM jit_explain_output LIMIT 10;
QUERY PLAN
Limit  (cost=0.00..431.00 rows=1 width=4) (actual time=1.103..1.107 rows=10 loops=1)
  ->  Gather Motion 3:1  (slice1; segments: 3)  (cost=0.00..431.00 rows=1 width=4) (actual time=0.013..0.014 rows=10 loops=1)
        ->  Seq Scan on jit_explain_output  (cost=0.00..431.00 rows=1 width=4) (actual time=0.025..0.030 rows=38 loops=1)
Optimizer: Pivotal Optimizer (GPORCA)
Planning Time: 5.824 ms
  (slice0)    Executor memory: 37K bytes.
  (slice1)    Executor memory: 36K bytes avg x 3 workers, 36K bytes max (seg0).
Memory used:  128000kB
JIT:
  Options: Inlining false, Optimization false, Expressions true, Deforming true.
  (slice0): Functions: 2.00. Timing: 1.137 ms total.
Execution Time: 1.597 ms
(12 rows)
```

