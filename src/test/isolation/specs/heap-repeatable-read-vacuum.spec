# Test behaviour with distributed (repeatable read) snapshots and vacuum.
#
# The crux of this test is that session 2 begins a repeatable read
# transaction, but it doesn't immediately do any actions in that transaction
# that would open the connections to the QEs. Hence, GetSnapshotData() isn't
# called in the QEs for session 2 the "s2select" stage. Meanwhile, session 1
# deletes rows from the table that session 2 later needs to see, and runs
# vacuum. If there is no mechanism to prevent the global "oldest xmin" from
# advancing, it will merrily vacuum away tuples that session 2 needs to see
# later.
#
# In a single-node system, the dummy "establish snapshot" query will set
# MyProc->xmin and prevent the tuples it needs to see later from being
# vacuumed away. In MPP, the dummy query establishes a distributed snapshot,
# but that doesn't immediately set the xmin values in QEs.
setup
{
    create table heaptest (i int, j int);
    insert into heaptest select i, i from generate_series(0, 99) i;
}

teardown
{
    drop table if exists heaptest;
}

session "s1"
step "s1begin"  { begin; }
step "s1delete"	{ delete from heaptest; }
step "s1commit"  { commit; }

step "s1vacuum"	{ vacuum heaptest; }

session "s2"
step "s2begin"	{ BEGIN ISOLATION LEVEL REPEATABLE READ;
		  select 123 as "establish snapshot"; }
step "s2select"	{ select count(*) from heaptest; }
teardown	{ abort; }

permutation "s1begin" "s1delete" "s2begin" "s1commit" "s1vacuum" "s2select"
