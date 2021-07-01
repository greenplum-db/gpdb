-- The test has two parts, first part is when I use normal sql command(no cached), I don't have problem with vacuum.
-- Second part is when I use cached plan sql command, I don't have problem with vacuum.
-- for example:
-- session 1: vacuum sales_row_1_prt_1
-- session 2: delete from sales_row;
-- The old lock behavior(If we acquires leaf locks in the InitPlan):
-- session 2 get locks for the sales_row
-- session 1 get locks for the sales_row_1_prt_1
-- session 2 will get snapshot and enter InitPlan, wait session 1 to release the locks for the sales_row_1_prt_1
-- session 1 complete
-- session 2 get locks but the snapshot is invalid.
--
-- Now the lock behavior is:(If we acquires all the locks in the parse and analyze):
-- session 2 get locks for the sales_row
-- session 1 get locks for the sales_row_1_prt_1
-- session 2 will wait session 1 to release the locks for the sales_row_1_prt_1 in the function setTargetTable or AcquirePlannerLocks
-- session 1 complete
-- session 2 get locks and get snapshot, then it will complete correctly
-- 
-- So the problem is solved

create extension if not exists gp_inject_fault;

create table sales_row (id int, date date, amt decimal(10,2))
with (appendonly=true) distributed by (id)
partition by range (date)
( start (date '2008-01-01') inclusive
end (date '2009-01-01') exclusive
every (interval '1 month') );

insert into sales_row values (generate_series(1,1000),'2008-01-01',10);
update sales_row set amt = amt + 1;

select gp_inject_fault('vacuum_hold_lock', 'suspend', dbid) from gp_segment_configuration where role = 'p' and content = -1;

-- session 1 will block after getting locks on leaf partition. 
1&: vacuum sales_row_1_prt_1;

select gp_wait_until_triggered_fault('vacuum_hold_lock', 1, dbid) from gp_segment_configuration where role = 'p' and content = -1;

select gp_inject_fault('parse_wait_lock', 'suspend', dbid) from gp_segment_configuration where role = 'p' and content = -1;

-- session 2 will get locks on the sales_row, but still wait locks on the leaf partition in the function setTargetTable.
2&: delete from sales_row;

-- session 2 will wait locks on leaf partition.
select gp_wait_until_triggered_fault('parse_wait_lock', 1, dbid) from gp_segment_configuration where role = 'p' and content = -1;

select gp_inject_fault('parse_wait_lock', 'reset', dbid) from gp_segment_configuration where role = 'p' and content = -1;

select pg_sleep(1);

-- session 1 will complete, so session 2 get locks and complete.
select gp_inject_fault('vacuum_hold_lock', 'reset', dbid) from gp_segment_configuration where role = 'p' and content = -1;

1<:
2<:

1q:
2q:

-- cached plan

drop table sales_row;
create table sales_row (id int, date date, amt decimal(10,2))
with (appendonly=true) distributed by (id)
partition by range (date)
( start (date '2008-01-01') inclusive
end (date '2009-01-01') exclusive
every (interval '1 month') );

insert into sales_row values (generate_series(1,1000),'2008-01-01',10);
update sales_row set amt = amt + 1;

2: prepare test as delete from sales_row;

select gp_inject_fault('vacuum_hold_lock', 'suspend', dbid) from gp_segment_configuration where role = 'p' and content = -1;

-- session 1 will block after getting locks on leaf partition. 
1&: vacuum sales_row_1_prt_1;

select gp_wait_until_triggered_fault('vacuum_hold_lock', 1, dbid) from gp_segment_configuration where role = 'p' and content = -1;

select gp_inject_fault('cache_wait_lock', 'suspend', dbid) from gp_segment_configuration where role = 'p' and content = -1;

-- session 2 will get locks on the sales_row, but still wait locks on the leaf partition in the function AcquirePlannerLocks.
2&: execute test;

select gp_wait_until_triggered_fault('cache_wait_lock', 1, dbid) from gp_segment_configuration where role = 'p' and content = -1;

-- session 2 will wait locks on leaf partition.
select gp_inject_fault('cache_wait_lock', 'reset', dbid) from gp_segment_configuration where role = 'p' and content = -1;

select pg_sleep(1);

-- session 1 will complete, so session 2 get locks and complete.
select gp_inject_fault('vacuum_hold_lock', 'reset', dbid) from gp_segment_configuration where role = 'p' and content = -1;

1<:
2<:

1q:
2q:

drop table sales_row;