-- @Description Ensures that an ALTER TABLE ADD COLUMN will drop segfiles in
--              AOSEG_STATE_AWAITING_DROP state left over by a previous vacuum.
--              We removed recycling dead segfiles from ADD COLUMN workflow, so
--              the test expected result were adjusted accordingly.
--
CREATE TABLE aoco_add_column_after_vacuum_skip_drop (a INT, b INT) WITH (appendonly=true, orientation=column);
INSERT INTO aoco_add_column_after_vacuum_skip_drop SELECT i as a, i as b FROM generate_series(1, 10) AS i;

DELETE FROM aoco_add_column_after_vacuum_skip_drop;

-- We should see all aocssegs in state 1
0U: SELECT segno, column_num, state FROM gp_toolkit.__gp_aocsseg('aoco_add_column_after_vacuum_skip_drop');

-- VACUUM while another session holds lock
1: BEGIN;
1: SELECT COUNT(*) FROM aoco_add_column_after_vacuum_skip_drop;
2: VACUUM aoco_add_column_after_vacuum_skip_drop;
1: END;

-- We should see an aocsseg in state 2 (AOSEG_STATE_AWAITING_DROP)
0U: SELECT segno, column_num, state FROM gp_toolkit.__gp_aocsseg('aoco_add_column_after_vacuum_skip_drop');

-- The ADD COLUMN should clean up aocssegs in state 2 (AOSEG_STATE_AWAITING_DROP)
1: ALTER TABLE aoco_add_column_after_vacuum_skip_drop ADD COLUMN c INT DEFAULT 0;
0U: SELECT segno, column_num, state FROM gp_toolkit.__gp_aocsseg('aoco_add_column_after_vacuum_skip_drop');

-- Check if insert goes into segno 1 instead of segno 2
1: INSERT INTO aoco_add_column_after_vacuum_skip_drop SELECT i as a, i as b, i as c FROM generate_series(1, 100) AS i;
0U: SELECT segno, tupcount > 0, state FROM gp_toolkit.__gp_aocsseg('aoco_add_column_after_vacuum_skip_drop');
