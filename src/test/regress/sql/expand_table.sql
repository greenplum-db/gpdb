-- start_ignore
create extension if not exists gp_debug_numsegments;
-- end_ignore

drop schema if exists test_expand_table cascade;
create schema test_expand_table;
set search_path=test_expand_table,public;
set default_table_access_method='heap';
set allow_system_table_mods=true;

-- Hash distributed tables
select gp_debug_set_create_table_default_numsegments(2);
Create table t1(a int, b int, c int) distributed by (a);
insert into t1 select i,i,0 from generate_series(1,100) I;
Update t1 set c = gp_segment_id;

Select gp_segment_id, count(*) from t1 group by gp_segment_id;

begin;
Alter table t1 expand table;
Select gp_segment_id, count(*) from t1 group by gp_segment_id;
abort;

Select gp_segment_id, count(*) from t1 group by gp_segment_id;

Alter table t1 expand table;

Select gp_segment_id, count(*) from t1 group by gp_segment_id;

select numsegments from gp_distribution_policy where localoid='t1'::regclass;
drop table t1;


select gp_debug_set_create_table_default_numsegments(1);
Create table t1(a int, b int, c int) distributed by (a,b);
insert into t1 select i,i,0 from generate_series(1,100) I;
Update t1 set c = gp_segment_id;

Select gp_segment_id, count(*) from t1 group by gp_segment_id;

begin;
Alter table t1 expand table;
Select gp_segment_id, count(*) from t1 group by gp_segment_id;
abort;

Select gp_segment_id, count(*) from t1 group by gp_segment_id;

Alter table t1 expand table;

Select gp_segment_id, count(*) from t1 group by gp_segment_id;

select numsegments from gp_distribution_policy where localoid='t1'::regclass;
drop table t1;

-- Test NULLs.
select gp_debug_set_create_table_default_numsegments(2);
Create table t1(a int, b int, c int) distributed by (a,b,c);
insert into t1 values
  (1,    1,    1   ),
  (null, 2,    2   ),
  (3,    null, 3   ),
  (4,    4,    null),
  (null, null, 5   ),
  (null, 6,    null),
  (7,    null, null),
  (null, null, null);
Select gp_segment_id, count(*) from t1 group by gp_segment_id;

begin;
Alter table t1 expand table;
Select gp_segment_id, count(*) from t1 group by gp_segment_id;
abort;

Select gp_segment_id, count(*) from t1 group by gp_segment_id;

Alter table t1 expand table;

Select gp_segment_id, count(*) from t1 group by gp_segment_id;

select numsegments from gp_distribution_policy where localoid='t1'::regclass;
drop table t1;

select gp_debug_set_create_table_default_numsegments(1);
Create table t1(a int, b int, c int) distributed by (a) partition by list(b) (partition t1_1 values(1), partition t1_2 values(2), default partition other);
insert into t1 select i,i,0 from generate_series(1,100) I;

Select gp_segment_id, count(*) from t1 group by gp_segment_id;

begin;
Alter table t1 expand table;
Select gp_segment_id, count(*) from t1 group by gp_segment_id;
abort;

Select gp_segment_id, count(*) from t1 group by gp_segment_id;

Alter table t1 expand table;

Select gp_segment_id, count(*) from t1 group by gp_segment_id;

select numsegments from gp_distribution_policy where localoid='t1'::regclass;
drop table t1;

-- Random distributed tables
select gp_debug_set_create_table_default_numsegments(2);
Create table r1(a int, b int, c int) distributed randomly;
insert into r1 select i,i,0 from generate_series(1,100) I;
Update r1 set c = gp_segment_id;

Select count(*) from r1;
Select count(*) > 0 from r1 where gp_segment_id=2;

begin;
Alter table r1 expand table;
Select count(*) from r1;
Select count(*) > 0 from r1 where gp_segment_id=2;
abort;

Select count(*) from r1;
Select count(*) > 0 from r1 where gp_segment_id=2;

Alter table r1 expand table;

Select count(*) from r1;
Select count(*) > 0 from r1 where gp_segment_id=2;

select numsegments from gp_distribution_policy where localoid='r1'::regclass;
drop table r1;

select gp_debug_set_create_table_default_numsegments(2);
Create table r1(a int, b int, c int) distributed randomly;
insert into r1 select i,i,0 from generate_series(1,100) I;
Update r1 set c = gp_segment_id;

Select count(*) from r1;
Select count(*) > 0 from r1 where gp_segment_id=2;

begin;
Alter table r1 expand table;
Select count(*) from r1;
Select count(*) > 0 from r1 where gp_segment_id=2;
abort;

Select count(*) from r1;
Select count(*) > 0 from r1 where gp_segment_id=2;

Alter table r1 expand table;

Select count(*) from r1;
Select count(*) > 0 from r1 where gp_segment_id=2;

select numsegments from gp_distribution_policy where localoid='r1'::regclass;
drop table r1;

select gp_debug_set_create_table_default_numsegments(2);
Create table r1(a int, b int, c int) distributed randomly partition by list(b) (partition r1_1 values(1), partition r1_2 values(2), default partition other);
insert into r1 select i,i,0 from generate_series(1,100) I;

Select count(*) from r1;
Select count(*) > 0 from r1 where gp_segment_id=2;

begin;
Alter table r1 expand table;
Select count(*) from r1;
Select count(*) > 0 from r1 where gp_segment_id=2;
abort;

Select count(*) from r1;
Select count(*) > 0 from r1 where gp_segment_id=2;

Alter table r1 expand table;

Select count(*) from r1;
Select count(*) > 0 from r1 where gp_segment_id=2;

select numsegments from gp_distribution_policy where localoid='r1'::regclass;
drop table r1;

-- Replicated tables
select gp_debug_set_create_table_default_numsegments(1);
Create table r1(a int, b int, c int) distributed replicated;

insert into r1 select i,i,0 from generate_series(1,100) I;

Select count(*) from gp_dist_random('r1');

begin;
Alter table r1 expand table;
Select count(*) from gp_dist_random('r1');
abort;

Select count(*) from gp_dist_random('r1');

Alter table r1 expand table;

Select count(*) from gp_dist_random('r1');

select numsegments from gp_distribution_policy where localoid='r1'::regclass;
drop table r1;

--
select gp_debug_set_create_table_default_numsegments(1);
Create table r1(a int, b int, c int) distributed replicated;

insert into r1 select i,i,0 from generate_series(1,100) I;

Select count(*) from gp_dist_random('r1');

begin;
Alter table r1 expand table;
Select count(*) from gp_dist_random('r1');
abort;

Select count(*) from gp_dist_random('r1');

Alter table r1 expand table;

Select count(*) from gp_dist_random('r1');

select numsegments from gp_distribution_policy where localoid='r1'::regclass;
drop table r1;

-- table with update triggers on distributed key column
CREATE OR REPLACE FUNCTION trigger_func() RETURNS trigger AS $$
BEGIN
	RAISE NOTICE 'trigger_func(%) called: action = %, when = %, level = %',
		TG_ARGV[0], TG_OP, TG_WHEN, TG_LEVEL;
	RETURN NULL;
END;
$$ LANGUAGE plpgsql;

select gp_debug_set_create_table_default_numsegments(2);
CREATE TABLE table_with_update_trigger(a int, b int, c int);
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
-- data expansion should success and not hiting any triggers.
Alter table table_with_update_trigger expand table;
select gp_segment_id, count(*) from table_with_update_trigger group by 1 order by 1;
--
-- Test expanding inheritance parent table, parent table has different
-- numsegments with child tables.
--
select gp_debug_set_create_table_default_numsegments(2);
create table mix_base_tbl (a int4, b int4) distributed by (a);
insert into mix_base_tbl select g, g from generate_series(1, 3) g;
select gp_debug_set_create_table_default_numsegments('full');
create table mix_child_a (a int4, b int4) inherits (mix_base_tbl) distributed by (a);
insert into mix_child_a select g, g from generate_series(11, 13) g;
select gp_debug_set_create_table_default_numsegments(2);
create table mix_child_b (a int4, b int4) inherits (mix_base_tbl) distributed by (b);
insert into mix_child_b select g, g from generate_series(21, 23) g;
select gp_segment_id, * from mix_base_tbl order by 2, 1;
-- update distributed column, numsegments does not change
update mix_base_tbl set a=a+1;
select gp_segment_id, * from mix_base_tbl order by 2, 1;
-- expand the parent table, both parent and child table will be rebalanced to all
-- segments
Alter table mix_base_tbl expand table;
select gp_segment_id, * from mix_base_tbl order by 2, 1;

-- multi-level partition tables
select gp_debug_set_create_table_default_numsegments(2);
CREATE TABLE part_t1(a int, b int, c int, d int, e int)
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

insert into part_t1 select i,i%3,i%4,i%5,i from generate_series(1,100) I;
Update part_t1 set e = gp_segment_id;

Select gp_segment_id, count(*) from part_t1 group by gp_segment_id;

begin;
Alter table part_t1 expand table;
Select gp_segment_id, count(*) from part_t1 group by gp_segment_id;
abort;

Select gp_segment_id, count(*) from part_t1 group by gp_segment_id;

Alter table part_t1 expand table;

Select gp_segment_id, count(*) from part_t1 group by gp_segment_id;

select numsegments from gp_distribution_policy where localoid='part_t1'::regclass;
drop table part_t1;

--
select gp_debug_set_create_table_default_numsegments(2);
CREATE TABLE part_t1(a int, b int, c int, d int, e int)
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

insert into part_t1 select i,i%3,i%4,i%5,i from generate_series(1,100) I;
Update part_t1 set e = gp_segment_id;

Select count(*) from part_t1;
Select count(*) > 0 from part_t1 where gp_segment_id=2;

begin;
Alter table part_t1 expand table;
Select count(*) from part_t1;
Select count(*) > 0 from part_t1 where gp_segment_id=2;
abort;

Select count(*) from part_t1;
Select count(*) > 0 from part_t1 where gp_segment_id=2;

Alter table part_t1 expand table;

Select count(*) from part_t1;
Select count(*) > 0 from part_t1 where gp_segment_id=2;

select numsegments from gp_distribution_policy where localoid='part_t1'::regclass;
drop table part_t1;

-- only expand leaf partitions, not allowed now
select gp_debug_set_create_table_default_numsegments(2);
CREATE TABLE part_t1(a int, b int, c int, d int, e int)
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

insert into part_t1 select i,i%3,i%4,i%5,i from generate_series(1,100) I;
Update part_t1 set e = gp_segment_id;

select gp_segment_id, * from part_t1_1_prt_other_b_2_prt_2_3_prt_others_d;

alter table part_t1_1_prt_other_b_2_prt_2_3_prt_others_d expand table;
select gp_segment_id, * from part_t1_1_prt_other_b_2_prt_2_3_prt_others_d;

-- try to expand root partition, should success
Alter table part_t1 expand table;
Select gp_segment_id, count(*) from part_t1 group by gp_segment_id;

drop table part_t1;


-- inherits tables
select gp_debug_set_create_table_default_numsegments(2);
CREATE TABLE inherit_t1_p1(a int, b int);
CREATE TABLE inherit_t1_p2(a int, b int) INHERITS (inherit_t1_p1);
CREATE TABLE inherit_t1_p3(a int, b int) INHERITS (inherit_t1_p1);
CREATE TABLE inherit_t1_p4(a int, b int) INHERITS (inherit_t1_p2);
CREATE TABLE inherit_t1_p5(a int, b int) INHERITS (inherit_t1_p3);

insert into inherit_t1_p1 select i,i from generate_series(1,10) i;
insert into inherit_t1_p2 select i,i from generate_series(1,10) i;
insert into inherit_t1_p3 select i,i from generate_series(1,10) i;
insert into inherit_t1_p4 select i,i from generate_series(1,10) i;
insert into inherit_t1_p5 select i,i from generate_series(1,10) i;

select count(*) > 0 from inherit_t1_p1 where gp_segment_id = 2;

begin;
alter table inherit_t1_p1 expand table;
select count(*) > 0 from inherit_t1_p1 where gp_segment_id = 2;
abort;

select count(*) > 0 from inherit_t1_p1 where gp_segment_id = 2;

alter table inherit_t1_p1 expand table;

select count(*) > 0 from inherit_t1_p1 where gp_segment_id = 2;

DROP TABLE inherit_t1_p1 CASCADE;


--
-- Cannot expand a native view and transformed view
--
CREATE TABLE expand_table1(a int) distributed by (a);
CREATE TABLE expand_table2(a int) distributed by (a);
CREATE VIEW expand_view AS select * from expand_table1;
CREATE rule "_RETURN" AS ON SELECT TO expand_table2
    DO INSTEAD SELECT * FROM expand_table1;
ALTER TABLE expand_table2 EXPAND TABLE;
ALTER TABLE expand_view EXPAND TABLE;
ALTER TABLE expand_table1 EXPAND TABLE;

--
-- Test expanding a table with a domain type as distribution key.
--
select gp_debug_set_create_table_default_numsegments(2);
create domain myintdomain as int4;

create table expand_domain_tab(d myintdomain, oldseg int4) distributed by(d);
insert into expand_domain_tab select generate_series(1,10);

update expand_domain_tab set oldseg = gp_segment_id;

select gp_segment_id, count(*) from expand_domain_tab group by gp_segment_id;

alter table expand_domain_tab expand table;
select gp_segment_id, count(*) from expand_domain_tab group by gp_segment_id;
select numsegments from gp_distribution_policy where localoid='expand_domain_tab'::regclass;

-- start_ignore
-- We need to do a cluster expansion which will check if there are partial
-- tables, we need to drop the partial tables to keep the cluster expansion
-- run correctly.
reset search_path;
drop schema test_reshuffle cascade;
-- end_ignore
