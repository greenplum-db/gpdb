# EXPLAIN 

Shows the query plan of a statement.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
EXPLAIN [ ( <option> [, ...] ) ] <statement>
EXPLAIN [ANALYZE] [VERBOSE] <statement>
```

where option can be one of:

```
    ANALYZE [ <boolean> ]
    VERBOSE [ <boolean> ]
    COSTS [ <boolean> ]
    BUFFERS [ <boolean> ]
    TIMING [ <boolean> ]
    FORMAT { TEXT | XML | JSON | YAML }
```

## <a id="section3"></a>Description 

`EXPLAIN` displays the query plan that the Greenplum or Postgres Planner generates for the supplied statement. Query plans are a tree plan of nodes. Each node in the plan represents a single operation, such as table scan, join, aggregation or a sort.

Plans should be read from the bottom up as each node feeds rows into the node directly above it. The bottom nodes of a plan are usually table scan operations \(sequential, index or bitmap index scans\). If the query requires joins, aggregations, or sorts \(or other operations on the raw rows\) then there will be additional nodes above the scan nodes to perform these operations. The topmost plan nodes are usually the Greenplum Database motion nodes \(redistribute, explicit redistribute, broadcast, or gather motions\). These are the operations responsible for moving rows between the segment instances during query processing.

The output of `EXPLAIN` has one line for each node in the plan tree, showing the basic node type plus the following cost estimates that the planner made for the execution of that plan node:

-   **cost** — the planner's guess at how long it will take to run the statement \(measured in cost units that are arbitrary, but conventionally mean disk page fetches\). Two cost numbers are shown: the start-up cost before the first row can be returned, and the total cost to return all the rows. Note that the total cost assumes that all rows will be retrieved, which may not always be the case \(if using `LIMIT` for example\).
-   **rows** — the total number of rows output by this plan node. This is usually less than the actual number of rows processed or scanned by the plan node, reflecting the estimated selectivity of any `WHERE` clause conditions. Ideally the top-level nodes estimate will approximate the number of rows actually returned, updated, or deleted by the query.
-   **width** — total bytes of all the rows output by this plan node.

It is important to note that the cost of an upper-level node includes the cost of all its child nodes. The topmost node of the plan has the estimated total execution cost for the plan. This is this number that the planner seeks to minimize. It is also important to realize that the cost only reflects things that the query optimizer cares about. In particular, the cost does not consider the time spent transmitting result rows to the client.

`EXPLAIN ANALYZE` causes the statement to be actually run, not only planned. The `EXPLAIN ANALYZE` plan shows the actual results along with the planner's estimates. This is useful for seeing whether the planner's estimates are close to reality. In addition to the information shown in the `EXPLAIN` plan, `EXPLAIN ANALYZE` will show the following additional information:

-   The total elapsed time \(in milliseconds\) that it took to run the query.
-   The number of *workers* \(segments\) involved in a plan node operation. Only segments that return rows are counted.
-   The maximum number of rows returned by the segment that produced the most rows for an operation. If multiple segments produce an equal number of rows, the one with the longest *time to end* is the one chosen.
-   The segment id number of the segment that produced the most rows for an operation.
-   For relevant operations, the work\_mem used by the operation. If `work_mem` was not sufficient to perform the operation in memory, the plan will show how much data was spilled to disk and how many passes over the data were required for the lowest performing segment. For example:

    ```
    Work_mem used: 64K bytes avg, 64K bytes max (seg0).
    Work_mem wanted: 90K bytes avg, 90K bytes max (seg0) to abate workfile 
    I/O affecting 2 workers.
    [seg0] pass 0: 488 groups made from 488 rows; 263 rows written to 
    workfile
    [seg0] pass 1: 263 groups made from 263 rows
    ```

-   The time \(in milliseconds\) it took to retrieve the first row from the segment that produced the most rows, and the total time taken to retrieve all rows from that segment. The *<time\> to first row* may be omitted if it is the same as the *<time\> to end*.

> **Important** Keep in mind that the statement is actually run when `ANALYZE` is used. Although `EXPLAIN ANALYZE` will discard any output that a `SELECT` would return, other side effects of the statement will happen as usual. If you wish to use `EXPLAIN ANALYZE` on a DML statement without letting the command affect your data, use this approach:

```
BEGIN;
EXPLAIN ANALYZE ...;
ROLLBACK;
```

Only the `ANALYZE` and `VERBOSE` options can be specified, and only in that order, without surrounding the option list in parentheses.

## <a id="section4"></a>Parameters 

ANALYZE
:   Carry out the command and show the actual run times and other statistics. This parameter defaults to `FALSE` if you omit it; specify `ANALYZE true` to enable it.

VERBOSE
:   Display additional information regarding the plan. Specifically, include the output column list for each node in the plan tree, schema-qualify table and function names, always label variables in expressions with their range table alias, and always print the name of each trigger for which statistics are displayed. This parameter defaults to `FALSE`if you omit it; specify `VERBOSE true` to enable it.

COSTS
:   Include information on the estimated startup and total cost of each plan node, as well as the estimated number of rows and the estimated width of each row. This parameter defaults to `TRUE` if you omit it; specify `COSTS false` to deactivate it.

BUFFERS
:   Include information on buffer usage. Specifically, include the number of shared blocks hit, read, dirtied, and written, the number of local blocks hit, read, dirtied, and written, and the number of temp blocks read and written. A *hit* means that a read was avoided because the block was found already in cache when needed. Shared blocks contain data from regular tables and indexes; local blocks contain data from temporary tables and indexes; while temp blocks contain short-term working data used in sorts, hashes, Materialize plan nodes, and similar cases. The number of blocks *dirtied* indicates the number of previously unmodified blocks that were changed by this query; while the number of blocks *written* indicates the number of previously-dirtied blocks evicted from cache by this backend during query processing. The number of blocks shown for an upper-level node includes those used by all its child nodes. In text format, only non-zero values are printed. This parameter may only be used when `ANALYZE` is also enabled. This parameter defaults to `FALSE` if you omit it; specify `BUFFERS true` to enable it.

TIMING
:   Include actual startup time and time spent in each node in the output. The overhead of repeatedly reading the system clock can slow down the query significantly on some systems, so it may be useful to set this parameter to `FALSE` when only actual row counts, and not exact times, are needed. Run time of the entire statement is always measured, even when node-level timing is turned off with this option. This parameter may only be used when `ANALYZE` is also enabled. It defaults to `TRUE`.

FORMAT
:   Specify the output format, which can be `TEXT`, `XML`, `JSON`, or `YAML`. Non-text output contains the same information as the text output format, but is easier for programs to parse. This parameter defaults to `TEXT`.

boolean
:   Specifies whether the selected option should be turned on or off. You can write `TRUE`, `ON`, or `1` to enable the option, and `FALSE`, `OFF`, or `0` to deactivate it. The boolean value can also be omitted, in which case `TRUE` is assumed.

statement
:   Any `SELECT`, `INSERT`, `UPDATE`, `DELETE`, `VALUES`, `EXECUTE`, `DECLARE`, or `CREATE TABLE AS` statement, whose execution plan you wish to see.

## <a id="section5"></a>Notes 

In order to allow the query optimizer to make reasonably informed decisions when optimizing queries, the `ANALYZE` statement should be run to record statistics about the distribution of data within the table. If you have not done this \(or if the statistical distribution of the data in the table has changed significantly since the last time `ANALYZE` was run\), the estimated costs are unlikely to conform to the real properties of the query, and consequently an inferior query plan may be chosen.

An SQL statement that is run during the execution of an `EXPLAIN ANALYZE` command is excluded from Greenplum Database resource queues.

For more information about query profiling, see "Query Profiling" in the *Greenplum Database Administrator Guide*. For more information about resource queues, see "Resource Management with Resource Queues" in the *Greenplum Database Administrator Guide*.

## <a id="section6"></a>Examples 

To illustrate how to read an `EXPLAIN` query plan, consider the following example for a very simple query:

```
EXPLAIN SELECT * FROM names WHERE name = 'Joelle';
                                  QUERY PLAN
-------------------------------------------------------------------------------
 Gather Motion 3:1  (slice1; segments: 3)  (cost=0.00..431.27 rows=1 width=58)
   ->  Seq Scan on names  (cost=0.00..431.27 rows=1 width=58)
         Filter: (name = 'Joelle'::text)
 Optimizer: Pivotal Optimizer (GPORCA) version 3.23.0
(4 rows)
```

If we read the plan from the bottom up, the query optimizer starts by doing a sequential scan of the `names` table. Notice that the `WHERE` clause is being applied as a *filter* condition. This means that the scan operation checks the condition for each row it scans, and outputs only the ones that pass the condition.

The results of the scan operation are passed up to a *gather motion* operation. In Greenplum Database, a gather motion is when segments send rows up to the coordinator. In this case we have 3 segment instances sending to 1 coordinator instance \(3:1\). This operation is working on `slice1` of the parallel query execution plan. In Greenplum Database a query plan is divided into *slices* so that portions of the query plan can be worked on in parallel by the segments.

The estimated startup cost for this plan is `00.00` \(no cost\) and a total cost of `431.27`. The planner is estimating that this query will return one row.

Here is the same query, with cost estimates suppressed:

```
EXPLAIN (COSTS FALSE) SELECT * FROM names WHERE name = 'Joelle';
                QUERY PLAN
------------------------------------------
 Gather Motion 3:1  (slice1; segments: 3)
   ->  Seq Scan on names
         Filter: (name = 'Joelle'::text)
 Optimizer: Pivotal Optimizer (GPORCA) version 3.23.0
(4 rows)
```

Here is the same query, with JSON formatting:

```
EXPLAIN (FORMAT JSON) SELECT * FROM names WHERE name = 'Joelle';
                  QUERY PLAN
-----------------------------------------------
 [                                            +
   {                                          +
     "Plan": {                                +
       "Node Type": "Gather Motion",          +
       "Senders": 3,                          +
       "Receivers": 1,                        +
       "Slice": 1,                            +
       "Segments": 3,                         +
       "Gang Type": "primary reader",         +
       "Startup Cost": 0.00,                  +
       "Total Cost": 431.27,                  +
       "Plan Rows": 1,                        +
       "Plan Width": 58,                      +
       "Plans": [                             +
         {                                    +
           "Node Type": "Seq Scan",           +
           "Parent Relationship": "Outer",    +
           "Slice": 1,                        +
           "Segments": 3,                     +
           "Gang Type": "primary reader",     +
           "Relation Name": "names",          +
           "Alias": "names",                  +
           "Startup Cost": 0.00,              +
           "Total Cost": 431.27,              +
           "Plan Rows": 1,                    +
           "Plan Width": 58,                  +
           "Filter": "(name = 'Joelle'::text)"+
         }                                    +
       ]                                      +
     },                                       +
     "Settings": {                            +
       "Optimizer": "Pivotal Optimizer (GPORCA) version 3.23.0"      +
     }                                        +
   }                                          +
 ]
(1 row)
```

If there is an index and we use a query with an indexable `WHERE` condition, `EXPLAIN` might show a different plan. This query generates a plan with an index scan, with YAML formatting:

```
EXPLAIN (FORMAT YAML) SELECT * FROM NAMES WHERE LOCATION='Sydney, Australia';
                          QUERY PLAN
--------------------------------------------------------------
 - Plan:                                                     +
     Node Type: "Gather Motion"                              +
     Senders: 3                                              +
     Receivers: 1                                            +
     Slice: 1                                                +
     Segments: 3                                             +
     Gang Type: "primary reader"                             +
     Startup Cost: 0.00                                      +
     Total Cost: 10.81                                       +
     Plan Rows: 10000                                        +
     Plan Width: 70                                          +
     Plans:                                                  +
       - Node Type: "Index Scan"                             +
         Parent Relationship: "Outer"                        +
         Slice: 1                                            +
         Segments: 3                                         +
         Gang Type: "primary reader"                         +
         Scan Direction: "Forward"                           +
         Index Name: "names_idx_loc"                         +
         Relation Name: "names"                              +
         Alias: "names"                                      +
         Startup Cost: 0.00                                  +
         Total Cost: 7.77                                    +
         Plan Rows: 10000                                    +
         Plan Width: 70                                      +
         Index Cond: "(location = 'Sydney, Australia'::text)"+
   Settings:                                                 +
     Optimizer: "Pivotal Optimizer (GPORCA) version 3.23.0"
(1 row)
```

## <a id="section7"></a>Compatibility 

There is no `EXPLAIN` statement defined in the SQL standard.

## <a id="section8"></a>See Also 

[ANALYZE](ANALYZE.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

