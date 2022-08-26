-- @Description Tests the behavior of full vacuum w.r.t. the pg_class statistics

CREATE TABLE uao_full_stats (a INT, b INT, c CHAR(128)) WITH (appendonly=true) DISTRIBUTED BY (a);
CREATE INDEX uao_full_stats_index ON uao_full_stats(b);
INSERT INTO uao_full_stats SELECT i as a, i as b, 'hello world' as c FROM generate_series(1, 50) AS i;
INSERT INTO uao_full_stats SELECT i as a, i as b, 'hello world' as c FROM generate_series(51, 100) AS i;
ANALYZE uao_full_stats;

-- ensure that the scan go through the index
SET enable_seqscan=false;
SELECT relname, reltuples FROM pg_class WHERE relname = 'uao_full_stats';
SELECT relname, reltuples FROM pg_class WHERE relname = 'uao_full_stats_index';
DELETE FROM uao_full_stats WHERE a < 16;
VACUUM FULL uao_full_stats;
SELECT relname, reltuples FROM pg_class WHERE relname = 'uao_full_stats';
-- Compaction is always triggered in VACUUM FULL, expecting
-- reltuples of both table and index are equal.
SELECT relname, reltuples FROM pg_class WHERE relname = 'uao_full_stats_index';
