-- @Description Tests the behavior while compacting is disabled
-- 

SET gp_appendonly_compaction=false;
SELECT COUNT(*) FROM foo;
VACUUM foo;
SELECT COUNT(*) FROM foo;
-- Insert afterwards
INSERT INTO foo SELECT i as a, 1 as b, 'hello world' as c FROM generate_series(1,10) AS i;
SELECT COUNT(*) FROM foo;
