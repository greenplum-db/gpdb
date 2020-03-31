
-- Check for MPP-19310 and MPP-19857 where mksort produces wrong result
-- on OPT build, and fails assertion on debug build if a "LIMIT" query
-- spills to disk.

CREATE TABLE mksort_limit_test_table(dkey INT, jkey INT, rval REAL, tval TEXT default repeat('abcdefghijklmnopqrstuvwxyz', 300)) DISTRIBUTED BY (dkey);
INSERT INTO mksort_limit_test_table VALUES(generate_series(1, 10000), generate_series(10001, 20000), sqrt(generate_series(10001, 20000)));
SET gp_enable_mk_sort = on;

--Should fit LESS (because of overhead) than (20 * 1024 * 1024) / (26 * 300 + 12) => 2684 tuples in memory, after that spills to disk
SET statement_mem="20MB";

-- Should work in memory
SELECT dkey, substring(tval from 1 for 2) as str  from (SELECT * from mksort_limit_test_table ORDER BY dkey LIMIT 200) as temp ORDER BY jkey LIMIT 3;
SELECT dkey, substring(tval from 1 for 2) as str  from (SELECT * from mksort_limit_test_table ORDER BY dkey LIMIT 200) as temp ORDER BY jkey DESC LIMIT 3;

-- Should spill to disk (tested with 2 segments, for more segments it may not spill)
SELECT dkey, substring(tval from 1 for 2) as str  from (SELECT * from mksort_limit_test_table ORDER BY dkey LIMIT 5000) as temp ORDER BY jkey LIMIT 3;
SELECT dkey, substring(tval from 1 for 2) as str  from (SELECT * from mksort_limit_test_table ORDER BY dkey LIMIT 5000) as temp ORDER BY jkey DESC LIMIT 3;

-- In memory descending sort
SELECT dkey, substring(tval from 1 for 2) as str  from (SELECT * from mksort_limit_test_table ORDER BY dkey DESC LIMIT 200) as temp ORDER BY jkey LIMIT 3;
SELECT dkey, substring(tval from 1 for 2) as str  from (SELECT * from mksort_limit_test_table ORDER BY dkey DESC LIMIT 200) as temp ORDER BY jkey DESC LIMIT 3;

-- Spilled descending sort (tested with 2 segments, for more segments it may not spill)
SELECT dkey, substring(tval from 1 for 2) as str from (SELECT * from mksort_limit_test_table ORDER BY dkey DESC LIMIT 5000) as temp ORDER BY jkey LIMIT 3;
SELECT dkey, substring(tval from 1 for 2) as str  from (SELECT * from mksort_limit_test_table ORDER BY dkey DESC LIMIT 5000) as temp ORDER BY jkey DESC LIMIT 3;

DROP TABLE  mksort_limit_test_table;

-- Check invalid things in LIMIT

select * from generate_series(1,10) g limit g;
select * from generate_series(1,10) g limit count(*);

-- Check volatile limit should not pushdown.
create table t_volatile_limit (i int4);
create table t_volatile_limit_1 (a int, b int) distributed randomly;

-- Greenplum may generate two-stage limit plan to improve performance.
-- But for limit clause contains volatile functions, if we push them down
-- below the final gather motion, those volatile functions will be evaluated
-- many times. For such cases, we should not push down the limit.

-- Below test cases' limit clause contain function call `random` with order by.
-- `random()` is a volatile function it may return different results each time
-- invoked. If we push down to generate two-stage limit plan, `random()` will
-- execute on each segment which leads to different limit values of QEs
-- and QD and this cannot guarantee correct results. Suppose seg 0 contains the
-- top 3 minimum values, but random() returns 1, then you lose 2 values.
explain select * from t_volatile_limit order by i limit (random() * 10);
explain select * from t_volatile_limit order by i limit 2 offset (random()*5);

explain select distinct(a), sum(b) from t_volatile_limit_1 group by a order by a, sum(b) limit (random()+3);
explain select distinct(a), sum(b) from t_volatile_limit_1 group by a order by a, sum(b) limit 2 offset (random()*2);

drop table t_volatile_limit;
drop table t_volatile_limit_1;
