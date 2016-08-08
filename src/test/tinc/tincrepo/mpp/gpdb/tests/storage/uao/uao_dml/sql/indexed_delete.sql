-- @Description Checks if delete using an index works correctly
-- 

SET enable_seqscan=false;
SET enable_bitmapscan=false;
DELETE FROM foo WHERE b >= 64000;
SELECT COUNT(*) FROM foo WHERE a = 0;
