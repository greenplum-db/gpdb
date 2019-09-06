--
-- Direct Dispatch Test when optimizer is on
--
-- start_ignore
set optimizer_log=on;
set optimizer_force_multistage_agg=on;
-- end_ignore

set test_print_direct_dispatch_info=on; 

set gp_autostats_mode = 'None';

-- composite keys
create table dd_multicol_1(a int, b int) distributed by (a,b);
create table dd_multicol_2(a int, b int) distributed by (b,a);

insert into dd_multicol_1 select g, g%2 from generate_series(1, 100) g;
insert into dd_multicol_1 values(null, null);
insert into dd_multicol_1 values(1, null);
insert into dd_multicol_1 values(null, 1);

analyze dd_multicol_1;

insert into dd_multicol_2 select g, g%2 from generate_series(1, 100) g;
insert into dd_multicol_2 values(null, null);
insert into dd_multicol_2 values(1, null);
insert into dd_multicol_2 values(null, 1);

-- composite distr key

select * from dd_multicol_1 where a in (1,3) and b in (1,2);

select * from dd_multicol_1 where a = 1 and b in (1,2);

select * from dd_multicol_1 where (a=1 and b=1) or (a=2 and b=2);

select * from dd_multicol_1 where (a=1 or a=3) and (b=1 or b=2);

select * from dd_multicol_1 where a is null or a=1;

select * from dd_multicol_1 where (a is null or a=1) and b=2;

select * from dd_multicol_1 where (a,b) in ((1,2),(3,4)); 

-- composite distr key: projections

select b from dd_multicol_1 where (a=1 or a=3) and (b=1 or b=2);

select count(*) from dd_multicol_1;
select count(*) from dd_multicol_2;

-- simple predicates
select * from dd_multicol_1 where a=1 and b=1;

select * from dd_multicol_1 where a=1 and b=2;

select * from dd_multicol_1 where b=1 and a=2;

select * from dd_multicol_1 where a is null and b=1;

select * from dd_multicol_1 where a is null and b is null;

select * from dd_multicol_2 where a=1 and b=1;

select * from dd_multicol_2 where a=1 and b=2;

select * from dd_multicol_2 where b=1 and a=2;

select * from dd_multicol_2 where a is null and b=1;

select * from dd_multicol_2 where a is null and b is null;

-- projections
select b from dd_multicol_1 where a=1 and b=1;
 
select a+b from dd_multicol_1 where a=1 and b=1;

select 'one' from dd_multicol_1 where a=1 and b=1;

select a, 'one' from dd_multicol_1 where a=1 and b=1;

-- group by and sort
select a, count(*) from dd_multicol_1 where a=1 and b=1 group by a,b;

select a, count(*) from dd_multicol_1 where a=1 and b=1 group by a,b  order by a,b;

-- incomplete specification
select * from dd_multicol_1 where a=1;

select * from dd_multicol_1 where a>80 and b=1;

select * from dd_multicol_1 where a<=5 and b>0;

reset optimizer_force_multistage_agg;
