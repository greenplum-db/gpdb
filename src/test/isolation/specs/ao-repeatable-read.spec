# Test reading an AO-table in REPEATABLE READ mode.

setup
{
    create table ao (i int, j int) with (appendonly=true);
}

teardown
{
    drop table if exists ao;
}

session "s1"
step "s1insert"	{ insert into ao select i, i from generate_series(0, 99) i; }
step "s1select"	{ select count(*) from ao; }

session "s2"
step "s2begin"	{ BEGIN ISOLATION LEVEL REPEATABLE READ; }
step "s2select"	{ select count(*) from ao; }
step "s2insert"	{ insert into ao select i, i from generate_series(0, 99) i; }
teardown	{ abort; }

permutation "s2begin" "s2select" "s1insert" "s1select" "s2select" "s2insert" "s2select"
