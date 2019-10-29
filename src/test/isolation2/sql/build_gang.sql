-- SnapshotNow and AccessShareLock is used while scanning
-- gp_segment_configuration table for building gangs for
-- query. SnapshotNow scans have the undesirable property that, in the
-- face of concurrent updates, the scan can fail to see either the old
-- or the new versions of the row. Given this is rare occurrence, fix
-- is to retry if duplicate rows with same dbid are found. So, this
-- test introduces duplicates (in-memory only) after
-- gp_segment_configuration table scan using fault injector and
-- validates the retry logic.

CREATE EXTENSION IF NOT EXISTS gp_inject_fault;
CREATE TABLE build_gang_test(a int);

-- inject fault infinitely to simulate duplicate entries fetched from
-- gp_segment_configuration during scan
SELECT gp_inject_fault_infinite('add_duplicate_segment_component_db_entry', 'skip', 1);
-- this query is expected to fail, as building gang should error out
-- after maximum retries
1: SELECT * from build_gang_test;
SELECT gp_inject_fault_new('add_duplicate_segment_component_db_entry', 'reset', 1);

-- inject fault "twice" to simulate duplicate entries fetched from
-- gp_segment_configuration during scan
SELECT gp_inject_fault_new('add_duplicate_segment_component_db_entry', 'skip', '', '', '', 1, 2, 0, 1);
-- build gang will retry for 2 times and then should be able to
-- successfully run the query
1: SELECT * from build_gang_test;
SELECT gp_inject_fault_new('add_duplicate_segment_component_db_entry', 'reset', 1);
