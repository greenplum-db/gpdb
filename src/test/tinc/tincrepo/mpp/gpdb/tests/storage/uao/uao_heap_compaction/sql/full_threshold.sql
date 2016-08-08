-- @Description Tests that that full vacuum is ignoring the threshold guc value.
-- 

VACUUM FULL foo;
DELETE FROM foo WHERE a < 4;
SET gp_appendonly_compaction_threshold=100;
VACUUM FULL foo;
