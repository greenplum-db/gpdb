-- start_matchsubs
-- m/nodeModifyTable.c:\d+/
-- s/nodeModifyTable.c:\d+/nodeModifyTable.c:XXX/
-- end_matchsubs

-- start_ignore
drop table tab1;
drop table tab2;
drop table tab3;
drop table tmp_save_dist_info;
-- end_ignore

-- We do some check to verify the tuple to delete|update
-- is from the segment it scans out. This case is to test
-- such check.
-- We build a plan that will add motion above result relation,
-- however, does not contain explicit motion to send tuples back,
-- and then login in segment using utility mode to insert some
-- bad data.
-- Then we carefully build some plans for orca and planner,
-- when reading these test cases, pay attention to the bad tuple
-- and see if it is motioned to other segments.

create table tab1(a int, b int) distributed by (b);
create table tab2(a int, b int) distributed by (a);
create table tab3 (a int, b int) distributed by (b);

insert into tab1 values (1, 1);
insert into tab2 values (1, 1);
insert into tab3 values (1, 1);

set allow_system_table_mods=true;
update pg_class set relpages = 10000 where relname='tab2';
update pg_class set reltuples = 100000000 where relname='tab2';
update pg_class set relpages = 100000000 where relname='tab3';
update pg_class set reltuples = 100000 where relname='tab3';

-- 6X code does not insert wrong data in utility mode
-- the following hack to build a wrong data senario is:
--   1. save the tab1's policy info into a tmp table
--   2. change tab1 to randomly distribtued without reorganization
--   3. use utility mode to login in seg0 and insert a wrong data(under
--      the hash policy)
--   4. restore tab1's policy directly using DML statements
create table tmp_save_dist_info as select * from gp_distribution_policy where localoid = 'tab1'::regclass::oid;
alter table tab1 set with(reorganize=false) distributed randomly;
0U: insert into tab1 values (1, 1);
delete from gp_distribution_policy where localoid = 'tab1'::regclass::oid;
insert into gp_distribution_policy select * from tmp_save_dist_info;


select gp_segment_id, * from tab1;

-- TODO: this case is for planner, it will not error out on 6X now,
--       because 6x does not remove explicit motion yet.
explain (costs off) delete from tab1 using tab2, tab3 where tab1.a = tab2.a and tab1.b = tab3.b;
begin;
delete from tab1 using tab2, tab3 where tab1.a = tab2.a and tab1.b = tab3.b;
abort;

-- TODO: this case is for planner, it will not error out on 6X now,
--       because 6x does not remove explicit motion yet.
explain (costs off) update tab1 set a = 999 from tab2, tab3 where tab1.a = tab2.a and tab1.b = tab3.b;
begin;
update tab1 set a = 999 from tab2, tab3 where tab1.a = tab2.a and tab1.b = tab3.b;
abort;

-- For orca, this will error out
explain (costs off) delete from tab1 using tab2, tab3 where tab1.a = tab2.a and tab1.b = tab3.a;
begin;
delete from tab1 using tab2, tab3 where tab1.a = tab2.a and tab1.b = tab3.a;
abort;

-- For orca, this will error out
explain (costs off) update tab1 set a = 999 from tab2, tab3 where tab1.a = tab2.a and tab1.b = tab3.a;
begin;
update tab1 set a = 999 from tab2, tab3 where tab1.a = tab2.a and tab1.b = tab3.a;
abort;

-- test splitupdate. 6X code, both orca and planner generate splitupdate with redistribute motion
-- so they will both error out.
explain (costs off) update tab1 set b = b + 1;
begin;
update tab1 set b = b + 1;
abort;

drop table tab1;
drop table tab2;
drop table tab3;
drop table tmp_save_dist_info;
