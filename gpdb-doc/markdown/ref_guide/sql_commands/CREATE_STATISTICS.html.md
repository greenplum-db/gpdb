# CREATE STATISTICS

Defines extended statistics.

## <a id="section2"></a>Synopsis 

``` {#sql_command_synopsis}
CREATE STATISTICS [ IF NOT EXISTS ] <statistics_name>
    [ ( <statistics_kind> [, ... ] ) ]
    ON <column_name>, <column_name> [, ...]
    FROM <table_name>
```

## <a id="section3"></a>Description 

`CREATE STATISTICS` creates a new extended statistics object tracking data about the specified table, foreign table, or materialized view. The statistics object is created in the current database and will be owned by the user issuing the command.

If a schema name is given \(for example, `CREATE STATISTICS myschema.mystat ...`\) then the statistics object is created in the specified schema. Otherwise it is created in the current schema. The name of the statistics object must be distinct from the name of any other statistics object in the same schema.

## <a id="section4"></a>Parameters 

IF NOT EXISTS

:   Do not throw an error if a statistics object with the same name already exists. Greenplum Database issues a notice in this case. Note that only the name of the statistics object is considered here, not the details of its definition.

statistics\_name
:   The name \(optionally schema-qualified\) of the statistics object to create.

statistics\_kind
:   A statistics kind to be computed in this statistics object. Currently supported kinds are `ndistinct`, which enables n-distinct statistics, `dependencies`, which enables functional dependency statistics, and `mcv` which enables most-common values lists. If this clause is omitted, all supported statistics kinds are included in the statistics object.

column\_name
:   The name of a table column to be covered by the computed statistics. You must specify at least two column names; the order of the column names is insignificant.

table\_name
:   The name \(optionally schema-qualified\) of the table containing the column\(s\) on which the statistics are computed; see [ANALYZE](ANALYZE.html) for an explanation of inheritance and partition handling.

## <a id="section5"></a>Notes 

You must be the owner of a table to create a statistics object that reads it. Once created, however, the ownership of the statistics object is independent of the underlying table\(s\).

## <a id="section6"></a>Examples 

Create table `t1` with two functionally-dependent columns, i.e., knowledge of a value in the first column is sufficient for determining the value in the other column. Then build functional dependency statistics on those columns:

```
CREATE TABLE t1 (
    a   int,
    b   int
);

INSERT INTO t1 SELECT i/100, i/500
                 FROM generate_series(1,1000000) s(i);

ANALYZE t1;

-- the number of matching rows will be drastically underestimated:
EXPLAIN ANALYZE SELECT * FROM t1 WHERE (a = 1) AND (b = 0);

CREATE STATISTICS s1 (dependencies) ON a, b FROM t1;

ANALYZE t1;

-- now the row count estimate is more accurate:
EXPLAIN ANALYZE SELECT * FROM t1 WHERE (a = 1) AND (b = 0);
```

Without functional-dependency statistics, the planner assumes that the two `WHERE` conditions are independent, and would multiply their selectivities together to arrive at a much-too-small row count estimate. With such statistics, the planner recognizes that the `WHERE` conditions are redundant and does not underestimate the row count.

Create table `t2` with two perfectly correlated columns \(containing identical data\), and a MCV list on those columns:

```
CREATE TABLE t2 (
    a   int,
    b   int
);

INSERT INTO t2 SELECT mod(i,100), mod(i,100)
                 FROM generate_series(1,1000000) s(i);

CREATE STATISTICS s2 (mcv) ON a, b FROM t2;

ANALYZE t2;

-- valid combination (found in MCV)
EXPLAIN ANALYZE SELECT * FROM t2 WHERE (a = 1) AND (b = 1);

-- invalid combination (not found in MCV)
EXPLAIN ANALYZE SELECT * FROM t2 WHERE (a = 1) AND (b = 2);
```

The MCV list gives the planner more detailed information about the specific values that commonly appear in the table, as well as an upper bound on the selectivities of combinations of values that do not appear in the table, allowing it to generate better estimates in both cases.

## <a id="section7"></a>Compatibility 

There is no `CREATE STATISTICS` statement in the SQL standard.

## <a id="section8"></a>See Also 

[ALTER STATISTICS](ALTER_STATISTICS.html), [DROP STATISTICS](DROP_STATISTICS.html)

**Parent topic:** [SQL Commands](../sql_commands/sql_ref.html)

