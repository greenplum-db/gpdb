-- Test locking behaviour. When creating, dropping, querying or adding indexes
-- partitioned tables, we want to lock only the master, not the children.

-- start_ignore
create view locktest as
select coalesce(
  case when relname like 'pg_toast%index' then 'toast index'
  	   when relname like 'pg_toast%' then 'toast table'
	   else relname end, 'dropped table'), 
mode,
locktype from 
pg_locks l left outer join pg_class c on (l.relation = c.oid),
pg_database d where relation is not null and l.database = d.oid and 
l.gp_segment_id = -1 and
(c.relname is null or 
 c.relname like 'locktest_t1%' or
 c.relname like 'pg_toast%index' or
 c.relname like 'pg_toast%') and
-- XXX XXX: ignore gp_fault_strategy and pg_class*, plan is changed, more slices are parallel executed 
-- on entry db, so pg_class* relations are not stable.
d.datname = current_database() order by 1, 3, 2;
-- end_ignore

-- Partitioned table with toast table
begin;

-- creation
create table locktest_t1 (i int, t text) partition by range(i)
(start(1) end(10) every(1));

-- start_ignore
-- Known_opt_diff: MPP-20936
-- end_ignore
select * from locktest;
commit;

-- drop
begin;
drop table locktest_t1;
-- start_ignore
-- Known_opt_diff: MPP-20936
-- end_ignore
select * from locktest;
commit;

-- AO table (ao segments, block directory won't exist after create)
begin;
-- creation
create table locktest_t1 (i int, t text, n numeric)
with (appendonly = true)
partition by list(i)
(values(1), values(2), values(3));
-- start_ignore
-- Known_opt_diff: MPP-20936
-- end_ignore
select * from locktest;
commit;
begin;

-- add a little data
insert into locktest_t1 values(1), (2), (3);
insert into locktest_t1 values(1), (2), (3);
insert into locktest_t1 values(1), (2), (3);
insert into locktest_t1 values(1), (2), (3);
insert into locktest_t1 values(1), (2), (3);
-- start_ignore
-- Known_opt_diff: MPP-20936
-- end_ignore
select * from locktest;

commit;
-- drop
begin;
drop table locktest_t1;
-- start_ignore
-- Known_opt_diff: MPP-20936
-- end_ignore
select * from locktest;
commit;

-- Indexing
create table locktest_t1 (i int, t text) partition by range(i)
(start(1) end(10) every(1));

begin;
create index locktest_t1_idx on locktest_t1(i);
-- start_ignore
-- Known_opt_diff: MPP-20936
-- end_ignore
select * from locktest;
commit;

-- test select locking
begin;
select * from locktest_t1 where i = 1;
-- start_ignore
-- Known_opt_diff: MPP-20936
-- end_ignore
select * from locktest;
commit;

begin;
-- insert locking
insert into locktest_t1 values(3, 'f');
-- start_ignore
-- Known_opt_diff: MPP-20936
-- end_ignore
select * from locktest;
commit;

-- delete locking
begin;
delete from locktest_t1 where i = 4;
-- start_ignore
-- Known_opt_diff: MPP-20936
-- end_ignore
select * from locktest;
commit;

-- drop index
begin;
drop table locktest_t1;
-- start_ignore
-- Known_opt_diff: MPP-20936
-- end_ignore
select * from locktest;
commit;
