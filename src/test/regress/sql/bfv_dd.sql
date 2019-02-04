--
-- Direct Dispatch Test when optimizer is on
--
-- start_ignore
set optimizer_log=on;
-- end_ignore

set test_print_direct_dispatch_info=on; 

set gp_autostats_mode = 'None';

-- create table with distribution on a single table

create table dd_singlecol_1(a int, b int) distributed by (a);
insert into dd_singlecol_1 select g, g%15 from generate_series(1,100) g;
insert into dd_singlecol_1 values(null, null);

analyze dd_singlecol_1;

-- ctas tests
create table dd_ctas_1 as select * from dd_singlecol_1 where a=1 distributed by (a);
create table dd_ctas_2  as select * from dd_singlecol_1 where a is NULL distributed by (a);

select * from dd_ctas_1;
select * from dd_ctas_2;

drop table dd_ctas_1;
drop table dd_ctas_2;

-- direct dispatch and queries having with clause

with cte as (select * from dd_singlecol_1 where a=1) select * from cte;

with cte as (select * from dd_singlecol_1) select * from cte where a=1;

with cte1 as (with cte2 as (select * from dd_singlecol_1) select * from cte2 where a=1) select * from cte1;

-- negative tests: joins not supported

with cte as (select * from dd_singlecol_1) select * from cte c1, cte c2 where c1.a=1 and c2.a=1 and c1.a=c2.a limit 10;

-- cte with function scans
with cte as (select generate_series(1,10) g)  select * from  dd_singlecol_1 t1, cte where t1.a=cte.g and t1.a=1 limit 100;

-- single column distr key

select * from dd_singlecol_1 where a in (10,11,12);

select * from dd_singlecol_1 where a=10 or a=11 or a=12;

select * from dd_singlecol_1 where a is null or a=1;

-- projections and disjunction

select b from dd_singlecol_1 where a=1 or a=2;

-- single column distr key, values hash to the same segment

select * from dd_singlecol_1 where a=10 or a=11;

select * from dd_singlecol_1 where a in (10, 11);

select * from dd_singlecol_1 where a is null or a=2;

select * from dd_singlecol_1 where (a,b) in ((10,2),(11,3));

select * from dd_singlecol_1 where a between 10 and 11;

-- partitioned tables

create table dd_part_singlecol(a int, b int, c int) distributed by (a) partition by range (b) 
(start(1) end(100) every (20), default partition extra);

insert into dd_part_singlecol select g, g*2, g*3 from generate_series(1,49) g;
insert into dd_part_singlecol values (NULL, NULL);

-- disjunction with partitioned tables

select * from dd_part_singlecol where a in (10,11,12);

select * from dd_part_singlecol where a=10 or a=11 or a=12;

select * from dd_part_singlecol where a is null or a=1;

-- simple predicates
select * from dd_part_singlecol where a=1;

select * from dd_part_singlecol where a=1 and b=2;

select * from dd_part_singlecol where a = 1 and b<10;

select * from dd_part_singlecol where a is null;

select * from dd_part_singlecol where a is null and b is null;

-- complex queries
-- projections
select b from dd_part_singlecol where a=1;
 
select a+b from dd_part_singlecol where a=1;

select 'one' from dd_part_singlecol where a=1;

select a, 'one' from dd_part_singlecol where a=1;

-- group by and sort
select a, count(*) from dd_part_singlecol where a=1 group by a;

select a, count(*) from dd_part_singlecol where a=1 group by a order by a;

-- indexes
create table dd_singlecol_idx(a int, b int, c int) distributed by (a);
create index sc_idx_b on dd_singlecol_idx(b);
create index sc_idx_bc on dd_singlecol_idx(b,c);

insert into dd_singlecol_idx select g, g%5,g%5 from generate_series(1,100) g;
insert into dd_singlecol_idx values(null, null);

create table dd_singlecol_idx2(a int, b int, c int) distributed by (a);
create index sc_idx_a on dd_singlecol_idx2(a);

insert into dd_singlecol_idx2 select g, g%5,g%5 from generate_series(1,100) g;
insert into dd_singlecol_idx2 values(null, null);

analyze dd_singlecol_idx;
analyze dd_singlecol_idx2;

-- disjunction with index scans

select * from dd_singlecol_idx where (a=1 or a=2) and b<2;

select 'one' from dd_singlecol_idx where (a=1 or a=2) and b=1;

select a, count(*) from dd_singlecol_idx where (a=1 or a=2) and b=1  group by a;

select count(*) from dd_singlecol_idx;

-- create table with bitmap indexes
create table dd_singlecol_bitmap_idx(a int, b int, c int) distributed by (a);
create index sc_bitmap_idx_b on dd_singlecol_bitmap_idx using bitmap (b);
create index sc_bitmap_idx_c on dd_singlecol_bitmap_idx using bitmap (c);

insert into dd_singlecol_bitmap_idx select g, g%5,g%5 from generate_series(1,100) g;
insert into dd_singlecol_bitmap_idx values(null, null);

analyze dd_singlecol_bitmap_idx;

-- disjunction with bitmap index scans
select * from dd_singlecol_bitmap_idx where (a=1 or a=2) and b<2;

select * from dd_singlecol_bitmap_idx where (a=1 or a=2) and b=2 and c=2;

select * from dd_singlecol_bitmap_idx where (a=1 or a=2) and (b=2 or c=2);

select * from dd_singlecol_bitmap_idx where a<5 and b=1;

-- conjunction with bitmap indexes
select * from dd_singlecol_bitmap_idx where a=1 and b=0;

select * from dd_singlecol_bitmap_idx where a=1 and b<3;

select * from dd_singlecol_bitmap_idx where a=1 and b>=1 and c<2;
 
select * from dd_singlecol_bitmap_idx where a=1 and b=3 and c=3;

-- bitmap indexes on part tables
create table dd_singlecol_part_bitmap_idx(a int, b int, c int) 
distributed by (a)
partition by range (b) 
(start(1) end(100) every (20), default partition extra);;
create index sc_part_bitmap_idx_b on dd_singlecol_part_bitmap_idx using bitmap(b);

insert into dd_singlecol_part_bitmap_idx select g, g%5,g%5 from generate_series(1,100) g;
insert into dd_singlecol_part_bitmap_idx values(null, null);

analyze dd_singlecol_part_bitmap_idx;

-- bitmap indexes on partitioned tables
select * from dd_singlecol_part_bitmap_idx where a=1 and b=0;

select * from dd_singlecol_part_bitmap_idx where a=1 and b<3;

select * from dd_singlecol_part_bitmap_idx where a=1 and b>=1 and c=3;

-- bitmap bool op
select * from dd_singlecol_bitmap_idx
where a=1 and b=3 and c=3;

-- multi column index
create table dd_multicol_idx(a int, b int, c int) distributed by (a,b);
create index mc_idx_b on dd_multicol_idx(c);
insert into dd_multicol_idx
select g, g%5, g%5 from generate_series(1,100) g;
insert into dd_multicol_idx values(null, null);
analyze dd_multicol_idx;

select count(*) from dd_multicol_idx;

-- simple index predicates

select * from dd_singlecol_idx where a=1 and b=0;

select * from dd_singlecol_idx where a=1 and b<3;

select * from dd_singlecol_idx where a<5 and b=1;

select * from dd_singlecol_idx where a=1 and b>=1 and c<2;

select * from dd_singlecol_idx2 where a=1;

select * from dd_singlecol_idx2 where a=1 and b>=1;

-- projection
select 'one' from dd_singlecol_idx where a=1 and b=1;

select a+b from dd_singlecol_idx where a=1 and b=1;

-- group by
select a, count(*) from dd_singlecol_idx where a=1 and b=1  group by a;

-- multicol
select * from dd_multicol_idx where a=1 and b=1 and c<5;

select * from dd_multicol_idx where (a=10 or a=11) and (b=1 or b=5) and c=1;

-- indexes on partitioned tables 
create table dd_singlecol_part_idx(a int, b int, c int) 
distributed by (a)
partition by range (b) 
(start(1) end(100) every (20), default partition extra);;
create index sc_part_idx_b on dd_singlecol_part_idx(b);

insert into dd_singlecol_part_idx select g, g%5,g%5 from generate_series(1,100) g;
insert into dd_singlecol_part_idx values(null, null);

create table dd_singlecol_part_idx2(a int, b int, c int) 
distributed by (a)
partition by range (b) 
(start(1) end(100) every (20), default partition extra);;
create index sc_part_idx_a on dd_singlecol_part_idx2(a);

insert into dd_singlecol_part_idx2 select g, g%5,g%5 from generate_series(1,100) g;
insert into dd_singlecol_part_idx2 values(null, null);

analyze dd_singlecol_part_idx;
analyze dd_singlecol_part_idx2;

-- indexes on partitioned tables
select * from dd_singlecol_part_idx where a=1 and b>0;

select * from dd_singlecol_part_idx2 where a=1;

select * from dd_singlecol_part_idx2 where a=1 and b>=1;

create table dd_singlecol_2(a int, b int) distributed by (b);
create table dd_singlecol_dropped(a int, b int, c int) distributed by (b);
alter table dd_singlecol_dropped drop column a;

insert into dd_singlecol_2
select g, g%10 from generate_series(1,100) g;

insert into dd_singlecol_dropped
select g, g%5 from generate_series(1,100) g;

-- aggregates
select count(*) from dd_singlecol_1;

select count(*) from dd_singlecol_2;

select count(*) from dd_singlecol_dropped;

-- simple predicates
select * from dd_singlecol_1 where a=1;

select * from dd_singlecol_2 where b=1;

select * from dd_singlecol_dropped where b=1;

select * from dd_singlecol_1 where a = 1 and b=2;

select * from dd_singlecol_1 where a = 1 and b<10;

select * from dd_singlecol_1 where a is null;

-- projections
select b from dd_singlecol_1 where a=1;
 
select a+b from dd_singlecol_1 where a=1;

select 'one' from dd_singlecol_1 where a=1;

select a, 'one' from dd_singlecol_1 where a=1;

-- group by and sort
select a, count(*) from dd_singlecol_1 where a=1 group by a;

select a, count(*) from dd_singlecol_1 where a=1 group by a order by a;

-- inner joins
select * from dd_singlecol_1 t1, dd_singlecol_2 t2 where t1.a=t2.a and t1.a=1;

select * from dd_singlecol_1 t1, dd_singlecol_2 t2 where t1.a=t2.b and t1.a=1;

select * from dd_singlecol_1 t1, dd_singlecol_2 t2 where t1.b>t2.a and t1.a=1;

-- outer joins
select * from dd_singlecol_1 t1 left outer join dd_singlecol_2 t2 on (t1.a=t2.a) where t1.a=1;

select * from dd_singlecol_1 t1 left outer join dd_singlecol_2 t2 on (t1.a=t2.b) where t1.a=1 and t2.b=1;

select * from dd_singlecol_1 t1 left outer join dd_singlecol_2 t2 on (t1.b=t2.b) where t1.a=1;

select * from dd_singlecol_2 t2 left outer join dd_singlecol_1 t1 on (t1.b=t2.b) where t1.a=1;

-- subqueries

select * from dd_singlecol_1 t1 where a=1 and b < (select count(*) from dd_singlecol_2 t2 where t2.a=t1.a);

select * from dd_singlecol_1 t1 where a=1 and b in (select count(*) from dd_singlecol_2 t2 where t2.a<=t1.a);

select t1.a, t1.b, (select sum(t2.a+t2.b) from dd_singlecol_2 t2 where t2.b=1) from dd_singlecol_1 t1  where t1.a=1; 

-- joins with function scans

select * from  dd_singlecol_1 t1, generate_series(1,10) g where t1.a=g.g and t1.a=1 limit 10;

-- negative cases

-- unsupported predicates
select * from dd_singlecol_1 where a>1 and a<5;

select * from dd_singlecol_1 where a=1 or b=5;

-- group by and sort
select b, count(*) from dd_singlecol_1 where a=1 group by b;

select b, count(*) from dd_singlecol_1 where a=1 group by b order by b;

-- randomly distributed tables
create table dd_random(a int, b int) distributed randomly;
insert into dd_random select g, g%15 from generate_series(1, 100) g;

-- non hash distributed tables
select * from dd_random  where a=1;

drop table dd_singlecol_1;
drop table dd_ctas_1;
drop table dd_ctas_2;
drop table dd_part_singlecol;
drop table dd_singlecol_idx;
drop table dd_singlecol_idx2;
drop table dd_singlecol_bitmap_idx;
drop table dd_singlecol_part_bitmap_idx;
drop table dd_multicol_idx;
drop table dd_singlecol_part_idx;
drop table dd_singlecol_part_idx2;
drop table dd_singlecol_2;
drop table dd_singlecol_dropped;
drop table dd_random;
