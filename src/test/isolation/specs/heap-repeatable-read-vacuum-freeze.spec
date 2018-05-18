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
}

teardown
{
    DROP TABLE IF EXISTS heaptest;
}

session "s1"
step "s1insert" { INSERT INTO heaptest SELECT i, i FROM generate_series(10, 19) i; }
step "s1delete"	{ DELETE FROM heaptest; }
step "s1setfreezeminage" { SET vacuum_freeze_min_age = 0; }
step "s1vacuumfreeze" { VACUUM heaptest; }
step "s1select" { SELECT COUNT(*) FROM heaptest; }

session "s2"
step "s2begin" { BEGIN ISOLATION LEVEL REPEATABLE READ;
		 SELECT 123 AS "establish snapshot"; }
step "s2select" {
    SELECT CASE 
         WHEN xmin = 2 THEN 'FrozenXid' 
         ELSE 'NormalXid' 
       END AS xmin, 
       CASE 
         WHEN xmax = 0 THEN 'InvalidXid' 
         ELSE 'NormalXid' 
       END AS xmax, 
       * 
    FROM heaptest 
    ORDER BY i;
}
step "s2abort" { ABORT; }

session "s3"
step "s3selectinvisible" { 
    SET gp_select_invisible=TRUE;

    SELECT CASE 
         WHEN xmin = 2 THEN 'FrozenXid' 
         ELSE 'NormalXid' 
       END AS xmin, 
       CASE 
         WHEN xmax = 0 THEN 'InvalidXid' 
         ELSE 'NormalXid' 
       END AS xmax, 
       * 
    FROM heaptest 
    ORDER BY i;
}
step "s3checkrelfrozenxidwithxmin" {
    SELECT *
    FROM   (SELECT gp_segment_id,
	    Min(Int4in(Xidout(xmin))) AS xmin
	    FROM   heaptest
	    WHERE  Int4in(Xidout(xmin)) > 2
	    GROUP  BY gp_segment_id) tb
    JOIN (SELECT gp_segment_id,
	  Int4in(Xidout(relfrozenxid)) AS relfrozenxid
	  FROM   Gp_dist_random('pg_class')
	  WHERE  relname = 'heaptest') pg
    ON ( tb.gp_segment_id = pg.gp_segment_id )
    WHERE  ( xmin < relfrozenxid )
}
step "s3checkrelfrozenxidwithxmax" {
    SELECT *
    FROM   (SELECT gp_segment_id,
	    Min(Int4in(Xidout(xmax))) AS xmax
	    FROM   heaptest
	    WHERE  Int4in(Xidout(xmax)) != 0
	    GROUP  BY gp_segment_id) tb
    JOIN (SELECT gp_segment_id,
	  Int4in(Xidout(relfrozenxid)) AS relfrozenxid
	  FROM   Gp_dist_random('pg_class')
	  WHERE  relname = 'heaptest') pg
    ON ( tb.gp_segment_id = pg.gp_segment_id )
    WHERE  ( xmax < relfrozenxid )
}

session "s4"
step "s4begin"  { BEGIN; }
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
permutation "s2begin" "s1delete" "s1setfreezeminage" "s1vacuumfreeze" "s2select" "s1select" "s2abort" "s3selectinvisible" "s3checkrelfrozenxidwithxmax" "s1vacuumfreeze" "s3selectinvisible" "s3checkrelfrozenxidwithxmax"

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
permutation "s2begin" "s1insert" "s1setfreezeminage" "s1vacuumfreeze" "s2select" "s1select" "s2abort" "s2select" "s3checkrelfrozenxidwithxmin" "s1vacuumfreeze" "s2select" "s3checkrelfrozenxidwithxmin"
