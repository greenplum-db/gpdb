-- Ensure all segfiles crossing vacuum threshold but cannot compact
-- due to concurrent transaction should be marked in state
-- AOSEG_STATE_AWAITING_DROP.

CREATE TABLE mark_all_aoseg_await_drop (a int) WITH (appendonly=true);

-- Create 3 aoseg entries
0: BEGIN;
1: BEGIN;
2: BEGIN;
0: INSERT INTO mark_all_aoseg_await_drop SELECT i FROM generate_series(1, 10)i;
1: INSERT INTO mark_all_aoseg_await_drop SELECT i FROM generate_series(1, 10)i;
2: INSERT INTO mark_all_aoseg_await_drop SELECT i FROM generate_series(1, 10)i;
0: COMMIT;
1: COMMIT;
2: COMMIT;

DELETE FROM mark_all_aoseg_await_drop;

-- We should see all 3 aosegs in state 1
0U: SELECT segno, state FROM gp_toolkit.__gp_aoseg('mark_all_aoseg_await_drop');

-- VACUUM while another session holds lock
1: BEGIN;
1: SELECT COUNT(*) FROM mark_all_aoseg_await_drop;
1: INSERT INTO mark_all_aoseg_await_drop SELECT i FROM generate_series(1, 10)i;
2: VACUUM mark_all_aoseg_await_drop;
1: END;

-- We should see segno 2 and 3 in state 2 (AOSEG_STATE_AWAITING_DROP)
-- and segno 1 and 4 in state 1 (AOSEG_STATE_DEFAULT). Segno 1 is not
-- marked as state 2 because a concurrent transaction has written to
-- it and did not commit before the vacuum got to it. The vacuum
-- correctly skipped segno 1 and continued to cycle through the aoseg
-- entries.
0U: SELECT segno, state FROM gp_toolkit.__gp_aoseg('mark_all_aoseg_await_drop');
