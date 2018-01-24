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
0U: vacuum full verbose test_recently_dead_utility;
0U: select count(*) from test_recently_dead_utility;
0U: set gp_select_invisible=1;
-- print to make sure deleted tuples were not cleaned due to distributed
-- snapshot to make test is future proof, if logic in
-- localXidSatisfiesAnyDistributedSnapshot() changes for utility mode.
0U: select count(*) from test_recently_dead_utility;
0U: set gp_select_invisible=0;
