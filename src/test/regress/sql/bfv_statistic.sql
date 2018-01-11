create schema bfv_statistic;
set search_path=bfv_statistic;
create table bfv_statistics_foo (a int, b int) distributed by (a);
insert into bfv_statistics_foo values (1,1);
insert into bfv_statistics_foo values (0,1);
insert into bfv_statistics_foo values (2,1);
insert into bfv_statistics_foo values (null,1);
analyze bfv_statistics_foo;

-- current statistics
select stanullfrac, stadistinct, stanumbers1 from pg_statistic where starelid='bfv_statistics_foo'::regclass and staattnum=1;

-- exercise GPORCA translator
explain select * from bfv_statistics_foo where a is not null and b >= 1;

create table bfv_statistics_foo2(a int) distributed by (a);
insert into bfv_statistics_foo2 select generate_series(1,5);
insert into bfv_statistics_foo2 select 1 from generate_series(1,5);
insert into bfv_statistics_foo2 select 2 from generate_series(1,4);
insert into bfv_statistics_foo2 select 3 from generate_series(1,3);
insert into bfv_statistics_foo2 select 4 from generate_series(1,2);
insert into bfv_statistics_foo2 select 5 from generate_series(1,1);
analyze bfv_statistics_foo2;
-- current stats
select stanumbers1, stavalues1 from pg_statistic where starelid='bfv_statistics_foo2'::regclass;

explain select a from bfv_statistics_foo2 where a > 1 order by a;

-- change stats manually so that MCV and MCF numbers do not match
set allow_system_table_mods=DML;
update pg_statistic set stavalues1='{6,3,1,5,4,2}'::int[] where starelid='bfv_statistics_foo2'::regclass;

-- excercise the translator
explain select a from bfv_statistics_foo2 where a > 1 order by a;

--
-- test missing statistics
--

set gp_create_table_random_default_distribution=off;
create table bfv_statistics_foo3(a int);

select * from gp_toolkit.gp_stats_missing where smischema = 'bfv_statistic' AND  smitable = 'bfv_statistics_foo3';

--
-- for Orca's Split Operator ensure that the columns needed for stats derivation is correct
--

set gp_create_table_random_default_distribution=off;

CREATE TABLE bar_dml (
    vtrg character varying(6) NOT NULL,
    tec_schuld_whg character varying(3) NOT NULL,
    inv character varying(11) NOT NULL,
    zed_id character varying(6) NOT NULL,
    mkl_id character varying(6) NOT NULL,
    zj integer NOT NULL,
    folio integer NOT NULL,
    zhlg_typ character varying(1) NOT NULL,
    zhlg character varying(8) NOT NULL,
    ant_zhlg double precision,
    zuordn_sys_dat character varying(11),
    zhlg_whg_bilkurs numeric(15,8),
    tec_whg_bilkurs numeric(15,8),
    zhlg_ziel_id character varying(1) NOT NULL,
    btg_tec_whg_gesh numeric(13,2),
    btg_tec_whg_makl numeric(13,2),
    btg_zhlg_whg numeric(13,2),
    zhlg_typ_org character varying(1),
    zhlg_org character varying(8),
    upd_dat date
)
WITH (appendonly=true) DISTRIBUTED RANDOMLY;

update bar_dml set (zhlg_org, zhlg_typ_org) = (zhlg, zhlg_typ);

--
-- Cardinality estimation when there is no histogram and MCV
--

create table bfv_statistics_foo4 (a int);

insert into bfv_statistics_foo4 select i from generate_series(1,99) i;
insert into bfv_statistics_foo4 values (NULL);
analyze bfv_statistics_foo4;

select stanullfrac, stadistinct, stanumbers1 from pg_statistic where starelid='bfv_statistics_foo4'::regclass and staattnum=1;

explain select a from bfv_statistics_foo4 where a > 888;

--
-- Testing that the merging of memo groups inside Orca does not crash cardinality estimation inside Orca
--

create table t1(c1 int);
insert into t1 values(1);

select v from (select max(c1) as v, 1 as r from t1 union select 1 as v, 2 as r ) as bfv_statistics_foo group by v;

select v from (select max(c1) as v, 1 as r from t1 union all select 1 as v, 2 as r ) as bfv_statistics_foo group by v;

select v from (select max(c1) as v, 1 as r from t1 union select 1 as v, 2 as r ) as bfv_statistics_foo;

--
-- test the generation of histogram boundaries for numeric and real data types
--

create table bfv_statistics_foo_real (a int4, b real) distributed randomly;

insert into bfv_statistics_foo_real values (0, 'Infinity');
insert into bfv_statistics_foo_real values (0, '-Infinity');
insert into bfv_statistics_foo_real values (0, 'NaN');
insert into bfv_statistics_foo_real values (0, 'Infinity');
insert into bfv_statistics_foo_real values (0, '-Infinity');
insert into bfv_statistics_foo_real values (0, 'NaN');
insert into bfv_statistics_foo_real values (0, 'Infinity');
insert into bfv_statistics_foo_real values (0, '-Infinity');
insert into bfv_statistics_foo_real values (0, 'NaN');
insert into bfv_statistics_foo_real values (0, 'Infinity');
insert into bfv_statistics_foo_real values (0, '-Infinity');
insert into bfv_statistics_foo_real values (0, 'NaN');
INSERT INTO bfv_statistics_foo_real VALUES (0, '0');
INSERT INTO bfv_statistics_foo_real VALUES (1, '0');
INSERT INTO bfv_statistics_foo_real VALUES (2, '-34338492.215397047');
INSERT INTO bfv_statistics_foo_real VALUES (3, '4.31');
INSERT INTO bfv_statistics_foo_real VALUES (4, '7799461.4119');
INSERT INTO bfv_statistics_foo_real VALUES (5, '16397.038491');
INSERT INTO bfv_statistics_foo_real VALUES (6, '93901.57763026');
INSERT INTO bfv_statistics_foo_real VALUES (7, '-83028485');
INSERT INTO bfv_statistics_foo_real VALUES (8, '74881');
INSERT INTO bfv_statistics_foo_real VALUES (9, '-24926804.045047420');
INSERT INTO bfv_statistics_foo_real VALUES (0, '0');
INSERT INTO bfv_statistics_foo_real VALUES (1, '0');
INSERT INTO bfv_statistics_foo_real VALUES (2, '-34338492.215397047');
INSERT INTO bfv_statistics_foo_real VALUES (3, '4.31');
INSERT INTO bfv_statistics_foo_real VALUES (4, '7799461.4119');
INSERT INTO bfv_statistics_foo_real VALUES (5, '16397.038491');
INSERT INTO bfv_statistics_foo_real VALUES (6, '93901.57763026');
INSERT INTO bfv_statistics_foo_real VALUES (7, '-83028485');
INSERT INTO bfv_statistics_foo_real VALUES (8, '74881');
INSERT INTO bfv_statistics_foo_real VALUES (9, '-24926804.045047420');
INSERT INTO bfv_statistics_foo_real VALUES (10, NULL);
INSERT INTO bfv_statistics_foo_real VALUES (10, NULL);
INSERT INTO bfv_statistics_foo_real VALUES (10, NULL);
INSERT INTO bfv_statistics_foo_real VALUES (10, NULL);
INSERT INTO bfv_statistics_foo_real VALUES (10, NULL);
INSERT INTO bfv_statistics_foo_real VALUES (10, NULL);
INSERT INTO bfv_statistics_foo_real VALUES (10, NULL);
INSERT INTO bfv_statistics_foo_real VALUES (10, NULL);
INSERT INTO bfv_statistics_foo_real VALUES (10, NULL);
INSERT INTO bfv_statistics_foo_real VALUES (9, '-24926804.045047420');
INSERT INTO bfv_statistics_foo_real VALUES (0, '0');
INSERT INTO bfv_statistics_foo_real VALUES (1, '0');
INSERT INTO bfv_statistics_foo_real VALUES (2, '-34338492.215397047');
INSERT INTO bfv_statistics_foo_real VALUES (3, '4.31');
INSERT INTO bfv_statistics_foo_real VALUES (4, '7799461.4119');
INSERT INTO bfv_statistics_foo_real VALUES (5, '16397.038491');
INSERT INTO bfv_statistics_foo_real VALUES (6, '93901.57763026');
INSERT INTO bfv_statistics_foo_real VALUES (7, '-83028485');
INSERT INTO bfv_statistics_foo_real VALUES (8, '74881');
INSERT INTO bfv_statistics_foo_real VALUES (9, '-24926804.045047420');

ANALYZE bfv_statistics_foo_real;

select histogram_bounds from pg_stats where tablename = 'bfv_statistics_foo_real' and attname = 'b';

select most_common_vals from pg_stats where tablename = 'bfv_statistics_foo_real' and attname = 'b';

create table bfv_statistics_foo_numeric (a int4, b numeric) distributed randomly;

insert into bfv_statistics_foo_numeric values (0, 'NaN');
insert into bfv_statistics_foo_numeric values (0, 'NaN');
insert into bfv_statistics_foo_numeric values (0, 'NaN');
insert into bfv_statistics_foo_numeric values (0, 'NaN');
insert into bfv_statistics_foo_numeric values (0, 'NaN');
insert into bfv_statistics_foo_numeric values (0, 'NaN');
insert into bfv_statistics_foo_numeric values (0, 'NaN');
insert into bfv_statistics_foo_numeric values (0, 'NaN');
INSERT INTO bfv_statistics_foo_numeric VALUES (0, '0');
INSERT INTO bfv_statistics_foo_numeric VALUES (1, '0');
INSERT INTO bfv_statistics_foo_numeric VALUES (2, '-34338492.215397047');
INSERT INTO bfv_statistics_foo_numeric VALUES (3, '4.31');
INSERT INTO bfv_statistics_foo_numeric VALUES (4, '7799461.4119');
INSERT INTO bfv_statistics_foo_numeric VALUES (5, '16397.038491');
INSERT INTO bfv_statistics_foo_numeric VALUES (6, '93901.57763026');
INSERT INTO bfv_statistics_foo_numeric VALUES (7, '-83028485');
INSERT INTO bfv_statistics_foo_numeric VALUES (8, '74881');
INSERT INTO bfv_statistics_foo_numeric VALUES (9, '-24926804.045047420');
INSERT INTO bfv_statistics_foo_numeric VALUES (0, '0');
INSERT INTO bfv_statistics_foo_numeric VALUES (1, '0');
INSERT INTO bfv_statistics_foo_numeric VALUES (2, '-34338492.215397047');
INSERT INTO bfv_statistics_foo_numeric VALUES (3, '4.31');
INSERT INTO bfv_statistics_foo_numeric VALUES (4, '7799461.4119');
INSERT INTO bfv_statistics_foo_numeric VALUES (5, '16397.038491');
INSERT INTO bfv_statistics_foo_numeric VALUES (6, '93901.57763026');
INSERT INTO bfv_statistics_foo_numeric VALUES (7, '-83028485');
INSERT INTO bfv_statistics_foo_numeric VALUES (8, '74881');
INSERT INTO bfv_statistics_foo_numeric VALUES (9, '-24926804.045047420');
INSERT INTO bfv_statistics_foo_numeric VALUES (10, NULL);
INSERT INTO bfv_statistics_foo_numeric VALUES (10, NULL);
INSERT INTO bfv_statistics_foo_numeric VALUES (10, NULL);
INSERT INTO bfv_statistics_foo_numeric VALUES (10, NULL);
INSERT INTO bfv_statistics_foo_numeric VALUES (10, NULL);
INSERT INTO bfv_statistics_foo_numeric VALUES (10, NULL);
INSERT INTO bfv_statistics_foo_numeric VALUES (10, NULL);
INSERT INTO bfv_statistics_foo_numeric VALUES (10, NULL);
INSERT INTO bfv_statistics_foo_numeric VALUES (10, NULL);
INSERT INTO bfv_statistics_foo_numeric VALUES (9, '-24926804.045047420');
INSERT INTO bfv_statistics_foo_numeric VALUES (0, '0');
INSERT INTO bfv_statistics_foo_numeric VALUES (1, '0');
INSERT INTO bfv_statistics_foo_numeric VALUES (2, '-34338492.215397047');
INSERT INTO bfv_statistics_foo_numeric VALUES (3, '4.31');
INSERT INTO bfv_statistics_foo_numeric VALUES (4, '7799461.4119');
INSERT INTO bfv_statistics_foo_numeric VALUES (5, '16397.038491');
INSERT INTO bfv_statistics_foo_numeric VALUES (6, '93901.57763026');
INSERT INTO bfv_statistics_foo_numeric VALUES (7, '-83028485');
INSERT INTO bfv_statistics_foo_numeric VALUES (8, '74881');
INSERT INTO bfv_statistics_foo_numeric VALUES (9, '-24926804.045047420');
INSERT INTO bfv_statistics_foo_numeric SELECT i,i FROM generate_series(1,30) i;

ANALYZE bfv_statistics_foo_numeric;

select histogram_bounds from pg_stats where tablename = 'bfv_statistics_foo_numeric' and attname = 'b';

select most_common_vals from pg_stats where tablename = 'bfv_statistics_foo_numeric' and attname = 'b';

reset gp_create_table_random_default_distribution;

--
-- Ensure that VACUUM ANALYZE does not result in incorrect statistics
--

CREATE TABLE T25289_T1 (c int);
INSERT INTO T25289_T1 VALUES (1);
DELETE FROM T25289_T1;
ANALYZE T25289_T1;

--
-- expect NO more notice after customer run VACUUM FULL
-- 

CREATE TABLE T25289_T2 (c int);
INSERT INTO T25289_T2 VALUES (1);
DELETE FROM T25289_T2;
VACUUM FULL T25289_T2;
ANALYZE T25289_T2;

--
-- expect NO notice during analyze if table doesn't have empty pages
--

CREATE TABLE T25289_T3 (c int);
INSERT INTO T25289_T3 VALUES (1);
ANALYZE T25289_T3;

--
-- expect NO notice when analyzing append only tables
-- 

CREATE TABLE T25289_T4 (c int, d int)
WITH (APPENDONLY=ON) DISTRIBUTED BY (c)
PARTITION BY RANGE(d) (START(1) END (100) EVERY(1));
ANALYZE T25289_T4;

