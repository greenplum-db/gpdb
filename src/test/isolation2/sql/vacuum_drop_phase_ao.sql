-- @Description Assert that QEs don't skip a vacuum drop phase (unless we have
-- an abort) and thus guarantees that seg file states are consistent across QD/QE.

-- Given we have an AO table
1: CREATE TABLE ao_test_drop_phase (a INT, b INT) WITH (appendonly=true);
-- And the AO table has all tuples on primary with content = 0
1: INSERT INTO ao_test_drop_phase SELECT 2,i from generate_series(1, 5)i;

-- We should see 1 pg_aoseg catalog table tuple in state 1 (AVAILABLE) for
-- segno = 1
0U: SELECT * FROM gp_toolkit.__gp_aoseg('ao_test_drop_phase');

-- And with a utility mode session on the primary with content = 0, we simulate
-- an access shared lock that would exist on the QE but not on the QD.
0U: BEGIN;
0U: SELECT COUNT(*) FROM ao_test_drop_phase;

-- And we delete 4/5 rows to trigger vacuum's compaction phase.
1: DELETE FROM ao_test_drop_phase where b != 5;
-- We should see that VACUUM blocks while the QE holds the access shared lock
1&: VACUUM ao_test_drop_phase;

0U: END;
1<:

-- We should see that the one visible tuple left after the DELETE gets compacted
-- from segno = 1 to segno = 2.
-- Also, segno = 1 should be empty and in state 1 (AVAILABLE)
0U: SELECT * FROM gp_toolkit.__gp_aoseg('ao_test_drop_phase');

-- We should see that the QD's hash table matches content = 0's pg_aoseg catalog
1: SELECT segno, total_tupcount, state
FROM gp_toolkit.__gp_get_ao_entry_from_cache('ao_test_drop_phase'::regclass::oid)
WHERE segno IN (1, 2);

-- We should see that a subsequent insert succeeds and lands on segno = 1
1: INSERT INTO ao_test_drop_phase SELECT 2,i from generate_series(11, 15)i;
0U: SELECT * FROM gp_toolkit.__gp_aoseg('ao_test_drop_phase');

1: SELECT * FROM ao_test_drop_phase;
