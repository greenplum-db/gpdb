# Test reading a heap table in REPEATABLE READ mode.
setup
{
    create table heap (i int, j int);
}

teardown
{
    drop table if exists heap;
}

session "s1"
step "s1insert"	{ insert into heap select i, i from generate_series(0, 99) i; }
step "s1select"	{ select count(*) from heap; }

session "s2"
step "s2begin"	{ BEGIN ISOLATION LEVEL REPEATABLE READ; }
step "s2select"	{ select count(*) from heap; }
step "s2insert"	{ insert into heap select i, i from generate_series(0, 99) i; }
teardown	{ abort; }

permutation "s2begin" "s2select" "s1insert" "s1select" "s2select" "s2insert" "s2select"
