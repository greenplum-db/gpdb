drop table if exists testswapna;
create table testswapna (a integer);
insert into testswapna  select a from generate_series(1,10) a;

select * from testswapna  where a>2;

drop table testswapna;


create table testswapna (a integer);
insert into testswapna  select a from generate_series(1,10) a;
alter table testswapna  add  COLUMN b integer;
insert into testswapna select b from generate_series(11,20) b;

select a,b from testswapna;

drop table testswapna;


create table testswapna (a integer);
insert into testswapna  select a from generate_series(1,10) a;
ALTER table testswapna ALTER COLUMN a SET NOT NULL;


select * from testswapna;

drop table testswapna;

create table testswapna (id  integer,rank integer,gender character(1),count integer) DISTRIBUTED BY rank,gender) ;
insert into testswapna  values(1,101,'F',10);




select * from testswapna;

drop table testswapna;


BEGIN
create table testswapna (id  integer,rank integer,gender character(1)) DISTRIBUTED BY rank,gender) ;
insert into testswapna  values(1,101,'F');




\echo -- start_ignore
select * from testswapna;
\echo -- end_ignore

drop table testswapna;

COMMIT

BEGIN
create table testswapna (id  integer,rank integer,gender character(1)) DISTRIBUTED BY rank,gender);
SAVEPOINT my_savepoint;
insert into testswapna  values(1,101,'F');
ROLLBACK TO SAVEPOINT my_savepoint;
insert into testswapna values(2,201,'M');





\echo -- start_ignore
select * from testswapna;
\echo -- end_ignore

drop table testswapna;

COMMIT

CREATE DATABASE lusiadas;
ALTER DATABASE lusiadas RENAME TO lusiadas1;

DROP DATABASE lusiadas1;

CREATE DOMAIN country_code char(2) NOT NULL;
ALTER DOMAIN country_code DROP NOT NULL;



DROP DOMAIN country_code;

CREATE FUNCTION addlab1111(integer,integer) RETURNS integer
AS 'select $1 + $2;'
LANGUAGE SQL 
IMMUTABLE
CONTAINS SQL
RETURNS NULL ON NULL INPUT;

ALTER FUNCTION addlab1111(integer,integer) RENAME TO addition111;


DROP FUNCTION addition111(integer,integer);

CREATE USER jonathan11 WITH PASSWORD 'abc1';
CREATE USER jonathan12 WITH PASSWORD 'abc2';

ALTER USER jonathan11 RENAME TO jona11;
ALTER USER jonathan12 RENAME TO jona12;

DROP USER jona11;
DROP USER jona12;

REATE USER jonathan11 WITH PASSWORD 'abc1';
CREATE USER jonathan12 WITH PASSWORD 'abc2';

ALTER USER jonathan11 RENAME TO jona11;
ALTER USER jonathan12 RENAME TO jona12;

CREATE GROUP marketing WITH USER jona11,jona12;

ALTER GROUP marketing RENAME TO market;

DROP GROUP market;
DROP USER jona11;
DROP USER jona12;

ALTER LANGUAGE plpgsql  RENAME TO plsamples;
ALTER LANGUAGE plsamples  RENAME TO plpgsql;
-- commentting the drop in the test case. Since jetpack is made implicit we cannot drop language plpgsql
-- DROP LANGUAGE plsamples;

select unnest('{}'::text[]);

drop table mpp_r6756 cascade; --ignore
drop table mpp_s6756 cascade; --ignore

create table mpp_r6756 ( a int, b int, x int, y int ) 
    distributed randomly
    partition by list(a) (
        values (0), 
        values (1)
        );

create table mpp_s6756 ( c int, d int, e int ) 
    distributed randomly;

insert into mpp_s6756 values 
    (0,0,0),(0,0,1),(0,1,0),(0,1,1),(1,0,0),(1,0,1),(1,1,0),(1,1,1);

insert into mpp_r6756 values
    (0, 0, 1, 1),
    (0, 1, 2, 2),
    (0, 1, 2, 2),
    (1, 0, 3, 3),
    (1, 0, 3, 3),
    (1, 0, 3, 3),
    (1, 1, 4, 4),
    (1, 1, 4, 4),
    (1, 1, 4, 4),
    (1, 1, 4, 4);

-- start_equiv
select a, b, count(distinct x), count(distinct y)
from mpp_r6756 r, mpp_s6756 c, mpp_s6756 d, mpp_s6756 e
where r.a = c.c and r.a = d.d and r.a = e.e
group by a, b
union all
select a, null, count(distinct x), count(distinct y)
from mpp_r6756 r, mpp_s6756 c, mpp_s6756 d, mpp_s6756 e
where r.a = c.c and r.a = d.d and r.a = e.e
group by a
union all
select null, null, count(distinct x), count(distinct y)
from mpp_r6756 r, mpp_s6756 c, mpp_s6756 d, mpp_s6756 e
where r.a = c.c and r.a = d.d and r.a = e.e;

select a, b, count(distinct x), count(distinct y)
from mpp_r6756 r, mpp_s6756 c, mpp_s6756 d, mpp_s6756 e
where r.a = c.c and r.a = d.d and r.a = e.e
group by rollup (a,b);
--end_equiv

drop table mpp_r6756 cascade; --ignore
drop table mpp_s6756 cascade; --ignore

--start_ignore
drop table if exists t1;
--end_ignore

set gp_autostats_mode='none';

create table t1 (i int, t text);

COPY t1 (i, t) FROM stdin;
0	- DW60\n- Version:\nPostgreSQL 8.2.4 (Greenplum Database Release-3_1_0_0-alpha1-branch build dev) on i386-pc-solaris2.10, compiled by GCC gcc.exe (GCC) 4.1.1 compiled on Oct  1 2007 01:01:01
0	- DW80\n- prodgp=# SELECT version();\n                                                                      version                                                                       \n----------------------------------------------------------------------------------------------------------------------------------------------------\n PostgreSQL 8.2.6 (Greenplum Database 3.1.1.5 build 2) on i386-pc-solaris2.10, compiled by GCC gcc.exe (GCC) 4.1.1 compiled on Jul 30 2008 16:58:44\n(1 row)
0	- DW80\n- Version\n\nprodgp=# SELECT version();\n                                                                      version                                                                       \n----------------------------------------------------------------------------------------------------------------------------------------------------\n PostgreSQL 8.2.6 (Greenplum Database 3.1.1.5 build 2) on i386-pc-solaris2.10, compiled by GCC gcc.exe (GCC) 4.1.1 compiled on Jul 30 2008 16:58:44\n
0	- DW80\n- Version\n\nprodgp=# SELECT version();\n                                                                      version                                                                       \n----------------------------------------------------------------------------------------------------------------------------------------------------\n PostgreSQL 8.2.6 (Greenplum Database 3.1.1.5 build 2) on i386-pc-solaris2.10, compiled by GCC gcc.exe (GCC) 4.1.1 compiled on Jul 30 2008 16:58:44\n
0	- DW80 \n- Version: \nprodgp=# SELECT version(); \n                                                                      version \n---------------------------------------------------------------------------------------------------------------------------------------------------- \n PostgreSQL 8.2.6 (Greenplum Database 3.1.1.5 build 2) on i386-pc-solaris2.10, compiled by GCC gcc.exe (GCC) 4.1.1 compiled on Jul 30 2008 16:58:44 \n(1 row) \n
0	- DW80\n- Version:\nprodgp=# SELECT version();\n                                                                      version                                                                       \n----------------------------------------------------------------------------------------------------------------------------------------------------\n PostgreSQL 8.2.6 (Greenplum Database 3.1.1.5 build 2) on i386-pc-solaris2.10, compiled by GCC gcc.exe (GCC) 4.1.1 compiled on Jul 30 2008 16:58:44\n(1 row)\n\nTime: 2.313 ms
0	- DW80\n- Version:\nprodgp=# SELECT version();\n                                                                      version                                                                       \n----------------------------------------------------------------------------------------------------------------------------------------------------\n PostgreSQL 8.2.6 (Greenplum Database 3.1.1.8 build 2) on i386-pc-solaris2.10, compiled by GCC gcc.exe (GCC) 4.1.1 compiled on Sep 22 2008 17:36:18\n(1 row)\n
1	- DW80\n- prodgp=# SELECT version();\n                                                                      version                                                                       \n----------------------------------------------------------------------------------------------------------------------------------------------------\n PostgreSQL 8.2.6 (Greenplum Database 3.1.1.5 build 2) on i386-pc-solaris2.10, compiled by GCC gcc.exe (GCC) 4.1.1 compiled on Jul 30 2008 16:58:44\n(1 row)
1	- DW80\n- Version\n\nprodgp=# SELECT version();\n                                                                      version                                                                       \n----------------------------------------------------------------------------------------------------------------------------------------------------\n PostgreSQL 8.2.6 (Greenplum Database 3.1.1.5 build 2) on i386-pc-solaris2.10, compiled by GCC gcc.exe (GCC) 4.1.1 compiled on Jul 30 2008 16:58:44\n
1	- DW80\n- Version:\nprodgp=# SELECT version();\n                                                                      version                                                                       \n----------------------------------------------------------------------------------------------------------------------------------------------------\n PostgreSQL 8.2.6 (Greenplum Database 3.1.1.5 build 2) on i386-pc-solaris2.10, compiled by GCC gcc.exe (GCC) 4.1.1 compiled on Jul 30 2008 16:58:44\n(1 row)
1	- DW80\n- Version:\nprodgp=# SELECT version();\n                                                                      version                                                                       \n----------------------------------------------------------------------------------------------------------------------------------------------------\n PostgreSQL 8.2.6 (Greenplum Database 3.1.1.5 build 2) on i386-pc-solaris2.10, compiled by GCC gcc.exe (GCC) 4.1.1 compiled on Jul 30 2008 16:58:44\n(1 row)
1	- DW80\n- Version:\nprodgp=# SELECT version();\n                                                                      version                                                                       \n----------------------------------------------------------------------------------------------------------------------------------------------------\n PostgreSQL 8.2.6 (Greenplum Database 3.1.1.5 build 2) on i386-pc-solaris2.10, compiled by GCC gcc.exe (GCC) 4.1.1 compiled on Jul 30 2008 16:58:44\n(1 row)
1	- DW80\n- Version:\nprodgp=# SELECT version();\n                                                                      version                                                                       \n----------------------------------------------------------------------------------------------------------------------------------------------------\n PostgreSQL 8.2.6 (Greenplum Database 3.1.1.5 build 2) on i386-pc-solaris2.10, compiled by GCC gcc.exe (GCC) 4.1.1 compiled on Jul 30 2008 16:58:44\n(1 row)
1	- DW80\n- Version:\nprodgp=# SELECT version();\n                                                                      version                                                                       \n----------------------------------------------------------------------------------------------------------------------------------------------------\n PostgreSQL 8.2.6 (Greenplum Database 3.1.1.5 build 2) on i386-pc-solaris2.10, compiled by GCC gcc.exe (GCC) 4.1.1 compiled on Jul 30 2008 16:58:44\n(1 row)\n\nTime: 2.313 ms
1	- DW80\n- Version:\nprodgp=# SELECT version();\n                                                                      version                                                                       \n----------------------------------------------------------------------------------------------------------------------------------------------------\n PostgreSQL 8.2.6 (Greenplum Database 3.1.1.5 build 2) on i386-pc-solaris2.10, compiled by GCC gcc.exe (GCC) 4.1.1 compiled on Jul 30 2008 16:58:44\n(1 row)\n\nTime: 2.313 ms
\.


select * from t1 order by t;

