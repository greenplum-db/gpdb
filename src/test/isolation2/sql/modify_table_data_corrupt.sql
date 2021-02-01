-- start_matchsubs
-- m/nodeModifyTable.c:\d+/
-- s/nodeModifyTable.c:\d+/nodeModifyTable.c:XXX/
-- end_matchsubs

-- start_ignore
drop table tab1;
drop table tab2;
drop table tab3;
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

analyze tab1;
analyze tab2;
analyze tab3;

set allow_system_table_mods=true;
update pg_class set relpages = 10000 where relname='tab2';
update pg_class set reltuples = 100000000 where relname='tab2';
update pg_class set relpages = 100000000 where relname='tab3';
update pg_class set reltuples = 100000 where relname='tab3';

0U: insert into tab1 values (1, 1);

select gp_segment_id, * from tab1;

-- For planner, this will error out
explain (costs off) delete from tab1 using tab2, tab3 where tab1.a = tab2.a and tab1.b = tab3.b;
begin;
delete from tab1 using tab2, tab3 where tab1.a = tab2.a and tab1.b = tab3.b;
abort;

-- For planner, this will error out
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

-- test splitupdate.
-- For orca, the plan contains a redistribute motion, so that
-- this following statement will error out.
-- For planner, the plan is using explicit redistribute motion,
-- the to-delete tuple is set to send back where it is from, so
-- it will not error out.
explain (costs off) update tab1 set b = b + 1;
begin;
update tab1 set b = b + 1;
abort;

drop table tab1;
drop table tab2;
drop table tab3;
