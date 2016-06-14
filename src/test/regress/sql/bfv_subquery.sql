-- count number of certain operators in a given plan
-- start_ignore
create language plpythonu;
-- end_ignore

create or replace function count_operator(explain_query text, operator text) returns int as
$$
rv = plpy.execute(explain_query)
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

select count_operator('explain select bfv_subquery_t1.i, (select bfv_subquery_t1.i from bfv_subquery_t2) from bfv_subquery_t1;', 'Table Scan') > 0;
select count_operator('explain select bfv_subquery_t1.i, (select bfv_subquery_t1.i from bfv_subquery_t2) from bfv_subquery_t1;', 'Seq Scan') > 0;

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
