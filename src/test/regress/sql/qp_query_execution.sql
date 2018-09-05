-- start_ignore
drop schema qp_query_execution cascade;
create schema qp_query_execution;
set search_path to qp_query_execution;

-- count number of certain operators in a given plan
create language plpythonu;
create or replace function qx_count_operator(query text, planner_operator text, optimizer_operator text) returns int as
$$
rv = plpy.execute('EXPLAIN '+ query)
plan = '\n'.join([row['QUERY PLAN'] for row in rv])
optimizer = plan.find('PQO')

if optimizer >= 0:
    return plan.count(optimizer_operator)
else:
    return plan.count(planner_operator)
$$
language plpythonu;

drop table if exists tmp1;
-- end_ignore
CREATE TABLE bugtest
(
  a1 integer,
  a2 integer,
  a3 integer,
  a4 character(1),
  a5 text,
  a6 text,
  a7 character(1),
  a8 integer,
  a9 integer,
  a10 integer,
  a11 integer,
  a12 integer,
  a13 double precision,
  a14 double precision,
  a15 double precision,
  a16 double precision,
  a17 double precision,
  a18 double precision,
  a19 double precision,
  a20 double precision,
  a21 double precision,
  a22 double precision,
  a23 double precision,
  a24 double precision,
  a25 integer,
  a26 integer,
  a27 integer,
  a28 text,
  a29 integer,
  a30 integer,
  a31 integer,
  a32 integer,
  a33 text,
  a34 integer,
  a35 integer,
  a36 character(1),
  a37 double precision,
  a38 integer,
  a39 integer,
  a40 integer,
  a41 integer,
  a42 integer,
  a43 integer,
  a44 integer,
  a45 integer,
  a46 integer,
  a47 integer,
  a48 integer,
  a49 integer,
  a50 integer,
  a51 integer,
  a52 integer,
  a53 double precision,
  a54 double precision,
  a55 double precision,
  a56 double precision
)
DISTRIBUTED BY (a1);

insert into bugtest
select a.* 
from 
  (
    select
1,1,1,'a','a','a','a',11111,1111,11,16,6,0,0.125,0,0.25,0.875,0.125,0,0,0.9375,0.0625,125.9375,20.30810708273,0,1,0,'asdf',0,0,89,1,'aaa',0,0,'',33.5,69,38,5,6,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,2.29411764705882,5.57142857142857,33.5,0
  ) a,
  generate_series(1,100000) b;

set statement_mem="10MB";
create temporary table tmp1 as SELECT * FROM bugtest order by a2 limit 300000;
drop table tmp1;
create temporary table tmp1 as SELECT * FROM bugtest order by a2 limit 90000;
drop table tmp1;
create temporary table tmp1 as SELECT * FROM bugtest order by a2 limit 20000;
drop table tmp1;
create temporary table tmp1 as SELECT * FROM bugtest order by a1 limit 100000;

-- End of mpp16458

drop table if exists lossmithe_colstor; 
create table lossmithe_colstor 
       (loannumber character varying(40), 
       var1 smallint, 
       var2 smallint, 
       partvar smallint, 
       isinterestonly bool,
       upb smallint
       )  
       with (appendonly=true, orientation=column) 
       partition by range(partvar) (start (0) end (100) every (10));

drop table if exists address_he_unique; 
create table address_he_unique (
       var1 integer,
       loannumber character varying(40), 
       var2 integer,
       rand_no smallint)
       distributed by (loannumber);

insert into lossmithe_colstor
       select i::text, i, i, i % 100, true, i % 23 from generate_series(0,99) i; 

insert into address_he_unique
       select i, i::text, i, i % 7 from generate_series(0,999999) i; 

SELECT SUM(upb), isinterestonly 
  FROM lossmithe_colstor b 
  LEFT JOIN address_he_unique a ON a.loannumber::text=b.loannumber::text 
  WHERE rand_no < 500 
  GROUP BY isinterestonly;
  
-- End of mpp16598  
  
-- Redistribute on top of Append with flow node
-- start_ignore
drop table if exists foo_p;
drop table if exists bar;
-- end_ignore

create table foo_p( a int, b int, k int, t text, p int) distributed by (a,b) partition by range(p) ( start(0) end(10) every (2), default partition other);

create table bar( a int, b int, k int, t text, p int) distributed by (a);

insert into foo_p select i, i % 10, i , i || 'SOME NUMBER SOME NUMBER', i % 10 from generate_series(1, 1000) i;

insert into bar select i % 7, i % 6, i % 9, i || 'SOME NUMBER', i % 4 from generate_series(1, 100) i;
insert into bar select i % 7, i % 6, i % 9, i || 'SOME NUMBER', i % 4 from generate_series(1, 10000) i;
insert into bar select i % 7, i % 6, i % 9, i || 'SOME NUMBER', i % 4 from generate_series(1, 10000) i;

analyze foo_p;

analyze bar;

select qx_count_operator('select foo_p.b, foo_p.t from foo_p left outer join bar on foo_p.a = bar.k  where foo_p.t is not null and foo_p.a = 6;', 'Hash Left Join', 'Hash Left Join');
select foo_p.b, foo_p.t from foo_p left outer join bar on foo_p.a = bar.k  where foo_p.t is not null and foo_p.a = 6 order by 1, 2 desc limit 10;

select qx_count_operator('select foo_p.k, foo_p.t from foo_p left outer join bar on foo_p.k = bar.k  where foo_p.t is not null and foo_p.p = 6;', 'Hash Left Join', 'Hash Left Join');
select foo_p.k, foo_p.t from foo_p left outer join bar on foo_p.k = bar.k  where foo_p.t is not null and foo_p.p = 6 order by 1, 2 desc limit 10;

-- Use all distribution keys in the select list
select foo_p.a,foo_p.b, foo_p.t from foo_p left outer join bar on foo_p.k = bar.k  where foo_p.t is not null and foo_p.a = 6 order by 1, 2, 3 desc limit 10;

-- Non-equality predicates on one of the distribution keys
select foo_p.b, foo_p.t from foo_p left outer join bar on foo_p.a = bar.k  where foo_p.t is not null and foo_p.a >= 6 order by 1, 2 desc limit 10;

-- Equality predicates on non-distribution keys
select foo_p.b, foo_p.t from foo_p left outer join bar on foo_p.a = bar.k  where foo_p.t is not null and foo_p.p = 6 order by 1, 2 desc limit 10;

-- All distribution keys in the predicates
select foo_p.b, foo_p.t from foo_p left outer join bar on foo_p.a = bar.k  where foo_p.t is not null and foo_p.a = 6 and foo_p.b = 6 order by 1, 2 desc limit 10;
select foo_p.b, foo_p.t from foo_p left outer join bar on foo_p.a = bar.k  where foo_p.t is not null and foo_p.a = 6 or foo_p.b = 7 order by 1, 2 desc limit 10;


-- Broadcast on top of Append with flow node
-- Forge stats to force a broadcast instead of append
-- Make bar appear too big so that the planner chooses to do a flow on foo_p
-- Make foo_p appear too small so that the planner chooses to do a broadcast
insert into foo_p select 6, i % 10, i , 1 || 'SOME NUMBER SOME NUMBER', i % 10 from generate_series(1, 1000) i;
insert into foo_p select 6, i % 10, i , 1 || 'SOME NUMBER SOME NUMBER', i % 10 from generate_series(1, 1000) i;
insert into foo_p select 6, i % 10, i , 1 || 'SOME NUMBER SOME NUMBER', i % 10 from generate_series(1, 1000) i;

set allow_system_table_mods=dml;
update pg_class set reltuples = 100000000, relpages = 10000000 where relname = 'bar' and relnamespace = (select oid from pg_namespace where nspname = 'qp_query_execution');
update pg_class set reltuples = 1, relpages = 1 where relname = 'foo_p_1_prt_other' and relnamespace = (select oid from pg_namespace where nspname = 'qp_query_execution');
update pg_class set reltuples = 1, relpages = 1 where relname = 'foo_p_1_prt_2' and relnamespace = (select oid from pg_namespace where nspname = 'qp_query_execution');

select foo_p.b, foo_p.t from foo_p left outer join bar on foo_p.a = bar.a  where foo_p.p =3 and foo_p.a = 6 order by 1, 2 desc limit 10;


-- Varchar in the select list with redistribute on top of an append with flow node
-- start_ignore
drop table if exists abbp;
drop table if exists b;
-- end_ignore
create table abbp ( a character varying(60), b character varying(60), k character varying(60), t int, p int) distributed by (a,b) partition by range(p)  ( start(0) end(10) every (2), default partition other);
create table b ( a character varying(60), b character varying(60), k character varying(60), t int, p int) distributed by(a);

insert into abbp select i || 'SOME NUMBER', i || 'SN', i || 'SN SN', i, i%10 from generate_series(1, 1000)i;

insert into b select i % 7 || 'SOME NUMBER', i%6 || 'SN', i % 9 || 'SOME NUMBER', i % 9, i % 4 from generate_series(1, 100)i;  
insert into b select i % 7 || 'SOME NUMBER', i%6 || 'SN' , i % 9 || 'SOME NUMBER', i % 9, i % 4 from generate_series(1, 1000)i;  
insert into b select i % 7 || 'SOME NUMBER', i%6 || 'SN' , i % 9 || 'SOME NUMBER', i % 9, i % 4 from generate_series(1, 1000)i;  

analyze abbp;
analyze b;

select qx_count_operator('select abbp.k, abbp.t from abbp left outer join b on abbp.k = b.k  where abbp.t is not null and abbp.p = 6;', 'Hash Left Join', 'Hash Left Join');
select abbp.k, abbp.t from abbp left outer join b on abbp.k = b.k  where abbp.t is not null and abbp.p = 6 order by 1, 2 desc limit 10;

select qx_count_operator('select abbp.b, abbp.t from abbp left outer join b on abbp.a = b.k  where abbp.t is not null and abbp.a = E''6SOME NUMBER''', 'Hash Left Join', 'Hash Left Join');
select abbp.b, abbp.t from abbp left outer join b on abbp.a = b.k  where abbp.t is not null and abbp.a = '6SOME NUMBER' order by 1, 2 desc limit 10;

-- Varchar in the select list with a broadcast on top of an append with flow node
-- Forge stats to force a broadcast instead of append
-- Make b appear too big so that the planner chooses to do a flow on abbp
-- Make abbp appear too small so that the planner chooses to do a broadcast 
insert into foo_p select 6, i % 10, i , 1 || 'SOME NUMBER SOME NUMBER', i % 10 from generate_series(1, 1000) i;
insert into foo_p select 6, i % 10, i , 1 || 'SOME NUMBER SOME NUMBER', i % 10 from generate_series(1, 1000) i;
insert into foo_p select 6, i % 10, i , 1 || 'SOME NUMBER SOME NUMBER', i % 10 from generate_series(1, 1000) i;

set allow_system_table_mods=dml;
update pg_class set reltuples = 100000000, relpages = 10000000 where relname = 'b' and relnamespace = (select oid from pg_namespace where nspname = 'qp_query_execution');
update pg_class set reltuples = 1, relpages = 1 where relname like 'abbp%' and relnamespace = (select oid from pg_namespace where nspname = 'qp_query_execution');

select abbp.b, abbp.t from abbp join (select abbp.* from b, abbp where abbp.a = b.k and abbp.a = '6SOME NUMBER') FOO on abbp.a = FOO.a  where abbp.t is not null order by 1, 2 desc limit 10;

select abbp.b, abbp.t from abbp left outer join b on abbp.a = b.k  where abbp.t is not null and abbp.a = '6SOME NUMBER' order by 1, 2 desc limit 10;


-- Queries without motion node on the partitioned table
-- start_ignore
drop table if exists abbp;
drop table if exists b;
-- end_ignore

create table abbp( a int, b int, k int, t text, p int) distributed by (a,b) partition by range(p) ( start(0) end(10) every (2), default partition other);
create table b (a int, b int, k int, t text, p int) distributed by (a);

insert into abbp select i, i, i , i || 'SOME NUMBER SOME NUMBER', i % 10 from generate_series(1, 1000) i;

insert into b select i%7, i%10, i , i || 'SOME NUMBER', i % 4 from generate_series(1, 100) i;
insert into b select i%7, i%10, i , i || 'SOME NUMBER', i % 4 from generate_series(1, 1000) i;
insert into b select i%7, i%10, i , i || 'SOME NUMBER', i % 4 from generate_series(1, 1000) i;

analyze abbp;
analyze b;

select qx_count_operator('select abbp.k, abbp.t from abbp left outer join b on abbp.k = b.k where abbp.t is not null and abbp.p = 6;', 'Hash Left Join', 'Hash Left Join');
select abbp.k, abbp.t from abbp left outer join b on abbp.k = b.k where abbp.t is not null and abbp.p = 6 order by 1, 2 desc limit 10;

select qx_count_operator('select abbp.b, abbp.t from abbp left outer join b on abbp.a = b.k where abbp.t is not null and abbp.a = 6;', 'Hash Left Join', 'Hash Left Join');
select abbp.b, abbp.t from abbp left outer join b on abbp.a = b.k where abbp.t is not null and abbp.a = 6 order by 1, 2 asc limit 10;

-- Partitioned tables with decimal type distribution keys
-- start_ignore
drop table if exists foo_p;
drop table if exists bar;
-- end_ignore

create table foo_p ( a decimal(10,2) , b int, k decimal(10,2), t text, p int) distributed by (a,b) partition by range(p)  ( start(0) end(10) every (2), default partition other);
create table bar ( a decimal(10, 2), b int, k decimal(10,2), t text, p int) distributed by(a);

insert into foo_p select i, i % 10, i , i || 'SOME NUMBER SOME NUMBER', i % 10 from generate_series(1, 1000) i;

insert into bar select i % 7, i % 6, i % 9, i || 'SOME NUMBER', i % 4 from generate_series(1, 100) i;
insert into bar select i % 7, i % 6, i % 9, i || 'SOME NUMBER', i % 4 from generate_series(1, 10000) i;
insert into bar select i % 7, i % 6, i % 9, i || 'SOME NUMBER', i % 4 from generate_series(1, 10000) i;

analyze foo_p;
analyze bar;

select qx_count_operator('select foo_p.k, foo_p.t from foo_p left outer join bar on foo_p.k = bar.k  where foo_p.t is not null and foo_p.p = 6;', 'Hash Left Join', 'Hash Left Join');
select foo_p.k, foo_p.t from foo_p left outer join bar on foo_p.k = bar.k  where foo_p.t is not null and foo_p.p = 6 order by 1, 2 desc limit 10;

select qx_count_operator('select foo_p.b, foo_p.t from foo_p left outer join bar on foo_p.a = bar.k  where foo_p.t is not null and foo_p.a = 6.00;', 'Hash Left Join', 'Hash Left Join');
select foo_p.b, foo_p.t from foo_p left outer join bar on foo_p.a = bar.k  where foo_p.t is not null and foo_p.a = 6.00 order by 1, 2 desc limit 10;

-- Partitioned tables with character type distribution keys used in predicates
-- start_ignore
drop table if exists abbp;
drop table if exists b;
-- end_ignore

create table abbp ( a character varying(60), b int, k character varying(60), t int, p int) distributed by (a,b) partition by range(p)  ( start(0) end(10) every (2), default partition other);
create table b ( a character varying(60), b int, k character varying(60), t int, p int) distributed by(a);

insert into abbp select i || 'SOME NUMBER', i, i || 'SN SN', i, i%10 from generate_series(1, 1000)i;

insert into b select i % 7 || 'SOME NUMBER', i%6, i % 9 || 'SOME NUMBER', i % 9, i % 4 from generate_series(1, 100)i;
insert into b select i % 7 || 'SOME NUMBER', i%6, i % 9 || 'SOME NUMBER', i % 9, i % 4 from generate_series(1, 1000)i;
insert into b select i % 7 || 'SOME NUMBER', i%6, i % 9 || 'SOME NUMBER', i % 9, i % 4 from generate_series(1, 1000)i;

analyze abbp;
analyze b;

select qx_count_operator('select abbp.k, abbp.t from abbp left outer join b on abbp.k = b.k  where abbp.t is not null and abbp.p = 6;', 'Hash Left Join', 'Hash Left Join');
select abbp.k, abbp.t from abbp left outer join b on abbp.k = b.k  where abbp.t is not null and abbp.p = 6 order by 1, 2 asc limit 10;

select qx_count_operator('select abbp.b, abbp.t from abbp left outer join b on abbp.a = b.k  where abbp.t is not null and abbp.a = E''6SOME NUMBER''', 'Hash Left Join', 'Hash Left Join');
select abbp.b, abbp.t from abbp left outer join b on abbp.a = b.k  where abbp.t is not null and abbp.a = '6SOME NUMBER' order by 1, 2 asc limit 10;

-- Partitioned tables on both sides of a join
-- start_ignore
drop table if exists foo_p;
drop table if exists bar_p;
-- end_ignore

create table foo_p( a int, b int, k int, t text, p int) distributed by (a,b) partition by range(p) ( start(0) end(10) every (2), default partition other);
create table bar_p( a int, b int, k int, t text, p int) distributed by (a,b) partition by range(p) ( start(0) end(10) every (2), default partition other);

insert into foo_p select i, i % 10, i , i || 'SOME NUMBER SOME NUMBER', i % 10 from generate_series(1, 1000) i;

insert into bar_p select i % 7, i % 6, i % 9, i || 'SOME NUMBER', i % 4 from generate_series(1, 100) i;
insert into bar_p select i % 7, i % 6, i % 9, i || 'SOME NUMBER', i % 4 from generate_series(1, 10000) i;
insert into bar_p select i % 7, i % 6, i % 9, i || 'SOME NUMBER', i % 4 from generate_series(1, 10000) i;

analyze foo_p;
analyze bar_p;

select qx_count_operator('select foo_p.k, foo_p.t from foo_p left outer join bar_p on foo_p.k = bar_p.k  where foo_p.t is not null and foo_p.p = 6;', 'Hash Left Join', 'Hash Left Join');
select foo_p.k, foo_p.t from foo_p left outer join bar_p on foo_p.k = bar_p.k  where foo_p.t is not null and foo_p.p = 6 order by 1, 2 desc limit 10;

select qx_count_operator('select foo_p.b, foo_p.t from foo_p left outer join bar_p on foo_p.a = bar_p.k  where foo_p.t is not null and foo_p.a = 6;', 'Hash Left Join', 'Hash Left Join');
select foo_p.b, foo_p.t from foo_p left outer join bar_p on foo_p.a = bar_p.k  where foo_p.t is not null and foo_p.a = 6 order by 1, 2 asc limit 10;

select qx_count_operator('select foo_p.b, foo_p.t from foo_p left outer join bar_p on foo_p.a = bar_p.k and foo_p.k = bar_p.k where foo_p.t is not null and foo_p.a = 6 and bar_p.a = 14;', 'Nested Loop', 'Hash Join');
select foo_p.b, foo_p.t from foo_p left outer join bar_p on foo_p.a = bar_p.k and foo_p.k = bar_p.k where foo_p.t is not null and foo_p.a = 6 and bar_p.a = 14 order by 1, 2 desc limit 10;

select qx_count_operator('select bar_p.a, foo_p.b, foo_p.t from foo_p left outer join bar_p on foo_p.a = bar_p.k and foo_p.k = bar_p.k where foo_p.t is not null and foo_p.a = 6;', 'Hash Left Join', 'Hash Left Join');
select bar_p.a, foo_p.b, foo_p.t from foo_p left outer join bar_p on foo_p.a = bar_p.k and foo_p.k = bar_p.k where foo_p.t is not null and foo_p.a = 6 order by 1, 2, 3 asc limit 10;

select qx_count_operator('select  foo_p.b, foo_p.t from foo_p left outer join bar_p on foo_p.a = bar_p.k and foo_p.k = bar_p.k where foo_p.t is not null and foo_p.a = 6 and bar_p.a = 4;', 'Nested Loop', 'Hash Join');
select  foo_p.b, foo_p.t from foo_p left outer join bar_p on foo_p.a = bar_p.k and foo_p.k = bar_p.k where foo_p.t is not null and foo_p.a = 6 and bar_p.a = 4 order by 1, 2 asc limit 10;

select qx_count_operator('select  foo_p.b, foo_p.t from foo_p left outer join bar_p on foo_p.a = bar_p.k and foo_p.k = bar_p.b where foo_p.t is not null and foo_p.a = 6 and bar_p.a = 4;', 'Hash Join', 'Hash Join');
select  foo_p.b, foo_p.t from foo_p left outer join bar_p on foo_p.a = bar_p.k and foo_p.k = bar_p.b where foo_p.t is not null and foo_p.a = 6 and bar_p.a = 4 order by 1, 2 asc limit 10;

select qx_count_operator('select  foo_p.b, foo_p.t from foo_p left outer join bar_p on foo_p.a = bar_p.k and foo_p.b = bar_p.b where foo_p.t is not null and foo_p.a = 6;', 'Hash Left Join', 'Hash Left Join');
select  foo_p.b, foo_p.t from foo_p left outer join bar_p on foo_p.a = bar_p.k and foo_p.b = bar_p.b where foo_p.t is not null and foo_p.a = 6 order by 1, 2 desc limit 10;

select qx_count_operator('select foo_p.b, foo_p.t from foo_p left outer join bar_p on foo_p.a = bar_p.a  where foo_p.t is not null and foo_p.a = 6;', 'Hash Left Join', 'Hash Left Join');
select foo_p.b, foo_p.t from foo_p left outer join bar_p on foo_p.a = bar_p.a  where foo_p.t is not null and foo_p.a = 6 order by 1, 2 asc limit 10;

-- Queries where equality predicate is not an immediate constant
-- start_ignore
drop table if exists foo_p;
drop table if exists bar;
-- end_ignore

create table foo_p( a int, b int, k int, t text, p int) distributed by (a,b) partition by range(p) ( start(0) end(10) every (2), default partition other);
create table bar( a int, b int, k int, t text, p int) distributed by (a);

insert into foo_p select i, i % 10, i , i || 'SOME NUMBER SOME NUMBER', i % 10 from generate_series(1, 1000) i;
insert into bar select i % 7, i % 6, i % 9, i || 'SOME NUMBER', i % 4 from generate_series(1, 100) i;
insert into bar select i % 7, i % 6, i % 9, i || 'SOME NUMBER', i % 4 from generate_series(1, 10000) i;
insert into bar select i % 7, i % 6, i % 9, i || 'SOME NUMBER', i % 4 from generate_series(1, 10000) i;

analyze foo_p;
analyze bar;

select qx_count_operator('select foo_p.b, foo_p.t from foo_p left outer join bar on foo_p.a = bar.k  where foo_p.t is not null and foo_p.a = (array[1])[1];', 'Hash Left Join', 'Hash Left Join');
select foo_p.b, foo_p.t from foo_p left outer join bar on foo_p.a = bar.k  where foo_p.t is not null and foo_p.a = (array[1])[1] order by 1, 2 desc limit 10;

create function mytest(integer) returns integer as 'select $1/100' language sql;
select qx_count_operator('select foo_p.b, foo_p.t from foo_p left outer join bar on foo_p.a = bar.k  where foo_p.t is not null and foo_p.a = mytest(100);', 'Hash Left Join', 'Hash Left Join');
select foo_p.b, foo_p.t from foo_p left outer join bar on foo_p.a = bar.k  where foo_p.t is not null and foo_p.a = mytest(100) order by 1, 2 asc limit 10;

drop function if exists mytest(integer);
-- Repeat test cases with inner join instead of inner join: C87849 (Queries with inner join)

-- Redistribute on top of Append with flow node
-- start_ignore
drop table if exists foo_p;
drop table if exists bar;
-- end_ignore

create table foo_p( a int, b int, k int, t text, p int) distributed by (a,b) partition by range(p) ( start(0) end(10) every (2), default partition other);

create table bar( a int, b int, k int, t text, p int) distributed by (a);

insert into foo_p select i, i % 10, i , i || 'SOME NUMBER SOME NUMBER', i % 10 from generate_series(1, 1000) i;

insert into bar select i % 7, i % 6, i % 9, i || 'SOME NUMBER', i % 4 from generate_series(1, 100) i;
insert into bar select i % 7, i % 6, i % 9, i || 'SOME NUMBER', i % 4 from generate_series(1, 10000) i;
insert into bar select i % 7, i % 6, i % 9, i || 'SOME NUMBER', i % 4 from generate_series(1, 10000) i;

analyze foo_p;

analyze bar;

select qx_count_operator('select foo_p.b, foo_p.t from foo_p inner join bar on foo_p.a = bar.k  where foo_p.t is not null and foo_p.a = 6;', 'Nested Loop', 'Hash Join');
select foo_p.b, foo_p.t from foo_p inner join bar on foo_p.a = bar.k  where foo_p.t is not null and foo_p.a = 6 order by 1, 2 desc limit 10;

select qx_count_operator('select foo_p.k, foo_p.t from foo_p inner join bar on foo_p.k = bar.k  where foo_p.t is not null and foo_p.p = 6;', 'Hash Join', 'Hash Join');
select foo_p.k, foo_p.t from foo_p inner join bar on foo_p.k = bar.k  where foo_p.t is not null and foo_p.p = 6 order by 1, 2 desc limit 10;

-- Varchar in the select list with redistribute on top of an append with flow node
-- start_ignore
drop table if exists a_p;
drop table if exists bar;
-- end_ignore
create table a_p ( a character varying(60), b character varying(60), k character varying(60), t int, p int) distributed by (a,b) partition by range(p)  ( start(0) end(10) every (2), default partition other);
create table b ( a character varying(60), b character varying(60), k character varying(60), t int, p int) distributed by(a);

insert into a_p select i || 'SOME NUMBER', i || 'SN', i || 'SN SN', i, i%10 from generate_series(1, 1000)i;

insert into b select i % 7 || 'SOME NUMBER', i%6 || 'SN', i % 9 || 'SOME NUMBER', i % 9, i % 4 from generate_series(1, 100)i;
insert into b select i % 7 || 'SOME NUMBER', i%6 || 'SN' , i % 9 || 'SOME NUMBER', i % 9, i % 4 from generate_series(1, 1000)i;
insert into b select i % 7 || 'SOME NUMBER', i%6 || 'SN' , i % 9 || 'SOME NUMBER', i % 9, i % 4 from generate_series(1, 1000)i;

analyze a_p;
analyze b;

select qx_count_operator('select a_p.k, a_p.t from a_p inner join b on a_p.k = b.k  where a_p.t is not null and a_p.p = 6;', 'Hash Join', 'Hash Join');
select a_p.k, a_p.t from a_p inner join b on a_p.k = b.k  where a_p.t is not null and a_p.p = 6 order by 1, 2 desc limit 10;

select qx_count_operator('select a_p.b, a_p.t from a_p inner join b on a_p.a = b.k  where a_p.t is not null and a_p.a = E''6SOME NUMBER''', 'Nested Loop', 'Hash Join');
select a_p.b, a_p.t from a_p inner join b on a_p.a = b.k  where a_p.t is not null and a_p.a = '6SOME NUMBER' order by 1, 2 desc limit 10;

-- Queries without motion node on the partitioned table
-- start_ignore
drop table if exists a_p;
drop table if exists b;
-- end_ignore

create table a_p( a int, b int, k int, t text, p int) distributed by (a,b) partition by range(p) ( start(0) end(10) every (2), default partition other);
create table b (a int, b int, k int, t text, p int) distributed by (a);

insert into a_p select i, i, i , i || 'SOME NUMBER SOME NUMBER', i % 10 from generate_series(1, 1000) i;

insert into b select i%7, i%10, i , i || 'SOME NUMBER', i % 4 from generate_series(1, 100) i;
insert into b select i%7, i%10, i , i || 'SOME NUMBER', i % 4 from generate_series(1, 1000) i;
insert into b select i%7, i%10, i , i || 'SOME NUMBER', i % 4 from generate_series(1, 1000) i;

analyze a_p;
analyze b;

select qx_count_operator('select a_p.k, a_p.t from a_p inner join b on a_p.k = b.k where a_p.t is not null and a_p.p = 6;', 'Hash Join', 'Hash Join');
select a_p.k, a_p.t from a_p inner join b on a_p.k = b.k where a_p.t is not null and a_p.p = 6 order by 1, 2 desc limit 10;

select qx_count_operator('select a_p.b, a_p.t from a_p inner join b on a_p.a = b.k where a_p.t is not null and a_p.a = 6;', 'Nested Loop', 'Hash Join');
select a_p.b, a_p.t from a_p inner join b on a_p.a = b.k where a_p.t is not null and a_p.a = 6 order by 1, 2 desc limit 10;

-- Partitioned tables with decimal type distribution keys
-- start_ignore
drop table if exists foo_p;
drop table if exists bar;
-- end_ignore

create table foo_p ( a decimal(10,2) , b int, k decimal(10,2), t text, p int) distributed by (a,b) partition by range(p)  ( start(0) end(10) every (2), default partition other);
create table bar ( a decimal(10, 2), b int, k decimal(10,2), t text, p int) distributed by(a);

insert into foo_p select i, i % 10, i , i || 'SOME NUMBER SOME NUMBER', i % 10 from generate_series(1, 1000) i;

insert into bar select i % 7, i % 6, i % 9, i || 'SOME NUMBER', i % 4 from generate_series(1, 100) i;
insert into bar select i % 7, i % 6, i % 9, i || 'SOME NUMBER', i % 4 from generate_series(1, 10000) i;
insert into bar select i % 7, i % 6, i % 9, i || 'SOME NUMBER', i % 4 from generate_series(1, 10000) i;

analyze foo_p;
analyze bar;

select qx_count_operator('select foo_p.k, foo_p.t from foo_p inner join bar on foo_p.k = bar.k  where foo_p.t is not null and foo_p.p = 6;', 'Hash Join', 'Hash Join');
select foo_p.k, foo_p.t from foo_p inner join bar on foo_p.k = bar.k  where foo_p.t is not null and foo_p.p = 6 order by 1, 2 desc limit 10;

select qx_count_operator('select foo_p.b, foo_p.t from foo_p inner join bar on foo_p.a = bar.k  where foo_p.t is not null and foo_p.a = 6.00;', 'Nested Loop', 'Hash Join');
select foo_p.b, foo_p.t from foo_p inner join bar on foo_p.a = bar.k  where foo_p.t is not null and foo_p.a = 6.00 order by 1, 2 desc limit 10;

-- Partitioned tables with character type distribution keys used in predicates
-- start_ignore
drop table if exists a_p;
drop table if exists b;
-- end_ignore

create table a_p ( a character varying(60), b int, k character varying(60), t int, p int) distributed by (a,b) partition by range(p)  ( start(0) end(10) every (2), default partition other);
create table b ( a character varying(60), b int, k character varying(60), t int, p int) distributed by(a);

insert into a_p select i || 'SOME NUMBER', i, i || 'SN SN', i, i%10 from generate_series(1, 1000)i;

insert into b select i % 7 || 'SOME NUMBER', i%6, i % 9 || 'SOME NUMBER', i % 9, i % 4 from generate_series(1, 100)i;
insert into b select i % 7 || 'SOME NUMBER', i%6, i % 9 || 'SOME NUMBER', i % 9, i % 4 from generate_series(1, 1000)i;
insert into b select i % 7 || 'SOME NUMBER', i%6, i % 9 || 'SOME NUMBER', i % 9, i % 4 from generate_series(1, 1000)i;

analyze a_p;
analyze b;

select qx_count_operator('select a_p.k, a_p.t from a_p inner join b on a_p.k = b.k  where a_p.t is not null and a_p.p = 6;', 'Hash Join', 'Hash Join');
select a_p.k, a_p.t from a_p inner join b on a_p.k = b.k  where a_p.t is not null and a_p.p = 6 order by 1, 2 asc limit 10;

select qx_count_operator('select a_p.b, a_p.t from a_p inner join b on a_p.a = b.k  where a_p.t is not null and a_p.a = E''6SOME NUMBER''', 'Nested Loop', 'Hash Join');
select a_p.b, a_p.t from a_p inner join b on a_p.a = b.k  where a_p.t is not null and a_p.a = '6SOME NUMBER' order by 1, 2 asc limit 10;

-- Partitioned tables on both sides of a join
-- start_ignore
drop table if exists foo_p;
drop table if exists bar_p;
-- end_ignore

create table foo_p( a int, b int, k int, t text, p int) distributed by (a,b) partition by range(p) ( start(0) end(10) every (2), default partition other);
create table bar_p( a int, b int, k int, t text, p int) distributed by (a,b) partition by range(p) ( start(0) end(10) every (2), default partition other);

insert into foo_p select i, i % 10, i , i || 'SOME NUMBER SOME NUMBER', i % 10 from generate_series(1, 1000) i;

insert into bar_p select i % 7, i % 6, i % 9, i || 'SOME NUMBER', i % 4 from generate_series(1, 100) i;
insert into bar_p select i % 7, i % 6, i % 9, i || 'SOME NUMBER', i % 4 from generate_series(1, 10000) i;
insert into bar_p select i % 7, i % 6, i % 9, i || 'SOME NUMBER', i % 4 from generate_series(1, 10000) i;

analyze foo_p;
analyze bar_p;

select qx_count_operator('select foo_p.k, foo_p.t from foo_p inner join bar_p on foo_p.k = bar_p.k  where foo_p.t is not null and foo_p.p = 6;', 'Hash Join', 'Hash Join');
select foo_p.k, foo_p.t from foo_p inner join bar_p on foo_p.k = bar_p.k  where foo_p.t is not null and foo_p.p = 6 order by 1, 2 desc limit 10;

select qx_count_operator('select foo_p.b, foo_p.t from foo_p inner join bar_p on foo_p.a = bar_p.k  where foo_p.t is not null and foo_p.a = 6;', 'Nested Loop', 'Hash Join');
select foo_p.b, foo_p.t from foo_p inner join bar_p on foo_p.a = bar_p.k  where foo_p.t is not null and foo_p.a = 6 order by 1, 2 desc limit 10;

select qx_count_operator('select foo_p.b, foo_p.t from foo_p inner join bar_p on foo_p.a = bar_p.k and foo_p.k = bar_p.k where foo_p.t is not null and foo_p.a = 6 and bar_p.a = 14;', 'Nested Loop', 'Hash Join');
select foo_p.b, foo_p.t from foo_p inner join bar_p on foo_p.a = bar_p.k and foo_p.k = bar_p.k where foo_p.t is not null and foo_p.a = 6 and bar_p.a = 14 order by 1, 2 asc limit 10;

select qx_count_operator('select bar_p.a, foo_p.b, foo_p.t from foo_p inner join bar_p on foo_p.a = bar_p.k and foo_p.k = bar_p.k where foo_p.t is not null and foo_p.a = 6;', 'Nested Loop', 'Hash Join');
select bar_p.a, foo_p.b, foo_p.t from foo_p inner join bar_p on foo_p.a = bar_p.k and foo_p.k = bar_p.k where foo_p.t is not null and foo_p.a = 6 order by 1, 2, 3 desc limit 10;

select qx_count_operator('select  foo_p.b, foo_p.t from foo_p inner join bar_p on foo_p.a = bar_p.k and foo_p.k = bar_p.k where foo_p.t is not null and foo_p.a = 6 and bar_p.a = 4;', 'Nested Loop', 'Hash Join');
select  foo_p.b, foo_p.t from foo_p inner join bar_p on foo_p.a = bar_p.k and foo_p.k = bar_p.k where foo_p.t is not null and foo_p.a = 6 and bar_p.a = 4 order by 1, 2 asc limit 10;

select qx_count_operator('select  foo_p.b, foo_p.t from foo_p inner join bar_p on foo_p.a = bar_p.k and foo_p.k = bar_p.b where foo_p.t is not null and foo_p.a = 6 and bar_p.a = 4;', 'Hash Join', 'Hash Join');
select  foo_p.b, foo_p.t from foo_p inner join bar_p on foo_p.a = bar_p.k and foo_p.k = bar_p.b where foo_p.t is not null and foo_p.a = 6 and bar_p.a = 4 order by 1, 2 desc limit 10;

select qx_count_operator('select  foo_p.b, foo_p.t from foo_p inner join bar_p on foo_p.a = bar_p.k and foo_p.b = bar_p.b where foo_p.t is not null and foo_p.a = 6;', 'Hash Join', 'Hash Join');
select  foo_p.b, foo_p.t from foo_p inner join bar_p on foo_p.a = bar_p.k and foo_p.b = bar_p.b where foo_p.t is not null and foo_p.a = 6 order by 1, 2 desc limit 10;

select qx_count_operator('select foo_p.b, foo_p.t from foo_p inner join bar_p on foo_p.a = bar_p.a  where foo_p.t is not null and foo_p.a = 6;', 'Nested Loop', 'Hash Join');
select foo_p.b, foo_p.t from foo_p inner join bar_p on foo_p.a = bar_p.a  where foo_p.t is not null and foo_p.a = 6 order by 1, 2 asc limit 10;

select qx_count_operator('select foo_p.b, foo_p.t from foo_p inner join bar_p on foo_p.a = bar_p.a  where foo_p.t is not null and foo_p.a = 6;', 'Nested Loop', 'Hash Join'); 
select foo_p.b, foo_p.t from foo_p inner join bar_p on foo_p.a = bar_p.a  where foo_p.t is not null and foo_p.a = 6 order by 1, 2 desc limit 10;

-- Queries where equality predicate is not an immediate constant
-- start_ignore
drop table if exists foo_p;
drop table if exists bar;
-- end_ignore

create table foo_p( a int, b int, k int, t text, p int) distributed by (a,b) partition by range(p) ( start(0) end(10) every (2), default partition other);
create table bar( a int, b int, k int, t text, p int) distributed by (a);

insert into foo_p select i, i % 10, i , i || 'SOME NUMBER SOME NUMBER', i % 10 from generate_series(1, 1000) i;
insert into bar select i % 7, i % 6, i % 9, i || 'SOME NUMBER', i % 4 from generate_series(1, 100) i;
insert into bar select i % 7, i % 6, i % 9, i || 'SOME NUMBER', i % 4 from generate_series(1, 10000) i;
insert into bar select i % 7, i % 6, i % 9, i || 'SOME NUMBER', i % 4 from generate_series(1, 10000) i;

analyze foo_p;
analyze bar;

select qx_count_operator('select foo_p.b, foo_p.t from foo_p inner join bar on foo_p.a = bar.k  where foo_p.t is not null and foo_p.a = (array[1])[1];', 'Nested Loop', 'Hash Join'); 
select foo_p.b, foo_p.t from foo_p inner join bar on foo_p.a = bar.k  where foo_p.t is not null and foo_p.a = (array[1])[1] order by 1, 2 asc limit 10;

create function mytest(integer) returns integer as 'select $1/100' language sql;
select qx_count_operator('select foo_p.b, foo_p.t from foo_p inner join bar on foo_p.a = bar.k  where foo_p.t is not null and foo_p.a = mytest(100);', 'Nested Loop', 'Hash Join'); 
select foo_p.b, foo_p.t from foo_p inner join bar on foo_p.a = bar.k  where foo_p.t is not null and foo_p.a = mytest(100) order by 1, 2 desc limit 10;

drop function if exists mytest(integer);
