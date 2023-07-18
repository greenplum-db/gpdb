-- Test locking behaviour when operating on partitions.
--
-- In previous versions of GPDB, we used to only lock the parent table in
-- many DDL operations. That was always a bit bogus, but we did it to avoid
-- running out of lock space when working on large partition hierarchies. We
-- don't play fast and loose like that anymore, but keep the tests. If a user
-- runs out of lock space, you can work around that by simply bumping up
-- max_locks_per_transactions.
--

-- Show locks in master and in segments. Because the number of segments
-- in the cluster depends on configuration, we print only summary information
-- of the locks in segments. If a relation is locked only on one segment,
-- we print that as a special case, but otherwise we just print "n segments",
-- meaning the relation is locked on more than one segment.
create or replace view locktest_master as
select coalesce(
  case when relname like 'pg_toast%index' then 'toast index'
       when relname like 'pg_toast%' then 'toast table'
       when relname like 'pg_aoseg%' then 'aoseg table'
       when relname like 'pg_aovisimap%index' then 'aovisimap index'
       when relname like 'pg_aovisimap%' then 'aovisimap table'
       else relname end, 'dropped table'),
  mode,
  locktype,
  'master'::text as node
from pg_locks l
left outer join pg_class c on l.relation = c.oid,
pg_database d
where relation is not null
and l.database = d.oid
and (relname <> 'gp_fault_strategy' and relname != 'locktest_master' or relname is NULL)
and d.datname = current_database()
and l.gp_segment_id = -1
group by l.gp_segment_id, relation, relname, locktype, mode
order by 1, 3, 2;

create or replace view locktest_segments_dist as
select relname,
  mode,
  locktype,
  l.gp_segment_id as node,
  relation
from pg_locks l
left outer join pg_class c on l.relation = c.oid,
pg_database d
where relation is not null
and l.database = d.oid
and (relname <> 'gp_fault_strategy' and relname != 'locktest_segments_dist' or relname is NULL)
and d.datname = current_database()
and l.gp_segment_id > -1
group by l.gp_segment_id, relation, relname, locktype, mode;

create or replace view locktest_segments as
SELECT coalesce(
  case when relname like 'pg_toast%index' then 'toast index'
       when relname like 'pg_toast%' then 'toast table'
       when relname like 'pg_aoseg%' then 'aoseg table'
       when relname like 'pg_aovisimap%index' then 'aovisimap index'
       when relname like 'pg_aovisimap%' then 'aovisimap table'
       else relname end, 'dropped table'),
  mode,
  locktype,
  case when count(*) = 1 then '1 segment'
       else 'n segments' end as node
  FROM gp_dist_random('locktest_segments_dist')
  group by relname, relation, mode, locktype;

-- Partitioned table with toast table
begin;

-- creation
create table partlockt (i int, t text) partition by range(i)
(start(1) end(10) every(1));

select * from locktest_master where coalesce not like 'gp_%' and coalesce not like 'pg_%';
select * from locktest_segments where coalesce not like 'gp_%' and coalesce not like 'pg_%';
commit;

-- select
select * from partlockt where i = 1;
-- check locks when the relation found in MD cache
begin;
select * from partlockt where i = 1;
select * from locktest_master where coalesce not like 'gp_%' and coalesce not like 'pg_%';
select * from locktest_segments where coalesce not like 'gp_%' and coalesce not like 'pg_%';
commit;

-- drop
begin;
drop table partlockt;
select * from locktest_master where coalesce not like 'gp_%' and coalesce not like 'pg_%';
select * from locktest_segments where coalesce not like 'gp_%' and coalesce not like 'pg_%';
commit;

-- AO table (ao segments, block directory won't exist after create)
begin;
-- creation
create table partlockt (i int, t text, n numeric)
with (appendonly = true)
partition by list(i)
(values(1), values(2), values(3));
select * from locktest_master where coalesce not like 'gp_%' and coalesce not like 'pg_%';
select * from locktest_segments where coalesce not like 'gp_%' and coalesce not like 'pg_%';
commit;
begin;

-- add a little data
-- This only needs a lock on the parent table in the QD. On the segments, where the
-- tuple is routed to the correct partition, the partitions are locked, too.
insert into partlockt values(1), (2), (3);
insert into partlockt values(1), (2), (3);
insert into partlockt values(1), (2), (3);
insert into partlockt values(1), (2), (3);
insert into partlockt values(1), (2), (3);
select * from locktest_master where coalesce not like 'gp_%' and coalesce not like 'pg_%';
select * from locktest_segments where coalesce not like 'gp_%' and coalesce not like 'pg_%';
commit;

-- drop
begin;
drop table partlockt;
select * from locktest_master where coalesce not like 'gp_%' and coalesce not like 'pg_%';
select * from locktest_segments where coalesce not like 'gp_%' and coalesce not like 'pg_%';
commit;

-- Indexing
create table partlockt (i int, t text) partition by range(i)
(start(1) end(10) every(1));

begin;
create index partlockt_idx on partlockt(i);
select * from locktest_master where coalesce not like 'gp_%' and coalesce not like 'pg_%';
select * from locktest_segments where coalesce not like 'gp_%' and coalesce not like 'pg_%';
commit;

-- Force use of the index in the select and delete below. We're not interested
-- in the plan we get, but a seqscan will not lock the index while an index
-- scan will, and we want to avoid the plan-dependent difference in the
-- expected output of this test.
set enable_seqscan=off;

-- test select locking
begin;
select * from partlockt where i = 1;
select * from locktest_master where coalesce not like 'gp_%' and coalesce not like 'pg_%';
select * from locktest_segments where coalesce not like 'gp_%' and coalesce not like 'pg_%';
commit;

-- In ORCA we are seeing discrepancy with 'AccessShareLock' on root partition table
-- with-assert and without-assert modes of DML queries, because
-- with-assert mode we are forced to release the cache, which leads to acquire
-- 'AccessShareLock' always on master.  Since the AccessShareLock on root
-- partition is redundant so we are skipping to evaluate.

begin;
-- insert locking
insert into partlockt values(3, 'f');
select * from locktest_master where coalesce not like 'gp_%' and coalesce not like 'pg_%' and
                                    not(coalesce like 'partlockt' and mode like 'AccessShareLock');
select * from locktest_segments where coalesce not like 'gp_%' and coalesce not like 'pg_%';
commit;

-- delete locking
begin;
delete from partlockt where i = 4;
select * from locktest_master where coalesce not like 'gp_%' and coalesce not like 'pg_%' and
                                    not(coalesce like 'partlockt' and mode like 'AccessShareLock');
select * from locktest_segments where coalesce not like 'gp_%' and coalesce not like 'pg_%';
commit;

-- drop index
begin;
drop table partlockt;
select * from locktest_master where coalesce not like 'gp_%' and coalesce not like 'pg_%';
select * from locktest_segments where coalesce not like 'gp_%' and coalesce not like 'pg_%';
commit;

