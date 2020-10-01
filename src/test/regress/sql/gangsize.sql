-- start_ignore
-- GPDB_12_MERGE_FIXME
-- Running this with ORCA in CI produces cache lookup failures. We suspect the
-- cause of this is in the collect_tabstat and this isn't an issue with
-- gangsize, but this test does expose a legitimate issue that needs to be
-- fixed. For now, we disable ORCA for this test until this can be debugged.
-- end_ignore
set optimizer=off;
set allow_system_table_mods = true;

create temp table random_2_0 (a int, b int, c int, d int) distributed randomly;

update gp_distribution_policy set numsegments = 2 where localoid = 'random_2_0'::regclass;

insert into random_2_0 select i,i,i,i from generate_series(1, 10)i;

create temp table replicate_2_1 (a int, b int, c int, d int) distributed replicated;

update gp_distribution_policy set numsegments = 2 where localoid = 'replicate_2_1'::regclass;

insert into replicate_2_1 select i,i,i,i from generate_series(1, 10)i;

create temp table hash_3_3_2 (a int, b int, c int, d int) distributed by (a,b,c);

update gp_distribution_policy set numsegments = 3 where localoid = 'hash_3_3_2'::regclass;

insert into hash_3_3_2 select i,i,i,i from generate_series(1, 10)i;

create temp table replicate_3_3 (a int, b int, c int, d int) distributed replicated;

update gp_distribution_policy set numsegments = 3 where localoid = 'replicate_3_3'::regclass;

insert into replicate_3_3 select i,i,i,i from generate_series(1, 10)i;

create temp table hash_2_3_4 (a int, b int, c int, d int) distributed by (a,b,c);

update gp_distribution_policy set numsegments = 2 where localoid = 'hash_2_3_4'::regclass;

insert into hash_2_3_4 select i,i,i,i from generate_series(1, 10)i;

create temp table replicate_2_5 (a int, b int, c int, d int) distributed replicated;

update gp_distribution_policy set numsegments = 2 where localoid = 'replicate_2_5'::regclass;

insert into replicate_2_5 select i,i,i,i from generate_series(1, 10)i;

create table gangsize_input_data(a int, b int, c int, d int);
insert into gangsize_input_data select i,i,i,i from generate_series(1,10)i;

set Test_print_direct_dispatch_info = true;

\o /dev/null

-- This test focuses on the whether slices' corresponding gang size in runtime.
-- So we enable the GUC Test_print_direct_dispatch_info to display dispatch info.
-- The following sqls are generated by a script. These queries are all multi-tables'
-- join and aggregation so they have many slices. We are not interested in their
-- results, so we redirect the output to /dev/null.

-- Some of the queries are only possible with Nested Loop Joins. Enable them, to
-- avoid the really high cost estimates on such plans, which skew the planners
-- decisions.
set enable_nestloop=on;

select replicate_2_1.c, hash_2_3_4.c, avg(hash_3_3_2.d), max(replicate_3_3.c) from ((random_2_0 right join replicate_2_1 on random_2_0.b = replicate_2_1.c) left join (hash_3_3_2 inner join replicate_3_3 on hash_3_3_2.c >= replicate_3_3.b) on replicate_2_1.d >= hash_3_3_2.a) inner join (hash_2_3_4 inner join replicate_2_5 on hash_2_3_4.c = replicate_2_5.d) on hash_3_3_2.a <> hash_2_3_4.a group by replicate_2_1.c, hash_2_3_4.c order by 1,2;

select hash_3_3_2.b, replicate_2_5.d, sum(replicate_2_1.d), sum(replicate_3_3.a) from (((random_2_0 left join replicate_2_1 on random_2_0.b <> replicate_2_1.d) left join hash_3_3_2 on random_2_0.c = hash_3_3_2.b) inner join (replicate_3_3 inner join hash_2_3_4 on replicate_3_3.d = hash_2_3_4.d) on hash_3_3_2.a <> hash_2_3_4.a) right join replicate_2_5 on random_2_0.d <> replicate_2_5.c group by hash_3_3_2.b, replicate_2_5.d order by 1,2;

select replicate_3_3.a, hash_2_3_4.d, sum(replicate_2_1.a), max(hash_3_3_2.d) from (((random_2_0 right join replicate_2_1 on random_2_0.d >= replicate_2_1.c) left join (hash_3_3_2 full join replicate_3_3 on hash_3_3_2.b = replicate_3_3.a) on replicate_2_1.a < hash_3_3_2.c) full join hash_2_3_4 on random_2_0.a >= hash_2_3_4.b) inner join replicate_2_5 on hash_3_3_2.c < replicate_2_5.d group by replicate_3_3.a, hash_2_3_4.d order by 1,2;

select replicate_3_3.d, replicate_2_1.d, avg(random_2_0.c), max(hash_3_3_2.a) from ((((random_2_0 left join replicate_2_1 on random_2_0.c <= replicate_2_1.d) full join hash_3_3_2 on random_2_0.a > hash_3_3_2.d) full join replicate_3_3 on hash_3_3_2.a >= replicate_3_3.b) right join hash_2_3_4 on random_2_0.a = hash_2_3_4.b) left join replicate_2_5 on random_2_0.a > replicate_2_5.b group by replicate_3_3.d, replicate_2_1.d order by 1,2;

select hash_3_3_2.b, replicate_2_1.c, max(replicate_3_3.d), avg(random_2_0.a) from (random_2_0 right join (replicate_2_1 left join hash_3_3_2 on replicate_2_1.b <> hash_3_3_2.b) on random_2_0.a <> hash_3_3_2.a) right join ((replicate_3_3 inner join hash_2_3_4 on replicate_3_3.b < hash_2_3_4.c) left join replicate_2_5 on hash_2_3_4.b < replicate_2_5.d) on hash_3_3_2.a > hash_2_3_4.b group by hash_3_3_2.b, replicate_2_1.c order by 1,2;

select replicate_2_1.c, random_2_0.c, sum(replicate_3_3.a), max(replicate_2_5.c) from (random_2_0 left join replicate_2_1 on random_2_0.d <= replicate_2_1.b) right join (hash_3_3_2 inner join (replicate_3_3 inner join (hash_2_3_4 right join replicate_2_5 on hash_2_3_4.b <> replicate_2_5.a) on replicate_3_3.b = hash_2_3_4.b) on hash_3_3_2.a <> replicate_2_5.a) on replicate_2_1.c <> replicate_3_3.c group by replicate_2_1.c, random_2_0.c order by 1,2;

select hash_2_3_4.d, replicate_2_1.b, sum(random_2_0.a), count(replicate_2_5.d) from ((random_2_0 left join replicate_2_1 on random_2_0.b = replicate_2_1.b) right join (hash_3_3_2 full join replicate_3_3 on hash_3_3_2.b <= replicate_3_3.d) on replicate_2_1.c > hash_3_3_2.b) inner join (hash_2_3_4 right join replicate_2_5 on hash_2_3_4.a < replicate_2_5.d) on replicate_2_1.b < replicate_2_5.d group by hash_2_3_4.d, replicate_2_1.b order by 1,2;

select replicate_3_3.d, random_2_0.c, count(hash_3_3_2.d), count(hash_2_3_4.b) from ((random_2_0 full join (replicate_2_1 right join hash_3_3_2 on replicate_2_1.b > hash_3_3_2.c) on random_2_0.b >= hash_3_3_2.a) inner join replicate_3_3 on hash_3_3_2.d < replicate_3_3.a) left join (hash_2_3_4 full join replicate_2_5 on hash_2_3_4.a <> replicate_2_5.a) on random_2_0.d = hash_2_3_4.c group by replicate_3_3.d, random_2_0.c order by 1,2;

select random_2_0.d, replicate_2_1.a, max(hash_3_3_2.b), count(replicate_2_5.b) from (random_2_0 left join replicate_2_1 on random_2_0.c <> replicate_2_1.b) inner join (hash_3_3_2 full join ((replicate_3_3 inner join hash_2_3_4 on replicate_3_3.d <> hash_2_3_4.d) left join replicate_2_5 on hash_2_3_4.d < replicate_2_5.b) on hash_3_3_2.c > hash_2_3_4.b) on random_2_0.b < replicate_2_5.d group by random_2_0.d, replicate_2_1.a order by 1,2;

select replicate_2_5.c, hash_3_3_2.c, sum(replicate_3_3.c), max(hash_2_3_4.c) from ((random_2_0 inner join replicate_2_1 on random_2_0.a = replicate_2_1.d) right join hash_3_3_2 on replicate_2_1.c <> hash_3_3_2.c) left join ((replicate_3_3 inner join hash_2_3_4 on replicate_3_3.d <= hash_2_3_4.c) right join replicate_2_5 on replicate_3_3.d > replicate_2_5.b) on hash_3_3_2.b = hash_2_3_4.b group by replicate_2_5.c, hash_3_3_2.c order by 1,2;

select hash_3_3_2.a, replicate_3_3.b, max(random_2_0.c), sum(replicate_2_1.a) from random_2_0 full join (replicate_2_1 inner join ((hash_3_3_2 inner join (replicate_3_3 inner join hash_2_3_4 on replicate_3_3.d >= hash_2_3_4.d) on hash_3_3_2.c >= replicate_3_3.d) inner join replicate_2_5 on replicate_3_3.b <= replicate_2_5.d) on replicate_2_1.c > replicate_3_3.a) on random_2_0.d = hash_3_3_2.d group by hash_3_3_2.a, replicate_3_3.b order by 1,2;

select replicate_2_1.d, random_2_0.c, sum(hash_3_3_2.d), max(replicate_3_3.d) from (random_2_0 right join (replicate_2_1 inner join hash_3_3_2 on replicate_2_1.d >= hash_3_3_2.c) on random_2_0.c <= hash_3_3_2.c) inner join ((replicate_3_3 full join hash_2_3_4 on replicate_3_3.d <= hash_2_3_4.b) left join replicate_2_5 on replicate_3_3.d <= replicate_2_5.b) on random_2_0.b > replicate_2_5.b group by replicate_2_1.d, random_2_0.c order by 1,2;

select replicate_3_3.d, hash_3_3_2.a, avg(replicate_2_5.d), max(replicate_2_1.d) from random_2_0 right join ((replicate_2_1 right join hash_3_3_2 on replicate_2_1.b <= hash_3_3_2.a) inner join (replicate_3_3 right join (hash_2_3_4 right join replicate_2_5 on hash_2_3_4.c = replicate_2_5.d) on replicate_3_3.b >= hash_2_3_4.c) on hash_3_3_2.b <> hash_2_3_4.c) on random_2_0.c >= hash_2_3_4.b group by replicate_3_3.d, hash_3_3_2.a order by 1,2;

select hash_2_3_4.c, replicate_2_1.b, sum(replicate_3_3.b), avg(replicate_2_5.a) from random_2_0 left join (replicate_2_1 right join ((hash_3_3_2 left join replicate_3_3 on hash_3_3_2.d >= replicate_3_3.b) right join (hash_2_3_4 right join replicate_2_5 on hash_2_3_4.d <> replicate_2_5.d) on replicate_3_3.d <= replicate_2_5.a) on replicate_2_1.b <= replicate_2_5.d) on random_2_0.a < replicate_3_3.a group by hash_2_3_4.c, replicate_2_1.b order by 1,2;

-- Test for BEGIN;
begin;
commit;

begin;
abort;

-- Test for UPDATE/DELETE/INSERT;

-- Insert
insert into random_2_0 select * from gangsize_input_data where gp_segment_id = 0;

begin;
insert into random_2_0 select * from gangsize_input_data where gp_segment_id = 0;
end;

insert into replicate_2_1 select * from gangsize_input_data where gp_segment_id = 0;

begin;
insert into replicate_2_1 select * from gangsize_input_data where gp_segment_id = 0;
end;

insert into hash_3_3_2 select * from gangsize_input_data where gp_segment_id = 0;

begin;
insert into hash_3_3_2 select * from gangsize_input_data where gp_segment_id = 0;
end;

insert into replicate_3_3 select * from gangsize_input_data where gp_segment_id = 0;

begin;
insert into replicate_3_3 select * from gangsize_input_data where gp_segment_id = 0;
end;

insert into hash_2_3_4 select * from gangsize_input_data where gp_segment_id = 0;

begin;
insert into hash_2_3_4 select * from gangsize_input_data where gp_segment_id = 0;
end;

--Update
update random_2_0 set a = a + 1;

begin;
update random_2_0 set a = a + 1;
end;

update random_2_0 set a = 1 from hash_3_3_2 where hash_3_3_2.b = random_2_0.c;

begin;
update random_2_0 set a = 1 from hash_3_3_2 where hash_3_3_2.b = random_2_0.c;
end;

update replicate_2_1 set a = a + 1;

begin;
update replicate_2_1 set a = a + 1;
end;

-- Delete
delete from hash_2_3_4 where a in (1, 2, 3);

begin;
delete from hash_2_3_4 where a = 4 or a = 5;
end;

-- add test for table expand
begin;
alter table random_2_0 expand table;
abort;

begin;
alter table replicate_2_1 expand table;
abort;

begin;
alter table hash_2_3_4 expand table;
abort;
reset optimizer;
