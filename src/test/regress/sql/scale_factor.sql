-- start_ignore
create or replace function get_explain_xml_output(query_string text)
returns xml as
$$
declare
  x xml;
begin
  execute 'explain (format xml) ' || query_string
  into x;
  return x;
end;
$$ language plpgsql;

create or replace function get_plan_rows(query_string text)
returns table(node_name xml, plan_rows xml) as
$_$
declare
  node_xml      text := '//*[local-name()="Node-Type"]/text()';
  plan_rows_xml text := '//*[local-name()="Plan-Rows"]/text()';
begin
   return query
   execute 'select unnest(xpath(''' || node_xml || ''', x)) node_name,
                   unnest(xpath(''' || plan_rows_xml || ''', x)) plan_rows
            from get_explain_xml_output($$' || query_string || '$$) as x';
end;
$_$ language plpgsql;

create or replace function get_plan_rows_in_slice(query_string text, slice int)
returns table(node_name xml, plan_rows xml) as
$_$
declare
  node_xml      text := '//*[local-name()="Slice"][contains(text(), %s)]/../*[local-name()="Node-Type"]/text()';
  plan_rows_xml text := '//*[local-name()="Slice"][contains(text(), %s)]/../*[local-name()="Plan-Rows"]/text()';
begin
   return query
   execute 'select unnest(xpath(''' || format(node_xml, slice) || ''', x)) node_name,
                   unnest(xpath(''' || format(plan_rows_xml, slice) || ''', x)) plan_rows
            from get_explain_xml_output($$' || query_string || '$$) as x';
end;
$_$ language plpgsql;

create or replace function get_motion_snd_recv(query_string text)
returns table(node_name xml, motion_snd xml, motion_recv xml) as
$_$
declare
  node_xml      text := '//*[local-name()="Node-Type"][contains(text(), "Motion")]/../*[local-name()="Node-Type"]/text()';
  motion_snd    text := '//*[local-name()="Node-Type"][contains(text(), "Motion")]/../*[local-name()="Senders"]/text()';
  motion_recv   text := '//*[local-name()="Node-Type"][contains(text(), "Motion")]/../*[local-name()="Receivers"]/text()';
begin
   return query
   execute 'select unnest(xpath(''' || node_xml || ''', x)) node_name,
                   unnest(xpath(''' || motion_snd || ''', x)) motion_snd,
                   unnest(xpath(''' || motion_recv || ''', x)) motion_recv
            from get_explain_xml_output($$' || query_string || '$$) as x';
end;
$_$ language plpgsql;

create table scale_factor_repl(c1 int, c2 int) distributed replicated;
create table scale_factor_distr(c1 int, c2 int) distributed by (c1);
create table scale_factor_rand_distr(c1 int, c2 int);
create table scale_factor_partitioned (a int) partition by range(a) (start(1) end(10) every(1));
create table scale_factor_master_only (a int);

set allow_system_table_mods=true;
delete from gp_distribution_policy where localoid='scale_factor_master_only'::regclass;
reset allow_system_table_mods;

set allow_system_table_mods = on;
create table scale_factor_part_distr(c1 int, c2 int) distributed by(c1);
update gp_distribution_policy set numsegments = 2 where localoid = 'scale_factor_part_distr'::regclass;
reset allow_system_table_mods;

insert into scale_factor_repl select i,i from generate_series(1, 10)i;
insert into scale_factor_distr select i,i from generate_series(1, 10)i;
insert into scale_factor_rand_distr select i,i from generate_series(5, 15)i;
insert into scale_factor_part_distr select i,i from generate_series(1, 10)i;
insert into scale_factor_partitioned values (1), (1), (1);
insert into scale_factor_master_only select generate_series(1, 10);

analyze scale_factor_repl;
analyze scale_factor_distr;
analyze scale_factor_rand_distr;
analyze scale_factor_part_distr;
analyze scale_factor_partitioned;
analyze scale_factor_master_only;
-- end_ignore


--
-- scaleFactor, motion_snd and motion_recv definition tests
-- Test cases cover conditions refactored or removed
-- from the old ExplainNode implementation.
--

set optimizer = off;

-- Limit node: CdbPathLocus_IsSingleQE, scaleFactor==1
--
--  Gather Motion 3:1  (slice3; segments: 3)  (cost=0.77..4.10 rows=6 width=8)
--    ->  Hash Semi Join  (cost=0.77..4.10 rows=2 width=8)
--          Hash Cond: (scale_factor_distr.c2 = ((scale_factor_rand_distr.c1 / 2)))
--          ->  Seq Scan on scale_factor_distr  (cost=0.00..3.20 rows=7 width=8)
--          ->  Hash  (cost=0.66..0.66 rows=3 width=4)
--                ->  Broadcast Motion 1:3  (slice2; segments: 1)  (cost=0.00..0.66 rows=9 width=4)
--                      ->  Limit  (cost=0.00..0.51 rows=3 width=4)
--                            ->  Gather Motion 3:1  (slice1; segments: 3)  (cost=0.00..0.51 rows=3 width=4)
--                                  ->  Limit  (cost=0.00..0.45 rows=1 width=4)
--                                        ->  Seq Scan on scale_factor_rand_distr  (cost=0.00..3.28 rows=8 width=4)
--
select * from get_plan_rows_in_slice($$
  select * from scale_factor_distr where c2 in (select c1/2 from scale_factor_rand_distr limit 3);
$$, 2);

-- SeqScan: CdbLocusType_Entry, scaleFactor==1
--
--  Seq Scan on scale_factor_master_only  (cost=0.00..1.10 rows=10 width=4)
--
select * from get_plan_rows($$
  select * from scale_factor_master_only;
$$);

-- SeqScan node: CdbLocusType_SegmentGeneral, scaleFactor==1
--
--  Limit  (cost=0.00..0.13 rows=1 width=8)
--    ->  Gather Motion 1:1  (slice1; segments: 1)  (cost=0.00..1.30 rows=10 width=8)
--          ->  Seq Scan on scale_factor_repl  (cost=0.00..1.10 rows=10 width=8)
--
select * from get_plan_rows_in_slice($$
  select * from scale_factor_repl limit 1;
$$, 1);

-- SeqScan: Direct dispatch in PO, scaleFactor==2
--
--  Gather Motion 2:1  (slice1; segments: 2)  (cost=0.00..3.17 rows=3 width=8)
--    ->  Seq Scan on scale_factor_distr  (cost=0.00..3.17 rows=2 width=8)
--          Filter: ((c1 = 2) OR (c1 = 5) OR (c1 = 9))
--
select * from get_plan_rows_in_slice($$
  select * from scale_factor_distr where c1 = 2 or c1 = 5 or c1 = 9;
$$, 1);

reset optimizer;

-- Direct dispatch in ORCA: scaleFactor==1
--
--  Aggregate  (cost=0.00..431.00 rows=1 width=8)
--    ->  Gather Motion 1:1  (slice1; segments: 1)  (cost=0.00..431.00 rows=3 width=1)
--          ->  Result  (cost=0.00..431.00 rows=3 width=1)
--                ->  Sequence  (cost=0.00..431.00 rows=3 width=4)
--                      ->  Partition Selector for scale_factor_partitioned (dynamic scan id: 1)  (cost=10.00..100.00 rows=100 width=4)
--                            Partitions selected: 1 (out of 9)
--                      ->  Dynamic Seq Scan on scale_factor_partitioned (dynamic scan id: 1)  (cost=0.00..431.00 rows=3 width=4)
--                            Filter: (a = 1)
--
select * from get_plan_rows_in_slice($$
  select count(*) from scale_factor_partitioned where a = 1;
$$, 1);

-- Partial table: SeqScan, scaleFactor==2
-- (fallback to PO)
--
--  Gather Motion 2:1  (slice1; segments: 2)  (cost=0.00..2.10 rows=10 width=8)
--    ->  Seq Scan on scale_factor_part_distr  (cost=0.00..2.10 rows=5 width=8)
--
select * from get_plan_rows_in_slice($$
  select * from scale_factor_part_distr;
$$, 1);

-- Explicit Gather Motion: scaleFactor==1,
-- TODO: ?! Update scaleFactor==3
-- (fallback to PO)
--
--  Explicit Gather Motion 3:1  (slice2; segments: 3)  (cost=10000000000.00..10000000006.98 rows=100 width=28)
--    ->  Update on scale_factor_repl a  (cost=10000000000.00..10000000006.98 rows=34 width=28)
--          ->  Nested Loop  (cost=10000000000.00..10000000006.98 rows=100 width=28)
--                ->  Seq Scan on scale_factor_repl a  (cost=0.00..1.10 rows=10 width=14)
--                ->  Materialize  (cost=0.00..2.65 rows=10 width=14)
--                      ->  Broadcast Motion 2:3  (slice1; segments: 2)  (cost=0.00..2.50 rows=15 width=14)
--                            ->  Seq Scan on scale_factor_part_distr b  (cost=0.00..2.10 rows=5 width=14)
--
select * from get_plan_rows_in_slice($$
  update scale_factor_repl a set c1 = b.c2 from scale_factor_part_distr b returning *;
$$, 2);

select * from get_motion_snd_recv($$
  update scale_factor_repl a set c1 = b.c2 from scale_factor_part_distr b returning *;
$$);

-- (Explicit) Redistribute Motion
-- (fallback to PO)
--
--  Delete on scale_factor_part_distr a  (cost=2.43..5.68 rows=5 width=16)
--    ->  Explicit Redistribute Motion 3:2  (slice2; segments: 3)  (cost=2.43..5.68 rows=4 width=16)
--          ->  Hash Join  (cost=2.43..5.68 rows=4 width=16)
--                Hash Cond: (b.c1 = a.c2)
--                ->  Seq Scan on scale_factor_rand_distr b  (cost=0.00..3.11 rows=4 width=10)
--                ->  Hash  (cost=2.30..2.30 rows=4 width=14)
--                      ->  Redistribute Motion 2:3  (slice1; segments: 2)  (cost=0.00..2.30 rows=5 width=14)
--                            Hash Key: a.c2
--                            ->  Seq Scan on scale_factor_part_distr a  (cost=0.00..2.10 rows=5 width=14)
--
select * from get_plan_rows_in_slice ($$
  delete from scale_factor_part_distr a using scale_factor_rand_distr b where b.c1=a.c2;
$$, 2);

select * from get_motion_snd_recv ($$
  delete from scale_factor_part_distr a using scale_factor_rand_distr b where b.c1=a.c2;
$$);

-- Gather Motion: scaleFactor == 1
-- slice0 in ORCA: scaleFactor == 1
--
--  WindowAgg  (cost=0.00..862.00 rows=11 width=12)
--    Order By: scale_factor_distr.c1
--    ->  Gather Motion 3:1  (slice3; segments: 3)  (cost=0.00..862.00 rows=11 width=4)
--          Merge Key: scale_factor_distr.c1
--          ->  Sort  (cost=0.00..862.00 rows=4 width=4)
--                Sort Key: scale_factor_distr.c1
--                ->  Hash Join  (cost=0.00..862.00 rows=4 width=4)
--                      Hash Cond: (scale_factor_distr.c2 = scale_factor_distr_1.c2)
--                      ->  Redistribute Motion 3:3  (slice1; segments: 3)  (cost=0.00..431.00 rows=4 width=8)
--                            Hash Key: scale_factor_distr.c2
--                            ->  Seq Scan on scale_factor_distr  (cost=0.00..431.00 rows=4 width=8)
--                      ->  Hash  (cost=431.00..431.00 rows=4 width=4)
--                            ->  Redistribute Motion 3:3  (slice2; segments: 3)  (cost=0.00..431.00 rows=4 width=4)
--                                  Hash Key: scale_factor_distr_1.c2
--                                  ->  Seq Scan on scale_factor_distr scale_factor_distr_1  (cost=0.00..431.00 rows=4 width=4)
--
select * from get_plan_rows_in_slice($$
  select t1.c1, row_number() over (order by t1.c1 desc) from scale_factor_distr t1 join scale_factor_distr t2 using(c2);
$$, 0);

select * from get_motion_snd_recv($$
  select t1.c1, row_number() over (order by t1.c1 desc) from scale_factor_distr t1 join scale_factor_distr t2 using(c2);
$$);

-- start_ignore
drop table scale_factor_repl;
drop table scale_factor_distr;
drop table scale_factor_rand_distr;
drop table scale_factor_part_distr;
drop table scale_factor_partitioned;
drop table scale_factor_master_only;
drop function get_motion_snd_recv(text);
drop function get_plan_rows_in_slice(text, int);
drop function get_explain_xml_output(text);
-- end_ignore
