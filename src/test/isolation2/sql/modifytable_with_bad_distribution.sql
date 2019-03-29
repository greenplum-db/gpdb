-- start_matchsubs
-- m/nodeDML.c:\d+/
-- s/nodeDML.c:\d+/nodeDML.c:XXX/
-- m/nodeModifyTable.c:\d+/
-- s/nodeModifyTable.c:\d+/nodeModifyTable.c:XXX/
-- end_matchsubs

create table bad_distribution1 (a int, b int) distributed by (a);
create table pbad_distribution1 (a int, b int) distributed by (a) PARTITION BY RANGE(a) (START(1) END(9) EVERY (4));
create table help_distribution (a int, b int) distributed by (a);

-- insert & verify test prerequisite: (2,2), (7,7) on seg0, (1,1) on seg1, (5,5) on seg2.
insert into bad_distribution1 values(2,2), (1,1), (5, 5), (7, 7);
insert into pbad_distribution1 values(2,2), (1,1), (5, 5), (7, 7);
select gp_segment_id, * from bad_distribution1 order by a;
delete from bad_distribution1 where a = 7;
delete from pbad_distribution1 where a = 7;
-- populate the help table.
insert into help_distribution select s,s from generate_series(1,10) s;

-- insert (7,7) on unexpected seg, i.e. seg2. Note 'insert into bad_distribution1 values(7,7)' does not work.
2U: insert into bad_distribution1 select s,s from generate_series(7,7) s;
2U: insert into pbad_distribution1_1_prt_2 select s,s from generate_series(7,7) s;
2Uq:

analyze bad_distribution1;
analyze pbad_distribution1;
analyze help_distribution;

-- Test update on distribution key. Expect error.
update bad_distribution1 set a=a+1;
update pbad_distribution1 set a=a+1;

-- Test delete. Expect error for orca plan.
explain verbose delete from bad_distribution1 using (select * from help_distribution where b < 20) s where s.a = bad_distribution1.b;
delete from bad_distribution1 using (select * from help_distribution where b < 20) s where s.a = bad_distribution1.b;
delete from pbad_distribution1 using (select * from help_distribution where b < 20) s where s.a = pbad_distribution1.b;

-- Test update on non-distribution key. Expect ok.
update bad_distribution1 set b=b+1;
update pbad_distribution1 set b=b+1;

-- check the final results.
select * from bad_distribution1 order by 1;
select * from pbad_distribution1 order by 1;

-- cleanup.
drop table bad_distribution1;
drop table pbad_distribution1;
drop table help_distribution;
