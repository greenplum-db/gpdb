set allow_system_table_mods=true;

-- Hash distributed tables
Create table t1_reshuffle_ao(a int, b int, c int) with (appendonly = true);
update gp_distribution_policy  set numsegments=2 where localoid='t1_reshuffle_ao'::regclass;
insert into t1_reshuffle_ao select i,i,0 from generate_series(1,100) I;
Update t1_reshuffle_ao set c = gp_segment_id;
Select count(*) from t1_reshuffle_ao where gp_segment_id=0;
Select count(*) from t1_reshuffle_ao where gp_segment_id=1;
Select count(*) from t1_reshuffle_ao where gp_segment_id=2;
Alter table t1_reshuffle_ao set with (reshuffle);
Select count(*) from t1_reshuffle_ao where gp_segment_id=0;
Select count(*) from t1_reshuffle_ao where gp_segment_id=1;
Select count(*) from t1_reshuffle_ao where gp_segment_id=2;
select numsegments from gp_distribution_policy where localoid='t1_reshuffle_ao'::regclass;
drop table t1_reshuffle_ao;

Create table t1_reshuffle_ao(a int, b int, c int) with (appendonly = true);
update gp_distribution_policy  set numsegments=1 where localoid='t1_reshuffle_ao'::regclass;
insert into t1_reshuffle_ao select i,i,0 from generate_series(1,100) I;
Update t1_reshuffle_ao set c = gp_segment_id;
Select count(*) from t1_reshuffle_ao where gp_segment_id=0;
Select count(*) from t1_reshuffle_ao where gp_segment_id=1;
Select count(*) from t1_reshuffle_ao where gp_segment_id=2;
Alter table t1_reshuffle_ao set with (reshuffle);
Select count(*) from t1_reshuffle_ao where gp_segment_id=0;
Select count(*) from t1_reshuffle_ao where gp_segment_id=1;
Select count(*) from t1_reshuffle_ao where gp_segment_id=2;
select numsegments from gp_distribution_policy where localoid='t1_reshuffle_ao'::regclass;
drop table t1_reshuffle_ao;

Create table t1_reshuffle_ao(a int, b int, c int) with (appendonly = true) distributed by (a,b);
update gp_distribution_policy  set numsegments=2 where localoid='t1_reshuffle_ao'::regclass;
insert into t1_reshuffle_ao select i,i,0 from generate_series(1,100) I;
Update t1_reshuffle_ao set c = gp_segment_id;
Select count(*) from t1_reshuffle_ao where gp_segment_id=0;
Select count(*) from t1_reshuffle_ao where gp_segment_id=1;
Select count(*) from t1_reshuffle_ao where gp_segment_id=2;
Alter table t1_reshuffle_ao set with (reshuffle);
Select count(*) from t1_reshuffle_ao where gp_segment_id=0;
Select count(*) from t1_reshuffle_ao where gp_segment_id=1;
Select count(*) from t1_reshuffle_ao where gp_segment_id=2;
select numsegments from gp_distribution_policy where localoid='t1_reshuffle_ao'::regclass;
drop table t1_reshuffle_ao;

Create table t1_reshuffle_ao(a int, b int, c int) with (appendonly = true) distributed by (a,b);
update gp_distribution_policy  set numsegments=1 where localoid='t1_reshuffle_ao'::regclass;
insert into t1_reshuffle_ao select i,i,0 from generate_series(1,100) I;
Update t1_reshuffle_ao set c = gp_segment_id;
Select count(*) from t1_reshuffle_ao where gp_segment_id=0;
Select count(*) from t1_reshuffle_ao where gp_segment_id=1;
Select count(*) from t1_reshuffle_ao where gp_segment_id=2;
Alter table t1_reshuffle_ao set with (reshuffle);
Select count(*) from t1_reshuffle_ao where gp_segment_id=0;
Select count(*) from t1_reshuffle_ao where gp_segment_id=1;
Select count(*) from t1_reshuffle_ao where gp_segment_id=2;
select numsegments from gp_distribution_policy where localoid='t1_reshuffle_ao'::regclass;
drop table t1_reshuffle_ao;

Create table t1_reshuffle_ao(a int, b int, c int) with (appendonly = true) distributed by (a) partition by list(b) (partition t1_reshuffle_ao_1 values(1), partition t1_reshuffle_ao_2 values(2), default partition other);
update gp_distribution_policy set numsegments = 1 where localoid='t1_reshuffle_ao_1_prt_t1_reshuffle_ao_1'::regclass;
update gp_distribution_policy set numsegments = 1 where localoid='t1_reshuffle_ao_1_prt_t1_reshuffle_ao_2'::regclass;
update gp_distribution_policy set numsegments = 1 where localoid='t1_reshuffle_ao_1_prt_other'::regclass;
update gp_distribution_policy set numsegments = 1 where localoid='t1_reshuffle_ao'::regclass;
insert into t1_reshuffle_ao select i,i,0 from generate_series(1,100) I;
Alter table t1_reshuffle_ao set with (reshuffle);
Select count(*) from t1_reshuffle_ao;
Select count(*) > 0 from t1_reshuffle_ao where gp_segment_id=1;
Select count(*) > 0 from t1_reshuffle_ao where gp_segment_id=2;
drop table t1_reshuffle_ao;

Create table t1_reshuffle_ao(a int, b int, c int) with (appendonly = true) distributed by (a) partition by list(b) (partition t1_reshuffle_ao_1 values(1), partition t1_reshuffle_ao_2 values(2), default partition other);
update gp_distribution_policy set numsegments = 2 where localoid='t1_reshuffle_ao_1_prt_t1_reshuffle_ao_1'::regclass;
update gp_distribution_policy set numsegments = 2 where localoid='t1_reshuffle_ao_1_prt_t1_reshuffle_ao_2'::regclass;
update gp_distribution_policy set numsegments = 2 where localoid='t1_reshuffle_ao_1_prt_other'::regclass;
update gp_distribution_policy set numsegments = 2 where localoid='t1_reshuffle_ao'::regclass;
insert into t1_reshuffle_ao select i,i,0 from generate_series(1,100) I;
Alter table t1_reshuffle_ao set with (reshuffle);
Select count(*) from t1_reshuffle_ao;
Select count(*) > 0 from t1_reshuffle_ao where gp_segment_id=2;
drop table t1_reshuffle_ao;

Create table t1_reshuffle_ao(a int, b int, c int) with (appendonly = true) distributed by (a,b) partition by list(b) (partition t1_reshuffle_ao_1 values(1), partition t1_reshuffle_ao_2 values(2), default partition other);
update gp_distribution_policy set numsegments = 1 where localoid='t1_reshuffle_ao_1_prt_t1_reshuffle_ao_1'::regclass;
update gp_distribution_policy set numsegments = 1 where localoid='t1_reshuffle_ao_1_prt_t1_reshuffle_ao_2'::regclass;
update gp_distribution_policy set numsegments = 1 where localoid='t1_reshuffle_ao_1_prt_other'::regclass;
update gp_distribution_policy set numsegments = 1 where localoid='t1_reshuffle_ao'::regclass;
insert into t1_reshuffle_ao select i,i,0 from generate_series(1,100) I;
Alter table t1_reshuffle_ao set with (reshuffle);
Select count(*) from t1_reshuffle_ao;
Select count(*) > 0 from t1_reshuffle_ao where gp_segment_id=1;
Select count(*) > 0 from t1_reshuffle_ao where gp_segment_id=2;
drop table t1_reshuffle_ao;

Create table t1_reshuffle_ao(a int, b int, c int) with (appendonly = true) distributed by (a,b) partition by list(b) (partition t1_reshuffle_ao_1 values(1), partition t1_reshuffle_ao_2 values(2), default partition other);
update gp_distribution_policy set numsegments = 2 where localoid='t1_reshuffle_ao_1_prt_t1_reshuffle_ao_1'::regclass;
update gp_distribution_policy set numsegments = 2 where localoid='t1_reshuffle_ao_1_prt_t1_reshuffle_ao_2'::regclass;
update gp_distribution_policy set numsegments = 2 where localoid='t1_reshuffle_ao_1_prt_other'::regclass;
update gp_distribution_policy set numsegments = 2 where localoid='t1_reshuffle_ao'::regclass;
insert into t1_reshuffle_ao select i,i,0 from generate_series(1,100) I;
Alter table t1_reshuffle_ao set with (reshuffle);
Select count(*) from t1_reshuffle_ao;
Select count(*) > 0 from t1_reshuffle_ao where gp_segment_id=2;
drop table t1_reshuffle_ao;


-- Random distributed tables
Create table r1_reshuffle_ao(a int, b int, c int) with (appendonly = true) distributed randomly;
update gp_distribution_policy  set numsegments=1 where localoid='r1_reshuffle_ao'::regclass;
insert into r1_reshuffle_ao select i,i,0 from generate_series(1,100) I;
Update r1_reshuffle_ao set c = gp_segment_id;
Select count(*) from r1_reshuffle_ao;
Alter table r1_reshuffle_ao set with (reshuffle);
Select count(*) from r1_reshuffle_ao;
Select count(*) > 0 from r1_reshuffle_ao where gp_segment_id=2;
drop table r1_reshuffle_ao;

Create table r1_reshuffle_ao(a int, b int, c int) with (appendonly = true) distributed randomly;
update gp_distribution_policy  set numsegments=2 where localoid='r1_reshuffle_ao'::regclass;
insert into r1_reshuffle_ao select i,i,0 from generate_series(1,100) I;
Update r1_reshuffle_ao set c = gp_segment_id;
Select count(*) from r1_reshuffle_ao;
Alter table r1_reshuffle_ao set with (reshuffle);
Select count(*) from r1_reshuffle_ao;
Select count(*) > 0 from r1_reshuffle_ao where gp_segment_id=2;
drop table r1_reshuffle_ao;

Create table r1_reshuffle_ao(a int, b int, c int) with (appendonly = true) distributed randomly partition by list(b) (partition r1_reshuffle_ao_1 values(1), partition r1_reshuffle_ao_2 values(2), default partition other);
update gp_distribution_policy set numsegments = 1 where localoid='r1_reshuffle_ao_1_prt_r1_reshuffle_ao_1'::regclass;
update gp_distribution_policy set numsegments = 1 where localoid='r1_reshuffle_ao_1_prt_r1_reshuffle_ao_2'::regclass;
update gp_distribution_policy set numsegments = 1 where localoid='r1_reshuffle_ao_1_prt_other'::regclass;
update gp_distribution_policy set numsegments = 1 where localoid='r1_reshuffle_ao'::regclass;
insert into r1_reshuffle_ao select i,i,0 from generate_series(1,100) I;
Alter table r1_reshuffle_ao set with (reshuffle);
Select count(*) from r1_reshuffle_ao;
Select count(*) > 0 from r1_reshuffle_ao where gp_segment_id=1;
Select count(*) > 0 from r1_reshuffle_ao where gp_segment_id=2;
drop table r1_reshuffle_ao;

Create table r1_reshuffle_ao(a int, b int, c int) with (appendonly = true) distributed randomly partition by list(b) (partition r1_reshuffle_ao_1 values(1), partition r1_reshuffle_ao_2 values(2), default partition other);
update gp_distribution_policy set numsegments = 2 where localoid='r1_reshuffle_ao_1_prt_r1_reshuffle_ao_1'::regclass;
update gp_distribution_policy set numsegments = 2 where localoid='r1_reshuffle_ao_1_prt_r1_reshuffle_ao_2'::regclass;
update gp_distribution_policy set numsegments = 2 where localoid='r1_reshuffle_ao_1_prt_other'::regclass;
update gp_distribution_policy set numsegments = 2 where localoid='r1_reshuffle_ao'::regclass;
insert into r1_reshuffle_ao select i,i,0 from generate_series(1,100) I;
Alter table r1_reshuffle_ao set with (reshuffle);
Select count(*) from r1_reshuffle_ao;
Select count(*) > 0 from r1_reshuffle_ao where gp_segment_id=1;
Select count(*) > 0 from r1_reshuffle_ao where gp_segment_id=2;
drop table r1_reshuffle_ao;

-- Replicated tables
Create table r1_reshuffle_ao(a int, b int, c int) with (appendonly = true) distributed replicated;
update gp_distribution_policy  set numsegments=1 where localoid='r1_reshuffle_ao'::regclass;
insert into r1_reshuffle_ao select i,i,0 from generate_series(1,100) I;
Select count(*) from r1_reshuffle_ao;
Alter table r1_reshuffle_ao set with (reshuffle);
Select count(*) from r1_reshuffle_ao;
drop table r1_reshuffle_ao;

Create table r1_reshuffle_ao(a int, b int, c int) with (appendonly = true) distributed replicated;
update gp_distribution_policy  set numsegments=2 where localoid='r1_reshuffle_ao'::regclass;
insert into r1_reshuffle_ao select i,i,0 from generate_series(1,100) I;
Select count(*) from r1_reshuffle_ao;
Alter table r1_reshuffle_ao set with (reshuffle);
Select count(*) from r1_reshuffle_ao;
drop table r1_reshuffle_ao;



