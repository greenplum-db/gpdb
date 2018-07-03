HyperLogLog Estimator
=====================

This is an implementation of HyperLogLog algorithm as first described in the
paper "HyperLogLog: the analysis of near-optimal cardinality estimation
algorithm", published by Flajolet, Fusy, Gandouet and Meunier in 2007.
Generally it is an improved version of LogLog algorithm with the last
step modified, to combine the parts using harmonic means.

Several improvements have been included that are described in "HyperLogLog 
in Practice: Algorithmic Engineering of a State of The Art Cardinality 
Estimation Algorithm", published by Stefan Heulem, Marc Nunkesse and 
Alexander Hall.

This is not the only (or first) PostgreSQL extension implementing the
HyperLogLog estimator - since 2013/02 there's [postgresql-hll](https://github.com/aggregateknowledge/postgresql-hll)
It's a nice mature extension, so you may try it.


Contents of the extension
-------------------------
The extension provides the following elements

* hyperloglog_estimator data type (may be used for columns, in PL/pgSQL)

* functions to work with the hyperloglog_estimator data type

    * `hyperloglog_size(error_rate real, ndistinct double precision)`
    * `hyperloglog_size(error_rate real)`
    * `hyperloglog_size()`
    * `hyperloglog_init(error_rate real, ndistinct double precision)`
    * `hyperloglog_init(error_rate real)`
    * `hyperloglog_init()`
    * `hyperloglog_add_item(counter hyperloglog_estimator, item anyelement)`
    * `hyperloglog_get_estimate(counter hyperloglog_estimator)`
    * `convert_to_scalar(counter hyperloglog_estimator)`
    * `hyperloglog_reset(counter hyperloglog_estunator)`
    * `length(counter hyperloglog_estimator)`
    * `hyperloglog_merge(counter1 hyperloglog_estimator, counter2 hyperloglog_estimator)`
    * `hyperloglog_comp(counter hyperloglog_estimator)`
    * `hyperloglog_decomp(counter hyperloglog_estimator)`
    * `hyperloglog_unpack(counter hyperloglog_estimator)`
    * `hyperloglog_info(counter hyperloglog_estimator)`
    * `hyperloglog_info()`
    * `hyperloglog_update(counter hyperloglog_estimator)`

	The purpose of the functions is typically obvious from the names, alternatively consult the SQL script for more details.

* aggregate functions 

    * `hyperloglog_distinct(anyelement, error_rate real, ndistinct double precision)` - Returns: double precision
    * `hyperloglog_distinct(anyelement, error_rate real)` - Returns: double precision
    * `hyperloglog_distinct(anyelement)` - Returns: double precision
    * `hyperloglog_accum(anyelement, error_rate real, ndistinct double precision)` - Returns: hyperloglog_estimator
    * `hyperloglog_accum(anyelement, error_rate real)` - Returns: hyperloglog_estimator
    * `hyperloglog_accum(anyelement)` - Returns: hyperloglog_estimator
    * `hyperloglog_accum(anyelement, error_rate real, ndistinct double precision, format text)` - Returns: hyperloglog_estimator
    * `hyperloglog_accum(anyelement, error_rate real, format text)` - Returns: hyperloglog_estimator
    * `hyperloglog_accum(anyelement, format text)` - Returns: hyperloglog_estimator
    * `sum(counter hyperloglog_estimator)` - Returns: bigint
    * `hyperloglog_merge(counter hyperloglog_estimator)` - Returns: hyperloglog_estimator

    NOTE: some aggregates feature an 'unsafe' version, these are an optimized version of the aggregate that does not support use repeated use in the same SELECT statement or in UNION/UNION ALL queries

* Parameters

    * `error_rate` - Error rate for the constructed estimator - Valid values 0..1 (Default: 0.008215) (Can only be set during initial creation)
    * `ndistinct` - Number of distinct values to support in the estimator (Default: 2^63) (Can only be set during initial creation)
    * `format` - Format of the resulting counter (bitpacked compressed or unpacked) - Valid values P,U (Default: P) (Once set on a "column" it will retain this setting in all other operations unless explicitly changed)

* operators

    * `#`
    * `||`
    * `=`
    * `<>`
    * `>`
    * `<`
    * `>=`
    * `<=`

* type casts
	
    * `hyperloglog_estimator::bytea`
    * `bytea::hyperloglog_estimator`
    
    Hyperloglog_estimator's can be cast to bytea's and apporpriate bytea's can be cast to hyperloglog_estimator's. This is useful for utilities that might not like use defined types but understand bytea's. Since the two are binary compatable the typecast is essentially free.
    
* set operations

    * `hyperloglog_union(counter1 hyperloglog_estimator, counter2 hyperloglog_estimator)`
    * `hyperloglog_intersection(counter1 hyperloglog_estimator, counter2 hyperloglog_estimator)`
    * `hyperloglog_compliment(counter1 hyperloglog_estimator, counter2 hyperloglog_estimator)`
    * `hyperloglog_symmetric_diff(counter1 hyperloglog_estimator, counter2 hyperloglog_estimator)`
    
    Its important to note all of these (except union) are based on the inclusion-exclusion principle to produce their results and can produce innacurate results especially if the two counters are of very different sizes.
    

Basic Usage
-----------
Using the aggregate is quite straightforward - just use it like a
regular aggregate function

```
db=# SELECT hyperloglog_distinct(i)
	 FROM generate_series(1,100000) s(i);

hyperloglog_distinct
--------------------
98643.35068214641
```

The above returns an evaluated counter but if you want a counter that you can save and work with later use hyperloglog_accum

```
db=# SELECT hyperloglog_accum(i)
	 FROM generate_series(1,100000) s(i);
     
hyperloglogg_accum
------------------
\362\006\002\000\377\377\377\ ...
```

Note the first example is equivalant to the following

```
db=# SELECT hyperloglog_get_estimate(hyperloglog_accum(i))
	 FROM generate_series(1,100000) s(i);

hyperloglog_get_estimate
------------------------
98643.35068214641
```

Say we have a table with a hyperloglog_estimator column and we want  to get the total distinct for the whole table (i.e. merge all the counters and evaluate)

```
db=# CREATE TEMP TABLE dummy_table AS
	 SELECT i, hyperloglog_accum(i) as distinct_count_column
     FROM generate_series(1,100000) s(i)
     GROUP BY 1;

db=# SELECT sum(distinct_count_column)
	 FROM dummy_table;

sum
---
98643.35068214641
```

Or if we want to save it for later usage we can use the hyperloglog_merge aggregation

```
db=# CREATE TEMP TABLE dummy_table AS
	 SELECT i, hyperloglog_accum(i) as distinct_count_column
     FROM generate_series(1,100000) s(i)
     GROUP BY 1;

db=# SELECT hypyerloglog_merge(distinct_count_column)
	 FROM dummy_table
     WHERE i%2 = 0;
     
hyperloglogg_merge
------------------
\362\006\002\000\377\377\377\ ...     
```

Installation
------------
To install on postgres run

```
make install
```

and if installing on greenplum then gscp the file to all nodes in the cluster

```
gpscp -f /home/gpadmin/hosts.all /usr/local/greenplum-db/lib/postgresql/hyperloglog_counter.so =:/usr/local/greenplum-db/lib/postgresql/
```

Then you must add the SQL file which contains the type/cast/function/aggregation/operation definitions (use the appropriate installation file for you environment)

```
psql < sql/postgres.sql

psql < sql/greenplum.sql
```

To run regression tests after installation

```
make tests
```

which should yeild the following

```
test/gp/sql/base
 .. PASS
test/gp/sql/aggs
 .. PASS
test/gp/sql/set_ops
 .. PASS
test/gp/sql/operators
 .. PASS
test/gp/sql/compression
 .. PASS
test/gp/sql/update
 .. PASS
test/gp/sql/gp_persistence
 .. PASS
7 / 7 tests passed
```

Details
-------
To see more implementation specific details look [here](documentation/README.md)

Tested Platforms
-------
OS:

* RHEL/CentOS 5/6

Postgres/Greenplum Versions
-----
Greenplum:

* 4.2.x
* 4.3.[0-4]

Postgres:

* 9.2 (some tests fail due to different updated compression algorithm from GP - hex data needs updated)

Problems
--------
Be careful about the implementation, as the estimators may easily
occupy several kilobytes (depends on the precision etc.). Keep in
mind that the PostgreSQL MVCC works so that it creates a copy of
the row on update, an that may easily lead to bloat. So group the
updates or something like that.

This is of course made worse by using unnecessarily large estimators,
so always tune the estimator to use the lowest amount of memory.

