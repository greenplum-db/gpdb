# Test behaviour with distributed (repeatable read) snapshots and vacuum freeze.
#
# At highlevel main intent of tests is to validate xmax and xmin correctly gets
# freezed on vacuum freeze. Also, relfrozenxid for pg_class correctly reflects
# lowest XID in the table, all this considering distributed aspect. Individual
# test details are mentioned below.

setup
{
    CREATE TABLE heaptest (i INT, j INT);
    INSERT INTO heaptest SELECT i, i FROM generate_series(0, 9) i;

    -- Helper function to get the xmin, xmax, and infomask bits for each row
    -- in the heaptest table, from every segment. This depends on the
    -- 'pageinspect' contrib module.
    CREATE FUNCTION show_heaptest_flags(
        OUT contentid integer,
        OUT ctid tid,
        OUT t_xmin xid,
        OUT t_xmax xid,
        OUT xmin_kind text,
        OUT xmax_kind text
    ) RETURNS SETOF record AS $$

        select current_setting('gp_contentid')::integer,
	       t_ctid,
	       t_xmin,
	       t_xmax,
	       -- Decode the HEAP_XMIN_* and HEAP_XMAX_* flags.
	       -- NOTE: this doesn't cover all the combinations, just enough
               -- for this test.
	       CASE WHEN (t_infomask & 768) = 768 THEN 'FrozenXid'     -- HEAP_XMIN_FROZEN
	            WHEN (t_infomask & 512) = 512 THEN 'InvalidXid'    -- HEAP_XMIN_INVALID
		    ELSE 'NormalXid' END,
	       CASE WHEN t_xmax = 0 THEN 'InvalidXid' ELSE 'NormalXid' END
	  from heap_page_items(
             CASE WHEN pg_relation_size('heaptest') > 0 THEN get_raw_page('heaptest', 0)
                  ELSE NULL END
          )

     $$ LANGUAGE sql EXECUTE ON ALL SEGMENTS;

    -- show the contents of heaptest table, including invisible rows,
    -- and the xmin/xmax status of each row.
    CREATE VIEW show_heaptest_content AS
      SELECT v.xmin_kind, v.xmax_kind, heaptest.*
        FROM show_heaptest_flags() v
        FULL OUTER JOIN heaptest ON heaptest.gp_segment_id = v.contentid AND
        heaptest.ctid = v.ctid;
}

teardown
{
    DROP VIEW show_heaptest_content;
    DROP TABLE IF EXISTS heaptest;
    DROP FUNCTION show_heaptest_flags();
}

session "s1"
step "s1insert" { SET Debug_print_full_dtm=on; INSERT INTO heaptest SELECT i, i FROM generate_series(10, 19) i; }
step "s1delete"	{ DELETE FROM heaptest; }
step "s1setfreezeminage" { SET vacuum_freeze_min_age = 0; }
step "s1vacuumfreeze" { VACUUM heaptest; }
step "s1select" { SELECT COUNT(*) FROM heaptest; }

session "s2"
step "s2begin" { SET Debug_print_full_dtm=on; BEGIN ISOLATION LEVEL REPEATABLE READ;
		 SELECT 123 AS "establish snapshot"; }
step "s2select" {
    SELECT * from show_heaptest_content ORDER BY i;
}
step "s2abort" { ABORT; }

session "s3"
step "s3select" {
    SELECT * from show_heaptest_content ORDER BY i;
}

session "s4"
step "s4begin"  { SET Debug_print_full_dtm=on; BEGIN; }
step "s4delete" { DELETE FROM heaptest; }
step "s4abort"  { ABORT; }

# Intent of this permutation is to validate xmax of deleted tuple is not freezed
# (marked invalid) after HeapTupleSatisfiesVacuum() returns tuple cannot be
# deleted based on global transaction state.
#
# Session 2 begins a repeatable read transaction, but it doesn't immediately do any
# command in that transaction that would acquire snapshot on QEs. Hence,
# GetSnapshotData() isn't called in the QEs for session 2 the "s2select"
# stage. Meanwhile, session 1 deletes rows from the table that due to session
# 2's existence will not be deleted by vacuum. But if there is no mechanism to
# validate the global transaction state while freezing, it will merrily mark
# xmax as invalid and make the deleted tuple visible to everyone. Also, make
# sure relfrozenxid in pg_class after vacuum freeze is not set to freezelimit
# locally but actually reflects the lowest xmax it was able to freeze till.
permutation "s2begin" "s1delete" "s1setfreezeminage" "s1vacuumfreeze" "s2select" "s1select" "s2abort" "s3select" "s1vacuumfreeze" "s3select"

# Intent of this permutation is to validate xmax freezing is correctly
# continuing to work, when deleting transaction is aborted.
#
# So, its pretty simple sequence, start transaction, delete and abort. Then
# vacuum freeze and check if xmax was freezed means set to invalidXid.
permutation "s4begin" "s4delete" "s4abort" "s1setfreezeminage" "s1vacuumfreeze" "s2select"

# Intent of this permutation is to validate xmin of tuple is not freezed
# incorrectly just because its lower than local lowest xmin for that segment but
# takes into account distributed nature and avoids marking it as frozen and
# making it visible to all the distributed transactions, if some distributed
# transaction still can't see it.
#
# Session 2 begins a repeatable read transaction, but it doesn't immediately do any
# command in that transaction that would acquire snapshot on the QEs. Hence,
# GetSnapshotData() isn't called in the QEs for session 2 the "s2select"
# stage. Meanwhile, session 1 inserts more rows to the table and then performs
# vacuum freeze. These additional rows, due to session 2's existence will not be
# freezed. Session 2 as started earlier than session1 shouldn't see these
# additional tuples after vacuum freeze. Also, make sure relfrozenxid in
# pg_class after vacuum freeze is not set to freezelimit locally but actually
# reflects the lowest xmin it was able to freeze till.
permutation "s2begin" "s1insert" "s1setfreezeminage" "s1vacuumfreeze" "s2select" "s1select" "s2abort" "s2select" "s3select" "s1vacuumfreeze" "s2select" "s3select"
