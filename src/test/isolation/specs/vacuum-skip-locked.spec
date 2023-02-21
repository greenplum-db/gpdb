# Test for SKIP_LOCKED option of VACUUM and ANALYZE commands.
#
# This also verifies that log messages are not emitted for skipped relations
# that were not specified in the VACUUM or ANALYZE command.

setup
{
	CREATE TABLE parted (a INT) PARTITION BY LIST (a);
	CREATE TABLE part1 PARTITION OF parted FOR VALUES IN (1);
	ALTER TABLE part1 SET (autovacuum_enabled = false);
	CREATE TABLE part2 PARTITION OF parted FOR VALUES IN (2);
	ALTER TABLE part2 SET (autovacuum_enabled = false);

	CREATE TABLE parted_ao (a INT) using ao_column
		distributed by (a) PARTITION BY LIST (a);
	CREATE TABLE part1_ao PARTITION OF parted_ao FOR VALUES IN (1);
	CREATE TABLE part2_ao PARTITION OF parted_ao FOR VALUES IN (2);
}

teardown
{
	DROP TABLE IF EXISTS parted;
	DROP TABLE IF EXISTS parted_ao;
}

session s1
step lock_share
{
	BEGIN;
	LOCK part1 IN SHARE MODE;
}
step lock_access_exclusive
{
	BEGIN;
	LOCK part1 IN ACCESS EXCLUSIVE MODE;
}
step commit
{
	COMMIT;
}
step "lock_share_ao"
{
	BEGIN;
	LOCK part1_ao IN SHARE MODE;
}
step "lock_access_exclusive_ao"
{
	BEGIN;
	LOCK part1_ao IN ACCESS EXCLUSIVE MODE;
}

session s2
step vac_specified			{ VACUUM (SKIP_LOCKED) part1, part2; }
step vac_all_parts			{ VACUUM (SKIP_LOCKED) parted; }
step analyze_specified		{ ANALYZE (SKIP_LOCKED) part1, part2; }
step analyze_all_parts		{ ANALYZE (SKIP_LOCKED) parted; }
step vac_analyze_specified	{ VACUUM (ANALYZE, SKIP_LOCKED) part1, part2; }
step vac_analyze_all_parts	{ VACUUM (ANALYZE, SKIP_LOCKED) parted; }
step vac_full_specified		{ VACUUM (SKIP_LOCKED, FULL) part1, part2; }
step vac_full_all_parts		{ VACUUM (SKIP_LOCKED, FULL) parted; }
step vac_specified_ao		{ VACUUM (SKIP_LOCKED) part1_ao, part2_ao; }
step vac_all_parts_ao		{ VACUUM (SKIP_LOCKED) parted_ao; }
step analyze_specified_ao	{ ANALYZE (SKIP_LOCKED) part1_ao, part2_ao; }
step analyze_all_parts_ao	{ ANALYZE (SKIP_LOCKED) parted_ao; }
step vac_analyze_specified_ao	{ VACUUM (ANALYZE, SKIP_LOCKED) part1_ao, part2_ao; }
step vac_analyze_all_parts_ao	{ VACUUM (ANALYZE, SKIP_LOCKED) parted_ao; }
step vac_full_specified_ao	{ VACUUM (SKIP_LOCKED, FULL) part1_ao, part2_ao; }
step vac_full_all_parts_ao	{ VACUUM (SKIP_LOCKED, FULL) parted_ao; }

permutation lock_share vac_specified commit
permutation lock_share vac_all_parts commit
permutation lock_share analyze_specified commit
permutation lock_share analyze_all_parts commit
permutation lock_share vac_analyze_specified commit
permutation lock_share vac_analyze_all_parts commit
permutation lock_share vac_full_specified commit
permutation lock_share vac_full_all_parts commit
permutation lock_access_exclusive vac_specified commit
permutation lock_access_exclusive vac_all_parts commit
permutation lock_access_exclusive analyze_specified commit
# GPDB: This blocks in GPDB, because the ANALYZE tries to acquire inherited
# sample on segments, which blocks. We consider that OK, even though it's
# different from upstream. Even in PostgreSQL, the documentation for
# SKIP_LOCKED says that it may still block if it needs to acquire sample
# rows from the partitions.
#permutation lock_access_exclusive analyze_all_parts commit
permutation lock_access_exclusive vac_analyze_specified commit
#permutation lock_access_exclusive vac_analyze_all_parts commit
permutation lock_access_exclusive vac_full_specified commit
permutation lock_access_exclusive vac_full_all_parts commit

permutation lock_share_ao vac_specified_ao commit
permutation lock_share_ao vac_all_parts_ao commit
permutation lock_share_ao analyze_specified_ao commit
permutation lock_share_ao analyze_all_parts_ao commit
permutation lock_share_ao vac_analyze_specified_ao commit
permutation lock_share_ao vac_analyze_all_parts_ao commit
permutation lock_share_ao vac_full_specified_ao commit
permutation lock_share_ao vac_full_all_parts_ao commit
permutation lock_access_exclusive_ao vac_specified_ao commit
permutation lock_access_exclusive_ao vac_all_parts_ao commit
permutation lock_access_exclusive_ao analyze_specified_ao commit
permutation lock_access_exclusive_ao vac_analyze_specified_ao commit
permutation lock_access_exclusive_ao vac_full_specified_ao commit
permutation lock_access_exclusive_ao vac_full_all_parts_ao commit
