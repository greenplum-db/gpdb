-- detailed background please refer to the thread in gpdb-dev
-- https://groups.google.com/a/greenplum.org/g/gpdb-dev/c/Y4ajINeKeUw
set gp_interconnect_queue_depth =1;
set gp_interconnect_snd_queue_depth =1;
set gp_autostats_mode = none;
set disable_cost = 1e20;

-- ==============================================
-- outer plan & inner plan deadlock
-- ==============================================
create table t_motion_deadlock_1(a int, b int);
create table t_motion_deadlock_2(a int, b int);

insert into t_motion_deadlock_1 select i,i from generate_series(1, 30000)i;
delete from t_motion_deadlock_1 where gp_segment_id <> 1;

insert into t_motion_deadlock_2 select i,i from generate_series(1, 30000)i;
delete from t_motion_deadlock_2 where gp_segment_id <> 0;
insert into t_motion_deadlock_2
select y.a, x.b from t_motion_deadlock_1 x, t_motion_deadlock_2 y limit 10;

-- below plan should redistribute both inner and outer
-- hash join
explain (costs off, verbose)
select count(1)
from
  t_motion_deadlock_1 x,
  t_motion_deadlock_2 y
where x.b = y.b;

select count(1)
from
  t_motion_deadlock_1 x,
  t_motion_deadlock_2 y
where x.b = y.b;

-- nestloop join
set enable_hashjoin = 0; set optimizer_enable_hashjoin = 0;
set enable_nestloop = 1;
set enable_mergejoin = 0; set optimizer_enable_mergejoin = 0;

explain (costs off, verbose)
select count(1)
from
  t_motion_deadlock_1 x,
  t_motion_deadlock_2 y
where x.b = y.b;

select count(1)
from
  t_motion_deadlock_1 x,
  t_motion_deadlock_2 y
where x.b = y.b;

-- merge join is OK, but lets also put a case here
set enable_hashjoin = 0; set optimizer_enable_hashjoin = 0;
set enable_nestloop = 0; 
set enable_mergejoin = 1; set optimizer_enable_mergejoin = 1;

explain (costs off, verbose)
select count(1)
from
  t_motion_deadlock_1 x,
  t_motion_deadlock_2 y
where x.b = y.b;

select count(1)
from
  t_motion_deadlock_1 x,
  t_motion_deadlock_2 y
where x.b = y.b;

reset enable_hashjoin; reset optimizer_enable_hashjoin;
reset enable_nestloop;
reset enable_mergejoin; reset optimizer_enable_mergejoin;

drop table t_motion_deadlock_1;
drop table t_motion_deadlock_2;

-- ==============================================
-- outer plan & joinqual
-- ==============================================
create table t_motion_deadlock_1(a int, b int);
create table t_motion_deadlock_2(a int, b int);
create table t_motion_deadlock_3(a int, b int);

insert into t_motion_deadlock_1 select i,i from generate_series(1, 10000)i;
delete from t_motion_deadlock_1 where gp_segment_id <> 1;
insert into t_motion_deadlock_2 select i,i from generate_series(1, 30)i;
insert into t_motion_deadlock_3 select i,i from generate_series(1, 10000)i;

-- hash join
explain (costs off, verbose)
select count(1)
from
  t_motion_deadlock_1 x join t_motion_deadlock_2 y
  on x.b = y.a and
     x.b + y.a > (select count(1) from t_motion_deadlock_3 z where z.b < x.a + y.b);

select count(1)
from
  t_motion_deadlock_1 x join t_motion_deadlock_2 y
  on x.b = y.a and
     x.b + y.a > (select count(1) from t_motion_deadlock_3 z where z.b < x.a + y.b);

-- nestloop join
set enable_hashjoin = 0; set optimizer_enable_hashjoin = 0;
set enable_nestloop = 1;
set enable_mergejoin = 0; set optimizer_enable_mergejoin = 0;

explain (costs off, verbose)
select count(1)
from
  t_motion_deadlock_1 x join t_motion_deadlock_2 y
  on x.b = y.a and
     x.b + y.a > (select count(1) from t_motion_deadlock_3 z where z.b < x.a + y.b);

select count(1)
from
  t_motion_deadlock_1 x join t_motion_deadlock_2 y
  on x.b = y.a and
     x.b + y.a > (select count(1) from t_motion_deadlock_3 z where z.b < x.a + y.b);

-- merge join is OK, but lets also put a case here
set enable_hashjoin = 0; set optimizer_enable_hashjoin = 0;
set enable_nestloop = 0;
set enable_mergejoin = 1; set optimizer_enable_mergejoin = 1;

explain (costs off, verbose)
select count(1)
from
  t_motion_deadlock_1 x join t_motion_deadlock_2 y
  on x.b = y.a and
     x.b + y.a > (select count(1) from t_motion_deadlock_3 z where z.b < x.a + y.b);

select count(1)
from
  t_motion_deadlock_1 x join t_motion_deadlock_2 y
  on x.b = y.a and
     x.b + y.a > (select count(1) from t_motion_deadlock_3 z where z.b < x.a + y.b);

reset enable_hashjoin; reset optimizer_enable_hashjoin;
reset enable_nestloop;
reset enable_mergejoin; reset optimizer_enable_mergejoin;

-- ==============================================
-- outer plan & qual
-- ==============================================
-- hash join
explain (costs off, verbose)
select count(1)
from
  t_motion_deadlock_1 x left join t_motion_deadlock_2 y
  on x.b = y.a
where
   x.a is null or exists (select random() from t_motion_deadlock_3 z where z.b < x.a + y.b);

select count(1)
from
  t_motion_deadlock_1 x left join t_motion_deadlock_2 y
  on x.b = y.a
where
   x.a is null or exists (select random() from t_motion_deadlock_3 z where z.b < x.a + y.b);

-- hashed subplan
explain (costs off, verbose)
select count(1)
from
  t_motion_deadlock_1 x left join t_motion_deadlock_2 y
  on x.b = y.a
where
   x.a is null or exists (select random() from t_motion_deadlock_3 z where z.b = x.a + y.b);

select count(1)
from
  t_motion_deadlock_1 x left join t_motion_deadlock_2 y
  on x.b = y.a
where
   x.a is null or exists (select random() from t_motion_deadlock_3 z where z.b = x.a + y.b);

-- nestloop join
set enable_hashjoin = 0; set optimizer_enable_hashjoin = 0;
set enable_nestloop = 1;
set enable_mergejoin = 0; set optimizer_enable_mergejoin = 0;

explain (costs off, verbose)
select count(1)
from
  t_motion_deadlock_1 x left join t_motion_deadlock_2 y
  on x.b = y.a
where
   x.a is null or exists (select random() from t_motion_deadlock_3 z where z.b < x.a + y.b);

select count(1)
from
  t_motion_deadlock_1 x left join t_motion_deadlock_2 y
  on x.b = y.a
where
   x.a is null or exists (select random() from t_motion_deadlock_3 z where z.b < x.a + y.b);

-- hashed subplan
explain (costs off, verbose)
select count(1)
from
  t_motion_deadlock_1 x left join t_motion_deadlock_2 y
  on x.b = y.a
where
   x.a is null or exists (select random() from t_motion_deadlock_3 z where z.b = x.a + y.b);

select count(1)
from
  t_motion_deadlock_1 x left join t_motion_deadlock_2 y
  on x.b = y.a
where
   x.a is null or exists (select random() from t_motion_deadlock_3 z where z.b = x.a + y.b);

-- merge join is OK, but lets also put a case here
set enable_hashjoin = 0; set optimizer_enable_hashjoin = 0;
set enable_nestloop = 0;
set enable_mergejoin = 1; set optimizer_enable_mergejoin = 1;

explain (costs off, verbose)
select count(1)
from
  t_motion_deadlock_1 x left join t_motion_deadlock_2 y
  on x.b = y.a
where
   x.a is null or exists (select random() from t_motion_deadlock_3 z where z.b < x.a + y.b);

select count(1)
from
  t_motion_deadlock_1 x left join t_motion_deadlock_2 y
  on x.b = y.a
where
   x.a is null or exists (select random() from t_motion_deadlock_3 z where z.b < x.a + y.b);

-- hashed subplan
explain (costs off, verbose)
select count(1)
from
  t_motion_deadlock_1 x left join t_motion_deadlock_2 y
  on x.b = y.a
where
   x.a is null or exists (select random() from t_motion_deadlock_3 z where z.b = x.a + y.b);

select count(1)
from
  t_motion_deadlock_1 x left join t_motion_deadlock_2 y
  on x.b = y.a
where
   x.a is null or exists (select random() from t_motion_deadlock_3 z where z.b = x.a + y.b);

reset enable_hashjoin; reset optimizer_enable_hashjoin;
reset enable_nestloop;
reset enable_mergejoin; reset optimizer_enable_mergejoin;

drop table t_motion_deadlock_1;
drop table t_motion_deadlock_2;
drop table t_motion_deadlock_3;

-- ==============================================
-- outer plan & target list
-- ==============================================
create table t_motion_deadlock_1(a int, b int);
create table t_motion_deadlock_2(a int, b int);
create table t_motion_deadlock_3(a int, b int);

insert into t_motion_deadlock_1 select i,i from generate_series(1, 30000)i;
delete from t_motion_deadlock_1 where gp_segment_id <> 1;
insert into t_motion_deadlock_2 select i,i from generate_series(1, 30)i;
insert into t_motion_deadlock_3 select i,i from generate_series(1, 10000)i;

-- hash join
explain (costs off, verbose)
select
  (select count(1) from t_motion_deadlock_3 z where z.a < x.a + y.b ) s
from t_motion_deadlock_1 x join t_motion_deadlock_2 y on x.b = y.a;

select
  (select count(1) from t_motion_deadlock_3 z where z.a < x.a + y.b ) s
from t_motion_deadlock_1 x join t_motion_deadlock_2 y on x.b = y.a;

-- nestloop join
set enable_hashjoin = 0; set optimizer_enable_hashjoin = 0;
set enable_nestloop = 1;
set enable_mergejoin = 0; set optimizer_enable_mergejoin = 0;

explain (costs off, verbose)
select
  (select count(1) from t_motion_deadlock_3 z where z.a < x.a + y.b ) s
from t_motion_deadlock_1 x join t_motion_deadlock_2 y on x.b = y.a;

select
  (select count(1) from t_motion_deadlock_3 z where z.a < x.a + y.b ) s
from t_motion_deadlock_1 x join t_motion_deadlock_2 y on x.b = y.a;

-- merge join is OK, but lets also put a case here
set enable_hashjoin = 0; set optimizer_enable_hashjoin = 0;
set enable_nestloop = 0;
set enable_mergejoin = 1; set optimizer_enable_mergejoin = 1;

explain (costs off, verbose)
select
  (select count(1) from t_motion_deadlock_3 z where z.a < x.a + y.b ) s
from t_motion_deadlock_1 x join t_motion_deadlock_2 y on x.b = y.a;

select
  (select count(1) from t_motion_deadlock_3 z where z.a < x.a + y.b ) s
from t_motion_deadlock_1 x join t_motion_deadlock_2 y on x.b = y.a;

drop table t_motion_deadlock_1;
drop table t_motion_deadlock_2;
drop table t_motion_deadlock_3;

-- reset all GUCs
reset enable_hashjoin; reset optimizer_enable_hashjoin;
reset enable_nestloop;
reset enable_mergejoin; reset optimizer_enable_mergejoin;

reset gp_interconnect_queue_depth;
reset gp_interconnect_snd_queue_depth;
reset gp_autostats_mode;
reset disable_cost;
