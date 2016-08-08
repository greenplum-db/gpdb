drop database if exists attrdef_db;
create database attrdef_db;
\c attrdef_db

create or replace function verify(varchar) returns bigint as
$$
        select count(distinct(foo.oid)) from (
               (select oid from pg_attrdef
               where adrelid = $1::regclass)
               union
               (select oid from gp_dist_random('pg_attrdef')
               where adrelid = $1::regclass)) foo;
$$ language sql;

-- Table with defaults
drop table if exists t cascade;
create table t(a int, b int default 0);

-- Inherits
drop table if exists t_inherits;
create table t_inherits(a int, b int) inherits(t);
select verify('t_inherits');

drop table if exists t_like;
create table t_like (like t including defaults);
select verify('t_like');

-- Add column
alter table t add column c int default(10);
select verify('t');

drop table if exists ao_t;
create table ao_t(a int, b int default 0) with(appendonly=true);
alter table ao_t add column c varchar default 'xyz';
select verify('ao_t');

drop table if exists co_t;
create table co_t(a int, b int default 0) with(appendonly=true, orientation=column);
alter table co_t add column c varchar default 'xyz';
select verify('co_t');

-- Alter column
alter table t alter column c set default(7);
select verify('t');

alter table ao_t alter column c set default('abc');
select verify('ao_t');

alter table co_t alter column c set default('abc');
select verify('co_t');

-- Serial type
drop table if exists t_serial;
create table t_serial(a serial, b varchar default 'xyz');
select verify('t_serial');

drop table if exists ao_serial;
create table ao_serial(a serial, b varchar default 'xyz') with(appendonly=true);
select verify('ao_serial');

drop table if exists co_serial;
create table co_serial(a serial, b varchar default 'xyz') with(appendonly=true, orientation=column);
select verify('co_serial');


-- Alter Type
alter table co_t alter column b type bigint;
select verify('co_t');

-- Partitioned tables
set client_min_messages='error';

CREATE TABLE pt1 (id int, date date, amt decimal(10,2) default 0.0) DISTRIBUTED BY (id)
PARTITION BY RANGE (date)
      (PARTITION Jan08 START (date '2008-01-01') INCLUSIVE ,
      PARTITION Feb08 START (date '2008-02-01') INCLUSIVE ,
      PARTITION Mar08 START (date '2008-03-01') INCLUSIVE
      END (date '2008-04-01') EXCLUSIVE);
select verify('pt1');
select verify('pt1_1_prt_jan08');
select verify('pt1_1_prt_feb08');
select verify('pt1_1_prt_mar08');

set gp_default_storage_options='appendonly=true';

CREATE TABLE ao_pt1 (id int, date date, amt decimal(10,2) default 0.0) DISTRIBUTED BY (id)
PARTITION BY RANGE (date)
      (PARTITION Jan08 START (date '2008-01-01') INCLUSIVE ,
      PARTITION Feb08 START (date '2008-02-01') INCLUSIVE ,
      PARTITION Mar08 START (date '2008-03-01') INCLUSIVE
      END (date '2008-04-01') EXCLUSIVE);
select verify('ao_pt1');
select verify('ao_pt1_1_prt_jan08');
select verify('ao_pt1_1_prt_feb08');
select verify('ao_pt1_1_prt_mar08');

CREATE TABLE co_pt1 (id int, year int, month int CHECK (month > 0),
       day int CHECK (day > 0), region text default('abc'))
WITH(ORIENTATION=COLUMN)
DISTRIBUTED BY (id)
PARTITION BY RANGE (year)
      SUBPARTITION BY RANGE (month)
      SUBPARTITION TEMPLATE (
      START (1) END (5) EVERY (1),
      DEFAULT SUBPARTITION other_months )
( START (2001) END (2003) EVERY (1),
DEFAULT PARTITION outlying_years );
select verify('co_pt1');
select verify('co_pt1_1_prt_3');
select verify('co_pt1_1_prt_2_2_prt_3');

set client_min_messages='notice';

-- Multiple Alter Table subcommands
alter table co_pt1 alter column month set default 3,
      	    	   add column foo int default 1;
select verify('co_pt1');

alter table ao_pt1 add default partition other_regions,
      	    	   alter column amt set not null;
select verify('ao_pt1');

