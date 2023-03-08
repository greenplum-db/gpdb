-- turn off autostats so we don't have to worry about the logging of the autostat queries
set gp_autostats_mode = None;

-- create needed tables (in a transaction, for speed)
begin;

create table direct_test
(
  key int NULL,
  value varchar(50) NULL
)
distributed by (key); 

create table direct_test_two_column
(
  key1 int NULL,
  key2 int NULL,
  value varchar(50) NULL
)
distributed by (key1, key2);

create table direct_test_bitmap  as select '2008-02-01'::DATE AS DT,
        case when j <= 996
                then 0
        when j<= 998 then 2
        when j<=999 then 3
        when i%10000 < 9000 then 4
        when i%10000 < 9800 then 5
        when i % 10000 <= 9998 then 5 else 6
        end as ind,
        (i*1017-j)::bigint as s from generate_series(1,10) i, generate_series(1,10) j distributed by (dt);
create index direct_test_bitmap_idx on direct_test_bitmap using bitmap (ind, dt);

CREATE TABLE direct_test_partition (trans_id int, date date, amount decimal(9,2), region text) DISTRIBUTED BY (trans_id) PARTITION BY RANGE (date) (START (date '2008-01-01') INCLUSIVE END (date '2009-01-01') EXCLUSIVE EVERY (INTERVAL '1month') );

create unique index direct_test_uk on direct_test_partition(trans_id,date);

create table direct_test_range_partition (a int, b int, c int, d int) distributed by (a) partition by range(d) (start(1) end(10) every(1));
insert into direct_test_range_partition select i, i+1, i+2, i+3 from generate_series(1, 2) i;

commit;

-- enable printing of printing info
set test_print_direct_dispatch_info=on;

-- Constant single-row insert, one column in distribution
-- DO direct dispatch
insert into direct_test values (100, 'cow');
-- verify
select * from direct_test order by key, value;

-- Constant single-row update, one column in distribution
-- DO direct dispatch
-- Known_opt_diff: MPP-21346
update direct_test set value = 'horse' where key = 100;
-- verify
select * from direct_test order by key, value;

-- Constant single-row delete, one column in distribution
-- DO direct dispatch
-- Known_opt_diff: MPP-21346
delete from direct_test where key = 100;
-- verify
select * from direct_test order by key, value;

-- Constant single-row insert, one column in distribution
-- DO direct dispatch
insert into direct_test values (NULL, 'cow');
-- verify
select * from direct_test order by key, value;

-- DELETE with an IS NULL predicate
-- DO direct dispatch
delete from direct_test where key is null;

-- Same single-row insert as above, but with DEFAULT instead of an explicit values.
-- DO direct dispatch
insert into direct_test values (default, 'cow');
-- verify
select * from direct_test order by key, value;

-- Constant single-row insert, two columns in distribution
-- DO direct dispatch
-- Known_opt_diff: MPP-21346
insert into direct_test_two_column values (100, 101, 'cow');
-- verify
select * from direct_test_two_column order by key1, key2, value;

-- Constant single-row update, two columns in distribution
-- DO direct dispatch
-- Known_opt_diff: MPP-21346
update direct_test_two_column set value = 'horse' where key1 = 100 and key2 = 101;
-- verify
select * from direct_test_two_column order by key1, key2, value;

-- Constant single-row delete, two columns in distribution
-- DO direct dispatch
delete from direct_test_two_column where key1 = 100 and key2 = 101;
-- verify
select * from direct_test_two_column order by key1, key2, value;

-- expression single-row insert
-- DO direct dispatch
insert into direct_test (key, value) values ('123',123123);
insert into direct_test (key, value) values (sqrt(100*10*10),123123);
--
-- should get 100 and 123 as the values
--
select * from direct_test where value = '123123' order by key;

delete from direct_test where value = '123123';

--------------------------------------------------------------------------------
-- Multiple row update, where clause lists multiple values which hash differently so no direct dispatch
--
-- note that if the hash function for values changes then certain segment configurations may actually 
--                hash all these values to the same content! (and so test would change)
--
update direct_test set value = 'pig' where key in (1,2,3,4,5);

update direct_test_two_column set value = 'pig' where key1 = 100 and key2 in (1,2,3,4);
update direct_test_two_column set value = 'pig' where key1 in (100,101,102,103,104) and key2 in (1);
update direct_test_two_column set value = 'pig' where key1 in (100,101) and key2 in (1,2);

-- Multiple row update, where clause lists values which all hash to same segment
-- DO direct dispatch
-- CAN'T IMPLEMENT THIS TEST BECAUSE THE # of segments changes again (unless we use a # of segments function, and exploit the simple nature of int4 hashing -- can we do that?)


------------------------------
-- Transaction cases
--
-- note that single-row insert can happen BUT DTM will always go to all contents
--
begin;
insert into direct_test values (1,100);
rollback;

begin;
insert into direct_test values (1,100);
insert into direct_test values (2,100);
insert into direct_test values (3,100);
rollback;

-------------------
-- MPP-7634: bitmap index scan
--
-- Known_opt_diff: MPP-21346
select count(*) from direct_test_bitmap where dt='2008-02-05';
-- Known_opt_diff: MPP-21346
select count(*) from direct_test_bitmap where dt='2008-02-01';
----------------------------------------------------------------------------------
-- MPP-7637: partitioned table
--
insert into direct_test_partition values (1,'2008-01-02',1,'usa');
select * from direct_test_partition where trans_id =1;
----------------------------------------------------------------------------------
-- MPP-7638: range table partition
--
-- Known_opt_diff: MPP-21346
select count(*) from direct_test_range_partition where a =1;
----------------------------------------------------------------------------------
-- Prepared statements
--  do same as above ones but using prepared statements, verify data goes to the right spot
prepare test_insert (int) as insert into direct_test values ($1,100);
execute test_insert(1);
execute test_insert(2);

select * from direct_test;

prepare test_update (int) as update direct_test set value = 'boo' where key = $1;
-- Known_opt_diff: MPP-21346
execute test_update(2);

select * from direct_test;

------------------------
-- A subquery
--
set test_print_direct_dispatch_info=off;
CREATE TEMP TABLE direct_dispatch_foo (id integer) DISTRIBUTED BY (id);
CREATE TEMP TABLE direct_dispatch_bar (id1 integer, id2 integer) DISTRIBUTED by (id1);

INSERT INTO direct_dispatch_foo VALUES (1);

INSERT INTO direct_dispatch_bar VALUES (1, 1);
INSERT INTO direct_dispatch_bar VALUES (2, 2);
INSERT INTO direct_dispatch_bar VALUES (3, 1);

set test_print_direct_dispatch_info=on;
-- Known_opt_diff: MPP-21346
SELECT * FROM direct_dispatch_foo WHERE id IN
    (SELECT id2 FROM (SELECT DISTINCT id1, id2 FROM direct_dispatch_bar WHERE direct_dispatch_bar.id1 = 1) AS s) ORDER BY 1;
--
-- this one will NOT do direct dispatch because it is a many slice query and those are disabled right now
SELECT * FROM direct_dispatch_foo WHERE id IN
    (SELECT id2 FROM (SELECT id1, id2 FROM direct_dispatch_bar WHERE direct_dispatch_bar.id1 = 1 UNION
                      SELECT id1, id2 FROM direct_dispatch_bar WHERE direct_dispatch_bar.id1 = 2) AS s) ORDER BY 1;

-- simple one using an expression on the variable
SELECT * from direct_dispatch_foo WHERE id * id = 1;
SELECT * from direct_dispatch_foo WHERE id * id = 1 OR id = 1;
SELECT * from direct_dispatch_foo where id * id = 1 AND id = 1;

-- main plan is direct dispatch and also has init plans
update direct_dispatch_bar set id2 = 1 where id1 = 1 and exists (select * from direct_dispatch_foo where id = 2);

-- init plan to see how transaction escalation happens
-- Known_opt_diff: MPP-21346
delete from direct_dispatch_foo where id = (select max(id2) from direct_dispatch_bar where id1 = 5);
-- Known_opt_diff: MPP-21346
delete from direct_dispatch_foo where id * id = (select max(id2) from direct_dispatch_bar where id1 = 5) AND id = 3;
-- Known_opt_diff: MPP-21346
delete from direct_dispatch_foo where id * id = (select max(id2) from direct_dispatch_bar) AND id = 3;

-- tests with subplans (MPP-22019)
CREATE TABLE MPP_22019_a ( i INT, j INT) DISTRIBUTED BY (i);
INSERT INTO MPP_22019_a (
 SELECT i, i * i FROM generate_series(1, 10) AS i);
CREATE TABLE MPP_22019_b (i INT, j INT) DISTRIBUTED BY (i);
INSERT INTO MPP_22019_b (SELECT i, i * i FROM generate_series(1, 10) AS i);
EXPLAIN SELECT a.* FROM MPP_22019_a a INNER JOIN MPP_22019_b b ON a.i = b.i WHERE a.j NOT IN (SELECT j FROM MPP_22019_a a2 where a2.j = b.j) and a.i = 1;
SELECT a.* FROM MPP_22019_a a INNER JOIN MPP_22019_b b ON a.i = b.i WHERE a.j NOT IN (SELECT j FROM MPP_22019_a a2 where a2.j = b.j) and a.i = 1;
SELECT a.* FROM MPP_22019_a a  WHERE a.j NOT IN (SELECT j FROM MPP_22019_a a2 where a2.j = a.j) and a.i = 1;


--
-- Test direct dispatch with volatile functions, and nextval().
--

-- Simple table.
create table ddtesttab (i int, j int, k int8) distributed by (k);
create sequence ddtestseq cache 1;

insert into ddtesttab values (1, 1, 5);
insert into ddtesttab values (1, 1, 5 + random()); -- volatile expression as distribution key
insert into ddtesttab values (1, 1, nextval('ddtestseq'));
insert into ddtesttab values (1, 1, 5 + nextval('ddtestseq'));

drop table ddtesttab;

-- Partitioned table, with mixed distribution keys.
create table ddtesttab (i int, j int, k int8) distributed by (i) partition by
range(k)
(start(1) end(20) every(10));


insert into ddtesttab values (1, 1, 5);
insert into ddtesttab values (1, 1, 5 + random()); -- volatile expression as distribution key
insert into ddtesttab values (1, 1, nextval('ddtestseq'));
insert into ddtesttab values (1, 1, 5 + nextval('ddtestseq'));

-- One partition is randomly distributed, while others are distributed by key.
alter table ddtesttab_1_prt_2 set distributed randomly;

insert into ddtesttab values (1, 1, 5);
insert into ddtesttab values (1, 1, 5 + random()); -- volatile expression as distribution key
insert into ddtesttab values (1, 1, nextval('ddtestseq'));
insert into ddtesttab values (1, 1, 5 + nextval('ddtestseq'));

drop table ddtesttab;
drop sequence ddtestseq;

-- Test prepare statement will choose custom plan instead of generic plan when
-- considering no direct dispatch cost.
create table test_prepare(i int, j int);
-- insert case
prepare p1 as insert into test_prepare values($1, 1);
execute p1(1);
execute p1(1);
execute p1(1);
execute p1(1);
execute p1(1);
-- the first 5 execute will always use custom plan, focus on the 6th one.
execute p1(1);

-- update case
prepare p2 as update test_prepare set j =2 where i =$1;
execute p2(1);
execute p2(1);
execute p2(1);
execute p2(1);
execute p2(1);
execute p2(1);

-- select case
prepare p3 as select * from test_prepare where i =$1;
execute p3(1);
execute p3(1);
execute p3(1);
execute p3(1);
execute p3(1);
execute p3(1);
drop table test_prepare;

-- Tests to check direct dispatch if the table is randomly distributed and the
-- filter has condition on gp_segment_id

-- NOTE: Only EXPLAIN query included, output of SELECT query is not shown.
-- Since the table is distributed randomly, the output of SELECT query
-- will differ everytime new table is created, and hence the during comparision
-- the tests will fail.

drop table if exists bar_randDistr;
create table bar_randDistr(col1 int, col2 int) distributed randomly;
insert into bar_randDistr select i,i*2 from generate_series(1, 10)i;

-- Case 1 : simple conditions on gp_segment_id
explain (costs off) select gp_segment_id, * from bar_randDistr where gp_segment_id=0;
explain (costs off) select gp_segment_id, * from bar_randDistr where gp_segment_id=1 or gp_segment_id=2;
explain (costs off) select gp_segment_id, count(*) from bar_randDistr group by gp_segment_id;

-- Case2: Conjunction scenario with filter condition on gp_segment_id and column
explain (costs off) select gp_segment_id, * from bar_randDistr where gp_segment_id=0 and col1 between 1 and 10;

-- Case3: Disjunction scenario with filter condition on gp_segment_id and column
explain (costs off) select gp_segment_id, * from bar_randDistr where gp_segment_id=1 or (col1=6 and gp_segment_id=2);

-- Case4: Scenario with constant/variable column and constant/variable gp_segment_id
explain (costs off) select gp_segment_id, * from bar_randDistr where col1 =3 and gp_segment_id in (0,1);
explain (costs off) select gp_segment_id, * from bar_randDistr where col1 =3 and gp_segment_id <>1;
explain (costs off) select gp_segment_id, * from bar_randDistr where col1 between 1 and 5 and gp_segment_id =0;
explain (costs off) select gp_segment_id, * from bar_randDistr where col1 in (1,5) and gp_segment_id <> 0;
explain (costs off) select gp_segment_id, * from bar_randDistr where col1 in (1,5) and gp_segment_id in (0,1);

-- Case5: Scenarios with special conditions
create function afunc() returns integer as $$ begin return 42; end; $$ language plpgsql;
create function immutable_func() returns integer as $$ begin return 42; end; $$ language plpgsql immutable;

explain (costs off) select * from bar_randDistr where col1 = 1;
explain (costs off) select * from bar_randDistr where gp_segment_id % 2 = 0;
explain (costs off) select * from bar_randDistr where gp_segment_id=immutable_func();
explain (costs off) select * from bar_randDistr where gp_segment_id=afunc();

drop table if exists bar_randDistr;


-- test direct dispatch via gp_segment_id qual
create table t_test_dd_via_segid(id int);
insert into t_test_dd_via_segid select * from generate_series(1, 6);

explain (costs off) select gp_segment_id, id from t_test_dd_via_segid where gp_segment_id=0;
select gp_segment_id, id from t_test_dd_via_segid where gp_segment_id=0;

explain (costs off) select gp_segment_id, id from t_test_dd_via_segid where gp_segment_id=1;
select gp_segment_id, id from t_test_dd_via_segid where gp_segment_id=1;

explain (costs off) select gp_segment_id, id from t_test_dd_via_segid where gp_segment_id=2;
select gp_segment_id, id from t_test_dd_via_segid where gp_segment_id=2;

explain (costs off) select gp_segment_id, id from t_test_dd_via_segid where gp_segment_id=1 or gp_segment_id=2;
select gp_segment_id, id from t_test_dd_via_segid where gp_segment_id=1 or gp_segment_id=2;

explain (costs off) select gp_segment_id, id from t_test_dd_via_segid where gp_segment_id=1 or gp_segment_id=2 or gp_segment_id=3;
select gp_segment_id, id from t_test_dd_via_segid where gp_segment_id=1 or gp_segment_id=2 or gp_segment_id=3;

explain (costs off) select t1.gp_segment_id, t2.gp_segment_id, * from t_test_dd_via_segid t1, t_test_dd_via_segid t2 where t1.gp_segment_id=t2.id;
select t1.gp_segment_id, t2.gp_segment_id, * from t_test_dd_via_segid t1, t_test_dd_via_segid t2 where t1.gp_segment_id=t2.id;

explain (costs off) select gp_segment_id, count(*) from t_test_dd_via_segid group by gp_segment_id;
select gp_segment_id, count(*) from t_test_dd_via_segid group by gp_segment_id;

-- test direct dispatch via gp_segment_id qual with conjunction
create table t_test_dd_via_segid_conj(a int, b int);
insert into t_test_dd_via_segid_conj select i,i from generate_series(1, 10)i;

explain (costs off) select gp_segment_id, * from t_test_dd_via_segid_conj where gp_segment_id=0 and a between 1 and 10;
select gp_segment_id, * from t_test_dd_via_segid_conj where gp_segment_id=0 and a between 1 and 10;

explain (costs off) select gp_segment_id, * from t_test_dd_via_segid_conj where b between 1 and 5 and gp_segment_id=2 and a between 1 and 10;
select gp_segment_id, * from t_test_dd_via_segid_conj where b between 1 and 5 and gp_segment_id=2 and a between 1 and 10;

--test direct dispatch via gp_segment_id with disjunction

explain (costs off) select * from t_test_dd_via_segid_conj where gp_segment_id=1 or (a=3 and gp_segment_id=2);
select * from t_test_dd_via_segid_conj where gp_segment_id=1 or (a=3 and gp_segment_id=2);

--test direct dispatch with constant distribution column and constant/variable gp_segment_id condition
explain (costs off) select gp_segment_id, * from t_test_dd_via_segid_conj where a =3 and b between 1 and 10 and gp_segment_id in (0,1);
select gp_segment_id, * from t_test_dd_via_segid_conj where a =3 and b between 1 and 10 and gp_segment_id in (0,1);

explain (costs off) select gp_segment_id, * from t_test_dd_via_segid_conj where a =3 and b between 1 and 10 and gp_segment_id <>1;
select gp_segment_id, * from t_test_dd_via_segid_conj where a =3 and b between 1 and 10 and gp_segment_id <>1;

explain (costs off) select gp_segment_id, * from t_test_dd_via_segid_conj where a =3 and b between 1 and 100 and gp_segment_id =0;
select gp_segment_id, * from t_test_dd_via_segid_conj where a =3 and b between 1 and 100 and gp_segment_id =0;

explain (costs off) select gp_segment_id, * from t_test_dd_via_segid_conj where a in (1,3) and gp_segment_id <> 0;
select gp_segment_id, * from t_test_dd_via_segid_conj where a in (1,3) and gp_segment_id <> 0;

explain (costs off) select gp_segment_id, * from t_test_dd_via_segid_conj where a in (1,3) and gp_segment_id in (0,1);
select gp_segment_id, * from t_test_dd_via_segid_conj where a in (1,3) and gp_segment_id in (0,1);

--test direct dispatch if distribution column is of varchar type
drop table if exists t1_varchar;
create table t1_varchar(col1_varchar varchar, col2_int int);
insert into t1_varchar values ('a',1);
insert into t1_varchar values ('b',2);
insert into t1_varchar values ('c',3);
insert into t1_varchar values ('d',4);
insert into t1_varchar values ('e',5);
insert into t1_varchar values ('97',6);

explain (costs off) select gp_segment_id,  * from t1_varchar where col1_varchar = 'c';
select gp_segment_id,  * from t1_varchar where col1_varchar = 'c';

explain (costs off) select gp_segment_id,  * from t1_varchar where col1_varchar <>'c';
select gp_segment_id,  * from t1_varchar where col1_varchar <>'c';

--test direct dispatch if distribution column is of varchar type and disjunction scenario
explain (costs off) select gp_segment_id, * from t1_varchar where col1_varchar in ('a','b');
select gp_segment_id, * from t1_varchar where col1_varchar in ('a','b');

explain (costs off) select gp_segment_id, * from t1_varchar where col1_varchar = 'a' or col1_varchar = 'b';
select gp_segment_id, * from t1_varchar where col1_varchar = 'a' or col1_varchar = 'b';

--test direct dispatch if distribution column is of varchar type, having disjunction condition
-- or an additional conjunction constraint using another table column or both
explain (costs off) select gp_segment_id, * from t1_varchar where col1_varchar = 'c' and col2_int=3;
select gp_segment_id, * from t1_varchar where col1_varchar = 'c' and col2_int=3;

explain (costs off) select gp_segment_id, * from t1_varchar where col1_varchar = 'a' and col2_int in (1,3);
select gp_segment_id, * from t1_varchar where col1_varchar = 'a' and col2_int in (1,3);

explain (costs off) select gp_segment_id, * from t1_varchar where col1_varchar = 'a' and col2_int not in (2,3);
select gp_segment_id, * from t1_varchar where col1_varchar = 'a' and col2_int not in (2,3);

explain (costs off) select gp_segment_id, * from t1_varchar where col1_varchar in ('a', 'b') and col2_int=2;
select gp_segment_id, * from t1_varchar where col1_varchar in ('a', 'b') and col2_int=2;

explain (costs off) select gp_segment_id, * from t1_varchar where (col1_varchar = 'a' or col1_varchar = 'b') and col2_int=1;
select gp_segment_id, * from t1_varchar where (col1_varchar = 'a' or col1_varchar = 'b') and col2_int=1;

--Test direct dispatch with explicit typecasting
explain (costs off) select gp_segment_id, * from t1_varchar where col1_varchar = 97::VARCHAR;
select gp_segment_id, * from t1_varchar where col1_varchar = 97::VARCHAR;

-- explicit cast using "char", generates a scenario of cast function from Dist Colm to datum in CTranslatorExprToDXLUtils::FDirectDispatchable(,,)
explain (costs off) select gp_segment_id,  * from t1_varchar where col1_varchar = 'c'::char;
select gp_segment_id,  * from t1_varchar where col1_varchar = 'c'::char;

explain (costs off) select gp_segment_id,  * from t1_varchar where col1_varchar = '2'::char;
select gp_segment_id,  * from t1_varchar where col1_varchar = '2'::char;

--No direct dispatch case, scenario: cast exists but not binary coercible
drop table if exists t3;
create table t3 (c1 timestamp without time zone);
insert into t3 values ('2015-07-03 00:00:00'::timestamp without time zone);

explain (costs off) select c1 from t3 where c1 = '2015-07-03'::date;
select c1 from t3 where c1 = '2015-07-03'::date;

drop table t3;
drop table t1_varchar;

--check direct dispatch working based on the distribution policy of relation
drop extension if exists citext cascade;
drop table if exists srt_dd;
CREATE EXTENSION citext;
create table srt_dd (name CITEXT);
INSERT INTO srt_dd (name)
VALUES ('abb'),
       ('ABA'),
       ('ABC'),
       ('abd');

explain (costs off) select LOWER(name) as aba FROM srt_dd WHERE name = 'ABA'::text;
select LOWER(name) as aba FROM srt_dd WHERE name = 'ABA'::text;

explain (costs off) delete from srt_dd where name='ABA'::text;
delete from srt_dd where name='ABA'::text;

drop extension if exists citext cascade;
drop table if exists srt_dd;

-- test direct dispatch via SQLValueFunction and FuncExpr for single row insertion.
create table t_sql_value_function1 (a int, b date);
create table t_sql_value_function2 (a date);

explain (costs off) insert into t_sql_value_function1 values(1, current_timestamp);
insert into t_sql_value_function1 values(1, current_timestamp);

explain (costs off) insert into t_sql_value_function2 values(current_timestamp);
insert into t_sql_value_function2 values(current_timestamp);

explain (costs off) insert into t_sql_value_function1 values(2, now());
insert into t_sql_value_function1 values(2, now());

explain (costs off) insert into t_sql_value_function2 values(now());
insert into t_sql_value_function2 values(now());

-- Convert policy of root to random when one of its children has a different distribution policy.
-- The only allowed difference between parent and children is for the parent to be hash distributed,
-- and its child part to be randomly distributed.
create table t_hash_partition
(
    r_regionkey integer not null,
    r_name char(25)
)
partition by range (r_regionkey)
(
    partition region1 start (0),
    partition region2 start (3),
    partition region3 start (5) end (8)
);

set allow_system_table_mods=true;

-- select: root & leaf are all the same hash dist, will direct dispatch
begin;
select * from t_hash_partition where r_regionkey=1;
select * from t_hash_partition_1_prt_region1 where r_regionkey=1;
abort;

-- delete: root & leaf are all the same hash dist, will direct dispatch
begin;
-- orca does not handle direct dispatch for DELETE or UPDATE now
-- also orca does not handle DELETE/UPDATE for partitioned tables now.
explain (costs off) delete from t_hash_partition where r_regionkey=1;
delete from t_hash_partition where r_regionkey=1;
abort;

begin;
-- orca does not handle direct dispatch for DELETE or UPDATE now
delete from t_hash_partition_1_prt_region1 where r_regionkey=1;
abort;

-- update: root & leaf are all the same hash dist, will direct dispatch
begin;
-- orca does not handle direct dispatch for DELETE or UPDATE now
-- also orca does not handle DELETE/UPDATE for partitioned tables now.
explain (costs off) update t_hash_partition set r_name = 'CHINA' where r_regionkey=1;
update t_hash_partition set r_name = 'CHINA' where r_regionkey=1;
abort;

begin;
-- orca does not handle direct dispatch for DELETE or UPDATE now
update t_hash_partition_1_prt_region1 set r_name = 'CHINA' where r_regionkey=1;
abort;

-- insert
begin;
-- only consider target's policy, will direct dispatch
insert into t_hash_partition values(1,'CHINA');
-- leaf is also hash dist, will direct dispatch
insert into t_hash_partition_1_prt_region1 values(1,'CHINA');
abort;

-- can not alter distributed of child partition, we change one child policy to random
update gp_distribution_policy set distkey='',distclass='' where localoid::regclass::text = 't_hash_partition_1_prt_region1';

-- select
begin;
-- root & leaf policy mismatch, will not direct dispatch
select * from t_hash_partition where r_regionkey=1;
-- the leaf is randomly dist now, will not direct dispatch
select * from t_hash_partition_1_prt_region1 where r_regionkey=1;
abort;

-- delete
begin;
-- root & leaf policy mismatch, will not direct dispatch
delete from t_hash_partition where r_regionkey=1;
abort;

begin;
-- this leaf is randomly dist, will not direct dispatch
delete from t_hash_partition_1_prt_region1 where r_regionkey=1;
abort;

-- update
begin;
-- root & leaf policy mismatch, will not direct dispatch
update t_hash_partition set r_name = 'CHINA' where r_regionkey=1;
abort;

begin;
-- this leaf is randomly dist, will not direct dispatch
update t_hash_partition_1_prt_region1 set r_name = 'CHINA' where r_regionkey=1;
abort;

-- insert
begin;
-- only consider target's policy, will direct dispatch
insert into t_hash_partition values(1,'CHINA');
abort;

begin;
-- this leaf is randomly dist, will not direct dispatch
insert into t_hash_partition_1_prt_region1 values(1,'CHINA');
abort;


-- cleanup
set test_print_direct_dispatch_info=off;
set allow_system_table_mods=off;

-- https://github.com/greenplum-db/gpdb/issues/14887
-- If opno of clause does not belong to opfamily of distributed key,
-- do not use direct dispatch to resolve wrong result
-- FIXME: orca still has wrong results
create table t_14887(a varchar);
insert into t_14887 values('a   ');
explain select * from t_14887 where a = 'a'::bpchar;
select * from t_14887 where a = 'a'::bpchar;

-- texteq does not belong to the hash opfamily of the table's citext distkey.
-- But from the implementation can deduce: texteq ==> citext_eq, and we can
-- do the direct dispatch.
-- But we do not have the kind of implication rule in Postgres: texteq ==> citext_eq.
-- Also partition table with citext as hash key and condition with text type
-- does not do partition prune.
CREATE EXTENSION if not exists citext;
drop table t_14887;
create table t_14887(a citext);
insert into t_14887 values('A'),('a');
explain select * from t_14887 where a = 'a'::text;
select * from t_14887 where a = 'a'::text;

drop table t_14887;
create table t_14887 (a citext) partition by hash (a);
create table t0_14887 partition of t_14887 for values with (modulus 3,remainder 0);
create table t1_14887 partition of t_14887 for values with (modulus 3,remainder 1);
create table t2_14887 partition of t_14887 for values with (modulus 3,remainder 2);
explain select * from t_14887 where a = 'a'::text;

begin;
drop table if exists direct_test;
drop table if exists direct_test_two_column;
drop table if exists direct_test_bitmap;
drop table if exists direct_test_partition;
drop table if exists direct_test_range_partition;
drop table if exists direct_dispatch_foo;
drop table if exists direct_dispatch_bar;

drop table if exists MPP_22019_a;
drop table if exists MPP_22019_b;

drop table if exists t_sql_value_function1;
drop table if exists t_sql_value_function2;

drop table if exists t_hash_partition;
drop table if exists t_14887;
drop extension if exists citext cascade;

commit;
