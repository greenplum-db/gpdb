-- 
-- @created 2009-01-27 14:00:00
-- @modified 2013-06-24 17:00:00
-- @tags ddl schema_topology

-- start_ignore
drop table if exists csq_t1;
drop table if exists csq_t2;
drop table if exists csq_t3;

create table csq_t1(a int, b int);
insert into csq_t1 values (1,2);
insert into csq_t1 values (3,4);
insert into csq_t1 values (5,6);
insert into csq_t1 values (7,8);

create table csq_t2(x int,y int);
insert into csq_t2 values(1,1);
insert into csq_t2 values(3,9);
insert into csq_t2 values(5,25);
insert into csq_t2 values(7,49);

create table csq_t3(c int, d text);
insert into csq_t3 values(1,'one');
insert into csq_t3 values(3,'three');
insert into csq_t3 values(5,'five');
insert into csq_t3 values(7,'seven');
-- end_ignore

-- CSQ 01: Basic query with where clause
select a, (select y from csq_t2 where x=a) from csq_t1 where b < 8 order by a;

-- CSQ 02: Basic query with exists
select b from csq_t1 where exists(select * from csq_t2 where y=a) order by b;

-- CSQ Q3: Basic query with not exists
select b from csq_t1 where not exists(select * from csq_t2 where y=a) order by b;

-- CSQ Q4: Basic query with any
select a, x from csq_t1, csq_t2 where csq_t1.a = any (select x) order by a, x;

-- CSQ Q5
select a, x from csq_t2, csq_t1 where csq_t1.a = (select x) order by a, x;


-- CSQ Q6
select a from csq_t1 where (select (y*2)>b from csq_t2 where a=x) order by a;

-- CSQ Q7
SELECT a, (SELECT d FROM csq_t3 WHERE a=c) FROM csq_t1 GROUP BY a order by a;

-- CSQ Q8
SELECT a, (SELECT (SELECT d FROM csq_t3 WHERE a=c)) FROM csq_t1 GROUP BY a order by a;
