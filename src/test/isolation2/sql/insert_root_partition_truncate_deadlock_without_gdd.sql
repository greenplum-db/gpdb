-- Insert statement on root partition, if GDD is enabled, QD will not lock leaf
-- partitions for better concurrency performance. So when the insert statement
-- is dispatched to segments async, some of statement will lock leaf partition
-- and execute, some might be blocked by other sessions and lead to global
-- deadlock. This test file is in GDD suites, it verify that such deadlock can
-- be broken by GDD.
-- See Issue https://github.com/greenplum-db/gpdb/issues/13652 for details.

-- NOTE: this test case is better to run both with GDD and withoug GDD.
-- with GDD it is running within gdd test suites to test GDD can break the deadlock;
-- without GDD it is running to show that no deadlock happens.

-- 6X will also lock AO table's leaf even with GDD enabled.

create extension if not exists gp_inject_fault;

-- test for heap, without GDD, no deadlock
create table rank_13652 (id int, year int)
partition by range (year)
(start (2006) end (2009) every (1));

select gp_inject_fault('func_init_plan_end', 'suspend', dbid) from gp_segment_configuration where content = 0 and role = 'p';

1&: insert into rank_13652 select i,i%3+2006 from generate_series(1, 30)i;
select gp_wait_until_triggered_fault('func_init_plan_end', 1, dbid) from gp_segment_configuration where content = 0 and role = 'p';

2&: truncate rank_13652_1_prt_2;

select gp_inject_fault('func_init_plan_end', 'reset', dbid) from gp_segment_configuration where content = 0 and role = 'p';

1<:
2<:

1q:
2q:

drop table rank_13652;

-- test AO, without GDD, no deadlock
create table rank_13652 (id int, year int)
with (appendonly=true)
partition by range (year)
(start (2006) end (2009) every (1));

select gp_inject_fault('func_init_plan_end', 'suspend', dbid) from gp_segment_configuration where content = 0 and role = 'p';

1&: insert into rank_13652 select i,i%3+2006 from generate_series(1, 30)i;
select gp_wait_until_triggered_fault('func_init_plan_end', 1, dbid) from gp_segment_configuration where content = 0 and role = 'p';

2&: truncate rank_13652_1_prt_2;

select gp_inject_fault('func_init_plan_end', 'reset', dbid) from gp_segment_configuration where content = 0 and role = 'p';

1<:
2<:

1q:
2q:

drop table rank_13652;
