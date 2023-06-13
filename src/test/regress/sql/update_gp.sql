-- Test DELETE and UPDATE on an inherited table.
-- The special aspect of this table is that the inherited table has
-- a different distribution key. 'p' table's distribution key matches
-- that of 'r', but 'p2's doesn't. Test that the planner adds a Motion
-- node correctly for p2.
create table todelete (a int) distributed by (a);
create table parent (a int, b int, c int) distributed by (a);
create table child (a int, b int, c int) inherits (parent) distributed by (b);

insert into parent select g, g, g from generate_series(1,5) g;
insert into child select g, g, g from generate_series(6,10) g;

insert into todelete select generate_series(3,4);

delete from parent using todelete where parent.a = todelete.a;

insert into todelete select generate_series(5,7);

update parent set c=c+100 from todelete where parent.a = todelete.a;

select * from parent;

drop table todelete;
drop table child;
drop table parent;

-- This is similar to the above, but with a partitioned table (which is
-- implemented by inheritance) rather than an explicitly inherited table.
-- The scans on some of the partitions degenerate into Result nodes with
-- False one-time filter, which don't need a Motion node.
create table todelete (a int, b int) distributed by (a);
create table target (a int, b int, c int)
        distributed by (a)
        partition by range (c) (start(1) end(5) every(1), default partition extra);

insert into todelete select g, g % 4 from generate_series(1, 10) g;
insert into target select g, 0, 3 from generate_series(1, 5) g;
insert into target select g, 0, 1 from generate_series(1, 5) g;

delete from target where c = 3 and a in (select b from todelete);

insert into todelete values (1, 5);

update target set b=target.b+100 where c = 3 and a in (select b from todelete);

select * from target;

-- Also test an update with a qual that doesn't match any partition. The
-- Append degenerates into a dummy Result with false One-Time Filter.
alter table target drop default partition;
update target set b = 10 where c = 10;

drop table todelete;
drop table target;

--
-- Test updated on inheritance parent table, where some child tables need a
-- Split Update, but not all.
--
create table base_tbl (a int4, b int4) distributed by (a);
create table child_a (a int4, b int4) inherits (base_tbl) distributed by (a);
create table child_b (a int4, b int4) inherits (base_tbl) distributed by (b);
insert into base_tbl select g, g from generate_series(1, 5) g;

explain (costs off) update base_tbl set a=a+1;
update base_tbl set a = 5;

--
-- Explicit Distribution motion must be added if any of the child nodes
-- contains any motion excluding the motions in initplans.
-- These test cases and expectation are applicable for GPDB planner not for ORCA.
--
SET gp_autostats_mode = NONE;
CREATE TABLE keo1 ( user_vie_project_code_pk character varying(24), user_vie_fiscal_year_period_sk character varying(24), user_vie_act_cntr_marg_cum character varying(24)) DISTRIBUTED RANDOMLY;
INSERT INTO keo1 VALUES ('1', '1', '1');

CREATE TABLE keo2 ( projects_pk character varying(24)) DISTRIBUTED RANDOMLY;
INSERT INTO keo2 VALUES ('1');

CREATE TABLE keo3 ( sky_per character varying(24), bky_per character varying(24)) DISTRIBUTED BY (sky_per);
INSERT INTO keo3 VALUES ('1', '1');

CREATE TABLE keo4 ( keo_para_required_period character varying(6), keo_para_budget_date character varying(24)) DISTRIBUTED RANDOMLY;
INSERT INTO keo4 VALUES ('1', '1');
-- Explicit Redistribution motion should be added in case of GPDB Planner (test case not applicable for ORCA)
EXPLAIN (COSTS OFF) UPDATE keo1 SET user_vie_act_cntr_marg_cum = 234.682 FROM
    ( SELECT a.user_vie_project_code_pk FROM keo1 a INNER JOIN keo2 b
        ON b.projects_pk=a.user_vie_project_code_pk
        WHERE a.user_vie_fiscal_year_period_sk =
          (SELECT MAX (sky_per) FROM keo3 WHERE bky_per =
             (SELECT keo4.keo_para_required_period FROM keo4 WHERE keo_para_budget_date =
                (SELECT min (keo4.keo_para_budget_date) FROM keo4)))
    ) t1
WHERE t1.user_vie_project_code_pk = keo1.user_vie_project_code_pk;
UPDATE keo1 SET user_vie_act_cntr_marg_cum = 234.682 FROM
    ( SELECT a.user_vie_project_code_pk FROM keo1 a INNER JOIN keo2 b
        ON b.projects_pk=a.user_vie_project_code_pk
        WHERE a.user_vie_fiscal_year_period_sk =
          (SELECT MAX (sky_per) FROM keo3 WHERE bky_per =
             (SELECT keo4.keo_para_required_period FROM keo4 WHERE keo_para_budget_date =
                (SELECT min (keo4.keo_para_budget_date) FROM keo4)))
    ) t1
WHERE t1.user_vie_project_code_pk = keo1.user_vie_project_code_pk;
SELECT user_vie_act_cntr_marg_cum FROM keo1;

-- Explicit Redistribution motion should not be added in case of GPDB Planner (test case not applicable to ORCA)
CREATE TABLE keo5 (x int, y int) DISTRIBUTED BY (x);
INSERT INTO keo5 VALUES (1,1);
EXPLAIN (COSTS OFF) DELETE FROM keo5 WHERE x IN (SELECT x FROM keo5 WHERE EXISTS (SELECT x FROM keo5 WHERE x < 2));
DELETE FROM keo5 WHERE x IN (SELECT x FROM keo5 WHERE EXISTS (SELECT x FROM keo5 WHERE x < 2));
SELECT x FROM keo5;

RESET gp_autostats_mode;
DROP TABLE keo1;
DROP TABLE keo2;
DROP TABLE keo3;
DROP TABLE keo4;
DROP TABLE keo5;

--
-- text types. We should support the following updates.
--

CREATE TEMP TABLE ttab1 (a varchar(15), b integer) DISTRIBUTED BY (a);
CREATE TEMP TABLE ttab2 (a varchar(15), b integer) DISTRIBUTED BY (a);

UPDATE ttab1 SET b = ttab2.b FROM ttab2 WHERE ttab1.a = ttab2.a;

DROP TABLE ttab1;
DROP TABLE ttab2;


CREATE TEMP TABLE ttab1 (a text, b integer) DISTRIBUTED BY (a);
CREATE TEMP TABLE ttab2 (a text, b integer) DISTRIBUTED BY (a);

UPDATE ttab1 SET b = ttab2.b FROM ttab2 WHERE ttab1.a = ttab2.a;


DROP TABLE ttab1;
DROP TABLE ttab2;

CREATE TEMP TABLE ttab1 (a varchar, b integer) DISTRIBUTED BY (a);
CREATE TEMP TABLE ttab2 (a varchar, b integer) DISTRIBUTED BY (a);

UPDATE ttab1 SET b = ttab2.b FROM ttab2 WHERE ttab1.a = ttab2.a;


DROP TABLE ttab1;
DROP TABLE ttab2;

CREATE TEMP TABLE ttab1 (a char(15), b integer) DISTRIBUTED BY (a);
CREATE TEMP TABLE ttab2 (a char(15), b integer) DISTRIBUTED BY (a);

UPDATE ttab1 SET b = ttab2.b FROM ttab2 WHERE ttab1.a = ttab2.a;

DROP TABLE IF EXISTS update_distr_key;

CREATE TEMP TABLE update_distr_key (a int, b int) DISTRIBUTED BY (a);
INSERT INTO update_distr_key select i, i* 10 from generate_series(0, 9) i;

UPDATE update_distr_key SET a = 5 WHERE b = 10;

SELECT * from update_distr_key;

DROP TABLE update_distr_key;

-- Update distribution key

-- start_ignore
drop table if exists r;
drop table if exists s;
drop table if exists update_dist;
drop table if exists ao_table;
drop table if exists aoco_table;
-- end_ignore

-- Update normal table distribution key
create table update_dist(a int) distributed by (a);
insert into update_dist values(1);
update update_dist set a=0 where a=1;
select * from update_dist;

-- Update distribution key with join

create table r (a int, b int) distributed by (a);
create table s (a int, b int) distributed by (a);
insert into r select generate_series(1, 5), generate_series(1, 5) * 2;
insert into s select generate_series(1, 5), generate_series(1, 5) * 2;
select * from r;
select * from s;
update r set a = r.a + 1 from s where r.a = s.a;
select * from r;
update r set a = r.a + 1 where a in (select a from s);
select * from r;

-- Update redistribution
delete from r;
delete from s;
insert into r select generate_series(1, 5), generate_series(1, 5);
insert into s select generate_series(1, 5), generate_series(1, 5) * 2;
select * from r;
select * from s;
update r set a = r.a + 1 from s where r.b = s.b;
select * from r;
update r set a = r.a + 1 where b in (select b from s);
select * from r;

-- Update redistribution with returning clause
delete from r;
delete from s;
insert into r select generate_series(1, 5), generate_series(1, 5);
insert into s select generate_series(6, 10), generate_series(1, 5);
update r set a = s.a from s where r.b = s.b returning r.b;
update r set a = s.a from s where r.b = s.b returning s.b;
select * from r order by 1;

-- Update hash aggreate group by
delete from r;
delete from s;
insert into r select generate_series(1, 5), generate_series(1, 5) * 2;
insert into s select generate_series(1, 5), generate_series(1, 5);
select * from r;
select * from s;
update s set a = s.a + 1 where exists (select 1 from r where s.a = r.b);
select * from s;

-- Update ao table distribution key
create table ao_table (a int, b int) WITH (appendonly=true) distributed by (a);
insert into ao_table select g, g from generate_series(1, 5) g;
select * from ao_table;
update ao_table set a = a + 1 where b = 3;
select * from ao_table;

-- Update aoco table distribution key
create table aoco_table (a int, b int) WITH (appendonly=true, orientation=column) distributed by (a);
insert into aoco_table select g,g from generate_series(1, 5) g;
select * from aoco_table;
update aoco_table set a = a + 1 where b = 3;
select * from aoco_table;

-- Update prepare
delete from s;
insert into s select generate_series(1, 5), generate_series(1, 5);
select * from r;
select * from s;
prepare update_s(int) as update s set a = s.a + $1 where exists (select 1 from r where s.a = r.b);
execute update_s(10);
select * from s;


-- Update distribution key of other columns dropped case
delete from s;
alter table s drop column b;
insert into s select i from generate_series(1, 5) i;
update s set a = a + 1;
select * from s;

-- Confirm that a split update is not created for a table excluded by
-- constraints in the planner.
create table nosplitupdate (a int) distributed by (a);
explain update nosplitupdate set a=0 where a=1 and a<1;

-- test split-update when split-node's flow is entry
create table tsplit_entry (c int);
insert into tsplit_entry values (1), (2);

explain update tsplit_entry set c = s.a from (select count(*) as a from gp_segment_configuration) s;
update tsplit_entry set c = s.a from (select count(*) as a from gp_segment_configuration) s;

CREATE TABLE update_gp_foo (
    a_dist int,
    b int,
    c_part int,
    d int
)
WITH (appendonly=false) DISTRIBUTED BY (a_dist) PARTITION BY RANGE(c_part)
          (
          PARTITION p20190305 START (1) END (2) WITH (tablename='update_gp_foo_1_prt_p20190305', appendonly=false)
          );

CREATE TABLE update_gp_foo1 (
        a_dist int,
        b int,
        c_part int,
        d int
)
WITH (appendonly=false) DISTRIBUTED BY (a_dist) PARTITION BY RANGE(c_part)
          (
          PARTITION p20190305 START (1) END (2) WITH (tablename='update_gp_foo1_1_prt_p20190305', appendonly=false)
          );

INSERT INTO update_gp_foo VALUES (12, 40, 1, 50);
INSERT INTO update_gp_foo1 VALUES (12, 3, 1, 50);

UPDATE update_gp_foo
SET    b = update_gp_foo.c_part,
       d = update_gp_foo1.a_dist
FROM   update_gp_foo1;

SELECT * from update_gp_foo;

-- Test for update with LASJ_NOTIN
-- See Issue: https://github.com/greenplum-db/gpdb/issues/13265
create table t1_13265(a int, b int, c int, d int) distributed by (a);
create table t2_13265(a int, b int, c int, d int) distributed by (a);

insert into t1_13265 values (1, null, 1, 1);
insert into t2_13265 values (2, null, 2, 2);

explain (verbose, costs off)
update t1_13265 set b = 2 where
(c, d) not in (select c, d from t2_13265 where a = 2);

update t1_13265 set b = 2 where
(c, d) not in (select c, d from t2_13265 where a = 2);

select * from t1_13265;

-- test for update on partition table
CREATE TABLE into_table (
  a numeric(10,0) NOT NULL,
  b numeric(10,0) NOT NULL,
  c numeric(10,0) NOT NULL,
  d character varying(4),
  e character varying(10),
  f int
) DISTRIBUTED BY (a, b, c) PARTITION BY RANGE(f) (start (1) end(5) every(1));

CREATE TABLE from_table (
  a numeric(10,0) NOT NULL,
  b numeric(10,0) NOT NULL,
  c numeric(10,0) NOT NULL,
  d character varying(4),
  e character varying(10),
  f int
) DISTRIBUTED BY (a);

insert into into_table select i*1.5,i*2,i*3,'dd'||i,'ee'||i, i from generate_series(1,4) i;
insert into from_table select i*1.5,i*2,i*3,'xx'||i,'yy'||i, i+1 from generate_series(1,3) i;

explain (costs off)
update into_table set d=from_table.d, e=from_table.e, f=from_table.f from from_table
where into_table.a=from_table.a and into_table.b=from_table.b and into_table.c=from_table.c;

-- start_matchsubs
-- m/"into_table_1_prt_\d" to partition "into_table_1_prt_\d"/
-- s/"into_table_1_prt_\d" to partition "into_table_1_prt_\d"/"XXX" to partition "YYY"/g
-- end_matchsubs
update into_table set d=from_table.d, e=from_table.e, f=from_table.f from from_table
where into_table.a=from_table.a and into_table.b=from_table.b and into_table.c=from_table.c;

select * from into_table order by a;

-- start_ignore
drop table r;
drop table s;
drop table update_dist;
drop table ao_table;
drop table aoco_table;
drop table nosplitupdate;
drop table tsplit_entry;
drop table t1_13265;
drop table t2_13265;
drop table into_table;
drop table from_table;
-- end_ignore
