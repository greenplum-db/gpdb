-- up the admin_group memory limits
ALTER RESOURCE GROUP admin_group SET memory_limit 30;

-- Test Mark/Restore in Material Node
create table spilltest1 (a integer);
create table spilltest2 (a integer);
insert into spilltest1 select a from generate_series(1,400000) a;
insert into spilltest2 select a from generate_series(1,400000) a;

-- go back to the default admin_group limit
ALTER RESOURCE GROUP admin_group SET memory_limit 10;

-- start_ignore
DROP ROLE IF EXISTS role1_memory_test;
DROP RESOURCE GROUP rg1_memory_test;
-- end_ignore
CREATE RESOURCE GROUP rg1_memory_test WITH
(concurrency=2, cpu_rate_limit=10, memory_limit=60, memory_shared_quota=0, memory_spill_ratio=10);
CREATE ROLE role1_memory_test SUPERUSER RESOURCE GROUP rg1_memory_test;
SET ROLE TO role1_memory_test;

set enable_hashagg=off;
set enable_mergejoin=on;
set enable_hashjoin=off;
set enable_nestloop=off;

create temporary table spilltestresult1 as
select t1.a as t1a, t2.a as t2a
from (select a from spilltest1 group by a) t1,
     (select a from spilltest2 group by a) t2
where t1.a = t2.a;

-- check that the result looks sane
select count(*), sum(t1a), sum(t2a), sum(t1a - t2a) from spilltestresult1;

-- Test Hash Aggregation when the work mem is too small for the hash table
create table spilltest (a integer, b integer);
insert into spilltest select a, a%25 from generate_series(1,8000) a;
analyze;
set enable_hashagg=on;
set enable_groupagg=off;

select b,count(*) from spilltest group by b order by b;

select b,count(*) from spilltest group by b order by b;
-- Test Hash Join when the work mem is too small for the hash table
drop table if exists spilltest;
create table spilltest (a integer, b integer);
insert into spilltest select a, a%25 from generate_series(1,800000) a;
analyze; -- We have to do an analyze to force a hash join
set enable_mergejoin=off;
set enable_nestloop=off;
set enable_hashjoin=on;

create temporary table spilltestresult2 as
select t1.a as t1a, t1.b as t1b, t2.a as t2a, t2.b as t2b from spilltest t1, spilltest t2 where t1.a = t2.a;
-- check that the result looks sane
select count(*), sum(t1a), sum(t2a), sum(t2a), sum(t2b), sum(t1a * t1b) from spilltestresult2;

drop table spilltest1;
drop table spilltest2;
drop table spilltest;
drop table spilltestresult1;
drop table spilltestresult2;

-- start_ignore
RESET ROLE;
DROP ROLE IF EXISTS role1_memory_test;
DROP RESOURCE GROUP rg1_memory_test;
-- end_ignore
