-- Test to validate bug fix for vacuum full and distributed snapshot.
--
-- Scenarios is where locally on segment tuple's xmin and xmax is lower than
-- OldestXmin and hence safely can be declared DEAD but based on distributed
-- snapshot HeapTupleSatisfiesVacuum() returns HEAPTUPLE_RECENTLY_DEAD. Later
-- though while moving the tuples around as part of vacuum, distributed snapshot
-- was not consulted but instead xmin was only checked against OldestXmin
-- raising the "ERROR: updated tuple is already HEAP_MOVED_OFF".
create table test_recently_dead_utility(a int, b int, c text);
-- Insert enough data that it doesn't all fit in one page.
insert into test_recently_dead_utility select 1, g, 'foobar' from generate_series(1, 1000) g;
-- Perform updates to form update chains and deleted tuples.
update test_recently_dead_utility set b = 1;
update test_recently_dead_utility set b = 1;

-- Run VACUUM FULL in utility mode. It sees some of the old, updated, tuple
-- versions as HEAPTUPLE_RECENTLY_DEAD, even though they are safely dead because
-- localXidSatisfiesAnyDistributedSnapshot() is conservative and returns 'true'
-- in utility mode. Helps to validate doesn't surprise the chain-following logic
-- in VACUUM FULL.
2U: vacuum full verbose test_recently_dead_utility;
2U: select count(*) from test_recently_dead_utility;
2U: set gp_select_invisible=1;
-- print to make sure deleted tuples were not cleaned due to distributed
-- snapshot to make test is future proof, if logic in
-- localXidSatisfiesAnyDistributedSnapshot() changes for utility mode.
2U: select count(*) from test_recently_dead_utility;
2U: set gp_select_invisible=0;

-- If gp_disable_dtx_visibility_check is set, all tuples should be vacuumed.
create table test_recently_dead_utility2(a int, b int, c text);
insert into test_recently_dead_utility2 select 1, g, 'foobar' from generate_series(1, 1000) g;
delete from test_recently_dead_utility2;

2U: set gp_select_invisible=1;
2U: select count(*) from test_recently_dead_utility2;
2U: set gp_select_invisible=0;

2U: set gp_disable_dtx_visibility_check to on;
2U: vacuum full test_recently_dead_utility2;

2U: set gp_select_invisible=1;
2U: select count(*) from test_recently_dead_utility2;
2U: set gp_select_invisible=0;

-- Ensure that we ERROR out if gp_disable_dtx_visibility_check is set in
-- non-utility mode.
3: set gp_disable_dtx_visibility_check to on;
