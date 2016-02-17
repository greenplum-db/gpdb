-- @Description Tests the basic behavior of (lazy) vacuum w.r.t. to the threshold guc.
-- The output depends on the number of segments as the skip decision and the 
-- notify is done on the segments.

CREATE TABLE uao_threshold (a INT, b INT, c CHAR(128)) WITH (appendonly=true);
CREATE INDEX uao_threshold_index ON uao_threshold(b);
INSERT INTO uao_threshold SELECT i as a, 1 as b, 'hello world' as c FROM generate_series(1, 100) AS i;

\set QUIET off

VACUUM uao_threshold;
DELETE FROM uao_threshold WHERE a < 4;
SELECT COUNT(*) FROM uao_threshold;
SELECT segno, tupcount, state FROM gp_toolkit.__gp_aoseg_name('uao_threshold');
-- 97 visible tuples, no vacuum
VACUUM uao_threshold;
SELECT segno, tupcount, state FROM gp_toolkit.__gp_aoseg_name('uao_threshold');
DELETE FROM uao_threshold WHERE a < 12;
SELECT segno, tupcount, state FROM gp_toolkit.__gp_aoseg_name('uao_threshold');
-- 89 visible tuples, do vacuum
VACUUM uao_threshold;
SELECT segno, tupcount, state FROM gp_toolkit.__gp_aoseg_name('uao_threshold');
-- no invisible tuples, no vacuum
VACUUM uao_threshold;
SELECT segno, tupcount, state FROM gp_toolkit.__gp_aoseg_name('uao_threshold');
DELETE FROM uao_threshold WHERE a < 15;
SELECT segno, tupcount, state FROM gp_toolkit.__gp_aoseg_name('uao_threshold');
-- 3 invisible tuples, no vacuum
VACUUM uao_threshold;
SELECT segno, tupcount, state FROM gp_toolkit.__gp_aoseg_name('uao_threshold');
SET gp_appendonly_compaction_threshold=2;
-- 3 invisible tuples, no vacuum
VACUUM uao_threshold;
SELECT segno, tupcount, state FROM gp_toolkit.__gp_aoseg_name('uao_threshold');
