--
-- NOTIN 
-- Test NOTIN clauses
--

create schema notin;
set search_path=notin;

--
-- generate a bunch of tables
--
create table t1 (
	c1 integer
);

create table t2 (
	c2 integer
);

create table t3 (
	c3 integer
);

create table t4 (
	c4 integer
);

create table t1n (
	c1n integer
);

create table g1 (
	a integer,
	b integer,
	c integer
);

create table l1 (
	w integer,
	x integer,
	y integer,
	z integer
);

--
-- stick in some values
--
insert into t1 values (generate_series (1,10));

insert into t2 values (generate_series (1,5));

insert into t3 values (1), (2), (3);

insert into t4 values (1), (2);

insert into t1n values (1), (2), (3), (null), (5), (6), (7);

insert into g1 values
  (1,1,1),
  (1,1,2),
  (1,2,2),
  (2,2,2),
  (2,2,3),
  (2,3,3),
  (3,3,3),
  (3,3,3),
  (3,3,4),
  (3,4,4),
  (4,4,4);

insert into l1 values (generate_series (1,10), generate_series (1,10), generate_series (1,10), generate_series (1,10));

--
-- queries
--

--
--q1
--
explain select c1 from t1 where c1 not in 
	(select c2 from t2);
select c1 from t1 where c1 not in 
	(select c2 from t2);

--
--q2
--
explain select c1 from t1 where c1 not in 
	(select c2 from t2 where c2 > 2 and c2 not in 
		(select c3 from t3));
select c1 from t1 where c1 not in 
	(select c2 from t2 where c2 > 2 and c2 not in 
		(select c3 from t3));
	
--
--q3
--
explain select c1 from t1 where c1 not in 
	(select c2 from t2 where c2 not in 
		(select c3 from t3 where c3 not in 
			(select c4 from t4)));
select c1 from t1 where c1 not in 
	(select c2 from t2 where c2 not in 
		(select c3 from t3 where c3 not in 
			(select c4 from t4)));

--
--q4
--
explain select c1 from t1, 
(select c2 from t2 where c2 not in 
	(select c3 from t3)) foo 
	where c1 = foo.c2;
select c1 from t1, 
(select c2 from t2 where c2 not in 
	(select c3 from t3)) foo 
	where c1 = foo.c2;

--
--q5
--
explain select c1 from t1, 
(select c2 from t2 where c2 not in 
	(select c3 from t3) and c2 > 4) foo 
	where c1 = foo.c2;
select c1 from t1, 
(select c2 from t2 where c2 not in 
	(select c3 from t3) and c2 > 4) foo 
	where c1 = foo.c2;

--
--q6
--
explain select c1 from t1 where c1 not in 
	(select c2 from t2) and c1 > 1;
select c1 from t1 where c1 not in 
	(select c2 from t2) and c1 > 1;

--
--q7
--
explain select c1 from t1 where c1 > 6 and c1 not in 
	(select c2 from t2) and c1 < 10;
select c1 from t1 where c1 > 6 and c1 not in 
	(select c2 from t2) and c1 < 10;

--
--q8 introduce join
--
explain select c1 from t1,t2 where c1 not in 
	(select c3 from t3) and c1 = c2;
select c1 from t1,t2 where c1 not in 
	(select c3 from t3) and c1 = c2;

--
--q9
--
select c1 from t1 where c1 not in 
	(select c2 from t2 where c2 > 2 and c2 < 5);

--
--q10
--
select count(c1) from t1 where c1 not in 
	(select sum(c2) from t2);

--
--q11
--
select c1 from t1 where c1 not in 
	(select count(*) from t1);

--
--q12
--
select a,b from g1 where (a,b) not in
	(select a,b from g1);

--
--q13
--
explain select x,y from l1 where (x,y) not in
	(select distinct y, sum(x) from l1 group by y having y < 4 order by y) order by 1,2;
select x,y from l1 where (x,y) not in
	(select distinct y, sum(x) from l1 group by y having y < 4 order by y) order by 1,2;

--
--q14
--
explain select * from g1 where (a,b,c) not in 
	(select x,y,z from l1);
select * from g1 where (a,b,c) not in 
	(select x,y,z from l1);

--
--q15
--
explain select c1 from t1, t2 where c1 not in 
	(select c3 from t3 where c3 = c1) and c1 = c2;
select c1 from t1, t2 where c1 not in 
	(select c3 from t3 where c3 = c1) and c1 = c2;

--
--q17
-- null test
--
select c1 from t1 where c1 not in 
	(select c1n from t1n);

--
--q18
-- null test
--
select c1 from t1 where c1 not in 
	(select c2 from t2 where c2 not in 
		(select c3 from t3 where c3 not in 
			(select c1n from t1n)));

--
--q19
--
select c1 from t1 join t2 on c1 = c2 where c1 not in 
	(select c3 from t3);

--
--q20
--
explain select c1 from t1 where c1 not in 
	(select sum(c2) as s from t2 where c2 > 2 group by c2 having c2 > 3);
select c1 from t1 where c1 not in 
	(select sum(c2) as s from t2 where c2 > 2 group by c2 having c2 > 3);

--
--q21
-- multiple not in in where clause
--
select c1 from t1 where c1 not in 
	(select c2 from t2) and c1 not in 
	(select c3 from t3);

--
--q22
-- coexist with joins
--
select c1 from t1,t3,t2 where c1 not in 
	(select c4 from t4) and c1 = c3 and c1 = c2;

--
--q23
-- union in subselect
--
select c1 from t1 where c1 not in 
	(select c2 from t2 union select c3 from t3);

--
--q24
--
select c1 from t1 where c1 not in 
	(select c2 from t2 union all select c3 from t3);

--
--q25
--
select c1 from t1 where c1 not in 
	(select (case when c1n is null then 1 else c1n end) as c1n from t1n);

--
--q26
--
explain select (case when c1%2 = 0 
 then (select sum(c2) from t2 where c2 not in (select c3 from t3)) 
 else (select sum(c3) from t3 where c3 not in (select c4 from t4)) end) as foo from t1;
select (case when c1%2 = 0 
 then (select sum(c2) from t2 where c2 not in (select c3 from t3)) 
 else (select sum(c3) from t3 where c3 not in (select c4 from t4)) end) as foo from t1;

--
--q27
--
explain select c1 from t1 where not c1 >= some (select c2 from t2);
select c1 from t1 where not c1 >= some (select c2 from t2);

--
--q28
--
explain select c2 from t2 where not c2 < all (select c2 from t2);
select c2 from t2 where not c2 < all (select c2 from t2);

--
--q29
--
explain select c3 from t3 where not c3 <> any (select c4 from t4);
select c3 from t3 where not c3 <> any (select c4 from t4);

--
--q31
--
explain select c1 from t1 where c1 not in (select c2 from t2 order by c2 limit 3) order by c1;
select c1 from t1 where c1 not in (select c2 from t2 order by c2 limit 3) order by c1;

--quantified/correlated subqueries
--
--q32
--
explain select c1 from t1 where c1 =all (select c2 from t2 where c2 > -1 and c2 <= 1);
select c1 from t1 where c1 =all (select c2 from t2 where c2 > -1 and c2 <= 1);

--
--q33
--
explain select c1 from t1 where c1 <>all (select c2 from t2);
select c1 from t1 where c1 <>all (select c2 from t2);

--
--q34
--
explain select c1 from t1 where c1 <=all (select c2 from t2 where c2 not in (select c1n from t1n));
select c1 from t1 where c1 <=all (select c2 from t2 where c2 not in (select c1n from t1n));

--
--q35
--
explain select c1 from t1 where not c1 =all (select c2 from t2 where not c2 >all (select c3 from t3));
select c1 from t1 where not c1 =all (select c2 from t2 where not c2 >all (select c3 from t3));

--
--q36
--
explain select c1 from t1 where not c1 <>all (select c1n from t1n where c1n <all (select c3 from t3 where c3 = c1n));
select c1 from t1 where not c1 <>all (select c1n from t1n where c1n <all (select c3 from t3 where c3 = c1n));

--
--q37
--
explain select c1 from t1 where not c1 >=all (select c2 from t2 where c2 = c1);
select c1 from t1 where not c1 >=all (select c2 from t2 where c2 = c1);

--
--q38
-- GPDB_90_MERGE_FIXME: We should generate c2 IS NOT NULL derived filter.
--
explain select c1 from t1 where not exists (select c2 from t2 where c2 = c1);
select c1 from t1 where not exists (select c2 from t2 where c2 = c1);

--
--q39
-- GPDB_90_MERGE_FIXME: Pull up nested sublinks.
--
explain select c1 from t1 where not exists (select c2 from t2 where c2 not in (select c3 from t3) and c2 = c1);
select c1 from t1 where not exists (select c2 from t2 where c2 not in (select c3 from t3) and c2 = c1);

--
--q40
-- GPDB_90_MERGE_FIXME: Pull up nested sublinks.
-- GPDB_90_MERGE_FIXME: We should be able to push down join filter on param $0 to a result node on top of LASJ (Not in)
--
explain select c1 from t1 where not exists (select c2 from t2 where exists (select c3 from t3) and c2 <>all (select c3 from t3) and c2 = c1);
select c1 from t1 where not exists (select c2 from t2 where exists (select c3 from t3) and c2 <>all (select c3 from t3) and c2 = c1);

--
--q41
--
select c1 from t1 where c1 not in (select c2 from t2) or c1 = 49;

--
--q42
--
select c1 from t1 where not not not c1 in (select c2 from t2);


--
--q43
--
explain select c1 from t1 where c1 not in (select c2 from t2 where c2 > 4) and c1 is not null;
select c1 from t1 where c1 not in (select c2 from t2 where c2 > 4) and c1 is not null;

--
--q44
--
select c1 from t1 where c1 not in (select c2 from t2 where c2 > 4) and c1 > 2;

reset search_path;
drop schema notin cascade;
