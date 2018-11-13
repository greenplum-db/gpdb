set allow_system_table_mods=true;

-- Hash distributed tables
Create table t1_reshuffle_aoco(a int, b int, c int) with (appendonly = true, orientation = column) distributed by (a);
update gp_distribution_policy  set numsegments=2 where localoid='t1_reshuffle_aoco'::regclass;
insert into t1_reshuffle_aoco select i,i,0 from generate_series(1,100) I;
Update t1_reshuffle_aoco set c = gp_segment_id;

Select gp_segment_id, count(*) from t1_reshuffle_aoco group by gp_segment_id;

begin;
Alter table t1_reshuffle_aoco set with (reshuffle);
Select gp_segment_id, count(*) from t1_reshuffle_aoco group by gp_segment_id;
abort;

Select gp_segment_id, count(*) from t1_reshuffle_aoco group by gp_segment_id;

Alter table t1_reshuffle_aoco set with (reshuffle);

Select gp_segment_id, count(*) from t1_reshuffle_aoco group by gp_segment_id;

select numsegments from gp_distribution_policy where localoid='t1_reshuffle_aoco'::regclass;
drop table t1_reshuffle_aoco;

-- Test NULLs.
Create table t1_reshuffle_aoco(a int, b int, c int) with (appendonly = true, orientation = column) distributed by (a,b,c);
update gp_distribution_policy  set numsegments=2 where localoid='t1_reshuffle_aoco'::regclass;
insert into t1_reshuffle_aoco values
  (1,    1,    1   ),
  (null, 2,    2   ),
  (3,    null, 3   ),
  (4,    4,    null),
  (null, null, 5   ),
  (null, 6,    null),
  (7,    null, null),
  (null, null, null);
Select gp_segment_id, count(*) from t1_reshuffle_aoco group by gp_segment_id;

begin;
Alter table t1_reshuffle_aoco set with (reshuffle);
Select gp_segment_id, count(*) from t1_reshuffle_aoco group by gp_segment_id;
abort;

Select gp_segment_id, count(*) from t1_reshuffle_aoco group by gp_segment_id;

Alter table t1_reshuffle_aoco set with (reshuffle);

Select gp_segment_id, count(*) from t1_reshuffle_aoco group by gp_segment_id;

select numsegments from gp_distribution_policy where localoid='t1_reshuffle_aoco'::regclass;
drop table t1_reshuffle_aoco;

Create table t1_reshuffle_aoco(a int, b int, c int) with (appendonly = true, orientation = column) distributed by (a) partition by list(b) (partition t1_reshuffle_aoco_1 values(1), partition t1_reshuffle_aoco_2 values(2), default partition other);
update gp_distribution_policy set numsegments = 1 where localoid='t1_reshuffle_aoco_1_prt_t1_reshuffle_aoco_1'::regclass;
update gp_distribution_policy set numsegments = 1 where localoid='t1_reshuffle_aoco_1_prt_t1_reshuffle_aoco_2'::regclass;
update gp_distribution_policy set numsegments = 1 where localoid='t1_reshuffle_aoco_1_prt_other'::regclass;
update gp_distribution_policy set numsegments = 1 where localoid='t1_reshuffle_aoco'::regclass;
insert into t1_reshuffle_aoco select i,i,0 from generate_series(1,100) I;

Select gp_segment_id, count(*) from t1_reshuffle_aoco group by gp_segment_id;

begin;
Alter table t1_reshuffle_aoco set with (reshuffle);
Select gp_segment_id, count(*) from t1_reshuffle_aoco group by gp_segment_id;
abort;

Select gp_segment_id, count(*) from t1_reshuffle_aoco group by gp_segment_id;

Alter table t1_reshuffle_aoco set with (reshuffle);

Select gp_segment_id, count(*) from t1_reshuffle_aoco group by gp_segment_id;

select numsegments from gp_distribution_policy where localoid='t1_reshuffle_aoco'::regclass;
drop table t1_reshuffle_aoco;

-- Random distributed tables
Create table r1_reshuffle_aoco(a int, b int, c int) with (appendonly = true, orientation = column) distributed randomly;
update gp_distribution_policy  set numsegments=2 where localoid='r1_reshuffle_aoco'::regclass;
insert into r1_reshuffle_aoco select i,i,0 from generate_series(1,100) I;
Update r1_reshuffle_aoco set c = gp_segment_id;

Select count(*) from r1_reshuffle_aoco;
Select count(*) > 0 from r1_reshuffle_aoco where gp_segment_id=2;

begin;
Alter table r1_reshuffle_aoco set with (reshuffle);
Select count(*) from r1_reshuffle_aoco;
Select count(*) > 0 from r1_reshuffle_aoco where gp_segment_id=2;
abort;

Select count(*) from r1_reshuffle_aoco;
Select count(*) > 0 from r1_reshuffle_aoco where gp_segment_id=2;

Alter table r1_reshuffle_aoco set with (reshuffle);

Select count(*) from r1_reshuffle_aoco;
Select count(*) > 0 from r1_reshuffle_aoco where gp_segment_id=2;

select numsegments from gp_distribution_policy where localoid='r1_reshuffle_aoco'::regclass;
drop table r1_reshuffle_aoco;

Create table r1_reshuffle_aoco(a int, b int, c int) with (appendonly = true, orientation = column) distributed randomly partition by list(b) (partition r1_reshuffle_aoco_1 values(1), partition r1_reshuffle_aoco_2 values(2), default partition other);
update gp_distribution_policy set numsegments = 2 where localoid='r1_reshuffle_aoco_1_prt_r1_reshuffle_aoco_1'::regclass;
update gp_distribution_policy set numsegments = 2 where localoid='r1_reshuffle_aoco_1_prt_r1_reshuffle_aoco_2'::regclass;
update gp_distribution_policy set numsegments = 2 where localoid='r1_reshuffle_aoco_1_prt_other'::regclass;
update gp_distribution_policy set numsegments = 2 where localoid='r1_reshuffle_aoco'::regclass;
insert into r1_reshuffle_aoco select i,i,0 from generate_series(1,100) I;

Select count(*) from r1_reshuffle_aoco;
Select count(*) > 0 from r1_reshuffle_aoco where gp_segment_id=2;

begin;
Alter table r1_reshuffle_aoco set with (reshuffle);
Select count(*) from r1_reshuffle_aoco;
Select count(*) > 0 from r1_reshuffle_aoco where gp_segment_id=2;
abort;

Select count(*) from r1_reshuffle_aoco;
Select count(*) > 0 from r1_reshuffle_aoco where gp_segment_id=2;

Alter table r1_reshuffle_aoco set with (reshuffle);

Select count(*) from r1_reshuffle_aoco;
Select count(*) > 0 from r1_reshuffle_aoco where gp_segment_id=2;

select numsegments from gp_distribution_policy where localoid='r1_reshuffle_aoco'::regclass;
drop table r1_reshuffle_aoco;

-- Replicated tables
-- We have to make sure replicated table successfully reshuffled.

Create table r1_reshuffle_aoco(a int, b int, c int) with (appendonly = true, orientation = column) distributed replicated;
select update_numsegments_in_policy('r1_reshuffle_aoco', 1);
insert into r1_reshuffle_aoco select i,i,0 from generate_series(1,100) I;

Select count(*) from r1_reshuffle_aoco;
Select gp_execute_on_server(1, 'Select count(*) from r1_reshuffle_aoco;');
Select gp_execute_on_server(2, 'Select count(*) from r1_reshuffle_aoco;');

begin;
Alter table r1_reshuffle_aoco set with (reshuffle);
Select count(*) from r1_reshuffle_aoco;
abort;

Select count(*) from r1_reshuffle_aoco;
Select gp_execute_on_server(1, 'Select count(*) from r1_reshuffle_aoco;');
Select gp_execute_on_server(2, 'Select count(*) from r1_reshuffle_aoco;');

Alter table r1_reshuffle_aoco set with (reshuffle);

Select count(*) from r1_reshuffle_aoco;
Select gp_execute_on_server(1, 'Select count(*) from r1_reshuffle_aoco;');
Select gp_execute_on_server(2, 'Select count(*) from r1_reshuffle_aoco;');

select numsegments from gp_distribution_policy where localoid='r1_reshuffle_aoco'::regclass;
drop table r1_reshuffle_aoco;
