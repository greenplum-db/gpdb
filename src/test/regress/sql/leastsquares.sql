--
-- leastsquares.sql - test for ordinary least squares regression aggregates:
--   * regr_count
--   * regr_avgx
--   * regr_avgy
--   * regr_sxx
--   * regr_sxy
--   * regr_syy
--   * regr_intercept
--   * regr_slope
--   * regr_r2
--   * mregr_coef
--   * mregr_r2
--   * mregr_tstats
--   * mregr_pvalues

-- start_matchsubs
-- m|Failed on request of size \d+ bytes|
-- s|Failed on request of size \d+ bytes|Failed on request of size BIGALLOC bytes|
--
-- m/(ERROR|WARNING|CONTEXT|NOTICE):.*\(float\.c\:\d+\)/
-- s/\(float\.c:\d+\)//
--
-- end_matchsubs

CREATE TABLE weibull
(
	id INTEGER NOT NULL,
	x1 DOUBLE PRECISION,
	x2 DOUBLE PRECISION,
	y DOUBLE PRECISION
) DISTRIBUTED BY (id);

COPY weibull (id, x1, x2, y) FROM stdin;
1	41.9	29.1	251.3
2	43.4	29.3	251.3
3	43.9	29.5	248.3
4	44.5	29.7	267.5
5	47.3	29.9	273.0
6	47.5	30.3	276.5
7	47.9	30.5	270.3
8	50.2	30.7	274.9
9	52.8	30.8	285.0
10	53.2	30.9	290.0
11	56.7	31.5	297.0
12	57.0	31.7	302.5
13	63.5	31.9	304.5
14	65.3	32.0	309.3
15	71.1	32.1	321.7
16	77.0	32.5	330.7
17	77.8	32.9	349.0
\.

-- Testing of basic single linear regression code
-- start_equiv
select 
    regr_count(y, x1)::real as count,
    regr_avgx(y, x1)::real as avgx,
    regr_avgy(y, x1)::real as avgy,
    regr_sxx(y, x1)::real as sxx,
    regr_sxy(y, x1)::real as sxy,
    regr_syy(y, x1)::real as syy,
    regr_intercept(y, x1)::real as intercept,
    regr_slope(y, x1)::real as slope,
    regr_r2(y, x1)::real as r2
from weibull;

select
    count(y)::real as count,
    avg(x1)::real as avgx,
    avg(y)::real as avgy,
    ((count(y) * sum(x1*x1) - sum(x1)^2)/count(y))::real as sxx,
    ((count(y) * sum(x1*y) - sum(x1)*sum(y))/count(y))::real as sxy,
    ((count(y) * sum(y*y) - sum(y)^2)/count(y))::real as sxy,
    ((sum(y)*sum(x1*x1) - sum(x1)*sum(x1*y))/(count(y) * sum(x1*x1) - sum(x1)^2))::real as intercept,
    ((count(y) * sum(x1*y) - sum(x1)*sum(y))/(count(y) * sum(x1*x1) - sum(x1)^2))::real as slope,
    ((count(y) * sum(x1*y) - sum(x1)*sum(y))^2/
        ((count(y) * sum(x1*x1) - sum(x1)^2) * (count(y) * sum(y*y) - sum(y)^2)))::real as r2
from weibull;
-- end_equiv

-- Single linear and multivariate should match for a single independent variable
-- start_equiv
select 
    array[regr_intercept(y, x1), regr_slope(y, x1)]::real[] as coef,
    regr_r2(y,x1)::real as r2
from weibull;

select 
    mregr_coef(y, array[1,x1])::real[] as coef,
    mregr_r2(y, array[1,x1])::real as r2
from weibull;
-- end_equiv

-- start_equiv
select 
    array[regr_intercept(y, x2), regr_slope(y, x2)]::real[] as coef,
    regr_r2(y,x2)::real as r2
from weibull;

select 
    mregr_coef(y, array[1, x2])::real[] as coef,
    mregr_r2(y, array[1, x2])::real as r2
from weibull;
-- end_equiv


-- When there is a single element regr returns null, mregr still tries to give numbers.
-- start_equiv
select
    id,
    regr_intercept(y, x2)::real as intercept,
    regr_slope(y, x2)::real as slope,
    regr_r2(y,x1)::real as r2
from weibull
group by id
order by id;

select 
    id,
    regr_intercept(y, x2)::real as intercept,
    regr_slope(y, x2)::real as slope,
    regr_r2(y,x1)::real as r2
from gp_dist_random('weibull')
group by id
order by id;

select 
    id,
    (regr_intercept(y, x2) over (partition by id))::real as intercept,
    (regr_slope(y, x2) over (partition by id))::real as slope,
    (regr_r2(y,x1) over (partition by id))::real as r2
from weibull
order by id;
-- end_equiv

-- start_equiv
select 
    id,
    mregr_coef(y, array[1, x2])::real[] as coef,
    mregr_r2(y, array[1, x2])::real as r2
from weibull
group by id
order by id;

select 
    id,
    mregr_coef(y, array[1, x2])::real[] as coef,
    mregr_r2(y, array[1, x2])::real as r2
from gp_dist_random('weibull')
group by id
order by id;

select 
    id,
    (mregr_coef(y, array[1, x2]) over (partition by id))::real[] as coef,
    (mregr_r2(y, array[1, x2]) over (partition by id))::real as r2
from weibull
order by id;
-- end_equiv

-- Test multivariate regression
select 
    mregr_coef(y, array[1, x1, x2])::real[] as coef,
    mregr_r2(y, array[1, x1, x2])::real as r2,
    mregr_tstats(y, array[1, x1, x2])::real[] as tstats,
    mregr_pvalues(y, array[1, x1, x2])::real[] as pvalues
from weibull;

-- MPP-11680: numeric stability on low cardinality
/*
select 
    id,
    mregr_coef(y, array[1, x1, x2])::real[] as coef,
    mregr_r2(y, array[1, x1, x2])::real as r2,
    mregr_tstats(y, array[1, x1, x2])::real[] as tstats,
    mregr_pvalues(y, array[1, x1, x2])::real[] as pvalues
from weibull
group by id
order by id;
*/

-- MPP-14102: Crash in mregr_coef() when number of independent variables is too
-- large.

/* This test should be moved to CDBfast

-- The following does not immediately violate any hard constraints, but will
-- only fail during execution with an out-of-memory error
SELECT mregr_coef(y, x)
FROM (
    SELECT 1::FLOAT8 AS y,
    ARRAY( SELECT no::FLOAT8 FROM generate_series(1,8191) AS no ) AS x
) AS one_row_subquery;

*/

-- The following tries to allocate a transition-state array with
-- size > MaxAllocSize, so it immediately fails.
SELECT mregr_coef(y, x)
FROM (
    SELECT 1::FLOAT8 AS y,
    ARRAY( SELECT no::FLOAT8 FROM generate_series(1,65535) AS no ) AS x
) AS one_row_subquery;

-- MPP-13580: infinity in design matrix causes infinite loop
SELECT mregr_coef(y,array[x]) FROM (
    SELECT 10 AS y, 1 AS x
    UNION
    SELECT 20 AS y, 'infinity'::FLOAT8 AS x
) AS q;

-- MPP-13582: dealing with less observations than variables
SELECT mregr_r2(y,x) FROM (
    SELECT array[1,0,0] AS x, 10 AS y
    UNION
    SELECT array[1,0,0], 20
) AS q;

-- Accumulation/combination order shouldn't matter to the result.
-- start_equiv
select float8_regr_accum(float8_regr_accum(array[0,0,0,0,0,0], 1, 2),  2, 1);
select float8_regr_accum(float8_regr_accum(array[0,0,0,0,0,0], 2, 1),  1, 2);
select float8_regr_amalg(float8_regr_accum(array[0,0,0,0,0,0], 1, 2),
                         float8_regr_accum(array[0,0,0,0,0,0], 2, 1));
-- end_equiv

select float8_mregr_combine(array[2,1,2,3,4,5,6,7,8,9],array[2,1,2,3,4,5,6,7,8,9]);


-- Component testing of the individual aggregate callback functions
--  * null handling
--  * malformed state
--  * check for invalid in-place updates of first parameter
select float8_regr_accum(null, 1, 2);
select float8_regr_accum(array[0,0,0,0,0,0], 1, null);
select float8_regr_accum(array[0,0,0,0,0,0], null, 2);
select float8_regr_amalg(array[0,0,0,0,0,0], null);
select float8_regr_amalg(null, array[0,0,0,0,0,0]);
select float8_regr_sxx(null);
select float8_regr_sxx(array[0,0,0,0,0,0]);
select float8_regr_sxx(float8_regr_accum(array[0,0,0,0,0,0], 1, 2));
select float8_regr_syy(null);
select float8_regr_syy(array[0,0,0,0,0,0]);
select float8_regr_syy(float8_regr_accum(array[0,0,0,0,0,0], 1, 2));
select float8_regr_sxy(null);
select float8_regr_sxy(array[0,0,0,0,0,0]);
select float8_regr_sxy(float8_regr_accum(array[0,0,0,0,0,0], 1, 2));
select float8_regr_avgx(null);
select float8_regr_avgx(array[0,0,0,0,0,0]);
select float8_regr_avgx(float8_regr_accum(array[0,0,0,0,0,0], 1, 2));
select float8_regr_avgy(null);
select float8_regr_avgy(array[0,0,0,0,0,0]);
select float8_regr_avgy(float8_regr_accum(array[0,0,0,0,0,0], 1, 2));
select float8_regr_r2(null);
select float8_regr_r2(array[0,0,0,0,0,0]);
select float8_regr_r2(float8_regr_accum(array[0,0,0,0,0,0], 1, 2));
select float8_regr_slope(null);
select float8_regr_slope(array[0,0,0,0,0,0]);
select float8_regr_slope(float8_regr_accum(array[0,0,0,0,0,0], 1, 2));
select float8_regr_intercept(null);
select float8_regr_intercept(array[0,0,0,0,0,0]);
select float8_regr_intercept(float8_regr_accum(array[0,0,0,0,0,0], 1, 2));

select float8_mregr_accum(null, 1, array[1,2]);
select float8_mregr_accum('{}'::float8[], null, array[1,2]);
select float8_mregr_accum('{}'::float8[], 1, null);
select float8_mregr_combine(null, '{}');
select float8_mregr_combine('{}', null);
select float8_mregr_combine('{0}'::float8[], '{0}'::float8[]);

select float8_regr_accum('{}'::float8[], 1, 2);
select float8_regr_amalg('{}'::float8[], array[0,0,0,0,0,0]);
select float8_regr_amalg(array[0,0,0,0,0,0], '{}'::float8[]);
select float8_regr_amalg(array[null,0,0,0,0,0], '{}'::float8[]);
select float8_regr_sxx('{}'::float8[]);
select float8_regr_syy('{}'::float8[]);
select float8_regr_sxy('{}'::float8[]);
select float8_regr_avgx('{}'::float8[]);
select float8_regr_avgy('{}'::float8[]);
select float8_regr_slope('{}'::float8[]);
select float8_regr_r2('{}'::float8[]);
select float8_regr_intercept('{}'::float8[]);

select float8_mregr_accum('{}'::float8[], 1, array[1,null]);
select float8_mregr_accum('{}'::float8[], 1, array[1,2]);
select float8_mregr_accum('{0}'::float8[], 1, array[1,2]);
select float8_mregr_combine('{}'::float8[], '{}'::float8[]);
select float8_mregr_combine(array[1,null,0,0,0,0,0], array[[1,0,0,0,0,0,0]]);
select float8_mregr_combine(array[1,0,0,0,0,0,0], array[[1,0,0,0,0,0,0]]);
select float8_mregr_combine(array[1,0,0,0,0,0,0], array[1,0,0,0,0,0,0]);
select float8_mregr_combine(array[0,0,0,0,0,0], array[1,2,3,4,5,6]);


CREATE TABLE regr_test as 
   select array[0,0,0,0,0,0]::float8[] as x, array[2,0,0,0,0,0,0,0,0,0] as y
DISTRIBUTED RANDOMLY;

select float8_regr_accum(x, 0, 3), 
       float8_regr_accum(x, 0, 2), 
       x 
from regr_test;
select float8_regr_amalg(x, array[1,3,9,0,0,0]), 
       float8_regr_amalg(x, array[1,2,4,0,0,0]), 
       x 
from regr_test;
select float8_mregr_accum(x, 0, array[1,3]), 
       float8_mregr_accum(x, 0, array[1,2]), 
       x 
from regr_test;
select float8_mregr_combine(y, array[2,1,2,3,4,5,6,7,8,9]), 
       float8_mregr_combine(y, array[2,9,8,7,6,5,4,3,2,1]), 
       y
from regr_test;

DROP TABLE regr_test;
DROP TABLE weibull;
