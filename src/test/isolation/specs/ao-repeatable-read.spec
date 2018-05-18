# Test reading an AO-table in REPEATABLE READ mode.
#
#
# This is an old repro script for a GPDB bug that was fixed in GPDB 4.1.0.0.
# Explanation from the original bug report follows:
#
# -----
# Try to repro this by running the following steps in two psql sessions:
#
# (1) In session 1,
#
# drop table if exists ao;
# create table ao (i int, j int) with (appendonly=true);
#
# (2) In session 2:
#
# begin TRANSACTION ISOLATION LEVEL REPEATABLE READ;
# select * from ao;
#  i | j
# ---+---
# (0 rows)
#
# (3) In session 1:
#
# insert into ao select i, i from generate_series(0, 99) i;
# select count(*) from ao;
#  count
# -------
#    100
# (1 row)
#
# (4) In session 2:
#
# select * from ao;
#  i | j
# ---+---
# (0 rows)
#
# insert into ao select i, i from generate_series(0, 99) i;
# -- This select used to return '200', i.e. the rows inserted by session 1
# erroneously became visible to us.
# select count(*) from ao;
#  count
# -------
#    200
# (1 row)
#
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
