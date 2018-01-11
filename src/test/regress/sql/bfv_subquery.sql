-- count number of certain operators in a given plan
-- start_ignore
create language plpythonu;
-- end_ignore

create or replace function count_operator(query text, operator text) returns int as
$$
rv = plpy.execute('EXPLAIN ' + query)
search_text = operator
result = 0
for i in range(len(rv)):
    cur_line = rv[i]['QUERY PLAN']
    if search_text.lower() in cur_line.lower():
        result = result+1
return result
$$
language plpythonu;

--start_ignore
DROP TABLE IF EXISTS bfv_subquery_p;
DROP TABLE IF EXISTS bfv_subquery_r;
--end_ignore

-- subquery over partitioned table
CREATE TABLE bfv_subquery_(a int, b int) partition by range(b) (start(1) end(10));
CREATE TABLE bfv_subquery_r (a int, b int);
INSERT INTO bfv_subquery_ SELECT i,i FROM generate_series(1,9)i;
INSERT INTO bfv_subquery_r SELECT i,i FROM generate_series(1,9)i;

SELECT a FROM bfv_subquery_r WHERE b < ( SELECT 0.5 * sum(a) FROM bfv_subquery_ WHERE b >= 3) ORDER BY 1;
	
--start_ignore
drop table if exists bfv_subquery_r2;
drop table if exists s;
--end_ignore

-- subquery with distinct and outer references	

create table bfv_subquery_r2(a int, b int) distributed by (a);
create table bfv_subquery_s2(a int, b int) distributed by (a);

insert into bfv_subquery_r2 values (1,1);
insert into bfv_subquery_r2 values (2,1);
insert into bfv_subquery_r2 values (2,NULL);
insert into bfv_subquery_r2 values (NULL,0);
insert into bfv_subquery_r2 values (NULL,NULL);

insert into bfv_subquery_s2 values (2,2);
insert into bfv_subquery_s2 values (1,0);
insert into bfv_subquery_s2 values (1,1);

select * from bfv_subquery_r2 
where a = (select x.a from (select distinct a from bfv_subquery_s2 where bfv_subquery_s2.b = bfv_subquery_r2	.b) x);

-- start_ignore
DROP FUNCTION IF EXISTS csq_f(a int);
-- end_ignore
CREATE FUNCTION csq_f(a int) RETURNS int AS $$ select $1 $$ LANGUAGE SQL;

--start_ignore
DROP TABLE IF EXISTS csq_r;
--end_ignore

CREATE TABLE csq_r(a int);
INSERT INTO csq_r VALUES (1);

SELECT * FROM csq_r WHERE a IN (SELECT * FROM csq_f(csq_r.a));

-- subquery in the select list

--start_ignore
drop table if exists  bfv_subquery_t1;
drop table if exists  bfv_subquery_t2;
--end_ignore

create table  bfv_subquery_t1(i int, j int);
create table  bfv_subquery_t2(i int, j int);

insert into  bfv_subquery_t1 select i, i%5 from generate_series(1,10)i;
insert into  bfv_subquery_t2 values (1, 10);

select count_operator('select bfv_subquery_t1.i, (select bfv_subquery_t1.i from bfv_subquery_t2) from bfv_subquery_t1;', 'Table Scan') > 0;
select count_operator('select bfv_subquery_t1.i, (select bfv_subquery_t1.i from bfv_subquery_t2) from bfv_subquery_t1;', 'Seq Scan') > 0;

select bfv_subquery_t1.i, (select bfv_subquery_t1.i from bfv_subquery_t2) from bfv_subquery_t1 order by 1, 2;

-- start_ignore
drop table if exists bfv_subquery_t3;
drop table if exists bfv_subquery_s3;
-- end_ignore

create table bfv_subquery_t3(a int, b int);
insert into bfv_subquery_t3 values (1,4),(0,3);

create table bfv_subquery_s3(i int, j int);

-- ALL subquery

select * from bfv_subquery_t3 where a < all (select i from bfv_subquery_s3 limit 1) order by a;

select * from bfv_subquery_t3 where a < all (select i from bfv_subquery_s3) order by a;

select * from bfv_subquery_t3 where a < all (select i from bfv_subquery_s3 limit 2) order by a;

select * from bfv_subquery_t3 where a < all (select i from bfv_subquery_s3) order by a;

-- Direct Dispatch caused reader gang process hanging  on start_xact_command

DROP TABLE IF EXISTS bfv_subquery_a1;
DROP TABLE IF EXISTS bfv_subquery_b1;
CREATE TABLE bfv_subquery_a1(i INT, j INT);
INSERT INTO bfv_subquery_a1(SELECT i, i * i FROM generate_series(1, 10) AS i);
CREATE TABLE bfv_subquery_b1(i INT, j INT);
INSERT INTO bfv_subquery_b1(SELECT i, i * i FROM generate_series(1, 10) AS i);

SELECT  bfv_subquery_a1.* FROM bfv_subquery_a1 INNER JOIN bfv_subquery_b1 ON  bfv_subquery_a1.i =  bfv_subquery_b1.i WHERE  bfv_subquery_a1.j NOT IN (SELECT j FROM bfv_subquery_a1 a2 where a2.j =  bfv_subquery_b1.j) and  bfv_subquery_a1.i = 1;

DROP TABLE IF EXISTS bfv_subquery_a2;

CREATE TABLE bfv_subquery_a2(i INT, j INT);
INSERT INTO bfv_subquery_a2(SELECT i, i * i FROM generate_series(1, 10) AS i);

SELECT bfv_subquery_a2.* FROM bfv_subquery_a2 WHERE bfv_subquery_a2.j NOT IN (SELECT j FROM bfv_subquery_a2 a2 where a2.j = bfv_subquery_a2.j) and bfv_subquery_a2.i = 1;

-- prohibit plans with Motions above outer references

--start_ignore
drop table if exists  bfv_subquery_foo1;
--end_ignore

create table  bfv_subquery_foo1(a integer, b integer) distributed by (a);
							 
insert into  bfv_subquery_foo1 values(1,1);
insert into  bfv_subquery_foo1 values(2,2);

select 
(select a from  bfv_subquery_foo1 inner1 where inner1.a=outer1.a  
union 
select b from  bfv_subquery_foo1 inner2 where inner2.b=outer1.b) 
from  bfv_subquery_foo1 outer1;

-- using of subqueries with unnest with IN or NOT IN predicates

select 1 where 22 not in (SELECT unnest(array[1,2]));

select 1 where 22 in (SELECT unnest(array[1,2]));

select 1 where 22  in (SELECT unnest(array[1,2,22]));

select 1 where 22 not in (SELECT unnest(array[1,2,22]));

-- Error when using subquery : no parameter found for initplan subquery

-- start_ignore
drop table if exists mpp_t1;
drop table if exists mpp_t2;
drop table if exists mpp_t3;

create table mpp_t1(a int,b int) distributed by (a); 
create table mpp_t2(a int,b int) distributed by (b);
create table mpp_t3(like mpp_t1);
-- end_ignore

select * from mpp_t1 where a=1 and a=2 and a > (select mpp_t2.b from mpp_t2);
select * from mpp_t1 where a<1 and a>2 and a > (select mpp_t2.b from mpp_t2);
select * from mpp_t3 where a in ( select a from mpp_t1 where a<1 and a>2 and a > (select mpp_t2.b from mpp_t2));
select * from mpp_t3 where a <1 and a=1 and a in ( select a from mpp_t1 where a > (select mpp_t2.b from mpp_t2));
select * from mpp_t1 where a <1 and a=1 and a in ( select a from mpp_t1 where a > (select mpp_t2.b from mpp_t2));
select * from mpp_t1 where a = (select a FROM mpp_t2 where mpp_t2.b > (select max(b) from mpp_t3 group by b) and mpp_t2.b=1 and mpp_t2.b=2);

-- start_ignore
drop table if exists mpp_t1;
drop table if exists mpp_t2;
drop table if exists mpp_t3;
-- end_ignore

--
-- Test case for when there is case clause in join filter
--

-- start_ignore
drop table if exists t_case_subquery1;
-- end_ignore

create table t_case_subquery1 (a int, b int, c text);
insert into t_case_subquery1 values(1, 5, NULL), (1, 2, NULL);

select t1.* from t_case_subquery1 t1 where t1.b = (
  select max(b) from t_case_subquery1 t2 where t1.a = t2.a and t2.b < 5 and
    case
    when t1.c is not null and t2.c is not null
    then t1.c = t2.c
    end
);
-- start_ignore
drop table if exists t_case_subquery1;
-- end_ignore

--
-- Test case for if coalesce is needed for specific cases where a subquery with
-- count aggregate has to return 0 or null. Count returns 0 on empty relations
-- where other queries return NULL.
--

-- start_ignore
drop table if exists t_coalesce_count_subquery;
drop table if exists t_coalesce_count_subquery_empty;
drop table if exists t_coalesce_count_subquery_empty2;
CREATE TABLE t_coalesce_count_subquery(a, b) AS VALUES (1, 1);
CREATE TABLE t_coalesce_count_subquery_empty(c int, d int);
CREATE TABLE t_coalesce_count_subquery_empty2(e int, f int);
-- end_ignore

SELECT (SELECT count(*) FROM t_coalesce_count_subquery_empty where c = a) FROM t_coalesce_count_subquery;

SELECT (SELECT COUNT(*) FROM t_coalesce_count_subquery_empty GROUP BY c LIMIT 1) FROM t_coalesce_count_subquery;

SELECT (SELECT a1 FROM (SELECT count(*) FROM t_coalesce_count_subquery_empty2 group by e
        union all
        SELECT count(*) from t_coalesce_count_subquery_empty group by c) x(a1) LIMIT 1)
FROM t_coalesce_count_subquery;

SELECT (SELECT a1 FROM (SELECT count(*) from t_coalesce_count_subquery_empty group by c
        union all
        SELECT count(*) FROM t_coalesce_count_subquery_empty2 group by e) x(a1) LIMIT 1)
FROM t_coalesce_count_subquery;

-- start_ignore
drop table if exists t_coalesce_count_subquery;
drop table if exists t_coalesce_count_subquery_empty;
drop table if exists t_coalesce_count_subquery_empty2;
-- start_ignore
drop table if exists t_outer;
drop table if exists t_inner;

create table t_outer (a oid, b tid);
create table t_inner (c int);
-- end_ignore
SET enable_nestloop=off;
SET enable_hashjoin=off;
set enable_mergejoin = on;
select * from t_outer where t_outer.b not in (select ctid from t_inner);

RESET enable_nestloop;
RESET enable_hashjoin;
RESET enable_mergejoin;

-- start_ignore
drop table if exists t_outer;
drop table if exists t_inner;
-- end_ignore

--
-- In some cases of a NOT EXISTS subquery, planner mistook one side of the
-- predicate as a (derived or direct) attribute on the inner relation, and
-- incorrectly decorrelated the subquery into a JOIN

-- start_ignore
drop table if exists foo;
drop table if exists bar;
create table foo(a, b) as (values (1, 'a'), (2, 'b'));
create table bar(c, d) as (values (1, 'a'), (2, 'b'));
-- end_ignore

select * from foo where not exists (select * from bar where foo.a + bar.c = 1);
select * from foo where not exists (select * from bar where foo.b || bar.d = 'hola');

select * from foo where not exists (select * from bar where foo.a = foo.a + 1);
select * from foo where not exists (select * from bar where foo.b = foo.b || 'a');

select * from foo where foo.a = (select min(bar.c) from bar where foo.b || bar.d = 'bb');

drop table foo, bar;

--
-- subqueries with unnest in projectlist
--
-- start_ignore
DROP TABLE IF EXISTS A;
CREATE TABLE A AS SELECT ARRAY[1,2,3] AS X;
INSERT INTO A VALUES(NULL::int4[]);
-- end_ignore

SELECT (NOT EXISTS (SELECT UNNEST(X))) AS B FROM A;
SELECT (EXISTS (SELECT UNNEST(X))) AS B FROM A;
EXPLAIN SELECT (EXISTS (SELECT UNNEST(X))) AS B FROM A;

DROP TABLE A;
