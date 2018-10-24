set allow_system_table_mods=true;

-- hash distributed heap table
create table thash_reshuffle(a int, b int) distributed by (a);
update gp_distribution_policy  set numsegments=2 where localoid='thash_reshuffle'::regclass;

insert into thash_reshuffle select i,i from generate_series(1,10) i;

select * from thash_reshuffle order by a, b;
select 2 in (select gp_segment_id from thash_reshuffle) as check;

1: begin;
1: alter table thash_reshuffle set with (reshuffle);

-- since alter reshuffle transaction has not committed
-- the other transaction can access the table, but
-- can only see tuples from seg0, seg1.
select * from thash_reshuffle order by a, b;
select 2 in (select gp_segment_id from thash_reshuffle) as check;

29: begin;
29&: insert into thash_reshuffle values (101), (102), (103);

30: begin;
30&: update thash_reshuffle set b = 102 where a = 1 or a = 2 or a = 3;

31: begin;
31&: delete from thash_reshuffle where a = 4 or a = 5 or a = 6;

1: abort;
-- test reshuffle can abort

29<:
29:abort;
29q:

30<:
30:abort;
30q:

31<:
31:abort;
31q:

select * from thash_reshuffle order by a, b;
select 2 in (select gp_segment_id from thash_reshuffle) as check;

1: begin;
1: alter table thash_reshuffle set with (reshuffle);

32: begin;
32&: alter table thash_reshuffle set with (reshuffle);

select * from thash_reshuffle order by a, b;
select 2 in (select gp_segment_id from thash_reshuffle) as check;

1: commit;
1q:

32<:
32:abort;
32q:

select * from thash_reshuffle order by a, b;
select 2 in (select gp_segment_id from thash_reshuffle) as check;

drop table thash_reshuffle;

-- random distributed heap table
create table trand_reshuffle(a int, b int) distributed randomly;
update gp_distribution_policy  set numsegments=2 where localoid='trand_reshuffle'::regclass;

insert into trand_reshuffle select i,i from generate_series(1,10) i;

select * from trand_reshuffle order by a, b;
select 2 in (select gp_segment_id from trand_reshuffle) as check;

2: begin;
2: alter table trand_reshuffle set with (reshuffle);

-- since alter reshuffle transaction has not committed
-- the other transaction can access the table, but
-- can only see tuples from seg0, seg1.
select * from trand_reshuffle order by a, b;
select 2 in (select gp_segment_id from trand_reshuffle) as check;

2: abort;
-- test reshuffle can abort

select * from trand_reshuffle order by a, b;
select 2 in (select gp_segment_id from trand_reshuffle) as check;

2: begin;
2: alter table trand_reshuffle set with (reshuffle);

select * from trand_reshuffle order by a, b;
select 2 in (select gp_segment_id from trand_reshuffle) as check;

2: commit;
2q:

select * from trand_reshuffle order by a, b;
select 2 in (select gp_segment_id from trand_reshuffle) as check;

drop table trand_reshuffle;

-- replicated heap table
create table trep_reshuffle(a int, b int) distributed replicated;
update gp_distribution_policy  set numsegments=2 where localoid='trep_reshuffle'::regclass;

create language plpythonu;
create function update_on_segment(tabname text, segid int) returns boolean as
$$
import pygresql.pg as pg
conn = pg.connect(dbname='isolation2test')
port = conn.query("select port from gp_segment_configuration where content = %d and role = 'p'" % segid).getresult()[0][0]
conn.close()

conn = pg.connect(dbname='isolation2test', opt='-c gp_session_role=utility', port=port)
conn.query("set allow_system_table_mods = true")
conn.query("update gp_distribution_policy set numsegments = 2 where localoid = '%s'::regclass" % tabname)
conn.close()

return True
$$
LANGUAGE plpythonu;

create function select_on_segment(sql text, segid int) returns table (like trep_reshuffle) as
$$
import pygresql.pg as pg
conn = pg.connect(dbname='isolation2test')
port = conn.query("select port from gp_segment_configuration where content = %d and role = 'p'" % segid).getresult()[0][0]
conn.close()

conn = pg.connect(dbname='isolation2test', opt='-c gp_session_role=utility', port=port)
r = conn.query(sql).getresult()
conn.close()

return r
$$
LANGUAGE plpythonu;

update gp_distribution_policy set numsegments = 2 where localoid = 'trep_reshuffle'::regclass;
select update_on_segment('trep_reshuffle', 0);
select update_on_segment('trep_reshuffle', 1);
select update_on_segment('trep_reshuffle', 2);
insert into trep_reshuffle select i,i from generate_series(1,10) i;

select * from trep_reshuffle order by a, b;
select select_on_segment('select * from trep_reshuffle', 2);

3: begin;
3: alter table trep_reshuffle set with (reshuffle);

-- since alter reshuffle transaction has not committed
-- the other transaction can access the table, but
-- can only see tuples from seg0, seg1.
select * from trep_reshuffle order by a, b;
select select_on_segment('select * from trep_reshuffle', 2);

3: abort;
-- test reshuffle can abort

select * from trep_reshuffle order by a, b;
select select_on_segment('select * from trep_reshuffle', 2);

3: begin;
3: alter table trep_reshuffle set with (reshuffle);

select * from trep_reshuffle order by a, b;
select select_on_segment('select * from trep_reshuffle', 2);

3: commit;
3q:

select * from trep_reshuffle order by a, b;
select select_on_segment('select * from trep_reshuffle', 2);

drop function update_on_segment(text, int);
drop function select_on_segment(text, int);
drop table trep_reshuffle;

drop language plpythonu;

-- hash distributed heap table partition
create table thash_part_reshuffle(a int, b int) distributed by (a) partition by list(b) (partition thash_part_reshuffle_1 values(1), partition thash_part_reshuffle_2 values(2), default partition other);
update gp_distribution_policy set numsegments=2 where localoid='thash_part_reshuffle'::regclass;
update gp_distribution_policy set numsegments=2 where localoid='thash_part_reshuffle_1_prt_thash_part_reshuffle_1'::regclass;
update gp_distribution_policy set numsegments=2 where localoid='thash_part_reshuffle_1_prt_thash_part_reshuffle_2'::regclass;
update gp_distribution_policy set numsegments=2 where localoid='thash_part_reshuffle_1_prt_other'::regclass;

insert into thash_part_reshuffle select i,i from generate_series(1,20) i;

select * from thash_part_reshuffle order by a, b;
select 2 in (select gp_segment_id from thash_part_reshuffle) as check;

4: begin;
4: alter table thash_part_reshuffle set with (reshuffle);

-- since alter reshuffle transaction has not committed
-- the other transaction can access the table, but
-- can only see tuples from seg0, seg1.
select * from thash_part_reshuffle order by a, b;
select 2 in (select gp_segment_id from thash_part_reshuffle) as check;

4: abort;
-- test reshuffle can abort

select * from thash_part_reshuffle order by a, b;
select 2 in (select gp_segment_id from thash_part_reshuffle) as check;

4: begin;
4: alter table thash_part_reshuffle set with (reshuffle);

select * from thash_part_reshuffle order by a, b;
select 2 in (select gp_segment_id from thash_part_reshuffle) as check;

4: commit;
4q:

select * from thash_part_reshuffle order by a, b;
select 2 in (select gp_segment_id from thash_part_reshuffle) as check;

drop table thash_part_reshuffle;

-- random distributed heap table
create table trand_part_reshuffle(a int, b int) distributed randomly partition by list(b) (partition trand_part_reshuffle_1 values(1), partition trand_part_reshuffle_2 values(2), default partition other);
update gp_distribution_policy set numsegments=2 where localoid='trand_part_reshuffle'::regclass;
update gp_distribution_policy set numsegments=2 where localoid='trand_part_reshuffle_1_prt_trand_part_reshuffle_1'::regclass;
update gp_distribution_policy set numsegments=2 where localoid='trand_part_reshuffle_1_prt_trand_part_reshuffle_2'::regclass;
update gp_distribution_policy set numsegments=2 where localoid='trand_part_reshuffle_1_prt_other'::regclass;

insert into trand_part_reshuffle select i,i from generate_series(1,20) i;

select * from trand_part_reshuffle order by a, b;
select 2 in (select gp_segment_id from trand_part_reshuffle) as check;

5: begin;
5: alter table trand_part_reshuffle set with (reshuffle);

-- since alter reshuffle transaction has not committed
-- the other transaction can access the table, but
-- can only see tuples from seg0, seg1.
select * from trand_part_reshuffle order by a, b;
select 2 in (select gp_segment_id from trand_part_reshuffle) as check;

5: abort;
-- test reshuffle can abort

select * from trand_part_reshuffle order by a, b;
select 2 in (select gp_segment_id from trand_part_reshuffle) as check;

5: begin;
5: alter table trand_part_reshuffle set with (reshuffle);

select * from trand_part_reshuffle order by a, b;
select 2 in (select gp_segment_id from trand_part_reshuffle) as check;

5: commit;
5q:

select * from trand_part_reshuffle order by a, b;
select 2 in (select gp_segment_id from trand_part_reshuffle) as check;

drop table trand_part_reshuffle;

-- hash distributed heap table ao
create table thash_ao_reshuffle(a int, b int) with (appendonly=true) distributed by (a);
update gp_distribution_policy  set numsegments=2 where localoid='thash_ao_reshuffle'::regclass;

insert into thash_ao_reshuffle select i,i from generate_series(1,10) i;

select * from thash_ao_reshuffle order by a, b;
select 2 in (select gp_segment_id from thash_ao_reshuffle) as check;

1: begin;
1: alter table thash_ao_reshuffle set with (reshuffle);

-- since alter reshuffle transaction has not committed
-- the other transaction can access the table, but
-- can only see tuples from seg0, seg1.
select * from thash_ao_reshuffle order by a, b;
select 2 in (select gp_segment_id from thash_ao_reshuffle) as check;

1: abort;
-- test reshuffle can abort

select * from thash_ao_reshuffle order by a, b;
select 2 in (select gp_segment_id from thash_ao_reshuffle) as check;

1: begin;
1: alter table thash_ao_reshuffle set with (reshuffle);

select * from thash_ao_reshuffle order by a, b;
select 2 in (select gp_segment_id from thash_ao_reshuffle) as check;

1: commit;
1q:

select * from thash_ao_reshuffle order by a, b;
select 2 in (select gp_segment_id from thash_ao_reshuffle) as check;

drop table thash_ao_reshuffle;

-- random distributed heap table ao
create table trand_ao_reshuffle(a int, b int) with (appendonly=true) distributed randomly;
update gp_distribution_policy  set numsegments=2 where localoid='trand_ao_reshuffle'::regclass;

insert into trand_ao_reshuffle select i,i from generate_series(1,10) i;

select * from trand_ao_reshuffle order by a, b;
select 2 in (select gp_segment_id from trand_ao_reshuffle) as check;

2: begin;
2: alter table trand_ao_reshuffle set with (reshuffle);

-- since alter reshuffle transaction has not committed
-- the other transaction can access the table, but
-- can only see tuples from seg0, seg1.
select * from trand_ao_reshuffle order by a, b;
select 2 in (select gp_segment_id from trand_ao_reshuffle) as check;

2: abort;
-- test reshuffle can abort

select * from trand_ao_reshuffle order by a, b;
select 2 in (select gp_segment_id from trand_ao_reshuffle) as check;

2: begin;
2: alter table trand_ao_reshuffle set with (reshuffle);

select * from trand_ao_reshuffle order by a, b;
select 2 in (select gp_segment_id from trand_ao_reshuffle) as check;

2: commit;
2q:

select * from trand_ao_reshuffle order by a, b;
select 2 in (select gp_segment_id from trand_ao_reshuffle) as check;

drop table trand_ao_reshuffle;

-- replicated heap table ao
create table trep_ao_reshuffle(a int, b int) with (appendonly=true) distributed replicated;
update gp_distribution_policy  set numsegments=2 where localoid='trep_ao_reshuffle'::regclass;

create language plpythonu;
create function update_on_segment(tabname text, segid int) returns boolean as
$$
import pygresql.pg as pg
conn = pg.connect(dbname='isolation2test')
port = conn.query("select port from gp_segment_configuration where content = %d and role = 'p'" % segid).getresult()[0][0]
conn.close()

conn = pg.connect(dbname='isolation2test', opt='-c gp_session_role=utility', port=port)
conn.query("set allow_system_table_mods = true")
conn.query("update gp_distribution_policy set numsegments = 2 where localoid = '%s'::regclass" % tabname)
conn.close()

return True
$$
LANGUAGE plpythonu;

create function select_on_segment(sql text, segid int) returns table (like trep_ao_reshuffle) as
$$
import pygresql.pg as pg
conn = pg.connect(dbname='isolation2test')
port = conn.query("select port from gp_segment_configuration where content = %d and role = 'p'" % segid).getresult()[0][0]
conn.close()

conn = pg.connect(dbname='isolation2test', opt='-c gp_session_role=utility', port=port)
r = conn.query(sql).getresult()
conn.close()

return r
$$
LANGUAGE plpythonu;

update gp_distribution_policy set numsegments = 2 where localoid = 'trep_ao_reshuffle'::regclass;
select update_on_segment('trep_ao_reshuffle', 0);
select update_on_segment('trep_ao_reshuffle', 1);
select update_on_segment('trep_ao_reshuffle', 2);
insert into trep_ao_reshuffle select i,i from generate_series(1,10) i;

select * from trep_ao_reshuffle order by a, b;
select select_on_segment('select * from trep_ao_reshuffle', 2);

3: begin;
3: alter table trep_ao_reshuffle set with (reshuffle);

-- since alter reshuffle transaction has not committed
-- the other transaction can access the table, but
-- can only see tuples from seg0, seg1.
select * from trep_ao_reshuffle order by a, b;
select select_on_segment('select * from trep_ao_reshuffle', 2);

3: abort;
-- test reshuffle can abort

select * from trep_ao_reshuffle order by a, b;
select select_on_segment('select * from trep_ao_reshuffle', 2);

3: begin;
3: alter table trep_ao_reshuffle set with (reshuffle);

select * from trep_ao_reshuffle order by a, b;
select select_on_segment('select * from trep_ao_reshuffle', 2);

3: commit;
3q:

select * from trep_ao_reshuffle order by a, b;
select select_on_segment('select * from trep_ao_reshuffle', 2);

drop function update_on_segment(text, int);
drop function select_on_segment(text, int);
drop table trep_ao_reshuffle;

drop language plpythonu;

-- hash distributed heap table ao partition
create table thash_part_ao_reshuffle(a int, b int) with (appendonly=true) distributed by (a) partition by list(b) (partition thash_part_ao_reshuffle_1 values(1), partition thash_part_ao_reshuffle_2 values(2), default partition other);
update gp_distribution_policy set numsegments=2 where localoid='thash_part_ao_reshuffle'::regclass;
update gp_distribution_policy set numsegments=2 where localoid='thash_part_ao_reshuffle_1_prt_thash_part_ao_reshuffle_1'::regclass;
update gp_distribution_policy set numsegments=2 where localoid='thash_part_ao_reshuffle_1_prt_thash_part_ao_reshuffle_2'::regclass;
update gp_distribution_policy set numsegments=2 where localoid='thash_part_ao_reshuffle_1_prt_other'::regclass;

insert into thash_part_ao_reshuffle select i,i from generate_series(1,20) i;

select * from thash_part_ao_reshuffle order by a, b;
select 2 in (select gp_segment_id from thash_part_ao_reshuffle) as check;

1: begin;
1: alter table thash_part_ao_reshuffle set with (reshuffle);

-- since alter reshuffle transaction has not committed
-- the other transaction can access the table, but
-- can only see tuples from seg0, seg1.
select * from thash_part_ao_reshuffle order by a, b;
select 2 in (select gp_segment_id from thash_part_ao_reshuffle) as check;

1: abort;
-- test reshuffle can abort

select * from thash_part_ao_reshuffle order by a, b;
select 2 in (select gp_segment_id from thash_part_ao_reshuffle) as check;

1: begin;
1: alter table thash_part_ao_reshuffle set with (reshuffle);

select * from thash_part_ao_reshuffle order by a, b;
select 2 in (select gp_segment_id from thash_part_ao_reshuffle) as check;

1: commit;
1q:

select * from thash_part_ao_reshuffle order by a, b;
select 2 in (select gp_segment_id from thash_part_ao_reshuffle) as check;

drop table thash_part_ao_reshuffle;

-- random distributed heap table ao partiton
create table trand_part_ao_reshuffle(a int, b int) with (appendonly=true) distributed randomly partition by list(b) (partition trand_part_ao_reshuffle_1 values(1), partition trand_part_ao_reshuffle_2 values(2), default partition other);
update gp_distribution_policy set numsegments=2 where localoid='trand_part_ao_reshuffle'::regclass;
update gp_distribution_policy set numsegments=2 where localoid='trand_part_ao_reshuffle_1_prt_trand_part_ao_reshuffle_1'::regclass;
update gp_distribution_policy set numsegments=2 where localoid='trand_part_ao_reshuffle_1_prt_trand_part_ao_reshuffle_2'::regclass;
update gp_distribution_policy set numsegments=2 where localoid='trand_part_ao_reshuffle_1_prt_other'::regclass;

insert into trand_part_ao_reshuffle select i,i from generate_series(1,20) i;

select * from trand_part_ao_reshuffle order by a, b;
select 2 in (select gp_segment_id from trand_part_ao_reshuffle) as check;

2: begin;
2: alter table trand_part_ao_reshuffle set with (reshuffle);

-- since alter reshuffle transaction has not committed
-- the other transaction can access the table, but
-- can only see tuples from seg0, seg1.
select * from trand_part_ao_reshuffle order by a, b;
select 2 in (select gp_segment_id from trand_part_ao_reshuffle) as check;

2: abort;
-- test reshuffle can abort

select * from trand_part_ao_reshuffle order by a, b;
select 2 in (select gp_segment_id from trand_part_ao_reshuffle) as check;

2: begin;
2: alter table trand_part_ao_reshuffle set with (reshuffle);

select * from trand_part_ao_reshuffle order by a, b;
select 2 in (select gp_segment_id from trand_part_ao_reshuffle) as check;

2: commit;
2q:

select * from trand_part_ao_reshuffle order by a, b;
select 2 in (select gp_segment_id from trand_part_ao_reshuffle) as check;

drop table trand_part_ao_reshuffle;

-- hash distributed heap table
create table thash_aoco_reshuffle(a int, b int) with (appendonly=true) distributed by (a);
update gp_distribution_policy  set numsegments=2 where localoid='thash_aoco_reshuffle'::regclass;

insert into thash_aoco_reshuffle select i,i from generate_series(1,10) i;

select * from thash_aoco_reshuffle order by a, b;
select 2 in (select gp_segment_id from thash_aoco_reshuffle) as check;

1: begin;
1: alter table thash_aoco_reshuffle set with (reshuffle);

-- since alter reshuffle transaction has not committed
-- the other transaction can access the table, but
-- can only see tuples from seg0, seg1.
select * from thash_aoco_reshuffle order by a, b;
select 2 in (select gp_segment_id from thash_aoco_reshuffle) as check;

1: abort;
-- test reshuffle can abort

select * from thash_aoco_reshuffle order by a, b;
select 2 in (select gp_segment_id from thash_aoco_reshuffle) as check;

1: begin;
1: alter table thash_aoco_reshuffle set with (reshuffle);

select * from thash_aoco_reshuffle order by a, b;
select 2 in (select gp_segment_id from thash_aoco_reshuffle) as check;

1: commit;
1q:

select * from thash_aoco_reshuffle order by a, b;
select 2 in (select gp_segment_id from thash_aoco_reshuffle) as check;

drop table thash_aoco_reshuffle;

-- random distributed heap table
create table trand_aoco_reshuffle(a int, b int) with (appendonly=true, orientation=column) distributed randomly;
update gp_distribution_policy  set numsegments=2 where localoid='trand_aoco_reshuffle'::regclass;

insert into trand_aoco_reshuffle select i,i from generate_series(1,10) i;

select * from trand_aoco_reshuffle order by a, b;
select 2 in (select gp_segment_id from trand_aoco_reshuffle) as check;

2: begin;
2: alter table trand_aoco_reshuffle set with (reshuffle);

-- since alter reshuffle transaction has not committed
-- the other transaction can access the table, but
-- can only see tuples from seg0, seg1.
select * from trand_aoco_reshuffle order by a, b;
select 2 in (select gp_segment_id from trand_aoco_reshuffle) as check;

2: abort;
-- test reshuffle can abort

select * from trand_aoco_reshuffle order by a, b;
select 2 in (select gp_segment_id from trand_aoco_reshuffle) as check;

2: begin;
2: alter table trand_aoco_reshuffle set with (reshuffle);

select * from trand_aoco_reshuffle order by a, b;
select 2 in (select gp_segment_id from trand_aoco_reshuffle) as check;

2: commit;
2q:

select * from trand_aoco_reshuffle order by a, b;
select 2 in (select gp_segment_id from trand_aoco_reshuffle) as check;

drop table trand_aoco_reshuffle;

-- replicated heap table
create table trep_aoco_reshuffle(a int, b int) with (appendonly=true, orientation=column) distributed replicated;
update gp_distribution_policy  set numsegments=2 where localoid='trep_aoco_reshuffle'::regclass;

create language plpythonu;
create function update_on_segment(tabname text, segid int) returns boolean as
$$
import pygresql.pg as pg
conn = pg.connect(dbname='isolation2test')
port = conn.query("select port from gp_segment_configuration where content = %d and role = 'p'" % segid).getresult()[0][0]
conn.close()

conn = pg.connect(dbname='isolation2test', opt='-c gp_session_role=utility', port=port)
conn.query("set allow_system_table_mods = true")
conn.query("update gp_distribution_policy set numsegments = 2 where localoid = '%s'::regclass" % tabname)
conn.close()

return True
$$
LANGUAGE plpythonu;

create function select_on_segment(sql text, segid int) returns table (like trep_aoco_reshuffle) as
$$
import pygresql.pg as pg
conn = pg.connect(dbname='isolation2test')
port = conn.query("select port from gp_segment_configuration where content = %d and role = 'p'" % segid).getresult()[0][0]
conn.close()

conn = pg.connect(dbname='isolation2test', opt='-c gp_session_role=utility', port=port)
r = conn.query(sql).getresult()
conn.close()

return r
$$
LANGUAGE plpythonu;

update gp_distribution_policy set numsegments = 2 where localoid = 'trep_aoco_reshuffle'::regclass;
select update_on_segment('trep_aoco_reshuffle', 0);
select update_on_segment('trep_aoco_reshuffle', 1);
select update_on_segment('trep_aoco_reshuffle', 2);
insert into trep_aoco_reshuffle select i,i from generate_series(1,10) i;

select * from trep_aoco_reshuffle order by a, b;
select select_on_segment('select * from trep_aoco_reshuffle', 2);

3: begin;
3: alter table trep_aoco_reshuffle set with (reshuffle);

-- since alter reshuffle transaction has not committed
-- the other transaction can access the table, but
-- can only see tuples from seg0, seg1.
select * from trep_aoco_reshuffle order by a, b;
select select_on_segment('select * from trep_aoco_reshuffle', 2);

3: abort;
-- test reshuffle can abort

select * from trep_aoco_reshuffle order by a, b;
select select_on_segment('select * from trep_aoco_reshuffle', 2);

3: begin;
3: alter table trep_aoco_reshuffle set with (reshuffle);

select * from trep_aoco_reshuffle order by a, b;
select select_on_segment('select * from trep_aoco_reshuffle', 2);

3: commit;
3q:

select * from trep_aoco_reshuffle order by a, b;
select select_on_segment('select * from trep_aoco_reshuffle', 2);

drop function update_on_segment(text, int);
drop function select_on_segment(text, int);
drop table trep_aoco_reshuffle;

drop language plpythonu;

-- hash distributed heap table
create table thash_part_aoco_reshuffle(a int, b int) with (appendonly=true, orientation=column) distributed by (a) partition by list(b) (partition thash_part_aoco_reshuffle_1 values(1), partition thash_part_aoco_reshuffle_2 values(2), default partition other);
update gp_distribution_policy set numsegments=2 where localoid='thash_part_aoco_reshuffle'::regclass;
update gp_distribution_policy set numsegments=2 where localoid='thash_part_aoco_reshuffle_1_prt_thash_part_aoco_reshuffle_1'::regclass;
update gp_distribution_policy set numsegments=2 where localoid='thash_part_aoco_reshuffle_1_prt_thash_part_aoco_reshuffle_2'::regclass;
update gp_distribution_policy set numsegments=2 where localoid='thash_part_aoco_reshuffle_1_prt_other'::regclass;

insert into thash_part_aoco_reshuffle select i,i from generate_series(1,20) i;

select * from thash_part_aoco_reshuffle order by a, b;
select 2 in (select gp_segment_id from thash_part_aoco_reshuffle) as check;

1: begin;
1: alter table thash_part_aoco_reshuffle set with (reshuffle);

-- since alter reshuffle transaction has not committed
-- the other transaction can access the table, but
-- can only see tuples from seg0, seg1.
select * from thash_part_aoco_reshuffle order by a, b;
select 2 in (select gp_segment_id from thash_part_aoco_reshuffle) as check;

1: abort;
-- test reshuffle can abort

select * from thash_part_aoco_reshuffle order by a, b;
select 2 in (select gp_segment_id from thash_part_aoco_reshuffle) as check;

1: begin;
1: alter table thash_part_aoco_reshuffle set with (reshuffle);

select * from thash_part_aoco_reshuffle order by a, b;
select 2 in (select gp_segment_id from thash_part_aoco_reshuffle) as check;

1: commit;
1q:

select * from thash_part_aoco_reshuffle order by a, b;
select 2 in (select gp_segment_id from thash_part_aoco_reshuffle) as check;

drop table thash_part_aoco_reshuffle;

-- random distributed heap table
create table trand_part_aoco_reshuffle(a int, b int) with (appendonly=true, orientation=column) distributed randomly partition by list(b) (partition trand_part_aoco_reshuffle_1 values(1), partition trand_part_aoco_reshuffle_2 values(2), default partition other);
update gp_distribution_policy set numsegments=2 where localoid='trand_part_aoco_reshuffle'::regclass;
update gp_distribution_policy set numsegments=2 where localoid='trand_part_aoco_reshuffle_1_prt_trand_part_aoco_reshuffle_1'::regclass;
update gp_distribution_policy set numsegments=2 where localoid='trand_part_aoco_reshuffle_1_prt_trand_part_aoco_reshuffle_2'::regclass;
update gp_distribution_policy set numsegments=2 where localoid='trand_part_aoco_reshuffle_1_prt_other'::regclass;

insert into trand_part_aoco_reshuffle select i,i from generate_series(1,20) i;

select * from trand_part_aoco_reshuffle order by a, b;
select 2 in (select gp_segment_id from trand_part_aoco_reshuffle) as check;

2: begin;
2: alter table trand_part_aoco_reshuffle set with (reshuffle);

-- since alter reshuffle transaction has not committed
-- the other transaction can access the table, but
-- can only see tuples from seg0, seg1.
select * from trand_part_aoco_reshuffle order by a, b;
select 2 in (select gp_segment_id from trand_part_aoco_reshuffle) as check;

2: abort;
-- test reshuffle can abort

select * from trand_part_aoco_reshuffle order by a, b;
select 2 in (select gp_segment_id from trand_part_aoco_reshuffle) as check;

2: begin;
2: alter table trand_part_aoco_reshuffle set with (reshuffle);

select * from trand_part_aoco_reshuffle order by a, b;
select 2 in (select gp_segment_id from trand_part_aoco_reshuffle) as check;

2: commit;
2q:

select * from trand_part_aoco_reshuffle order by a, b;
select 2 in (select gp_segment_id from trand_part_aoco_reshuffle) as check;

drop table trand_part_aoco_reshuffle;

