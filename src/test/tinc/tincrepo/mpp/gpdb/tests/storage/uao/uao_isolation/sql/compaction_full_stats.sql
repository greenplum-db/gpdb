-- @Description Tests the behavior of full vacuum w.r.t. the pg_class statistics
-- 

-- ensure that the scan go through the index
SET enable_seqscan=false;
SELECT relname, reltuples FROM pg_class WHERE relname = 'foo';
SELECT relname, reltuples FROM pg_class WHERE relname = 'foo_index';
DELETE FROM foo WHERE a < 16;
VACUUM FULL foo;
SELECT relname, reltuples FROM pg_class WHERE relname = 'foo';
SELECT relname, reltuples FROM pg_class WHERE relname = 'foo_index';
SELECT segno, tupcount,state FROM gp_aoseg_name('foo');
