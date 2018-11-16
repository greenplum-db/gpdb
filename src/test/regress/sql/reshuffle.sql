set allow_system_table_mods=true;

-- Hash distributed tables
Create table t1_reshuffle(a int, b int, c int) distributed by (a);
update gp_distribution_policy  set numsegments=2 where localoid='t1_reshuffle'::regclass;
insert into t1_reshuffle select i,i,0 from generate_series(1,100) I;
Update t1_reshuffle set c = gp_segment_id;

Select gp_segment_id, count(*) from t1_reshuffle group by gp_segment_id;

begin;
Alter table t1_reshuffle set with (reshuffle);
Select gp_segment_id, count(*) from t1_reshuffle group by gp_segment_id;
abort;

Select gp_segment_id, count(*) from t1_reshuffle group by gp_segment_id;

Alter table t1_reshuffle set with (reshuffle);

Select gp_segment_id, count(*) from t1_reshuffle group by gp_segment_id;

select numsegments from gp_distribution_policy where localoid='t1_reshuffle'::regclass;
drop table t1_reshuffle;


Create table t1_reshuffle(a int, b int, c int) with OIDS distributed by (a,b);
update gp_distribution_policy  set numsegments=1 where localoid='t1_reshuffle'::regclass;
insert into t1_reshuffle select i,i,0 from generate_series(1,100) I;
Update t1_reshuffle set c = gp_segment_id;

Select gp_segment_id, count(*) from t1_reshuffle group by gp_segment_id;

begin;
Alter table t1_reshuffle set with (reshuffle);
Select gp_segment_id, count(*) from t1_reshuffle group by gp_segment_id;
abort;

Select gp_segment_id, count(*) from t1_reshuffle group by gp_segment_id;

Alter table t1_reshuffle set with (reshuffle);

Select gp_segment_id, count(*) from t1_reshuffle group by gp_segment_id;

select numsegments from gp_distribution_policy where localoid='t1_reshuffle'::regclass;
drop table t1_reshuffle;

-- Test NULLs.
Create table t1_reshuffle(a int, b int, c int) distributed by (a,b,c);
update gp_distribution_policy  set numsegments=2 where localoid='t1_reshuffle'::regclass;
insert into t1_reshuffle values
  (1,    1,    1   ),
  (null, 2,    2   ),
  (3,    null, 3   ),
  (4,    4,    null),
  (null, null, 5   ),
  (null, 6,    null),
  (7,    null, null),
  (null, null, null);
Select gp_segment_id, count(*) from t1_reshuffle group by gp_segment_id;

begin;
Alter table t1_reshuffle set with (reshuffle);
Select gp_segment_id, count(*) from t1_reshuffle group by gp_segment_id;
abort;

Select gp_segment_id, count(*) from t1_reshuffle group by gp_segment_id;

Alter table t1_reshuffle set with (reshuffle);

Select gp_segment_id, count(*) from t1_reshuffle group by gp_segment_id;

select numsegments from gp_distribution_policy where localoid='t1_reshuffle'::regclass;
drop table t1_reshuffle;

Create table t1_reshuffle(a int, b int, c int) distributed by (a) partition by list(b) (partition t1_reshuffle_1 values(1), partition t1_reshuffle_2 values(2), default partition other);
update gp_distribution_policy set numsegments = 1 where localoid in (select oid from pg_class where relname like 't1_reshuffle%');
insert into t1_reshuffle select i,i,0 from generate_series(1,100) I;

Select gp_segment_id, count(*) from t1_reshuffle group by gp_segment_id;

begin;
Alter table t1_reshuffle set with (reshuffle);
Select gp_segment_id, count(*) from t1_reshuffle group by gp_segment_id;
abort;

Select gp_segment_id, count(*) from t1_reshuffle group by gp_segment_id;

Alter table t1_reshuffle set with (reshuffle);

Select gp_segment_id, count(*) from t1_reshuffle group by gp_segment_id;

select numsegments from gp_distribution_policy where localoid='t1_reshuffle'::regclass;
drop table t1_reshuffle;

-- Random distributed tables
Create table r1_reshuffle(a int, b int, c int) distributed randomly;
update gp_distribution_policy  set numsegments=2 where localoid='r1_reshuffle'::regclass;
insert into r1_reshuffle select i,i,0 from generate_series(1,100) I;
Update r1_reshuffle set c = gp_segment_id;

Select count(*) from r1_reshuffle;
Select count(*) > 0 from r1_reshuffle where gp_segment_id=2;

begin;
Alter table r1_reshuffle set with (reshuffle);
Select count(*) from r1_reshuffle;
Select count(*) > 0 from r1_reshuffle where gp_segment_id=2;
abort;

Select count(*) from r1_reshuffle;
Select count(*) > 0 from r1_reshuffle where gp_segment_id=2;

Alter table r1_reshuffle set with (reshuffle);

Select count(*) from r1_reshuffle;
Select count(*) > 0 from r1_reshuffle where gp_segment_id=2;

select numsegments from gp_distribution_policy where localoid='r1_reshuffle'::regclass;
drop table r1_reshuffle;

Create table r1_reshuffle(a int, b int, c int) with OIDS distributed randomly;
update gp_distribution_policy  set numsegments=2 where localoid='r1_reshuffle'::regclass;
insert into r1_reshuffle select i,i,0 from generate_series(1,100) I;
Update r1_reshuffle set c = gp_segment_id;

Select count(*) from r1_reshuffle;
Select count(*) > 0 from r1_reshuffle where gp_segment_id=2;

begin;
Alter table r1_reshuffle set with (reshuffle);
Select count(*) from r1_reshuffle;
Select count(*) > 0 from r1_reshuffle where gp_segment_id=2;
abort;

Select count(*) from r1_reshuffle;
Select count(*) > 0 from r1_reshuffle where gp_segment_id=2;

Alter table r1_reshuffle set with (reshuffle);

Select count(*) from r1_reshuffle;
Select count(*) > 0 from r1_reshuffle where gp_segment_id=2;

select numsegments from gp_distribution_policy where localoid='r1_reshuffle'::regclass;
drop table r1_reshuffle;

Create table r1_reshuffle(a int, b int, c int) distributed randomly partition by list(b) (partition r1_reshuffle_1 values(1), partition r1_reshuffle_2 values(2), default partition other);
update gp_distribution_policy set numsegments = 2 where localoid in (select oid from pg_class where relname like 'r1_reshuffle%');
insert into r1_reshuffle select i,i,0 from generate_series(1,100) I;

Select count(*) from r1_reshuffle;
Select count(*) > 0 from r1_reshuffle where gp_segment_id=2;

begin;
Alter table r1_reshuffle set with (reshuffle);
Select count(*) from r1_reshuffle;
Select count(*) > 0 from r1_reshuffle where gp_segment_id=2;
abort;

Select count(*) from r1_reshuffle;
Select count(*) > 0 from r1_reshuffle where gp_segment_id=2;

Alter table r1_reshuffle set with (reshuffle);

Select count(*) from r1_reshuffle;
Select count(*) > 0 from r1_reshuffle where gp_segment_id=2;

select numsegments from gp_distribution_policy where localoid='r1_reshuffle'::regclass;
drop table r1_reshuffle;

-- Replicated tables
Create table r1_reshuffle(a int, b int, c int) distributed replicated;

select update_numsegments_in_policy('r1_reshuffle'::regclass, 1);

insert into r1_reshuffle select i,i,0 from generate_series(1,100) I;

Select gp_segment_id, count(*) from gp_dist_random('r1_reshuffle') group by gp_segment_id;

begin;
Alter table r1_reshuffle set with (reshuffle);
Select gp_segment_id, count(*) from gp_dist_random('r1_reshuffle') group by gp_segment_id;
abort;

Select gp_segment_id, count(*) from gp_dist_random('r1_reshuffle') group by gp_segment_id;

Alter table r1_reshuffle set with (reshuffle);

Select gp_segment_id, count(*) from gp_dist_random('r1_reshuffle') group by gp_segment_id;

select numsegments from gp_distribution_policy where localoid='r1_reshuffle'::regclass;
drop table r1_reshuffle;

--
Create table r1_reshuffle(a int, b int, c int) with OIDS distributed replicated;

select update_numsegments_in_policy('r1_reshuffle'::regclass, 1);

insert into r1_reshuffle select i,i,0 from generate_series(1,100) I;

Select gp_segment_id, count(*) from gp_dist_random('r1_reshuffle') group by gp_segment_id;

begin;
Alter table r1_reshuffle set with (reshuffle);
Select gp_segment_id, count(*) from gp_dist_random('r1_reshuffle') group by gp_segment_id;
abort;

Select gp_segment_id, count(*) from gp_dist_random('r1_reshuffle') group by gp_segment_id;

Alter table r1_reshuffle set with (reshuffle);

Select gp_segment_id, count(*) from gp_dist_random('r1_reshuffle') group by gp_segment_id;

select numsegments from gp_distribution_policy where localoid='r1_reshuffle'::regclass;
drop table r1_reshuffle;

-- table with update triggers on distributed key column
CREATE OR REPLACE FUNCTION trigger_func() RETURNS trigger LANGUAGE plpgsql AS '
BEGIN
	RAISE NOTICE ''trigger_func(%) called: action = %, when = %, level = %'', TG_ARGV[0], TG_OP, TG_WHEN, TG_LEVEL;
	RETURN NULL;
END;';

CREATE TABLE table_with_update_trigger(a int, b int, c int);
update gp_distribution_policy set numsegments=2 where localoid='table_with_update_trigger'::regclass;
insert into table_with_update_trigger select i,i,0 from generate_series(1,100) I;
select gp_segment_id, count(*) from table_with_update_trigger group by 1 order by 1;

CREATE TRIGGER foo_br_trigger BEFORE INSERT OR UPDATE OR DELETE ON table_with_update_trigger 
FOR EACH ROW EXECUTE PROCEDURE trigger_func('before_stmt');
CREATE TRIGGER foo_ar_trigger AFTER INSERT OR UPDATE OR DELETE ON table_with_update_trigger 
FOR EACH ROW EXECUTE PROCEDURE trigger_func('before_stmt');

CREATE TRIGGER foo_bs_trigger BEFORE INSERT OR UPDATE OR DELETE ON table_with_update_trigger 
FOR EACH STATEMENT EXECUTE PROCEDURE trigger_func('before_stmt');
CREATE TRIGGER foo_as_trigger AFTER INSERT OR UPDATE OR DELETE ON table_with_update_trigger 
FOR EACH STATEMENT EXECUTE PROCEDURE trigger_func('before_stmt');

-- update should fail
update table_with_update_trigger set a = a + 1;
-- reshuffle should success and not hiting any triggers.
Alter table table_with_update_trigger set with (reshuffle);
select gp_segment_id, count(*) from table_with_update_trigger group by 1 order by 1;
--
-- Test reshuffle inheritance parent table, parent table has different numsegments with
-- child tables.
--
create table mix_base_tbl (a int4, b int4) distributed by (a);
update gp_distribution_policy  set numsegments=2 where localoid='mix_base_tbl'::regclass;
insert into mix_base_tbl select g, g from generate_series(1, 3) g;
create table mix_child_a (a int4, b int4) inherits (mix_base_tbl) distributed by (a);
insert into mix_child_a select g, g from generate_series(11, 13) g;
create table mix_child_b (a int4, b int4) inherits (mix_base_tbl) distributed by (b);
update gp_distribution_policy  set numsegments=2 where localoid='mix_child_b'::regclass;
insert into mix_child_b select g, g from generate_series(21, 23) g;
select gp_segment_id, * from mix_base_tbl order by 2, 1;
-- update distributed column, numsegments does not change
update mix_base_tbl set a=a+1;
select gp_segment_id, * from mix_base_tbl order by 2, 1;
-- reshuffle the parent table, both parent and child table will be rebalanced to all
-- segments
Alter table mix_base_tbl set with (reshuffle);
select gp_segment_id, * from mix_base_tbl order by 2, 1;

-- multi-level partition tables
CREATE TABLE part_t1_reshuffle(a int, b int, c int, d int, e int)
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

update gp_distribution_policy  set numsegments=2 where localoid in (select oid from pg_class where relname like 'part_t1_reshuffle%');
insert into part_t1_reshuffle select i,i%3,i%4,i%5,i from generate_series(1,100) I;
Update part_t1_reshuffle set e = gp_segment_id;

Select gp_segment_id, count(*) from part_t1_reshuffle group by gp_segment_id;

begin;
Alter table part_t1_reshuffle set with (reshuffle);
Select gp_segment_id, count(*) from part_t1_reshuffle group by gp_segment_id;
abort;

Select gp_segment_id, count(*) from part_t1_reshuffle group by gp_segment_id;

Alter table part_t1_reshuffle set with (reshuffle);

Select gp_segment_id, count(*) from part_t1_reshuffle group by gp_segment_id;

select numsegments from gp_distribution_policy where localoid='part_t1_reshuffle'::regclass;
drop table part_t1_reshuffle;

--
CREATE TABLE part_t1_reshuffle(a int, b int, c int, d int, e int)
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

update gp_distribution_policy  set numsegments=2 where localoid in (select oid from pg_class where relname like 'part_t1_reshuffle%');
insert into part_t1_reshuffle select i,i%3,i%4,i%5,i from generate_series(1,100) I;
Update part_t1_reshuffle set e = gp_segment_id;

Select count(*) from part_t1_reshuffle;
Select count(*) > 0 from part_t1_reshuffle where gp_segment_id=2;

begin;
Alter table part_t1_reshuffle set with (reshuffle);
Select count(*) from part_t1_reshuffle;
Select count(*) > 0 from part_t1_reshuffle where gp_segment_id=2;
abort;

Select count(*) from part_t1_reshuffle;
Select count(*) > 0 from part_t1_reshuffle where gp_segment_id=2;

Alter table part_t1_reshuffle set with (reshuffle);

Select count(*) from part_t1_reshuffle;
Select count(*) > 0 from part_t1_reshuffle where gp_segment_id=2;

select numsegments from gp_distribution_policy where localoid='part_t1_reshuffle'::regclass;
drop table part_t1_reshuffle;

-- only alter leaf partition
CREATE TABLE part_t1_reshuffle(a int, b int, c int, d int, e int)
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

update gp_distribution_policy  set numsegments=2 where localoid in (select oid from pg_class where relname like 'part_t1_reshuffle%');
insert into part_t1_reshuffle select i,i%3,i%4,i%5,i from generate_series(1,100) I;
Update part_t1_reshuffle set e = gp_segment_id;

select gp_segment_id, * from part_t1_reshuffle_1_prt_other_b_2_prt_2_3_prt_others_d;

begin;
alter table part_t1_reshuffle_1_prt_other_b_2_prt_2_3_prt_others_d set with (reshuffle);
select gp_segment_id, * from part_t1_reshuffle_1_prt_other_b_2_prt_2_3_prt_others_d;
abort;

select gp_segment_id, * from part_t1_reshuffle_1_prt_other_b_2_prt_2_3_prt_others_d;

alter table part_t1_reshuffle_1_prt_other_b_2_prt_2_3_prt_others_d set with (reshuffle);

select gp_segment_id, * from part_t1_reshuffle_1_prt_other_b_2_prt_2_3_prt_others_d;

-- try to reshuffle root partition, it will raise a notice
Alter table part_t1_reshuffle set with (reshuffle);
Select gp_segment_id, count(*) from part_t1_reshuffle group by gp_segment_id;

drop table part_t1_reshuffle;


-- inherits tables
CREATE TABLE inherit_t1_reshuffle_p1(a int, b int);
CREATE TABLE inherit_t1_reshuffle_p2(a int, b int) INHERITS (inherit_t1_reshuffle_p1);
CREATE TABLE inherit_t1_reshuffle_p3(a int, b int) INHERITS (inherit_t1_reshuffle_p1);
CREATE TABLE inherit_t1_reshuffle_p4(a int, b int) INHERITS (inherit_t1_reshuffle_p2);
CREATE TABLE inherit_t1_reshuffle_p5(a int, b int) INHERITS (inherit_t1_reshuffle_p3);

update gp_distribution_policy  set numsegments=2 where localoid in (select oid from pg_class where relname like 'inherit_t1_reshuffle%');

insert into inherit_t1_reshuffle_p1 select i,i from generate_series(1,10) i;
insert into inherit_t1_reshuffle_p2 select i,i from generate_series(1,10) i;
insert into inherit_t1_reshuffle_p3 select i,i from generate_series(1,10) i;
insert into inherit_t1_reshuffle_p4 select i,i from generate_series(1,10) i;
insert into inherit_t1_reshuffle_p5 select i,i from generate_series(1,10) i;

select count(*) > 0 from inherit_t1_reshuffle_p1 where gp_segment_id = 2;

begin;
alter table inherit_t1_reshuffle_p1 set with(reshuffle);
select count(*) > 0 from inherit_t1_reshuffle_p1 where gp_segment_id = 2;
abort;

select count(*) > 0 from inherit_t1_reshuffle_p1 where gp_segment_id = 2;

alter table inherit_t1_reshuffle_p1 set with(reshuffle);

select count(*) > 0 from inherit_t1_reshuffle_p1 where gp_segment_id = 2;

DROP TABLE inherit_t1_reshuffle_p1 CASCADE;

-- test reshuffle from full random to full hash distributed
create table t1_reshuffle_full_random (c1 int, c2 int) distributed randomly;
insert into t1_reshuffle_full_random select * from generate_series(1, 10);

select attrnums, policytype, numsegments from gp_distribution_policy where localoid = 't1_reshuffle_full_random'::regclass;

begin;
alter table t1_reshuffle_full_random set with(reshuffle) distributed by (c1);
select gp_segment_id, count(*) from t1_reshuffle_full_random group by gp_segment_id;
select attrnums, policytype, numsegments from gp_distribution_policy where localoid = 't1_reshuffle_full_random'::regclass;
abort;

select attrnums, policytype, numsegments from gp_distribution_policy where localoid = 't1_reshuffle_full_random'::regclass;

alter table t1_reshuffle_full_random set with(reshuffle) distributed by (c1);

select gp_segment_id, count(*) from t1_reshuffle_full_random group by gp_segment_id;
select attrnums, policytype, numsegments from gp_distribution_policy where localoid = 't1_reshuffle_full_random'::regclass;

-- test fail cases
begin;
alter table t1_reshuffle_full_random set with(reshuffle) distributed by (c2);
abort;

begin;
alter table t1_reshuffle_full_random set with(reshuffle) distributed randomly;
abort;

begin;
alter table t1_reshuffle_full_random set with(reshuffle) distributed replicated;
abort;

DROP TABLE t1_reshuffle_full_random;

create table t1_reshuffle_full_random (c1 int, c2 int) distributed randomly;
update gp_distribution_policy set numsegments = 1 where localoid = 't1_reshuffle_full_random'::regclass;
insert into t1_reshuffle_full_random select * from generate_series(1, 20);

begin;
alter table t1_reshuffle_full_random set with(reshuffle) distributed by (c1);
abort;

DROP TABLE t1_reshuffle_full_random;
