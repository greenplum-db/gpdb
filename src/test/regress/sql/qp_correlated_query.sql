-- ----------------------------------------------------------------------
-- Test: setup.sql
-- ----------------------------------------------------------------------
create schema qp_correlated_query;
set search_path to qp_correlated_query;
set optimizer_trace_fallback TO on;

-- ----------------------------------------------------------------------
-- Test: csq_heap_in.sql (Correlated Subquery: CSQ using IN clause (Heap))
-- ----------------------------------------------------------------------
begin;
create table qp_csq_t1(a int, b int) distributed by (a);
insert into qp_csq_t1 values (1,2);
insert into qp_csq_t1 values (3,4);
insert into qp_csq_t1 values (5,6);
insert into qp_csq_t1 values (7,8);

analyze qp_csq_t1;

create table qp_csq_t2(x int,y int) distributed by (x);
insert into qp_csq_t2 values(1,1);
insert into qp_csq_t2 values(3,9);
insert into qp_csq_t2 values(5,25);
insert into qp_csq_t2 values(7,49);

analyze qp_csq_t2;

create table qp_csq_t3(c int, d text) distributed by (c);
insert into qp_csq_t3 values(1,'one');
insert into qp_csq_t3 values(3,'three');
insert into qp_csq_t3 values(5,'five');
insert into qp_csq_t3 values(7,'seven');

analyze qp_csq_t3;

create table A(i integer, j integer) distributed by (i);
insert into A values(1,1);
insert into A values(19,5);
insert into A values(99,62);
insert into A values(1,1);
insert into A values(78,-1);

analyze A;

create table B(i integer, j integer) distributed by (i);
insert into B values(1,43);
insert into B values(88,1);
insert into B values(-1,62);
insert into B values(1,1);
insert into B values(32,5);
insert into B values(2,7);

analyze B;

create table C(i integer, j integer) distributed by (i);
insert into C values(1,889);
insert into C values(288,1);
insert into C values(-1,625);
insert into C values(32,65);
insert into C values(32,62);
insert into C values(3,-1);
insert into C values(99,7);
insert into C values(78,62);
insert into C values(2,7);

analyze C;

create table D(i integer, j integer) distributed by (j);
insert into D values(1,1);
insert into D values(19,5);
insert into D values(99,62);
insert into D values(1,1);
insert into D values(78,-1);

analyze D;

create table E(i integer, j integer) distributed by (i);
insert into E values(1,889);
insert into E values(288,1);
insert into E values(-1,625);
insert into E values(32,65);
insert into E values(32,62);
insert into E values(3,-1);
insert into E values(99,7);
insert into E values(78,62);

analyze E;

commit;

-- -- -- --
-- Basic queries with IN clause
-- -- -- --
select a, x from qp_csq_t1, qp_csq_t2 where qp_csq_t1.a in (select x);
select A.i from A where A.i in (select B.i from B where A.i = B.i) order by A.i;
select * from B where exists (select * from C,A where C.j = A.j and B.i in (select C.i from C where C.i = A.i and C.i != 10)) order by 1, 2;

select * from B where not exists (select * from C,A where C.j = A.j and B.i in (select C.i from C where C.i = A.i and C.i != 10)) order by 1,2;
select * from A where not exists (select * from C,B where C.j = A.j and B.i in (select C.i from C where C.i = B.i and C.i != 10));

select A.i, B.i, C.j from A, B, C where A.j = (select C.j from C where C.j = A.j and C.i in (select B.i from B where C.i = B.i and B.i !=10)) order by A.i, B.i, C.j limit 10; 
select A.i, B.i, C.j from A, B, C where A.j in (select C.j from C where C.j = A.j and C.i in (select B.i from B where C.i = B.i and B.i !=10)) order by A.i, B.i, C.j limit 10; 
select A.i, B.i, C.j from A, B, C where A.j = any(select sum(C.j) from C where C.j = A.j and C.i in (select B.i from B where C.i = B.i and B.i !=10)) order by A.i, B.i, C.j limit 10;
select A.i, B.i, C.j from A, B, C where A.j in ( select C.j from C where exists(select C.i from C,A where C.i = A.i and C.i =10)) order by A.i, B.i, C.j limit 10;
select A.i, B.i, C.j from A, B, C where A.j in (select C.j from C where C.j = A.j and not exists (select sum(B.i) from B where C.i = B.i and C.i !=10)) order by A.i, B.i, C.j limit 10;


-- Test for sublink pull-up based on both left-hand and right-hand input
explain (costs off)
select * from A where exists (select * from B where A.i in (select C.i from C where C.i = B.i));
select * from A where exists (select * from B where A.i in (select C.i from C where C.i = B.i));


-- Test for ALL_SUBLINK pull-up based on both left-hand and right-hand input
explain (costs off)
select * from A,B where exists (select * from C where B.i not in (select C.i from C where C.i != 10));
select * from A,B where exists (select * from C where B.i not in (select C.i from C where C.i != 10));

-- -- -- --
-- Basic queries with NOT IN clause
-- -- -- --
select a, x from qp_csq_t1, qp_csq_t2 where qp_csq_t1.a not in (select x) order by a,x;
select A.i from A where A.i not in (select B.i from B where A.i = B.i) order by A.i;
select * from A where exists (select * from B,C where C.j = A.j and B.i not in (select sum(C.i) from C where C.i = B.i and C.i != 10)) order by 1,2;
select * from A,B where exists (select * from E where E.j = A.j and B.i not in (select E.i from E where E.i != 10)) order by 1,2,3,4;

select * from B where not exists (select * from A,C where C.j = A.j and B.i in (select max(C.i) from C where C.i = A.i and C.i != 10)) order by 1, 2;
select * from B where not exists (select * from A,C where C.j = A.j and B.i not in (select max(C.i) from C where C.i = A.i and C.i != 10)) order by 1, 2;
select * from A where not exists (select * from B,C where C.j = A.j and B.i not in (select max(C.i) from C where C.i = B.i and C.i != 10)) order by 1, 2;

select A.i, B.i, C.j from A, B, C where A.j not in (select C.j from C where C.j = A.j and C.i not in (select B.i from B where C.i = B.i and B.i !=10)) order by A.i, B.i, C.j limit 10; 
select A.i, B.i, C.j from A, B, C where A.j = any(select sum(C.j) from C where C.j = A.j and C.i not in (select B.i from B where C.i = B.i and B.i !=10)) order by A.i, B.i, C.j limit 10;
select A.i, B.i, C.j from A, B, C where A.j not in ( select C.j from C where exists(select C.i from C,A where C.i = A.i and C.i =10)) order by A.i, B.i, C.j limit 10;
select A.i, B.i, C.j from A, B, C where A.j not in (select C.j from C where C.j = A.j and not exists (select sum(B.i) from B where C.i = B.i and C.i !=10)) order by A.i, B.i, C.j limit 10;
select A.j from A, B, C where A.j = (select C.j from C where C.j = A.j and C.i in (select B.i from B where C.i = B.i and B.i !=10)) order by A.j limit 10;

-- MPP-14222
explain select A.i, B.i, C.j from A, B, C where A.j = (select C.j from C where C.j = A.j and C.i not in (select B.i from B where C.i = B.i and B.i !=10)) order by A.i, B.i, C.j limit 10;
select A.i, B.i, C.j from A, B, C where A.j = (select C.j from C where C.j = A.j and C.i not in (select B.i from B where C.i = B.i and B.i !=10)) order by A.i, B.i, C.j limit 10;
explain select A.j from A, B, C where A.j = (select C.j from C where C.j = A.j and C.i not in (select B.i from B where C.i = B.i and B.i !=10)) order by A.j limit 10;
select A.j from A, B, C where A.j = (select C.j from C where C.j = A.j and C.i not in (select B.i from B where C.i = B.i and B.i !=10)) order by A.j limit 10;

explain select A.i from A where A.j = (select C.j from C where C.j = A.j and C.i = any (select B.i from B where C.i = B.i and B.i !=10));
select A.i from A where A.j = (select C.j from C where C.j = A.j and C.i = any (select B.i from B where C.i = B.i and B.i !=10));


-- ----------------------------------------------------------------------
-- Test: csq_heap_any.sql - Correlated Subquery: CSQ using ANY clause (Heap)
-- ----------------------------------------------------------------------


-- -- -- --
-- Basic queries with ANY clause
-- -- -- --
select a, x from qp_csq_t1, qp_csq_t2 where qp_csq_t1.a = any (select x);
select a, x from qp_csq_t1, qp_csq_t2 where qp_csq_t1.a = any (select x) order by a, x;
select A.i from A where A.i = any (select B.i from B where A.i = B.i) order by A.i;

select * from A where A.j = any (select C.j from C where C.j = A.j) order by 1,2;
select * from A,B where A.j = any (select C.j from C where C.j = A.j and B.i = any (select C.i from C)) order by 1,2,3,4;
select * from A where A.j = any (select C.j from C,B where C.j = A.j and B.i = any (select C.i from C)) order by 1,2;
select * from A where A.j = any (select C.j from C,B where C.j = A.j and B.i = any (select C.i from C where C.i != 10 and C.i = B.i)) order by 1,2;
-- Planner should fail due to skip-level correlation not supported. ORCA should pass
select * from A,B where A.j = any (select C.j from C where C.j = A.j and B.i = any (select C.i from C where C.i != 10 and C.i = A.i)) order by 1,2,3,4;

explain select A.i, B.i, C.j from A, B, C where A.j = (select C.j from C where C.j = A.j and C.i = any (select B.i from B where C.i = B.i and B.i !=10)) order by A.i, B.i, C.j limit 10;
select A.i, B.i, C.j from A, B, C where A.j = (select C.j from C where C.j = A.j and C.i = any (select B.i from B where C.i = B.i and B.i !=10)) order by A.i, B.i, C.j limit 10;
explain select A.i, B.i, C.j from A, B, C where A.j = any ( select C.j from C where not exists(select C.i from C,A where C.i = A.i and C.i =10)) order by A.i, B.i, C.j limit 10;
select A.i, B.i, C.j from A, B, C where A.j = any ( select C.j from C where not exists(select C.i from C,A where C.i = A.i and C.i =10)) order by A.i, B.i, C.j limit 10;
explain select A.i, B.i, C.j from A, B, C where A.j = any (select C.j from C where C.j = A.j and not exists (select sum(B.i) from B where C.i = B.i and C.i !=10)) order by A.i, B.i, C.j limit 10;
select A.i, B.i, C.j from A, B, C where A.j = any (select C.j from C where C.j = A.j and not exists (select sum(B.i) from B where C.i = B.i and C.i !=10)) order by A.i, B.i, C.j limit 10;


-- ----------------------------------------------------------------------
-- Test: Correlated Subquery: CSQ using ALL clause (Heap)
-- ----------------------------------------------------------------------


-- -- -- --
-- Basic queries with ALL clause
-- -- -- --
select a, x from qp_csq_t1, qp_csq_t2 where qp_csq_t1.a = all (select x) order by a;
select A.i from A where A.i = all (select B.i from B where A.i = B.i) order by A.i;

select * from A,B where exists (select * from C where C.j = A.j and B.i = all (select min(C.j) from C)) order by 1,2,3,4;
select * from A,B where exists (select * from C where C.j = A.j and B.i = all (select min(C.j) from C where C.j = 1)) order by 1,2,3,4;
select * from A,B where exists (select * from C where C.j = A.j and B.i = all (select min(C.j) from C where C.j = B.j)) order by 1,2,3,4;
explain select A.i, B.i, C.j from A, B, C where A.j = (select sum(C.j) from C where C.j = A.j and C.i = all (select B.i from B where C.i = B.i and B.i !=10)) order by A.i, B.i, C.j limit 10;
select A.i, B.i, C.j from A, B, C where A.j = (select sum(C.j) from C where C.j = A.j and C.i = all (select B.i from B where C.i = B.i and B.i !=10)) order by A.i, B.i, C.j limit 10;
explain select A.i, B.i, C.j from A, B, C where A.j < all ( select C.j from C where not exists(select C.i from C,A where C.i = A.i and C.i =10)) order by A.i, B.i, C.j limit 10;
select A.i, B.i, C.j from A, B, C where A.j < all ( select C.j from C where not exists(select C.i from C,A where C.i = A.i and C.i =10)) order by A.i, B.i, C.j limit 10;
explain select A.i, B.i, C.j from A, B, C where A.j = all (select C.j from C where C.j = A.j and not exists (select sum(B.i) from B where C.i = B.i and C.i !=10)) order by A.i, B.i, C.j limit 10;
select A.i, B.i, C.j from A, B, C where A.j = all (select C.j from C where C.j = A.j and not exists (select sum(B.i) from B where C.i = B.i and C.i !=10)) order by A.i, B.i, C.j limit 10;

-- -- -- --
-- Test ALL clause with subqueries
-- -- -- --
create table qp_csq_all_t1(a int) distributed by (a);
create table qp_csq_all_t2(b int) distributed by (b);

insert into qp_csq_all_t1 values (null);
select * from qp_csq_all_t1 where (select a from qp_csq_all_t1 limit 1) = all(select b from qp_csq_all_t2);

truncate qp_csq_all_t1, qp_csq_all_t2;
insert into qp_csq_all_t2 values (null);
select * from qp_csq_all_t1 where (select a from qp_csq_all_t1 limit 1) = all(select b from qp_csq_all_t2);

truncate qp_csq_all_t1, qp_csq_all_t2;
insert into qp_csq_all_t1 values (1);
select * from qp_csq_all_t1 where (select a from qp_csq_all_t1 limit 1) = all(select b from qp_csq_all_t2);

truncate qp_csq_all_t1, qp_csq_all_t2;
insert into qp_csq_all_t2 values (1);
select * from qp_csq_all_t1 where (select a from qp_csq_all_t1 limit 1) = all(select b from qp_csq_all_t2);

truncate qp_csq_all_t1, qp_csq_all_t2;
insert into qp_csq_all_t1 values (1);
insert into qp_csq_all_t2 values (1);
select * from qp_csq_all_t1 where (select a from qp_csq_all_t1 limit 1) = all(select b from qp_csq_all_t2);

truncate qp_csq_all_t1, qp_csq_all_t2;
insert into qp_csq_all_t1 values (1);
insert into qp_csq_all_t2 values (2);
select * from qp_csq_all_t1 where (select a from qp_csq_all_t1 limit 1) = all(select b from qp_csq_all_t2);


-- ----------------------------------------------------------------------
-- Test: Correlated Subquery: CSQ using EXISTS clause (Heap)
-- ----------------------------------------------------------------------


-- -- -- -- 
-- Basic queries with EXISTS clause
-- -- -- --
select b from qp_csq_t1 where exists(select * from qp_csq_t2 where y=a);
select b from qp_csq_t1 where exists(select * from qp_csq_t2 where y=a) order by b;
select A.i from A where exists(select B.i from B where A.i = B.i) order by A.i;

-- with CTE 
with t as (select 1) select b from qp_csq_t1 where exists(select * from qp_csq_t2 where y=a);
with t as (select * from qp_csq_t2) select b from qp_csq_t1 where exists(select * from t where y=a);

select * from A where exists (select * from C where C.j = A.j) order by 1,2;
select * from A where exists (select * from C,B where C.j = A.j and exists (select * from C where C.i = B.i)) order by 1,2;
select * from A,B where exists (select * from C where C.j = A.j and exists (select * from C where C.i = B.i));

select * from A where exists (select * from B, C where C.j = A.j and exists (select sum(C.i) from C where C.i != 10 and C.i = B.i)) order by 1, 2;
select * from A where exists (select * from C where C.j = A.j and exists (select sum(C.i) from C where C.i !=10 and C.i = A.i)) order by 1, 2;

select A.i, B.i, C.j from A, B, C where A.j = (select C.j from C where C.j = A.j and exists (select B.i from B where C.i = B.i and B.i !=10)) order by A.i, B.i, C.j limit 20;
select A.i, B.i, C.j from A, B, C where exists (select C.j from C where C.j = A.j and exists (select sum(B.i) from B where C.i = B.i and C.i !=10)) order by A.i, B.i, C.j limit 20;

select * from A where exists (select * from C where C.j = A.j and not exists (select sum(B.i) from B where B.i = C.i));

select * from A where exists (select * from C where C.i = A.i and exists (select * from B where C.j = B.j and B.j < 10)) order by 1,2;
select * from A where exists (select * from C where C.i = A.i and exists (select * from B where C.j = B.j and A.j < 10));
select * from A where exists (select * from C where C.i = A.i and not exists (select * from B where C.j = B.j and B.j < 10)) order by 1,2;

select * from A,B,C where C.i = A.i and exists (select C.j where C.j = B.j and A.j < 10);
-- -- -- --
-- Basic queries with NOT EXISTS clause
-- -- -- --
select b from qp_csq_t1 where not exists(select * from qp_csq_t2 where y=a);
select b from qp_csq_t1 where not exists(select * from qp_csq_t2 where y=a) order by b;
select A.i from A where not exists(select B.i from B where A.i = B.i) order by A.i;

select * from A where not exists (select * from C,B where C.j = A.j and exists (select * from C where C.i = B.i and C.j < B.j)) order by 1,2;
select * from A where exists (select * from C,B where C.j = A.j and not exists (select * from C where C.i = B.i and C.j < B.j)) order by 1,2;
select * from A where exists (select * from C,B where C.j = A.j and exists (select * from C where C.i = B.i and C.j < B.j)) order by 1,2;

select A.i, B.i, C.j from A, B, C where A.j = (select C.j from C where C.j = A.j and not exists (select B.i from B where C.i = B.i and B.i !=10)) order by A.i, B.i, C.j limit 10;
select A.i, B.i, C.j from A, B, C where A.j = (select C.j from C where C.j = A.j and not exists (select sum(B.i) from B where C.i = B.i and C.i !=10)) order by A.i, B.i, C.j limit 10;
select * from A where not exists (select sum(C.i) from C where C.i = A.i);
explain select * from A where not exists (select sum(C.i) from C where C.i = A.i limit 0);
select * from A where not exists (select sum(C.i) from C where C.i = A.i limit 0);
explain select * from A where not exists (select sum(C.i) from C where C.i = A.i limit 5 offset 3);
select * from A where not exists (select sum(C.i) from C where C.i = A.i limit 5 offset 3);
explain select * from A where not exists (select sum(C.i) from C where C.i = A.i limit 1 offset 0);
select * from A where not exists (select sum(C.i) from C where C.i = A.i limit 1 offset 0);
explain select C.j from C where not exists (select max(B.i) from B  where C.i = B.i having max(B.i) is not null) order by C.j;
select C.j from C where not exists (select max(B.i) from B  where C.i = B.i having max(B.i) is not null) order by C.j;
explain select C.j from C where not exists (select max(B.i) from B  where C.i = B.i offset 1000) order by C.j;
select C.j from C where not exists (select max(B.i) from B  where C.i = B.i offset 1000) order by C.j;
explain select C.j from C where not exists (select rank() over (order by B.i) from B  where C.i = B.i) order by C.j;
select C.j from C where not exists (select rank() over (order by B.i) from B  where C.i = B.i) order by C.j;
explain select * from A where not exists (select sum(C.i) from C where C.i = A.i group by a.i);
select * from A where not exists (select sum(C.i) from C where C.i = A.i group by a.i);
explain select A.i from A where not exists (select B.i from B where B.i in (select C.i from C) and B.i = A.i);
select A.i from A where not exists (select B.i from B where B.i in (select C.i from C) and B.i = A.i);
explain select * from B where not exists (select * from C,A where C.i in (select C.i from C where C.i = A.i and C.i != 10) AND B.i = C.i);
select * from B where not exists (select * from C,A where C.i in (select C.i from C where C.i = A.i and C.i != 10) AND B.i = C.i);
explain select * from A where A.i in (select C.j from C,B where B.i in (select i from C));
select * from A where A.i in (select C.j from C,B where B.i in (select i from C));
explain select * from A where not exists (select sum(c.i) from C where C.i = A.i group by C.i having c.i > 3);
select * from A where not exists (select sum(c.i) from C where C.i = A.i group by C.i having c.i > 3);


-- ----------------------------------------------------------------------
-- Test:  Correlated Subquery: CSQ using DML (Heap) 
-- ----------------------------------------------------------------------
begin;
create table qp_csq_t4(a int, b int) distributed by (b);
insert into qp_csq_t4 values (1,2);
insert into qp_csq_t4 values (3,4);
insert into qp_csq_t4 values (5,6);
insert into qp_csq_t4 values (7,8);
analyze qp_csq_t4;
commit;

-- -- -- --
-- Basic CSQ with UPDATE statements
-- -- -- --
select * from qp_csq_t4 order by a;
update qp_csq_t4 set a = (select y from qp_csq_t2 where x=a) where b < 8;
select * from qp_csq_t4 order by a;
update qp_csq_t4 set a = 9999 where qp_csq_t4.a = (select max(x) from qp_csq_t2);
select * from qp_csq_t4 order by a;
update qp_csq_t4 set a = (select max(y) from qp_csq_t2 where x=a) where qp_csq_t4.a = (select min(x) from qp_csq_t2);
select * from qp_csq_t4 order by a;
update qp_csq_t4 set a = 8888 where (select (y*2)>b from qp_csq_t2 where a=x);
select * from qp_csq_t4 order by a;
update qp_csq_t4 set a = 3333 where qp_csq_t4.a in (select x from qp_csq_t2);
select * from qp_csq_t4 order by a;

update D set i = 11111 from C where C.i = D.i and exists (select C.j from C,B where C.j = B.j and D.j < 10);
select * from D;
update D set i = 22222 from C where C.i = D.i and not exists (select C.j from C,B where C.j = B.j and D.j < 10);
select * from D;

-- -- -- --
-- Basic CSQ with DELETE statements
-- -- -- --
select * from qp_csq_t4 order by a;
delete from qp_csq_t4 where a <= (select min(y) from qp_csq_t2 where x=a);
select * from qp_csq_t4 order by a;
delete from qp_csq_t4 where qp_csq_t4.a = (select min(x) from qp_csq_t2);
select * from qp_csq_t4 order by a;
delete from qp_csq_t4 where exists (select (y*2)>b from qp_csq_t2 where a=x);
select * from qp_csq_t4 order by a;
delete from qp_csq_t4  where qp_csq_t4.a = (select x from qp_csq_t2 where a=x);
select * from qp_csq_t4 order by a;

delete from  D TableD where exists (select C.j from C, B where C.j = B.j and TableD.j < 10);
select * from D order by D.i;
delete from D TableD where not exists (select C.j from C,B where C.j = B.j and TableD.j < 10);
select * from D order by D.i;



-- ----------------------------------------------------------------------
-- Test: Correlated Subquery: CSQ using WHERE clause (Heap)
-- ----------------------------------------------------------------------


-- -- -- --
-- Basic queries with WHERE clause
-- -- -- --
select a, (select y from qp_csq_t2 where x=a) from qp_csq_t1 where b < 8 order by a;
select a, x from qp_csq_t2, qp_csq_t1 where qp_csq_t1.a = (select x) order by a;
select a from qp_csq_t1 where (select (y*2)>b from qp_csq_t2 where a=x) order by a;
SELECT a, (SELECT d FROM qp_csq_t3 WHERE a=c) FROM qp_csq_t1 GROUP BY a order by a;

-- Planner should fail due to skip-level correlation not supported. ORCA should pass
SELECT a, (SELECT (SELECT d FROM qp_csq_t3 WHERE a=c)) FROM qp_csq_t1 GROUP BY a order by a;

-- ----------------------------------------------------------------------
-- Test: Correlated Subquery: CSQ in select list (Heap) 
-- ----------------------------------------------------------------------


-- -- -- --
-- Basic queries in SELECT list
-- -- -- --
select A.i, (select C.j from C group by C.j having max(C.j) = any (select min(B.j) from B)) as C_j from A,B,C where A.i = 99 order by A.i, C_j limit 10;
select (select avg(x) from qp_csq_t1, qp_csq_t2 where qp_csq_t1.a = any (select x)) as avg_x from qp_csq_t1 order by 1;

-- Planner should fail due to skip-level correlation not supported. Query should not cause segfault like in issue #12054.
select A.j, (select array_agg(a_B) from (select B.j, (select array_agg(a_C) from (select C.j from C where C.i = A.i) a_C) from B where B.i = A.i order by A.j) a_B) from A;
-- Planner should fail due to skip-level correlation not supported. Query should not return wrong results like in issue #12054.
select A.j, (select array_agg(a_B) from (select B.j, (select sum(a_C.j) from (select C.j from C where C.i = A.i) a_C) from B where B.i = A.i order by A.j) a_B) from A;

-- ----------------------------------------------------------------------
-- Test: Correlated Subquery: CSQ with multiple columns (Heap)
-- ----------------------------------------------------------------------

select A.i, B.i from A, B where (A.i,A.j) = (select min(B.i),min(B.j) from B where B.i = A.i) order by A.i, B.i;
select A.i, B.i from A, B where (A.i,A.j) = all(select B.i,B.j from B where B.i = A.i) order by A.i, B.i;
select A.i, B.i from A, B where not exists (select B.i,B.j from B where B.i = A.i) order by A.i, B.i;
select A.i, B.i from A, B where (A.i,A.j) in (select B.i,B.j from B where B.i = A.i) order by A.i, B.i;

select A.i, B.i,C.i from A, B, C where (A.i,B.i) = any (select A.i, B.i from A,B where A.i = C.i and B.i = C.i) order by A.i, B.i, C.i;
select A.i, B.i,C.i from A, B, C where not exists (select A.i, B.i from A,B where A.i = C.i and B.i = C.i) order by A.i, B.i, C.i;
select A.i, B.i,C.i from A, B, C where (A.i,B.i) in (select A.i, B.i from A,B where A.i = C.i and B.i = C.i) order by A.i, B.i, C.i;

select * from A,B,C where (A.i,B.i) = any (select A.i, B.i from A,B where A.i < C.i and B.i = C.i and C.i not in (select A.i from A where A.j = 1 and A.j = B.j)) order by 1,2,3,4,5,6;

select A.i as A_i, B.i as B_i,C.i as C_i from A, B, C where (A.i,B.i) = (select min(A.i), min(B.i) from A,B where A.i = C.i and B.i = C.i) order by A_i, B_i, C_i;

-- ----------------------------------------------------------------------
-- Test: Correlated Subquery: CSQ using HAVING clause (Heap) 
-- ----------------------------------------------------------------------

-- -- -- --
-- Basic queries with HAVING clause
-- -- -- -- 
select A.i from A group by A.i having min(A.i) not in (select B.i from B where A.i = B.i) order by A.i;
select A.i, B.i, C.j from A, B, C group by A.j,A.i,B.i,C.j having max(A.j) = any(select max(C.j) from C where C.j = A.j) order by A.i, B.i, C.j limit 10; 
select A.i, B.i, C.j from A, B, C where exists (select C.j from C group by C.j having max(C.j) = all (select min(B.j) from B)) order by A.i, B.i, C.j limit 10;

begin;
create table csq_emp(name text, department text, salary numeric);
insert into csq_emp values('a','adept',11200.00);
insert into csq_emp values('b','adept',22222.00);
insert into csq_emp values('c','bdept',99222.00);
analyze csq_emp;
commit;

SELECT name, department, salary FROM csq_emp ea group by name, department,salary
  HAVING avg(salary) >
    (SELECT MAX(salary) FROM csq_emp eb WHERE eb.department = ea.department);


-- ----------------------------------------------------------------------
-- Test: Correlated Subquery: CSQ with multi-row subqueries (Heap)
-- ----------------------------------------------------------------------

-- Multi-row queries (See http://www.java2s.com/Tutorial/Oracle/0040__Query-Select/0680__Multiple-Row-Subquery.htm)
-- Using IN clause with multi-row subqueries
begin;
create table Employee(
      ID                 VARCHAR(4)         NOT NULL,
      First_Name         VARCHAR(10),
      Last_Name          VARCHAR(10),
      Start_Date         DATE,
      End_Date           DATE,
      Salary             Decimal(8,2),
      City               VARCHAR(10),
      Description        VARCHAR(15)
   ) distributed by(ID);
insert into Employee(ID, First_Name, Last_Name, Start_Date, End_Date, Salary, City, Description) 
    values ('01','Jason', 'Martin', to_date('19960725','YYYYMMDD'), to_date('20060725','YYYYMMDD'), 1234.56, 'Toronto',  'Programmer');
insert into Employee(ID,  First_Name, Last_Name, Start_Date, End_Date, Salary,  City, Description)
    values ('02','Alison',   'Mathews', to_date('19760321','YYYYMMDD'), to_date('19860221','YYYYMMDD'), 6661.78, 'Vancouver','Tester');
insert into Employee(ID, First_Name, Last_Name, Start_Date, End_Date, Salary, City, Description)
    values('03','James',    'Smith',   to_date('19781212','YYYYMMDD'), to_date('19900315','YYYYMMDD'), 6544.78, 'Vancouver','Tester');
insert into Employee(ID, First_Name, Last_Name, Start_Date, End_Date, Salary, City, Description)
    values('04','Celia',    'Rice',    to_date('19821024','YYYYMMDD'), to_date('19990421','YYYYMMDD'), 2344.78, 'Vancouver','Manager');
insert into Employee(ID, First_Name, Last_Name, Start_Date, End_Date, Salary, City, Description)
    values('05','Robert',   'Black',   to_date('19840115','YYYYMMDD'), to_date('19980808','YYYYMMDD'), 2334.78, 'Vancouver','Tester');
insert into Employee(ID, First_Name, Last_Name, Start_Date, End_Date, Salary, City, Description)
    values('06','Linda',    'Green',   to_date('19870730','YYYYMMDD'), to_date('19960104','YYYYMMDD'), 4322.78,'New York',  'Tester');
insert into Employee(ID, First_Name, Last_Name, Start_Date, End_Date, Salary, City, Description)
    values('07','David',    'Larry',   to_date('19901231','YYYYMMDD'), to_date('19980212','YYYYMMDD'), 7897.78,'New York',  'Manager');
insert into Employee(ID, First_Name, Last_Name, Start_Date, End_Date, Salary, City, Description)
    values('08','James',    'Cat',     to_date('19960917','YYYYMMDD'), to_date('20020415','YYYYMMDD'), 1232.78,'Vancouver', 'Tester');
analyze Employee;

create table job (
      EMPNO         VARCHAR(4),
      jobtitle      VARCHAR(20)
    );
insert into job (EMPNO, Jobtitle) values ('01','Tester');
insert into job (EMPNO, Jobtitle) values ('02','Accountant');
insert into job (EMPNO, Jobtitle) values ('03','Developer');
insert into job (EMPNO, Jobtitle) values ('04','COder');
insert into job (EMPNO, Jobtitle) values ('05','Director');
insert into job (EMPNO, Jobtitle) values ('06','Mediator');
insert into job (EMPNO, Jobtitle) values ('07','Proffessor');
insert into job (EMPNO, Jobtitle) values ('08','Programmer');
insert into job (EMPNO, Jobtitle) values ('09','Developer');
analyze job;

commit;

SELECT id, first_name FROM employee WHERE id IN 
    (SELECT id FROM employee WHERE first_name LIKE '%e%') order by id;

-- Multiple Column Subqueries
SELECT id, first_name, salary from employee
    where (id, salary) IN
        (SELECT id, MIN(salary) FROM employee GROUP BY id) order by id;

-- Uses NOT IN to check if an id is not in the list of id values in the employee table
SELECT id, first_name, last_name
  FROM employee
  WHERE id NOT IN (SELECT empno FROM job);

-- Using UPDATE  (Update products that aren't selling)
begin;
CREATE TABLE product_order (
         product_name  VARCHAR(25),
         salesperson   VARCHAR(3),
         order_date    DATE,
         quantity      decimal(4,2)
    );
INSERT INTO product_order VALUES ('Product 1', 'CA', '14-JUL-03', 1);
INSERT INTO product_order VALUES ('Product 2', 'BB', '14-JUL-03', 75);
INSERT INTO product_order VALUES ('Product 3', 'GA', '14-JUL-03', 2);
INSERT INTO product_order VALUES ('Product 4', 'GA', '15-JUL-03', 8);
INSERT INTO product_order VALUES ('Product 4', 'GA', '15-JUL-03', 8);
INSERT INTO product_order VALUES ('Product 6', 'CA', '16-JUL-03', 5);
INSERT INTO product_order VALUES ('Product 7', 'CA', '17-JUL-03', 1);
analyze product_order;

CREATE TABLE product (
         product_name     VARCHAR(25) PRIMARY KEY,
         product_price    decimal(4,2),
         quantity_on_hand decimal(5,0),
         last_stock_date  DATE
    ) distributed by (product_name);
INSERT INTO product VALUES ('Product 1', 99,  1,    '15-JAN-03');
INSERT INTO product VALUES ('Product 2', 75,  1000, '15-JAN-02');
INSERT INTO product VALUES ('Product 3', 50,  100,  '15-JAN-03');
INSERT INTO product VALUES ('Product 4', 25,  10000, null);
INSERT INTO product VALUES ('Product 5', 9.95,1234, '15-JAN-04');
INSERT INTO product VALUES ('Product 6', 45,  1,    TO_DATE('December 31, 2008, 11:30 P.M.','Month dd, YYYY, HH:MI P.M.'));
analyze product;
commit;

UPDATE product SET product_price = product_price * .9 
    where product_name NOT IN (SELECT DISTINCT product_name FROM product_order);

SELECT * FROM  product order by product_name;

-- Show products that aren't selling
SELECT * FROM product
	 WHERE  product_name NOT IN (SELECT DISTINCT product_name FROM product_order)
	 ORDER BY product_name;

-- ----------------------------------------------------------------------
-- Test: Misc Queries
-- ----------------------------------------------------------------------
create table with_test1 (i int, t text, value int);
insert into with_test1 select i%10, 'text' || i%20, i%30 from generate_series(0, 99) i;
analyze with_test1;

create table with_test2 (i int, t text, value int);
insert into with_test2 select i%100, 'text' || i%200, i%300 from generate_series(0, 999) i;
insert into with_test2
select i, i || '', total
from (select i, sum(value) as total from with_test1 group by i) as tmp;

analyze with_test2;

select with_test2.* from with_test2
where value < any (select sum(value) from with_test1 group by i having i = with_test2.i) order by i, t, value;

select with_test2.* from with_test2
where value < all (select sum(value) from with_test1 group by i having i = with_test2.i) order by i, t, value;

drop table if exists csq_emp;
create table csq_emp(name text, department text, salary numeric) distributed by (name);
insert into csq_emp values('a','adept',11200.00);
insert into csq_emp values('b','adept',22222.00);
insert into csq_emp values('c','bdept',99222.00);
insert into csq_emp values('d','adept',23211.00);
insert into csq_emp values('e','adept',45222.00);
insert into csq_emp values('f','adept',992222.00);
insert into csq_emp values('g','adept',90343.00);
insert into csq_emp values('h','adept',11200.00);
insert into csq_emp values('i','bdept',11200.00);
insert into csq_emp values('j','adept',11200.00);
analyze csq_emp;

SELECT name, department, salary FROM csq_emp ea
  WHERE salary IN
    (SELECT MAX(salary) FROM csq_emp eb WHERE eb.department = ea.department) order by name, department;

SELECT name, department, salary FROM csq_emp ea
  WHERE  salary = ANY
    (SELECT MAX(salary) FROM csq_emp eb WHERE eb.department = ea.department) order by name, department;

SELECT name, department, salary FROM csq_emp ea
  WHERE salary = 
    (SELECT MAX(salary) FROM csq_emp eb WHERE eb.department = ea.department) order by name, department, salary;

SELECT name, department, salary FROM csq_emp ea 
  WHERE salary > 
    (SELECT MAX(salary) FROM csq_emp eb WHERE eb.department = ea.department) order by name, department, salary;

SELECT name, department, salary FROM csq_emp ea 
  WHERE salary < 
    (SELECT MAX(salary) FROM csq_emp eb WHERE eb.department = ea.department) order by name, department, salary;

SELECT name, department, salary FROM csq_emp ea 
  WHERE salary IN 
    (SELECT MAX(salary) FROM csq_emp eb WHERE eb.department = ea.department) order by name, department, salary;

SELECT name, department, salary FROM csq_emp ea 
  WHERE salary NOT IN 
    (SELECT MAX(salary) FROM csq_emp eb WHERE eb.department = ea.department) order by name, department, salary;

SELECT name, department, salary FROM csq_emp ea 
  WHERE  salary = ANY 
    (SELECT MAX(salary) FROM csq_emp eb WHERE eb.department = ea.department) order by name, department, salary;

SELECT name, department, salary FROM csq_emp ea 
  WHERE salary = ALL 
    (SELECT MAX(salary) FROM csq_emp eb WHERE eb.department = ea.department) order by name, department, salary;

SELECT name, department, salary FROM csq_emp ea group by name, department,salary
  HAVING avg(salary) >
    (SELECT MAX(salary) FROM csq_emp eb WHERE eb.department = ea.department) order by name, department, salary;

SELECT name, department, salary FROM csq_emp ea group by name, department,salary
  HAVING avg(salary) > ALL
    (SELECT salary FROM csq_emp eb WHERE eb.department = ea.department) order by name, department, salary;


CREATE OR REPLACE FUNCTION f(a int) RETURNS int AS $$ select $1 $$ LANGUAGE SQL;
CREATE TABLE t1(a int) distributed by (a);
INSERT INTO t1 VALUES (1);
analyze t1;

SELECT * FROM t1 WHERE a IN (SELECT * FROM f(t1.a));

SELECT * FROM t1 WHERE exists (SELECT * FROM f(t1.a));

SELECT * FROM t1 where a not in (SELECT f FROM f(t1.a));


CREATE TABLE tversion (
    rnum integer NOT NULL,
    c1 integer,
    cver character(6),
    cnnull integer,
    ccnull character(1)
) DISTRIBUTED BY (rnum);

COPY tversion (rnum, c1, cver, cnnull, ccnull) FROM stdin;
0	1	1.0   	\N	\N
\.

CREATE TABLE qp_tjoin1 (
    rnum integer NOT NULL,
    c1 integer,
    c2 integer
) DISTRIBUTED BY (rnum);

CREATE TABLE qp_tjoin2 (
    rnum integer NOT NULL,
    c1 integer,
    c2 character(2)
) DISTRIBUTED BY (rnum);

CREATE TABLE qp_tjoin3 (
    rnum integer NOT NULL,
    c1 integer,
    c2 character(2)
) DISTRIBUTED BY (rnum);

CREATE TABLE qp_tjoin4 (
    rnum integer NOT NULL,
    c1 integer,
    c2 character(2)
) DISTRIBUTED BY (rnum);

COPY qp_tjoin1 (rnum, c1, c2) FROM stdin;
1	20	25
0	10	15
2	\N	50
\.

COPY qp_tjoin2 (rnum, c1, c2) FROM stdin;
1	15	DD
0	10	BB
3	10	FF
2	\N	EE
\.

COPY qp_tjoin3 (rnum, c1, c2) FROM stdin;
1	15	YY
0	10	XX
\.

COPY qp_tjoin4 (rnum, c1, c2) FROM stdin;
0	20	ZZ
\.

analyze tversion;
analyze qp_tjoin1;
analyze qp_tjoin2;
analyze qp_tjoin3;
analyze qp_tjoin4;

select qp_tjoin1.rnum, qp_tjoin1.c1, case when 10 in ( select 1 from tversion ) then 'yes' else 'no' end from qp_tjoin1 order by rnum;

select rnum, c1, c2 from qp_tjoin2 where 50 not in ( select c2 from qp_tjoin1 where c2=25) order by rnum;

select rnum, c1, c2 from qp_tjoin2 where 20 > all ( select c1 from qp_tjoin1 where c1 = 100) order by rnum;

select rnum, c1, c2 from qp_tjoin2 where 75 > all ( select c2 from qp_tjoin1) order by rnum;

select rnum, c1, c2 from qp_tjoin2 where 20 > all ( select c1 from qp_tjoin1) order by rnum;

CREATE TABLE qp_tab1(a int, b int);
CREATE TABLE qp_tab2(c int, d int);
CREATE TABLE qp_tab3(e int, f int);
INSERT INTO qp_tab1 VALUES (1,2);
INSERT INTO qp_tab2 VALUES (3,4);
INSERT INTO qp_tab3 VALUES (4,5);
ANALYZE qp_tab1;
ANALYZE qp_tab2;
ANALYZE qp_tab3;

EXPLAIN SELECT a FROM qp_tab1 f1 LEFT JOIN qp_tab2 on a=c WHERE NOT EXISTS(SELECT 1 FROM qp_tab1 f2 WHERE f1.a = f2.a);

EXPLAIN SELECT DISTINCT a FROM qp_tab1 WHERE NOT (SELECT TRUE FROM qp_tab2 WHERE EXISTS (SELECT * FROM qp_tab3 WHERE qp_tab2.c = qp_tab3.e));
SELECT DISTINCT a FROM qp_tab1 WHERE NOT (SELECT TRUE FROM qp_tab2 WHERE EXISTS (SELECT * FROM qp_tab3 WHERE qp_tab2.c = qp_tab3.e));

-- ----------------------------------------------------------------------
-- Test: non-equivalence clauses
-- ----------------------------------------------------------------------
CREATE TABLE qp_non_eq_a (i int, f float8);
CREATE TABLE qp_non_eq_b (i int, f float8);
INSERT INTO qp_non_eq_a VALUES (1, '0'), (2, '-0');
INSERT INTO qp_non_eq_b VALUES (3, '0'), (1, '-0');
ANALYZE qp_non_eq_a;
ANALYZE qp_non_eq_b;

EXPLAIN SELECT * FROM qp_non_eq_a, qp_non_eq_b WHERE qp_non_eq_a.f = qp_non_eq_b.f AND qp_non_eq_a.f::text <> '-0';
SELECT * FROM qp_non_eq_a, qp_non_eq_b WHERE qp_non_eq_a.f = qp_non_eq_b.f AND qp_non_eq_a.f::text <> '-0';

EXPLAIN SELECT * FROM qp_non_eq_a INNER JOIN qp_non_eq_b ON qp_non_eq_a.f = qp_non_eq_b.f AND CASE WHEN qp_non_eq_b.f::text = '-0' THEN 1 ELSE -1::float8 END < '0';
SELECT * FROM qp_non_eq_a INNER JOIN qp_non_eq_b ON qp_non_eq_a.f = qp_non_eq_b.f AND CASE WHEN qp_non_eq_b.f::text = '-0' THEN 1 ELSE -1::float8 END < '0';

EXPLAIN SELECT * FROM qp_non_eq_a, qp_non_eq_b WHERE qp_non_eq_a.i = qp_non_eq_b.i AND qp_non_eq_a.i = ANY('{1,2,3}'::integer[]);
SELECT * FROM qp_non_eq_a, qp_non_eq_b WHERE qp_non_eq_a.i = qp_non_eq_b.i AND qp_non_eq_a.i = ANY('{1,2,3}'::integer[]);

EXPLAIN SELECT * FROM qp_non_eq_a, qp_non_eq_b WHERE qp_non_eq_a.i = qp_non_eq_b.i AND qp_non_eq_a.i = ANY('{1,2,3}'::numeric[]);
SELECT * FROM qp_non_eq_a, qp_non_eq_b WHERE qp_non_eq_a.i = qp_non_eq_b.i AND qp_non_eq_a.i = ANY('{1,2,3}'::numeric[]);

-- ----------------------------------------------------------------------
-- Test: Nestloop within a correlated subquery.
-- Nestloop get empty results from outer in the first run and we cannot
-- squelch (early end of retrieval) inner node if the outer expected to
-- be rescanned.
-- ----------------------------------------------------------------------
CREATE TABLE qp_nl_tab1 (c1 int, c2 int);
CREATE TABLE qp_nl_tab2 (c1 int, c2 int);
INSERT INTO qp_nl_tab1 values (1, 0), (1, 1);
INSERT INTO qp_nl_tab2 values (1, 1), (1, 1);
ANALYZE qp_nl_tab2;
VACUUM qp_nl_tab2;
SELECT * FROM qp_nl_tab1 t1 WHERE t1.c1 + 5 > ANY(SELECT t2.c2 FROM qp_nl_tab2 t2, generate_series(1, 1) i WHERE i = t1.c2 LIMIT 1);


-- ----------------------------------------------------------------------
-- Test: Various single & skip-level correlated subqueries
-- ----------------------------------------------------------------------
DROP TABLE IF EXISTS t1;
DROP TABLE IF EXISTS supplier;
create table t1(a int, b int);
create table supplier(city text);
insert into t1 values (1, 1), (2, 2), (3, 3);
insert into supplier values ('a'),('b'),('c'),('d'),('e');
analyze t1;
analyze supplier;

set optimizer_enforce_subplans = 1;

-- with TVF
explain select x1.a, (select count(*) from generate_series(1, x1.a)) from t1 x1;
select x1.a, (select count(*) from generate_series(1, x1.a)) from t1 x1;

-- with limit
explain select t1.a, (select count(*) c from (select city from supplier limit t1.a) x) from t1;
select t1.a, (select count(*) c from (select city from supplier limit t1.a) x) from t1;

-- with nested join
explain select t1.*, (select count(*) as ct from generate_series(1, a), t1) from t1;
select t1.*, (select count(*) as ct from generate_series(1, a), t1) from t1;

explain select * from t1 where 0 < (select count(*) from generate_series(1, a), t1);
select * from t1 where 0 < (select count(*) from generate_series(1, a), t1);

reset optimizer_enforce_subplans;

DROP TABLE IF EXISTS t1;
DROP TABLE IF EXISTS supplier;

-- ----------------------------------------------------------------------
-- Test: teardown.sql
-- ----------------------------------------------------------------------
set client_min_messages='warning';
drop schema qp_correlated_query cascade;
reset optimizer_trace_fallback;
