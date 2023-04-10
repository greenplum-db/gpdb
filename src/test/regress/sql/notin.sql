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

analyze t1;
analyze t2;
analyze t3;
analyze t4;
analyze t1n;
analyze g1;
analyze l1;

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
--
explain select c1 from t1 where not exists (select c2 from t2 where c2 = c1);
select c1 from t1 where not exists (select c2 from t2 where c2 = c1);

--
--q39
--
explain select c1 from t1 where not exists (select c2 from t2 where c2 not in (select c3 from t3) and c2 = c1);
select c1 from t1 where not exists (select c2 from t2 where c2 not in (select c3 from t3) and c2 = c1);

--
--q40
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

-- Test if the equality operator is implemented by a SQL function
--
--q45
--
create domain absint as int4;
create function iszero(absint) returns bool as $$ begin return $1::int4 = 0; end; $$ language plpgsql immutable strict;
create or replace function abseq (absint, absint) returns bool as $$ select iszero(abs($1) - abs($2)); $$ language sql immutable strict;
create operator = (PROCEDURE = abseq, leftarg=absint, rightarg=absint);
explain select c1 from t1 where c1::absint not in
	(select c1n::absint from t1n);
select c1 from t1 where c1::absint not in
	(select c1n::absint from t1n);


-- Test the null not in an empty set
-- null not in an unempty set, always returns false
-- null not in an empty set, always returns true
--
-- q46
--
create table table_source (c1 varchar(100),c2 varchar(100),c3 varchar(100),c4 varchar(100));
insert into table_source (c1 ,c2 ,c3 ,c4 ) values ('000181202006010000003158',null,'INC','0000000001') ;
create table table_source2 as select * from table_source distributed by (c2);
create table table_source3 as select * from table_source distributed replicated;
create table table_source4 (c1 varchar(100),c2 varchar(100) not null,c3 varchar(100),c4 varchar(100));
insert into table_source4 (c1 ,c2 ,c3 ,c4 ) values ('000181202006010000003158','a','INC','0000000001') ;
create table table_config (c1 varchar(10) ,c2 varchar(10) ,PRIMARY KEY (c1));
insert into table_config select i, 'test' from generate_series(1, 1000)i;
analyze table_config;
delete from table_config where gp_segment_id = 0;

explain select * from table_source where c3 = 'INC' and c4 = '0000000001' and c2 not in (SELECT c1 from table_config where c2='test');
select * from table_source where c3 = 'INC' and c4 = '0000000001' and c2 not in (SELECT c1 from table_config where c2='test');
explain select * from table_source2 where c3 = 'INC' and c4 = '0000000001' and c2 not in (SELECT c1 from table_config where c2='test');
select * from table_source2 where c3 = 'INC' and c4 = '0000000001' and c2 not in (SELECT c1 from table_config where c2='test');
explain select * from table_source3 where c3 = 'INC' and c4 = '0000000001' and c2 not in (SELECT c1 from table_config where c2='test');
select * from table_source3 where c3 = 'INC' and c4 = '0000000001' and c2 not in (SELECT c1 from table_config where c2='test');
explain select * from table_source4 where c3 = 'INC' and c4 = '0000000001' and c2 not in (SELECT c1 from table_config where c2='test');
select * from table_source4 where c3 = 'INC' and c4 = '0000000001' and c2 not in (SELECT c1 from table_config where c2='test');

--
-- Multi Column NOT-IN
-- Please refer to https://github.com/greenplum-db/gpdb/issues/12930
--
create table t1_12930(a int not null, b int not null);
create table t2_12930(a int not null, b int not null);

-- non-nullable: t1.a, t1.b, t2.a, t2.b
insert into t1_12930 values (1, 1), (2, 2);
insert into t2_12930 values (1, 1), (2, 3), (3,3);
explain select * from t1_12930 where (a, b) not in (select a, b from t2_12930);
select * from t1_12930 where (a, b) not in (select a, b from t2_12930);
explain select * from t1_12930 where (a+1, b+1) not in (select a, b from t2_12930);
select * from t1_12930 where (a+1, b+1) not in (select a, b from t2_12930);
explain select * from t1_12930 where (a,b) <> ALL (select a, b from t2_12930);
select * from t1_12930 where (a,b) <> ALL (select a, b from t2_12930);

-- non-nullable: t1.a, t2.a, t2.b
-- nullable: t1.b
truncate t1_12930;
truncate t2_12930;
alter table t2_12930 alter column b set not null;
alter table t1_12930 alter column b drop not null;
insert into t1_12930 values (1, null);
insert into t2_12930 values (1, 1);
explain select * from t1_12930 where (a, b) <>ALL (select a, b from t2_12930);
select * from t1_12930 where (a, b) <>ALL (select a, b from t2_12930);

-- non-nullable: t1.a, t1.b, t2.a
-- nullable: t2.b
truncate t1_12930;
truncate t2_12930;
alter table t2_12930 alter column b drop not null;
insert into t1_12930 values (1, 1);
insert into t2_12930 values (1, null);
explain select * from t1_12930 where (a, b) not in (select a, b from t2_12930);
select * from t1_12930 where (a, b) not in (select a, b from t2_12930);

-- non-nullable: t1.a, t2.a, t2.b
-- nullable: t1.b
truncate t1_12930;
truncate t2_12930;
alter table t2_12930 alter column b set not null;
alter table t1_12930 alter column b drop not null;
insert into t1_12930 values (1, null);
insert into t2_12930 values (1, 1);
explain select * from t1_12930 where (a, b) not in (select a, b from t2_12930);
select * from t1_12930 where (a, b) not in (select a, b from t2_12930);
explain select * from t1_12930 where (a, b) not in (select a, b from t2_12930) and b is not null;
select * from t1_12930 where (a, b) not in (select a, b from t2_12930) and b is not null;

-- outer plan's target list does not contain the columns in qual
-- non-nullable: t1.a, t1.b, t2.a, t2.b
truncate t1_12930;
truncate t2_12930;
alter table t1_12930 alter column b set not null;
insert into t1_12930 values (1, 1);
insert into t2_12930 values (1, 1);
explain select 1 from t1_12930 where (a, b) not in (select a, b from t2_12930);
select 1 from t1_12930 where (a, b) not in (select a, b from t2_12930);

-- RTE contains view 
-- non-nullable: t1.a, t1.b, t2.a, t2.b
create view v_12930 as select a,b from t1_12930;
explain select * from t2_12930 where (a,b) not in (select a,b from v_12930);
select * from t2_12930 where (a,b) not in (select a,b from v_12930);
explain select * from v_12930 where (a,b) not in (select a,b from t2_12930);
select * from v_12930 where (a,b) not in (select a,b from t2_12930);

-- var from RTE_JOIN
-- non-nullable: t1.a, t1.b, t2.a, t2.b, r1.x, r1.y
truncate t1_12930;
truncate t2_12930;
create table r1_12930(x int not null, y int not null);
insert into t1_12930 values (1, 2), (1, 3);
insert into t2_12930 values (1, 3), (1, 4);
insert into r1_12930 values (1, 2), (1, 5);
explain select * from r1_12930
inner join t1_12930 on a = x
where (x, b) not in (select a,b from t2_12930);
select * from r1_12930
inner join t1_12930 on a = x
where (x, b) not in (select a,b from t2_12930);

-- test for issue https://github.com/greenplum-db/gpdb/issues/13212
create table t1_13212(a int not null, b int not null);
create table t2_13212(a int not null, b int not null);
explain (costs off)  select 1 from t1_13212 where (NULL, b) not in (select a, b from t2_13212);
select 1 from t1_13212 where (NULL, b) not in (select a, b from t2_13212);
insert into t1_13212 values (1, 1);
insert into t2_13212 values (1, 1);
select 1 from t1_13212 where (NULL, b) not in (select a, b from t2_13212);
update t2_13212 set b = 2;
select 1 from t1_13212 where (NULL, b) not in (select a, b from t2_13212);

-- test for params of not-in sublink
create table t1p(a int, b int, c int);
create table t2p(b int, a int);
explain (costs off)
update
  t1p
set
  a = age.aging
from
  (
    select
      c,
      (
        select
            diff
        from
            generate_series(1, 10) diff
        where
            (diff, t1p.b) not in (select a, b from t2p)
      ) as aging
    from
      t1p
  ) age
where a = age.c;

update
  t1p
set
  a = age.aging
from
  (
    select
      c,
      (
        select
            diff
        from
            generate_series(1, 10) diff
        where
            (diff, t1p.b) not in (select a, b from t2p)
      ) as aging
    from
      t1p
  ) age
where a = age.c;

drop table t1p;
drop table t2p;

-- test for github issue 14858
create table t1_14858(a varchar(32), b varchar(32), c int, d int);
create table t2_14858(a varchar(32), b varchar(32), c int, d int);
create table t3_14858(a varchar(32), b varchar(32), c int, d int);

create view mv_14858 as select * from t1_14858 full join
(select a, b, count(c) from t2_14858 group by a,b union
 select a, b, count(c) from t3_14858 group by a, b) x using (a, b);

select * from mv_14858 where a not in (select a from t1_14858);

drop view mv_14858;
drop table t1_14858;
drop table t2_14858;
drop table t3_14858;

reset search_path;
drop schema notin cascade;
