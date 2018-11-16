set allow_system_table_mods=true;

-- Hash distributed tables
Create table ao_t1_reshuffle(a int, b int, c int) with (appendonly=true) distributed by (a);
update gp_distribution_policy  set numsegments=2 where localoid='ao_t1_reshuffle'::regclass;
insert into ao_t1_reshuffle select i,i,0 from generate_series(1,100) I;
Update ao_t1_reshuffle set c = gp_segment_id;

Select gp_segment_id, count(*) from ao_t1_reshuffle group by gp_segment_id;

begin;
Alter table ao_t1_reshuffle set with (reshuffle);
Select gp_segment_id, count(*) from ao_t1_reshuffle group by gp_segment_id;
abort;

Select gp_segment_id, count(*) from ao_t1_reshuffle group by gp_segment_id;

Alter table ao_t1_reshuffle set with (reshuffle);

Select gp_segment_id, count(*) from ao_t1_reshuffle group by gp_segment_id;

select numsegments from gp_distribution_policy where localoid='ao_t1_reshuffle'::regclass;
drop table ao_t1_reshuffle;


Create table ao_t1_reshuffle(a int, b int, c int) with (appendonly=true, OIDS) distributed by (a,b);
update gp_distribution_policy  set numsegments=1 where localoid='ao_t1_reshuffle'::regclass;
insert into ao_t1_reshuffle select i,i,0 from generate_series(1,100) I;
Update ao_t1_reshuffle set c = gp_segment_id;

Select gp_segment_id, count(*) from ao_t1_reshuffle group by gp_segment_id;

begin;
Alter table ao_t1_reshuffle set with (reshuffle);
Select gp_segment_id, count(*) from ao_t1_reshuffle group by gp_segment_id;
abort;

Select gp_segment_id, count(*) from ao_t1_reshuffle group by gp_segment_id;

Alter table ao_t1_reshuffle set with (reshuffle);

Select gp_segment_id, count(*) from ao_t1_reshuffle group by gp_segment_id;

select numsegments from gp_distribution_policy where localoid='ao_t1_reshuffle'::regclass;
drop table ao_t1_reshuffle;

-- Test NULLs.
Create table ao_t1_reshuffle(a int, b int, c int) with (appendonly=true) distributed by (a,b,c);
update gp_distribution_policy  set numsegments=2 where localoid='ao_t1_reshuffle'::regclass;
insert into ao_t1_reshuffle values
  (1,    1,    1   ),
  (null, 2,    2   ),
  (3,    null, 3   ),
  (4,    4,    null),
  (null, null, 5   ),
  (null, 6,    null),
  (7,    null, null),
  (null, null, null);
Select gp_segment_id, count(*) from ao_t1_reshuffle group by gp_segment_id;

begin;
Alter table ao_t1_reshuffle set with (reshuffle);
Select gp_segment_id, count(*) from ao_t1_reshuffle group by gp_segment_id;
abort;

Select gp_segment_id, count(*) from ao_t1_reshuffle group by gp_segment_id;

Alter table ao_t1_reshuffle set with (reshuffle);

Select gp_segment_id, count(*) from ao_t1_reshuffle group by gp_segment_id;

select numsegments from gp_distribution_policy where localoid='ao_t1_reshuffle'::regclass;
drop table ao_t1_reshuffle;

Create table ao_t1_reshuffle(a int, b int, c int) with (appendonly=true) distributed by (a) partition by list(b) (partition ao_t1_reshuffle_1 values(1), partition ao_t1_reshuffle_2 values(2), default partition other);
update gp_distribution_policy set numsegments = 1 where localoid in (select oid from pg_class where relname like 'ao_t1_reshuffle%');
insert into ao_t1_reshuffle select i,i,0 from generate_series(1,100) I;

Select gp_segment_id, count(*) from ao_t1_reshuffle group by gp_segment_id;

begin;
Alter table ao_t1_reshuffle set with (reshuffle);
Select gp_segment_id, count(*) from ao_t1_reshuffle group by gp_segment_id;
abort;

Select gp_segment_id, count(*) from ao_t1_reshuffle group by gp_segment_id;

Alter table ao_t1_reshuffle set with (reshuffle);

Select gp_segment_id, count(*) from ao_t1_reshuffle group by gp_segment_id;

select numsegments from gp_distribution_policy where localoid='ao_t1_reshuffle'::regclass;
drop table ao_t1_reshuffle;

-- Random distributed tables
Create table ao_r1_reshuffle(a int, b int, c int) with (appendonly=true) distributed randomly;
update gp_distribution_policy  set numsegments=2 where localoid='ao_r1_reshuffle'::regclass;
insert into ao_r1_reshuffle select i,i,0 from generate_series(1,100) I;
Update ao_r1_reshuffle set c = gp_segment_id;

Select count(*) from ao_r1_reshuffle;
Select count(*) > 0 from ao_r1_reshuffle where gp_segment_id=2;

begin;
Alter table ao_r1_reshuffle set with (reshuffle);
Select count(*) from ao_r1_reshuffle;
Select count(*) > 0 from ao_r1_reshuffle where gp_segment_id=2;
abort;

Select count(*) from ao_r1_reshuffle;
Select count(*) > 0 from ao_r1_reshuffle where gp_segment_id=2;

Alter table ao_r1_reshuffle set with (reshuffle);

Select count(*) from ao_r1_reshuffle;
Select count(*) > 0 from ao_r1_reshuffle where gp_segment_id=2;

select numsegments from gp_distribution_policy where localoid='ao_r1_reshuffle'::regclass;
drop table ao_r1_reshuffle;

Create table ao_r1_reshuffle(a int, b int, c int) with (appendonly=true, OIDS) distributed randomly;
update gp_distribution_policy  set numsegments=2 where localoid='ao_r1_reshuffle'::regclass;
insert into ao_r1_reshuffle select i,i,0 from generate_series(1,100) I;
Update ao_r1_reshuffle set c = gp_segment_id;

Select count(*) from ao_r1_reshuffle;
Select count(*) > 0 from ao_r1_reshuffle where gp_segment_id=2;

begin;
Alter table ao_r1_reshuffle set with (reshuffle);
Select count(*) from ao_r1_reshuffle;
Select count(*) > 0 from ao_r1_reshuffle where gp_segment_id=2;
abort;

Select count(*) from ao_r1_reshuffle;
Select count(*) > 0 from ao_r1_reshuffle where gp_segment_id=2;

Alter table ao_r1_reshuffle set with (reshuffle);

Select count(*) from ao_r1_reshuffle;
Select count(*) > 0 from ao_r1_reshuffle where gp_segment_id=2;

select numsegments from gp_distribution_policy where localoid='ao_r1_reshuffle'::regclass;
drop table ao_r1_reshuffle;

Create table ao_r1_reshuffle(a int, b int, c int) with (appendonly=true) distributed randomly partition by list(b) (partition ao_r1_reshuffle_1 values(1), partition ao_r1_reshuffle_2 values(2), default partition other);
update gp_distribution_policy set numsegments = 2 where localoid in (select oid from pg_class where relname like 'ao_r1_reshuffle%');
insert into ao_r1_reshuffle select i,i,0 from generate_series(1,100) I;

Select count(*) from ao_r1_reshuffle;
Select count(*) > 0 from ao_r1_reshuffle where gp_segment_id=2;

begin;
Alter table ao_r1_reshuffle set with (reshuffle);
Select count(*) from ao_r1_reshuffle;
Select count(*) > 0 from ao_r1_reshuffle where gp_segment_id=2;
abort;

Select count(*) from ao_r1_reshuffle;
Select count(*) > 0 from ao_r1_reshuffle where gp_segment_id=2;

Alter table ao_r1_reshuffle set with (reshuffle);

Select count(*) from ao_r1_reshuffle;
Select count(*) > 0 from ao_r1_reshuffle where gp_segment_id=2;

select numsegments from gp_distribution_policy where localoid='ao_r1_reshuffle'::regclass;
drop table ao_r1_reshuffle;

-- Replicated tables
Create table ao_r1_reshuffle(a int, b int, c int) with (appendonly=true) distributed replicated;

select update_numsegments_in_policy('ao_r1_reshuffle'::regclass, 1);

insert into ao_r1_reshuffle select i,i,0 from generate_series(1,100) I;

Select gp_segment_id, count(*) from gp_dist_random('ao_r1_reshuffle') group by gp_segment_id;

begin;
Alter table ao_r1_reshuffle set with (reshuffle);
Select gp_segment_id, count(*) from gp_dist_random('ao_r1_reshuffle') group by gp_segment_id;
abort;

Select gp_segment_id, count(*) from gp_dist_random('ao_r1_reshuffle') group by gp_segment_id;

Alter table ao_r1_reshuffle set with (reshuffle);

Select gp_segment_id, count(*) from gp_dist_random('ao_r1_reshuffle') group by gp_segment_id;

select numsegments from gp_distribution_policy where localoid='ao_r1_reshuffle'::regclass;
drop table ao_r1_reshuffle;

--
Create table ao_r1_reshuffle(a int, b int, c int) with (appendonly=true, OIDS) distributed replicated;

select update_numsegments_in_policy('ao_r1_reshuffle'::regclass, 1);

insert into ao_r1_reshuffle select i,i,0 from generate_series(1,100) I;

Select gp_segment_id, count(*) from gp_dist_random('ao_r1_reshuffle') group by gp_segment_id;

begin;
Alter table ao_r1_reshuffle set with (reshuffle);
Select gp_segment_id, count(*) from gp_dist_random('ao_r1_reshuffle') group by gp_segment_id;
abort;

Select gp_segment_id, count(*) from gp_dist_random('ao_r1_reshuffle') group by gp_segment_id;

Alter table ao_r1_reshuffle set with (reshuffle);

Select gp_segment_id, count(*) from gp_dist_random('ao_r1_reshuffle') group by gp_segment_id;

select numsegments from gp_distribution_policy where localoid='ao_r1_reshuffle'::regclass;
drop table ao_r1_reshuffle;

-- multi-level partition tables
CREATE TABLE part_ao_t1_reshuffle(a int, b int, c int, d int, e int) with (appendonly=true)
DISTRIBUTED BY(a)
PARTITION BY RANGE (b)
    SUBPARTITION BY RANGE (c)
        SUBPARTITION TEMPLATE (
            START(1) END (3) EVERY(1),
            DEFAULT SUBPARTITION others_c)

    SUBPARTITION BY LIST (d)
        SUBPARTITION TEMPLATE (
            SUBPARTITION one VALUES (1),
            SUBPARTITION two VALUES (2),
            SUBPARTITION three VALUES (3),
            DEFAULT SUBPARTITION others_d)

( START (1) END (2) EVERY (1),
    DEFAULT PARTITION other_b);

update gp_distribution_policy  set numsegments=2 where localoid in (select oid from pg_class where relname like 'part_ao_t1_reshuffle%');
insert into part_ao_t1_reshuffle select i,i%3,i%4,i%5,i from generate_series(1,100) I;
Update part_ao_t1_reshuffle set e = gp_segment_id;

Select gp_segment_id, count(*) from part_ao_t1_reshuffle group by gp_segment_id;

begin;
Alter table part_ao_t1_reshuffle set with (reshuffle);
Select gp_segment_id, count(*) from part_ao_t1_reshuffle group by gp_segment_id;
abort;

Select gp_segment_id, count(*) from part_ao_t1_reshuffle group by gp_segment_id;

Alter table part_ao_t1_reshuffle set with (reshuffle);

Select gp_segment_id, count(*) from part_ao_t1_reshuffle group by gp_segment_id;

select numsegments from gp_distribution_policy where localoid='part_ao_t1_reshuffle'::regclass;
drop table part_ao_t1_reshuffle;

--
CREATE TABLE part_ao_t1_reshuffle(a int, b int, c int, d int, e int) with (appendonly=true)
DISTRIBUTED RANDOMLY
PARTITION BY RANGE (b)
    SUBPARTITION BY RANGE (c)
        SUBPARTITION TEMPLATE (
            START(1) END (3) EVERY(1),
            DEFAULT SUBPARTITION others_c)

    SUBPARTITION BY LIST (d)
        SUBPARTITION TEMPLATE (
            SUBPARTITION one VALUES (1),
            SUBPARTITION two VALUES (2),
            SUBPARTITION three VALUES (3),
            DEFAULT SUBPARTITION others_d)

( START (1) END (2) EVERY (1),
    DEFAULT PARTITION other_b);

update gp_distribution_policy  set numsegments=2 where localoid in (select oid from pg_class where relname like 'part_ao_t1_reshuffle%');
insert into part_ao_t1_reshuffle select i,i%3,i%4,i%5,i from generate_series(1,100) I;
Update part_ao_t1_reshuffle set e = gp_segment_id;

Select count(*) from part_ao_t1_reshuffle;
Select count(*) > 0 from part_ao_t1_reshuffle where gp_segment_id=2;

begin;
Alter table part_ao_t1_reshuffle set with (reshuffle);
Select count(*) from part_ao_t1_reshuffle;
Select count(*) > 0 from part_ao_t1_reshuffle where gp_segment_id=2;
abort;

Select count(*) from part_ao_t1_reshuffle;
Select count(*) > 0 from part_ao_t1_reshuffle where gp_segment_id=2;

Alter table part_ao_t1_reshuffle set with (reshuffle);

Select count(*) from part_ao_t1_reshuffle;
Select count(*) > 0 from part_ao_t1_reshuffle where gp_segment_id=2;

select numsegments from gp_distribution_policy where localoid='part_ao_t1_reshuffle'::regclass;
drop table part_ao_t1_reshuffle;


-- only reshuffle leaf partition
CREATE TABLE part_ao_t1_reshuffle(a int, b int, c int, d int, e int) with (appendonly=true)
DISTRIBUTED BY(a)
PARTITION BY RANGE (b)
    SUBPARTITION BY RANGE (c)
        SUBPARTITION TEMPLATE (
            START(1) END (3) EVERY(1),
            DEFAULT SUBPARTITION others_c)

    SUBPARTITION BY LIST (d)
        SUBPARTITION TEMPLATE (
            SUBPARTITION one VALUES (1),
            SUBPARTITION two VALUES (2),
            SUBPARTITION three VALUES (3),
            DEFAULT SUBPARTITION others_d)

( START (1) END (2) EVERY (1),
    DEFAULT PARTITION other_b);

update gp_distribution_policy  set numsegments=2 where localoid in (select oid from pg_class where relname like 'part_ao_t1_reshuffle%');
insert into part_ao_t1_reshuffle select i,i%3,i%4,i%5,i from generate_series(1,100) I;
Update part_ao_t1_reshuffle set e = gp_segment_id;

select gp_segment_id, * from part_ao_t1_reshuffle_1_prt_other_b_2_prt_2_3_prt_others_d;

begin;
Alter table part_ao_t1_reshuffle_1_prt_other_b_2_prt_2_3_prt_others_d set with (reshuffle);
Select gp_segment_id, * from part_ao_t1_reshuffle_1_prt_other_b_2_prt_2_3_prt_others_d;
abort;

Select gp_segment_id, * from part_ao_t1_reshuffle_1_prt_other_b_2_prt_2_3_prt_others_d;
Alter table part_ao_t1_reshuffle_1_prt_other_b_2_prt_2_3_prt_others_d set with (reshuffle);
Select gp_segment_id, * from part_ao_t1_reshuffle_1_prt_other_b_2_prt_2_3_prt_others_d;

-- try to reshuffle root partition, it will raise a notice
Alter table part_ao_t1_reshuffle set with (reshuffle);
Select gp_segment_id, count(*) from part_ao_t1_reshuffle group by gp_segment_id;

drop table part_ao_t1_reshuffle;

-- test reshuffle from full random to full hash distributed
create table ao_t1_reshuffle_full_random (c1 int, c2 int) with (appendonly=true) distributed randomly;
insert into ao_t1_reshuffle_full_random select * from generate_series(1, 10);

select attrnums, policytype, numsegments from gp_distribution_policy where localoid = 'ao_t1_reshuffle_full_random'::regclass;

begin;
alter table ao_t1_reshuffle_full_random set with(reshuffle) distributed by (c1);
select gp_segment_id, count(*) from ao_t1_reshuffle_full_random group by gp_segment_id;
select attrnums, policytype, numsegments from gp_distribution_policy where localoid = 'ao_t1_reshuffle_full_random'::regclass;
abort;

select attrnums, policytype, numsegments from gp_distribution_policy where localoid = 'ao_t1_reshuffle_full_random'::regclass;

alter table ao_t1_reshuffle_full_random set with(reshuffle) distributed by (c1);

select gp_segment_id, count(*) from ao_t1_reshuffle_full_random group by gp_segment_id;
select attrnums, policytype, numsegments from gp_distribution_policy where localoid = 'ao_t1_reshuffle_full_random'::regclass;

-- test fail cases
begin;
alter table ao_t1_reshuffle_full_random set with(reshuffle) distributed by (c2);
abort;

begin;
alter table ao_t1_reshuffle_full_random set with(reshuffle) distributed randomly;
abort;

begin;
alter table ao_t1_reshuffle_full_random set with(reshuffle) distributed replicated;
abort;

DROP TABLE ao_t1_reshuffle_full_random;

create table ao_t1_reshuffle_full_random (c1 int, c2 int) with (appendonly=true) distributed randomly;
update gp_distribution_policy set numsegments = 1 where localoid = 'ao_t1_reshuffle_full_random'::regclass;
insert into ao_t1_reshuffle_full_random select * from generate_series(1, 20);

begin;
alter table ao_t1_reshuffle_full_random set with(reshuffle) distributed by (c1);
abort;

DROP TABLE ao_t1_reshuffle_full_random;
