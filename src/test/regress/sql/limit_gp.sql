
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
