set allow_system_table_mods=true;

-- Hash distributed tables
Create table t1_reshuffle_ao(a int, b int, c int) with (appendonly = true) distributed by (a);
update gp_distribution_policy  set numsegments=2 where localoid='t1_reshuffle_ao'::regclass;
insert into t1_reshuffle_ao select i,i,0 from generate_series(1,100) I;
Update t1_reshuffle_ao set c = gp_segment_id;

Select gp_segment_id, count(*) from t1_reshuffle_ao group by gp_segment_id;

begin;
Alter table t1_reshuffle_ao set with (reshuffle);
Select gp_segment_id, count(*) from t1_reshuffle_ao group by gp_segment_id;
abort;

Select gp_segment_id, count(*) from t1_reshuffle_ao group by gp_segment_id;

Alter table t1_reshuffle_ao set with (reshuffle);

Select gp_segment_id, count(*) from t1_reshuffle_ao group by gp_segment_id;

select numsegments from gp_distribution_policy where localoid='t1_reshuffle_ao'::regclass;
drop table t1_reshuffle_ao;


Create table t1_reshuffle_ao(a int, b int, c int) with (appendonly = true, OIDS = true) distributed by (a,b);
update gp_distribution_policy  set numsegments=1 where localoid='t1_reshuffle_ao'::regclass;
insert into t1_reshuffle_ao select i,i,0 from generate_series(1,100) I;
Update t1_reshuffle_ao set c = gp_segment_id;

Select gp_segment_id, count(*) from t1_reshuffle_ao group by gp_segment_id;

begin;
Alter table t1_reshuffle_ao set with (reshuffle);
Select gp_segment_id, count(*) from t1_reshuffle_ao group by gp_segment_id;
abort;

Select gp_segment_id, count(*) from t1_reshuffle_ao group by gp_segment_id;

Alter table t1_reshuffle_ao set with (reshuffle);

Select gp_segment_id, count(*) from t1_reshuffle_ao group by gp_segment_id;

select numsegments from gp_distribution_policy where localoid='t1_reshuffle_ao'::regclass;
drop table t1_reshuffle_ao;

-- Test NULLs.
Create table t1_reshuffle_ao(a int, b int, c int) with (appendonly = true) distributed by (a,b,c);
update gp_distribution_policy  set numsegments=2 where localoid='t1_reshuffle_ao'::regclass;
insert into t1_reshuffle_ao values
  (1,    1,    1   ),
  (null, 2,    2   ),
  (3,    null, 3   ),
  (4,    4,    null),
  (null, null, 5   ),
  (null, 6,    null),
  (7,    null, null),
  (null, null, null);
Select gp_segment_id, count(*) from t1_reshuffle_ao group by gp_segment_id;

begin;
Alter table t1_reshuffle_ao set with (reshuffle);
Select gp_segment_id, count(*) from t1_reshuffle_ao group by gp_segment_id;
abort;

Select gp_segment_id, count(*) from t1_reshuffle_ao group by gp_segment_id;

Alter table t1_reshuffle_ao set with (reshuffle);

Select gp_segment_id, count(*) from t1_reshuffle_ao group by gp_segment_id;

select numsegments from gp_distribution_policy where localoid='t1_reshuffle_ao'::regclass;
drop table t1_reshuffle_ao;

Create table t1_reshuffle_ao(a int, b int, c int) with (appendonly = true) distributed by (a) partition by list(b) (partition t1_reshuffle_ao_1 values(1), partition t1_reshuffle_ao_2 values(2), default partition other);
update gp_distribution_policy set numsegments = 1 where localoid='t1_reshuffle_ao_1_prt_t1_reshuffle_ao_1'::regclass;
update gp_distribution_policy set numsegments = 1 where localoid='t1_reshuffle_ao_1_prt_t1_reshuffle_ao_2'::regclass;
update gp_distribution_policy set numsegments = 1 where localoid='t1_reshuffle_ao_1_prt_other'::regclass;
update gp_distribution_policy set numsegments = 1 where localoid='t1_reshuffle_ao'::regclass;
insert into t1_reshuffle_ao select i,i,0 from generate_series(1,100) I;

Select gp_segment_id, count(*) from t1_reshuffle_ao group by gp_segment_id;

begin;
Alter table t1_reshuffle_ao set with (reshuffle);
Select gp_segment_id, count(*) from t1_reshuffle_ao group by gp_segment_id;
abort;

Select gp_segment_id, count(*) from t1_reshuffle_ao group by gp_segment_id;

Alter table t1_reshuffle_ao set with (reshuffle);

Select gp_segment_id, count(*) from t1_reshuffle_ao group by gp_segment_id;

select numsegments from gp_distribution_policy where localoid='t1_reshuffle_ao'::regclass;
drop table t1_reshuffle_ao;

-- Random distributed tables
Create table r1_reshuffle_ao(a int, b int, c int) with (appendonly = true) distributed randomly;
update gp_distribution_policy  set numsegments=2 where localoid='r1_reshuffle_ao'::regclass;
insert into r1_reshuffle_ao select i,i,0 from generate_series(1,100) I;
Update r1_reshuffle_ao set c = gp_segment_id;

Select count(*) from r1_reshuffle_ao;
Select count(*) > 0 from r1_reshuffle_ao where gp_segment_id=2;

begin;
Alter table r1_reshuffle_ao set with (reshuffle);
Select count(*) from r1_reshuffle_ao;
Select count(*) > 0 from r1_reshuffle_ao where gp_segment_id=2;
abort;

Select count(*) from r1_reshuffle_ao;
Select count(*) > 0 from r1_reshuffle_ao where gp_segment_id=2;

Alter table r1_reshuffle_ao set with (reshuffle);

Select count(*) from r1_reshuffle_ao;
Select count(*) > 0 from r1_reshuffle_ao where gp_segment_id=2;

select numsegments from gp_distribution_policy where localoid='r1_reshuffle_ao'::regclass;
drop table r1_reshuffle_ao;

Create table r1_reshuffle_ao(a int, b int, c int) with (appendonly = true, OIDS = true) distributed randomly;
update gp_distribution_policy  set numsegments=2 where localoid='r1_reshuffle_ao'::regclass;
insert into r1_reshuffle_ao select i,i,0 from generate_series(1,100) I;
Update r1_reshuffle_ao set c = gp_segment_id;

Select count(*) from r1_reshuffle_ao;
Select count(*) > 0 from r1_reshuffle_ao where gp_segment_id=2;

begin;
Alter table r1_reshuffle_ao set with (reshuffle);
Select count(*) from r1_reshuffle_ao;
Select count(*) > 0 from r1_reshuffle_ao where gp_segment_id=2;
abort;

Select count(*) from r1_reshuffle_ao;
Select count(*) > 0 from r1_reshuffle_ao where gp_segment_id=2;

Alter table r1_reshuffle_ao set with (reshuffle);

Select count(*) from r1_reshuffle_ao;
Select count(*) > 0 from r1_reshuffle_ao where gp_segment_id=2;

select numsegments from gp_distribution_policy where localoid='r1_reshuffle_ao'::regclass;
drop table r1_reshuffle_ao;

Create table r1_reshuffle_ao(a int, b int, c int) with (appendonly = true) distributed randomly partition by list(b) (partition r1_reshuffle_ao_1 values(1), partition r1_reshuffle_ao_2 values(2), default partition other);
update gp_distribution_policy set numsegments = 2 where localoid='r1_reshuffle_ao_1_prt_r1_reshuffle_ao_1'::regclass;
update gp_distribution_policy set numsegments = 2 where localoid='r1_reshuffle_ao_1_prt_r1_reshuffle_ao_2'::regclass;
update gp_distribution_policy set numsegments = 2 where localoid='r1_reshuffle_ao_1_prt_other'::regclass;
update gp_distribution_policy set numsegments = 2 where localoid='r1_reshuffle_ao'::regclass;
insert into r1_reshuffle_ao select i,i,0 from generate_series(1,100) I;

Select count(*) from r1_reshuffle_ao;
Select count(*) > 0 from r1_reshuffle_ao where gp_segment_id=2;

begin;
Alter table r1_reshuffle_ao set with (reshuffle);
Select count(*) from r1_reshuffle_ao;
Select count(*) > 0 from r1_reshuffle_ao where gp_segment_id=2;
abort;

Select count(*) from r1_reshuffle_ao;
Select count(*) > 0 from r1_reshuffle_ao where gp_segment_id=2;

Alter table r1_reshuffle_ao set with (reshuffle);

Select count(*) from r1_reshuffle_ao;
Select count(*) > 0 from r1_reshuffle_ao where gp_segment_id=2;

select numsegments from gp_distribution_policy where localoid='r1_reshuffle_ao'::regclass;
drop table r1_reshuffle_ao;

-- Replicated tables
-- We have to make sure replicated table successfully reshuffled.
-- Currently we could only connect to the specific segments in utility mode
-- to see if the data is consistent there. We create some python function here.
-- The case can only work under the assumption: it's running on 3-segment cluster
-- in a single machine.
-- start_ignore
create language plpythonu;
-- end_ignore
create function update_on_segment_ao(tabname text, segid int, numseg int) returns boolean as
$$
import pygresql.pg as pg
conn = pg.connect(dbname='regression')
port = conn.query("select port from gp_segment_configuration where content = %d and role = 'p'" % segid).getresult()[0][0]
conn.close()

conn = pg.connect(dbname='regression', opt='-c gp_session_role=utility', port=port)
conn.query("set allow_system_table_mods = true")
conn.query("update gp_distribution_policy set numsegments = %d where localoid = '%s'::regclass" % (numseg, tabname))
conn.close()

return True
$$
LANGUAGE plpythonu;

create function select_on_segment_ao(sql text, segid int) returns int as
$$
import pygresql.pg as pg
conn = pg.connect(dbname='regression')
port = conn.query("select port from gp_segment_configuration where content = %d and role = 'p'" % segid).getresult()[0][0]
conn.close()

conn = pg.connect(dbname='regression', opt='-c gp_session_role=utility', port=port)
r = conn.query(sql).getresult()
conn.close()

return r[0][0]
$$
LANGUAGE plpythonu;

Create table r1_reshuffle_ao(a int, b int, c int) with (appendonly = true) distributed replicated;
update gp_distribution_policy  set numsegments=1 where localoid='r1_reshuffle_ao'::regclass;
select update_on_segment_ao('r1_reshuffle_ao', 0, 1);
select update_on_segment_ao('r1_reshuffle_ao', 1, 1);
select update_on_segment_ao('r1_reshuffle_ao', 2, 1);
insert into r1_reshuffle_ao select i,i,0 from generate_series(1,100) I;

Select count(*) from r1_reshuffle_ao;
Select select_on_segment_ao('Select count(*) from r1_reshuffle_ao;', 1);
Select select_on_segment_ao('Select count(*) from r1_reshuffle_ao;', 2);

begin;
Alter table r1_reshuffle_ao set with (reshuffle);
Select count(*) from r1_reshuffle_ao;
abort;

Select count(*) from r1_reshuffle_ao;
Select select_on_segment_ao('Select count(*) from r1_reshuffle_ao;', 1);
Select select_on_segment_ao('Select count(*) from r1_reshuffle_ao;', 2);

Alter table r1_reshuffle_ao set with (reshuffle);

Select count(*) from r1_reshuffle_ao;
Select select_on_segment_ao('Select count(*) from r1_reshuffle_ao;', 1);
Select select_on_segment_ao('Select count(*) from r1_reshuffle_ao;', 2);

select numsegments from gp_distribution_policy where localoid='r1_reshuffle_ao'::regclass;
drop table r1_reshuffle_ao;

Create table r1_reshuffle_ao(a int, b int, c int) with (appendonly = true, OIDS = true) distributed replicated;
update gp_distribution_policy  set numsegments=2 where localoid='r1_reshuffle_ao'::regclass;
select update_on_segment_ao('r1_reshuffle_ao', 0, 2);
select update_on_segment_ao('r1_reshuffle_ao', 1, 2);
select update_on_segment_ao('r1_reshuffle_ao', 2, 2);
insert into r1_reshuffle_ao select i,i,0 from generate_series(1,100) I;

Select count(*) from r1_reshuffle_ao;
Select select_on_segment_ao('Select count(*) from r1_reshuffle_ao;', 1);
Select select_on_segment_ao('Select count(*) from r1_reshuffle_ao;', 2);

begin;
Alter table r1_reshuffle_ao set with (reshuffle);
Select count(*) from r1_reshuffle_ao;
abort;

Select count(*) from r1_reshuffle_ao;
Select select_on_segment_ao('Select count(*) from r1_reshuffle_ao;', 1);
Select select_on_segment_ao('Select count(*) from r1_reshuffle_ao;', 2);

Alter table r1_reshuffle_ao set with (reshuffle);

Select count(*) from r1_reshuffle_ao;
Select select_on_segment_ao('Select count(*) from r1_reshuffle_ao;', 1);
Select select_on_segment_ao('Select count(*) from r1_reshuffle_ao;', 2);

select numsegments from gp_distribution_policy where localoid='r1_reshuffle_ao'::regclass;
drop table r1_reshuffle_ao;
