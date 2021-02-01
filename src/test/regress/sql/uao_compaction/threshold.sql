-- @Description Tests the basic behavior of (lazy) vacuum w.r.t. to the threshold guc.
CREATE TABLE uao_threshold (a INT, b INT, c CHAR(128)) WITH (appendonly=true) distributed by (b);
CREATE INDEX uao_threshold_index ON uao_threshold(a);
INSERT INTO uao_threshold SELECT i as a, 1 as b, 'hello world' as c FROM generate_series(1, 100) AS i;
ANALYZE uao_threshold;

\set QUIET off

VACUUM uao_threshold;
DELETE FROM uao_threshold WHERE a < 4;
SELECT COUNT(*) FROM uao_threshold;
SELECT segment_id, segno, tupcount, state FROM gp_toolkit.__gp_aoseg('uao_threshold');
-- 97 visible tuples, no vacuum
VACUUM uao_threshold;
SELECT segment_id, segno, tupcount, state FROM gp_toolkit.__gp_aoseg('uao_threshold');
DELETE FROM uao_threshold WHERE a < 12;
SELECT segment_id, segno, tupcount, state FROM gp_toolkit.__gp_aoseg('uao_threshold');
-- 89 visible tuples, do vacuum
VACUUM uao_threshold;
SELECT segment_id, segno, tupcount, state FROM gp_toolkit.__gp_aoseg('uao_threshold');
-- no invisible tuples, no vacuum
VACUUM uao_threshold;
SELECT segment_id, segno, tupcount, state FROM gp_toolkit.__gp_aoseg('uao_threshold');
DELETE FROM uao_threshold WHERE a < 15;
SELECT segment_id, segno, tupcount, state FROM gp_toolkit.__gp_aoseg('uao_threshold');
-- 3 invisible tuples, no vacuum
VACUUM uao_threshold;
SELECT segment_id, segno, tupcount, state FROM gp_toolkit.__gp_aoseg('uao_threshold');
-- set guc to 2%, so vacuum should trigger
SET gp_appendonly_compaction_threshold=2;
-- 3 invisible tuples, do vacuum
VACUUM uao_threshold;
SELECT segment_id, segno, tupcount, state FROM gp_toolkit.__gp_aoseg('uao_threshold');

INSERT INTO uao_threshold SELECT i as a, i as b, 'hello world' as c FROM generate_series(100, 200) AS i;
DELETE FROM uao_threshold WHERE a > 100 and a < 175;
SELECT segment_id, segno, tupcount as tupcount, state FROM gp_toolkit.__gp_aoseg('uao_threshold');
VACUUM uao_threshold;
SELECT segment_id, segno, tupcount as tupcount, state FROM gp_toolkit.__gp_aoseg('uao_threshold');



-- The percentage of hidden tuples should be 10.1%
-- The threshold guc is set to 10%
SET gp_appendonly_compaction_threshold=10;
CREATE TABLE uao_threshold_boundary(a int, b int) WITH(appendonly=TRUE) distributed by(a);
INSERT INTO uao_threshold_boundary SELECT 1, i from generate_series(1, 1000) i;
DELETE FROM uao_threshold_boundary WHERE b < 102;
VACUUM uao_threshold_boundary;
SELECT * FROM gp_toolkit.__gp_aovisimap_compaction_info('uao_threshold_boundary'::regclass);
