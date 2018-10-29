-- Ensure segfiles in AOSEG_STATE_AWAITING_DROP are not leaked in
-- AppendOnlyHash after doing a TRUNCATE.

CREATE TABLE truncate_after_ao_vacuum_skip_drop (a INT, b INT) WITH (appendonly=true);
INSERT INTO truncate_after_ao_vacuum_skip_drop SELECT i as a, i as b FROM generate_series(1, 10) AS i;

DELETE FROM truncate_after_ao_vacuum_skip_drop;

-- We should see all aosegs in state 1
0U: SELECT segno, state FROM gp_toolkit.__gp_aoseg('truncate_after_ao_vacuum_skip_drop');

-- VACUUM while another session holds lock
1: BEGIN;
1: SELECT COUNT(*) FROM truncate_after_ao_vacuum_skip_drop;
2: VACUUM truncate_after_ao_vacuum_skip_drop;
1: END;

-- We should see an aoseg in state 2 (AOSEG_STATE_AWAITING_DROP)
0U: SELECT segno, state FROM gp_toolkit.__gp_aoseg('truncate_after_ao_vacuum_skip_drop');

-- The AO relation should be rewritten and AppendOnlyHash entry invalidated
1: TRUNCATE truncate_after_ao_vacuum_skip_drop;
0U: SELECT segno, state FROM gp_toolkit.__gp_aoseg('truncate_after_ao_vacuum_skip_drop');

-- Check if insert goes into segno 1 instead of segno 2. If it did not
-- go into segno 1, there was a leak in the AppendOnlyHash entry.
1: INSERT INTO truncate_after_ao_vacuum_skip_drop SELECT i as a, i as b FROM generate_series(1, 100) AS i;
0U: SELECT segno, tupcount > 0, state FROM gp_toolkit.__gp_aoseg('truncate_after_ao_vacuum_skip_drop');
