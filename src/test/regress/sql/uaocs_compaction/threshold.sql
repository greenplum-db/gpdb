-- @Description Tests the basic behavior of (lazy) vacuum w.r.t. to the threshold guc.
CREATE TABLE uaocs_threshold (a INT, b INT, c CHAR(128)) WITH (appendonly=true, orientation=column) distributed by (b);
CREATE INDEX uaocs_threshold_index ON uaocs_threshold(a);
INSERT INTO uaocs_threshold SELECT i as a, 1 as b, 'hello world' as c FROM generate_series(1, 100) AS i;
ANALYZE uaocs_threshold;

\set QUIET off
VACUUM uaocs_threshold;
DELETE FROM uaocs_threshold WHERE a < 4;
SELECT COUNT(*) FROM uaocs_threshold;
SELECT DISTINCT segno, tupcount, state FROM gp_toolkit.__gp_aocsseg('uaocs_threshold');
-- 97 visible tuples, no vacuum
VACUUM uaocs_threshold;
SELECT DISTINCT segno, tupcount, state FROM gp_toolkit.__gp_aocsseg('uaocs_threshold');
DELETE FROM uaocs_threshold WHERE a < 12;
SELECT DISTINCT segno, tupcount, state FROM gp_toolkit.__gp_aocsseg('uaocs_threshold');
-- 89 visible tuples, do vacuum
VACUUM uaocs_threshold;
SELECT DISTINCT segno, tupcount, state FROM gp_toolkit.__gp_aocsseg('uaocs_threshold');
-- no invisible tuples, no vacuum
VACUUM uaocs_threshold;
SELECT DISTINCT segno, tupcount, state FROM gp_toolkit.__gp_aocsseg('uaocs_threshold');
DELETE FROM uaocs_threshold WHERE a < 15;
SELECT DISTINCT segno, tupcount, state FROM gp_toolkit.__gp_aocsseg('uaocs_threshold');
-- 3 invisible tuples, no vacuum
VACUUM uaocs_threshold;
SELECT DISTINCT segno, tupcount, state FROM gp_toolkit.__gp_aocsseg('uaocs_threshold');
SET gp_appendonly_compaction_threshold=2;
-- 3 invisible tuples, no vacuum
VACUUM uaocs_threshold;
SELECT DISTINCT segno, tupcount, state FROM gp_toolkit.__gp_aocsseg('uaocs_threshold');

INSERT INTO uaocs_threshold SELECT i as a, i as b, 'hello world' as c FROM generate_series(100, 200) AS i;
DELETE FROM uaocs_threshold WHERE a > 100 and a < 175;
SELECT DISTINCT segno, tupcount, state FROM gp_toolkit.__gp_aocsseg('uaocs_threshold');
VACUUM uaocs_threshold;
SELECT DISTINCT segno, tupcount, state FROM gp_toolkit.__gp_aocsseg('uaocs_threshold');

-- The percentage of hidden tuples should be 10.1%
-- The threshold guc is set to 10%
SET gp_appendonly_compaction_threshold=10;
CREATE TABLE uaocs_threshold_boundary(a int, b int) WITH(appendonly=TRUE, orientation=COLUMN) distributed by(a);
INSERT INTO uaocs_threshold_boundary SELECT 1, i from generate_series(1, 1000) i;
DELETE FROM uaocs_threshold_boundary WHERE b < 102;
VACUUM uaocs_threshold_boundary;
SELECT * FROM gp_toolkit.__gp_aovisimap_compaction_info('uaocs_threshold_boundary'::regclass);
