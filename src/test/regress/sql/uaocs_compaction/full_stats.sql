-- @Description Tests the behavior of full vacuum w.r.t. the pg_class statistics
-- ensure that the scan go through the index
CREATE TABLE uaocs_full_stats (a INT, b INT, c CHAR(128)) WITH (appendonly=true, orientation=column) DISTRIBUTED BY (a);
CREATE INDEX uaocs_full_stats_index ON uaocs_full_stats(b);
INSERT INTO uaocs_full_stats SELECT i as a, i as b, 'hello world' as c FROM generate_series(1, 50) AS i;
INSERT INTO uaocs_full_stats SELECT i as a, i as b, 'hello world' as c FROM generate_series(51, 100) AS i;
ANALYZE uaocs_full_stats;

SET enable_seqscan=false;
SELECT COUNT(*) FROM uaocs_full_stats;
SELECT relname, reltuples FROM pg_class WHERE relname = 'uaocs_full_stats';
SELECT relname, reltuples FROM pg_class WHERE relname = 'uaocs_full_stats_index';
DELETE FROM uaocs_full_stats WHERE a < 16;
SELECT COUNT(*) FROM uaocs_full_stats;
VACUUM FULL uaocs_full_stats;
SELECT COUNT(*) FROM uaocs_full_stats;
SELECT relname, reltuples FROM pg_class WHERE relname = 'uaocs_full_stats';
-- Compaction is always triggered in VACUUM FULL, expecting
-- reltuples of both table and index are equal.
SELECT relname, reltuples FROM pg_class WHERE relname = 'uaocs_full_stats_index';
