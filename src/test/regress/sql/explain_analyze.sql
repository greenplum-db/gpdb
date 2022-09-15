
CREATE TEMP TABLE empty_table(a int);
-- We used to incorrectly report "never executed" for a node that returns 0 rows
-- from every segment. This was misleading because "never executed" should
-- indicate that the node was never executed by its parent.
-- explain_processing_off
EXPLAIN (ANALYZE, TIMING OFF, COSTS OFF, SUMMARY OFF) SELECT a FROM empty_table;
-- explain_processing_on
