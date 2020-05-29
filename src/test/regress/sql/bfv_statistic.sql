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
set allow_system_table_mods=true;
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
PARTITION BY RANGE(d) (START(1) END (5) EVERY(1));
ANALYZE T25289_T4;

--
-- expect NO crash when the statistic slot for an attribute is broken
--
CREATE TABLE good_tab(a int, b text);

CREATE TABLE test_broken_stats(a int, b text);
INSERT INTO test_broken_stats VALUES(1, 'abc'), (2, 'cde'), (3, 'efg'), (3, 'efg'), (3, 'efg'), (1, 'abc'), (2, 'cde'); 
ANALYZE test_broken_stats;
SET allow_system_table_mods=true;

-- Simulate broken stats by changing the data type of MCV slot to a different type than in pg_attribute 

-- start_matchsubs
-- m/ERROR:  invalid .* of type .*, for attribute of type .* \(selfuncs\.c\:\d+\)/
-- s/\(selfuncs\.c:\d+\)//
-- end_matchsubs

-- Broken MCVs
UPDATE pg_statistic SET stavalues1='{1,2,3}'::int[] WHERE starelid ='test_broken_stats'::regclass AND staattnum=2;
SELECT * FROM test_broken_stats t1, good_tab t2 WHERE t1.b = t2.b;

-- Broken histogram
UPDATE pg_statistic SET stakind2=2 WHERE starelid ='test_broken_stats'::regclass AND staattnum=2;
UPDATE pg_statistic SET stavalues2='{1,2,3}'::int[] WHERE starelid ='test_broken_stats'::regclass AND staattnum=2 and stakind2=2;
SELECT * FROM test_broken_stats t1, good_tab t2 WHERE t1.b = t2.b;

RESET allow_system_table_mods;

-- cardinality estimation for join on varchar, text, char and bpchar columns must account for FreqRemaining and NDVRemaining
-- resulting in better cardinality numbers for the joins in orca
-- start_ignore
DROP TABLE IF EXISTS test_join_card1;
DROP TABLE IF EXISTS test_join_card2;
-- end_ignore
CREATE TABLE test_join_card1 (a varchar, b varchar);
CREATE TABLE test_join_card2 (a varchar, b varchar);
CREATE TABLE test_join_card3 (a varchar, b varchar);
INSERT INTO test_join_card1 SELECT i::text, i::text FROM generate_series(1, 20000)i;
INSERT INTO test_join_card2 SELECT i::text, NULL FROM generate_series(1, 179)i;
INSERT INTO test_join_card2 SELECT 1::text, 'a' FROM generate_series(1, 5820)i;
INSERT INTO test_join_card3 SELECT i::text, i::text FROM generate_series(1,10000)i;
ANALYZE test_join_card1;
ANALYZE test_join_card2;
ANALYZE test_join_card3;
EXPLAIN SELECT * FROM test_join_card1 t1, test_join_card2 t2, test_join_card3 t3 WHERE t1.b = t2.b and t3.b = t2.b;
-- start_ignore
DROP TABLE IF EXISTS test_join_card1;
DROP TABLE IF EXISTS test_join_card2;
-- end_ignore

-- Test if the table pg_statistic has data in segments

DROP TABLE IF EXISTS test_statistic_1;
CREATE TABLE test_statistic_1(a int, b int);
INSERT INTO test_statistic_1 SELECT i, i FROM generate_series(1, 1000)i;

select count(*) from pg_class c, pg_statistic s where c.oid = s.starelid and relname = 'test_statistic_1';
select count(*) from pg_class c, gp_dist_random('pg_statistic') s where c.oid = s.starelid and relname = 'test_statistic_1';

DROP TABLE test_statistic_1;
