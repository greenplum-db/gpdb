# Test reading an AO-table in REPEATABLE READ mode, with concurrent VACUUM.
#
setup
{
    create table ao (i int, j int) with (appendonly=true);
    insert into ao select i, i from generate_series(0, 99) i;
}

teardown
{
    drop table if exists ao;
}

session "s1"
step "s1delete"	{ delete from ao; }
step "s1vacuum"	{ vacuum ao; }

session "s2"
step "s2begin"	{ BEGIN ISOLATION LEVEL REPEATABLE READ;
		  select 123 as "establish snapshot"; }
step "s2select"	{ select count(*) from ao; }
step "s2insert"	{ insert into ao select i, i from generate_series(0, 99) i; }
teardown	{ abort; }

# 1. Begin repeatable read transaction in session 2.
# 2. Delete all rows and vacuum the table in session 1.
# 3. Select in session 2. This should still see the rows deleted by session 1.
permutation "s2begin" "s1delete" "s1vacuum" "s2select" "s2insert" "s2select"
