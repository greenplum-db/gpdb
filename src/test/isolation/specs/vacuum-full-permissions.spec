# Test validates VACUUM FULL is not blocked on a table
# that the role doesn't have permission to vacuum.

setup
{
	CREATE ROLE testrole_vacuum_full;
}

teardown
{
	DROP ROLE testrole_vacuum_full;
}

session "s1"
step "s1_in"	{ SET gp_select_invisible=on; }
step "s1_ct"	{ CREATE TABLE testtable_vacuum_full_to_skip AS SELECT generate_series(1,10); }
step "s1_d"	{ DELETE FROM testtable_vacuum_full_to_skip; }
step "s1_r"	{ SELECT COUNT(*) FROM testtable_vacuum_full_to_skip; }
step "s1_b"	{ BEGIN; }
step "s1_l"	{ LOCK TABLE pg_database IN ACCESS SHARE MODE; }

teardown
{
	END;
	DROP TABLE testtable_vacuum_full_to_skip;
	RESET gp_select_invisible;
}

session "s2"
step "s2_sr"	{ SET ROLE testrole_vacuum_full; }
step "s2_in"	{ SET gp_select_invisible=on; }
step "s2_sw"	{ SET client_min_messages TO ERROR; }
step "s2_ct"	{ CREATE TABLE testtable_vacuum_full AS SELECT generate_series(1,10); }
step "s2_r"	{ SELECT COUNT(*) FROM testtable_vacuum_full; }
step "s2_d"	{ DELETE FROM testtable_vacuum_full; }
step "s2_vf"	{ VACUUM FULL; }

teardown
{
	DROP TABLE testtable_vacuum_full;
	RESET client_min_messages;
	RESET gp_select_invisible;
}

# This permutation asserts that s2, running VACUUM FULL, is not blocked on s1 which has an active lock on catalog table.
# s2 should skip catalog tables as well as user tables that it doesn't have access rights to.
# To check if a table has been vacuumed we ensure that no dead rows are returned on a select with gp_select_invisible=on.
permutation "s2_sr" "s1_in" "s2_in" "s1_ct" "s2_ct" "s2_r" "s1_r" "s1_d" "s2_d" "s2_r" "s1_r" "s1_b" "s1_l" "s2_sw" "s2_vf" "s2_r" "s1_r"
