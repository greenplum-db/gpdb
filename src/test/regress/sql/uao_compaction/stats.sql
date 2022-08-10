-- @Description Tests that the pg_class statistics are updated on
-- lazy vacuum.

CREATE TABLE uao_stats (a INT, b INT, c CHAR(128)) WITH (appendonly=true) DISTRIBUTED BY (a);
CREATE INDEX uao_stats_index ON uao_stats(b);
INSERT INTO uao_stats SELECT i as a, i as b, 'hello world' as c FROM generate_series(1, 50) AS i;
INSERT INTO uao_stats SELECT i as a, i as b, 'hello world' as c FROM generate_series(51, 100) AS i;
ANALYZE uao_stats;

-- ensure that the scan go through the index
SET enable_seqscan=false;
SELECT relname, reltuples FROM pg_class WHERE relname = 'uao_stats';
SELECT relname, reltuples FROM pg_class WHERE relname = 'uao_stats_index';
DELETE FROM uao_stats WHERE a < 16;

-- New strategy of VACUUM AO/CO was introduced by PR #13255 for performance enhancement.
-- Index dead tuples will not always be cleaned up completely after VACUUM, leading to
-- index->reltuples not always equal to table->reltuples, it depends on the GUC
-- gp_appendonly_compaction_threshold:

-- a) without changing gp_appendonly_compaction_threshold (default is 10), index->reltuples
-- might greater than table->reltuples if no compaction was triggered during VACUUM;
VACUUM uao_stats;
SELECT relname, reltuples FROM pg_class WHERE relname = 'uao_stats';
-- expect index->reltuples greater than table->reltuples
SELECT relname, reltuples FROM pg_class WHERE relname = 'uao_stats_index';

-- re-setup for next case
TRUNCATE uao_stats;
INSERT INTO uao_stats SELECT i as a, i as b, 'hello world' as c FROM generate_series(1, 50) AS i;
INSERT INTO uao_stats SELECT i as a, i as b, 'hello world' as c FROM generate_series(51, 100) AS i;
ANALYZE uao_stats;

DELETE FROM uao_stats WHERE a < 16;
-- b) changing gp_appendonly_compaction_threshold to make sure compaction could be triggered,
-- index->reltuples should be equal to table->reltuples
SET gp_appendonly_compaction_threshold = 8;
VACUUM uao_stats;
SELECT relname, reltuples FROM pg_class WHERE relname = 'uao_stats';
-- expect index->reltuples equals to table->reltuples
SELECT relname, reltuples FROM pg_class WHERE relname = 'uao_stats_index';
