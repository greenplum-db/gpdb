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

create unique index direct_test_uk on direct_test_partition(trans_id);

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
create sequence ddtestseq;

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

-- cleanup
set test_print_direct_dispatch_info=off;

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
commit;
