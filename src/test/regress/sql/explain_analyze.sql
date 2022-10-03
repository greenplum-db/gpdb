-- start_matchsubs
-- m/ Gather Motion [12]:1  \(slice1; segments: [12]\)/
-- s/ Gather Motion [12]:1  \(slice1; segments: [12]\)/ Gather Motion XXX/
-- m/Memory Usage: \d+\w?B/
-- s/Memory Usage: \d+\w?B/Memory Usage: ###B/
-- m/Buckets: \d+/
-- s/Buckets: \d+/Buckets: ###/
-- m/Batches: \d+/
-- s/Batches: \d+/Batches: ###/
-- end_matchsubs

CREATE TEMP TABLE empty_table(a int);
-- We used to incorrectly report "never executed" for a node that returns 0 rows
-- from every segment. This was misleading because "never executed" should
-- indicate that the node was never executed by its parent.
-- explain_processing_off
EXPLAIN (ANALYZE, TIMING OFF, COSTS OFF, SUMMARY OFF) SELECT a FROM empty_table;
-- explain_processing_on

-- For a node that is truly never executed, we still expect "never executed" to
-- be reported
-- explain_processing_off
EXPLAIN (ANALYZE, TIMING OFF, COSTS OFF, SUMMARY OFF) SELECT t1.a FROM empty_table t1 join empty_table t2 on t1.a = t2.a;
-- explain_processing_on