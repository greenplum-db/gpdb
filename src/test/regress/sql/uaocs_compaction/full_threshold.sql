-- @Description Tests that that full vacuum is ignoring the threshold guc value.
CREATE TABLE uaocs_full_threshold (a INT, b INT, c CHAR(128)) WITH (appendonly=true, orientation=column) DISTRIBUTED BY (a);
CREATE INDEX uaocs_full_threshold_index ON uaocs_full_threshold(b);
INSERT INTO uaocs_full_threshold SELECT i as a, 1 as b, 'hello world' as c FROM generate_series(1, 100) AS i;
ANALYZE uaocs_full_threshold;

\set QUIET off
VACUUM FULL uaocs_full_threshold;
DELETE FROM uaocs_full_threshold WHERE a < 4;
SET gp_appendonly_compaction_threshold=100;
VACUUM FULL uaocs_full_threshold;
