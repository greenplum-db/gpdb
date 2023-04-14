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

-- If all QEs hit errors when executing sort, we might not receive stat data for sort.
-- rethrow error before print explain info.
create extension if not exists gp_inject_fault;
create table sort_error_test1(tc1 int, tc2 int);
create table sort_error_test2(tc1 int, tc2 int);
insert into sort_error_test1 select i,i from generate_series(1,20) i;
select gp_inject_fault('explain_analyze_sort_error', 'error', dbid)
    from gp_segment_configuration where role = 'p' and content > -1;
EXPLAIN analyze insert into sort_error_test2 select * from sort_error_test1 order by 1;
select count(*) from sort_error_test2;
select gp_inject_fault('explain_analyze_sort_error', 'reset', dbid)
    from gp_segment_configuration where role = 'p' and content > -1;
drop table sort_error_test1;
drop table sort_error_test2;
