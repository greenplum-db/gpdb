-- In GPDB, it's important that every object (table, index, etc.) has the same
-- OID in the master and in all the segments.

--
-- pg_attrdef
--
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
create table t_with_default(a int, b int default 0);

-- Inherits
create table t_inherits(a int, b int) inherits(t_with_default);
select verify('t_inherits');

create table t_like (like t_with_default including defaults);
select verify('t_like');

-- Add column
alter table t_with_default add column c int default(10);
select verify('t_with_default');

create table ao_t(a int, b int default 0) with(appendonly=true);
alter table ao_t add column c varchar default 'xyz';
select verify('ao_t');

create table co_t(a int, b int default 0) with(appendonly=true, orientation=column);
alter table co_t add column c varchar default 'xyz';
select verify('co_t');

-- Alter column
alter table t_with_default alter column c set default(7);
select verify('t_with_default');

alter table ao_t alter column c set default('abc');
select verify('ao_t');

alter table co_t alter column c set default('abc');
select verify('co_t');

-- Serial type
create table t_serial(a serial, b varchar default 'xyz');
select verify('t_serial');

create table ao_serial(a serial, b varchar default 'xyz') with(appendonly=true);
select verify('ao_serial');

create table co_serial(a serial, b varchar default 'xyz') with(appendonly=true, orientation=column);
select verify('co_serial');


-- Alter Type
alter table co_t alter column b type bigint;
select verify('co_t');

-- Partitioned tables
set client_min_messages='error';

CREATE TABLE oid_check_pt1 (id int, date date, amt decimal(10,2) default 0.0)
USING heap
DISTRIBUTED BY (id)
PARTITION BY RANGE (date)
      (PARTITION Jan08 START (date '2008-01-01') INCLUSIVE ,
      PARTITION Feb08 START (date '2008-02-01') INCLUSIVE ,
      PARTITION Mar08 START (date '2008-03-01') INCLUSIVE
      END (date '2008-04-01') EXCLUSIVE);
select verify('oid_check_pt1');
select verify('oid_check_pt1_1_prt_jan08');
select verify('oid_check_pt1_1_prt_feb08');
select verify('oid_check_pt1_1_prt_mar08');

CREATE TABLE oid_check_ao_pt1 (id int, date date, amt decimal(10,2) default 0.0)
USING ao_row
DISTRIBUTED BY (id)
PARTITION BY RANGE (date)
      (PARTITION Jan08 START (date '2008-01-01') INCLUSIVE ,
      PARTITION Feb08 START (date '2008-02-01') INCLUSIVE ,
      PARTITION Mar08 START (date '2008-03-01') INCLUSIVE
      END (date '2008-04-01') EXCLUSIVE);
select verify('oid_check_ao_pt1');
select verify('oid_check_ao_pt1_1_prt_jan08');
select verify('oid_check_ao_pt1_1_prt_feb08');
select verify('oid_check_ao_pt1_1_prt_mar08');

CREATE TABLE oid_check_co_pt1 (id int, year int, month int CHECK (month > 0),
       day int CHECK (day > 0), region text default('abc'))
USING ao_column
DISTRIBUTED BY (id)
PARTITION BY RANGE (year)
      SUBPARTITION BY RANGE (month)
      SUBPARTITION TEMPLATE (
      START (1) END (5) EVERY (1),
      DEFAULT SUBPARTITION other_months )
( START (2001) END (2003) EVERY (1),
DEFAULT PARTITION outlying_years );
select verify('oid_check_co_pt1');
select verify('oid_check_co_pt1_1_prt_3');
select verify('oid_check_co_pt1_1_prt_2_2_prt_3');

set client_min_messages='notice';

-- Multiple Alter Table subcommands
alter table oid_check_co_pt1 alter column month set default 3,
      	    	   add column foo int default 1;
select verify('oid_check_co_pt1');

alter table oid_check_ao_pt1 add default partition other_regions,
      	    	   alter column amt set not null;
select verify('oid_check_ao_pt1');

--
-- pg_constraint
--
create or replace function verify(varchar) returns bigint as
$$
        select count(distinct(foo.oid)) from (
               (select oid from pg_constraint
               where conrelid = $1::regclass)
               union
               (select oid from gp_dist_random('pg_constraint')
               where conrelid = $1::regclass)) foo;
$$ language sql;

CREATE TABLE constraint_pt1 (id int, date date, amt decimal(10,2),
  CONSTRAINT amt_check CHECK (amt > 0)) DISTRIBUTED BY (id)
PARTITION BY RANGE (date)
      (PARTITION Jan08 START (date '2008-01-01') INCLUSIVE ,
      PARTITION Feb08 START (date '2008-02-01') INCLUSIVE ,
      PARTITION Mar08 START (date '2008-03-01') INCLUSIVE
      END (date '2008-04-01') EXCLUSIVE);
CREATE TABLE constraint_t1 (id int, date date, amt decimal(10,2),
  CONSTRAINT amt_check CHECK (amt > 0)) DISTRIBUTED BY (id);

INSERT INTO constraint_pt1 SELECT i, '2008-01-13', i FROM generate_series(1,5)i;
INSERT INTO constraint_pt1 SELECT i, '2008-02-13', i FROM generate_series(1,5)i;
INSERT INTO constraint_pt1 SELECT i, '2008-03-13', i FROM generate_series(1,5)i;
INSERT INTO constraint_t1 SELECT i, '2008-02-02', i FROM generate_series(11,15)i;
ANALYZE constraint_pt1;
ALTER TABLE constraint_pt1 EXCHANGE PARTITION Feb08 WITH TABLE constraint_t1;

select verify('constraint_pt1_1_prt_feb08');
select verify('constraint_t1');

SELECT * FROM constraint_pt1 ORDER BY date, id;

ALTER TABLE constraint_pt1 ALTER COLUMN amt SET DEFAULT 67,
 EXCHANGE PARTITION Feb08 WITH TABLE constraint_t1;

select verify('constraint_pt1_1_prt_feb08');

SELECT * FROM constraint_t1 ORDER BY date, id;
SELECT * FROM constraint_pt1 ORDER BY date, id;

-- exchange with appendonly
CREATE TABLE constraint_pt2 (id int, date date, amt decimal(10,2)
       CONSTRAINT amt_check CHECK (amt > 0)) DISTRIBUTED BY (id)
PARTITION BY RANGE (date)
      (PARTITION Jan08 START (date '2008-01-01') INCLUSIVE,
      PARTITION Feb08 START (date '2008-02-01') INCLUSIVE,
      PARTITION Mar08 START (date '2008-03-01') INCLUSIVE
      END (date '2008-04-01') EXCLUSIVE);

CREATE TABLE constraint_t2 (id int, date date, amt decimal(10,2),
       CONSTRAINT amt_check CHECK (amt > 0))
       WITH (appendonly=true, orientation=column) DISTRIBUTED BY (id);

INSERT INTO constraint_pt2 SELECT i, '2008-01-13', i FROM generate_series(1,5)i;
INSERT INTO constraint_pt2 SELECT i, '2008-01-20', i FROM generate_series(11,15)i;
INSERT INTO constraint_pt2 SELECT i, '2008-02-13', i FROM generate_series(1,5)i;
INSERT INTO constraint_pt2 SELECT i, '2008-03-13', i FROM generate_series(1,5)i;
INSERT INTO constraint_t2 SELECT i, '2008-02-02', i FROM generate_series(11,15)i;

-- split and exchange
ALTER TABLE constraint_pt2 EXCHANGE PARTITION Feb08 WITH TABLE constraint_t2,
      SPLIT PARTITION FOR ('2008-01-01') AT ('2008-01-16') INTO
       (PARTITION jan08_15, PARTITION jan08_31);

ANALYZE constraint_pt2;

select verify('constraint_pt2_1_prt_feb08');
select verify('constraint_t2');

SELECT * FROM constraint_pt2 ORDER BY date, id;
SELECT * FROM constraint_t2 ORDER BY date, id;

CREATE TABLE constraint_t3 (id int, date date, amt decimal(10,2)
       CONSTRAINT amt_check CHECK (amt > 0))
       WITH (appendonly=true, orientation=column) DISTRIBUTED BY (id);
INSERT INTO constraint_t3 SELECT i, '2008-03-02', i FROM generate_series(11,15)i;

-- add, rename and exchange
ALTER TABLE constraint_pt2 ADD PARTITION START (date '2009-02-01') INCLUSIVE
       END (date '2009-03-01') EXCLUSIVE,
       EXCHANGE PARTITION mar08 WITH TABLE constraint_t3,
       RENAME PARTITION FOR ('2008-01-16') TO jan2ndhalf;

select verify('constraint_pt2_1_prt_mar08');

-- truncate and exchage
ALTER TABLE constraint_pt2 TRUNCATE PARTITION feb08,
       EXCHANGE PARTITION feb08 WITH TABLE constraint_t2;

SELECT * FROM constraint_pt2 ORDER BY date, id;
SELECT * FROM constraint_t2;

select verify('constraint_pt2_1_prt_feb08');

-- exchange a subpartition
SET client_min_messages='warning';
CREATE TABLE constraint_pt3 (id int, year int, month int CONSTRAINT month_check CHECK (month > 0),
       day int CONSTRAINT day_check CHECK (day > 0), region text)
DISTRIBUTED BY (id)
PARTITION BY RANGE (year)
      SUBPARTITION BY RANGE (month)
      SUBPARTITION TEMPLATE (
      START (1) END (5) EVERY (1),
      DEFAULT SUBPARTITION other_months )
      SUBPARTITION BY LIST (region)
      SUBPARTITION TEMPLATE (
      SUBPARTITION usa VALUES ('usa'),
      SUBPARTITION europe VALUES ('europe'),
      SUBPARTITION asia VALUES ('asia'),
      DEFAULT SUBPARTITION other_regions )
( START (2001) END (2003) EVERY (1),
DEFAULT PARTITION outlying_years );
RESET client_min_messages;
INSERT INTO constraint_pt3 SELECT i, 2001, 02, i, 'usa' FROM generate_series(1,5)i;
INSERT INTO constraint_pt3 SELECT i, 2001, 02, i, 'europe' FROM generate_series(1,5)i;
INSERT INTO constraint_pt3 SELECT i, 2002, 02, i, 'europe' FROM generate_series(1,5)i;
INSERT INTO constraint_pt3 SELECT i, 2002, 4, i, 'asia' FROM generate_series(1,5)i;
INSERT INTO constraint_pt3 SELECT i, 2002, 4, i, 'europe' FROM generate_series(1,5)i;

-- look at the constraints of the partition we plan to exchange
SELECT conname, pg_get_constraintdef(oid) from pg_constraint where conrelid =
       'constraint_pt3_1_prt_2_2_prt_3_3_prt_europe'::regclass;

drop table if exists constraint_t3;
CREATE TABLE constraint_t3 (id int, year int, month int CONSTRAINT month_check CHECK (month > 0),
       day int CONSTRAINT day_check CHECK (day > 0), region text)
       WITH (appendonly=true) DISTRIBUTED BY (id);

ALTER TABLE constraint_pt3 ALTER PARTITION FOR ('2001')
      ALTER PARTITION FOR ('2')
      EXCHANGE PARTITION FOR ('europe') WITH TABLE constraint_t3;

select verify('constraint_pt3_1_prt_2_2_prt_3_3_prt_europe');
select verify('constraint_t3');

INSERT INTO constraint_pt3 SELECT i, 2001, 02, i, 'europe' FROM generate_series(11,15)i;
SELECT * FROM constraint_pt3 ORDER BY year, month, region, id;
SELECT * FROM constraint_t3 ORDER BY year, month, region, id;
\d+ constraint_pt3_1_prt_2_2_prt_3_3_prt_europe

CREATE DOMAIN const_domain1 AS TEXT
      CONSTRAINT cons_check1 check (char_length(VALUE) = 5);
CREATE DOMAIN const_domain2 AS TEXT;

ALTER DOMAIN const_domain2 ADD CONSTRAINT
      cons_check2 CHECK (char_length(VALUE) = 5);

select count(distinct(foo.oid)) from (
       (select oid from pg_constraint
        where conname ~ 'cons_check')
       union
       (select oid from gp_dist_random('pg_constraint')
        where conname ~ 'cons_check')) foo;

create table contest (
	a int constraint contest_primary primary key,
	name varchar(40) constraint contest_check check (a > 99 AND name <> '')
) distributed by (a);

select verify('contest');

create table contest_like (like contest including constraints) distributed randomly;
select verify('contest_like');

create table contest_inherit() inherits (contest) distributed randomly;
select verify('contest_inherit');

--
-- pg_index
--

-- Check that the OIDs in the indpred expressions  match between master and segments.
create table indoid_t1(a int, b int);
create index it1 on indoid_t1(a) where a < 100;
create index it2 on indoid_t1(a) where a != 100;
create index it3 on indoid_t1(a, b) where a <= 100 and b >= 100;

select indexrelid::regclass, count(distinct indpred) from
(
  select indexrelid, indpred from pg_index
  where indrelid = 'indoid_t1'::regclass
  union all
  select indexrelid, indpred from gp_dist_random('pg_index')
  where indrelid = 'indoid_t1'::regclass
) as tmp
group by indexrelid;

-- Same for AO tables
create table indoid_ao_t1(a int, b int) with(appendonly=true);
create index i_aot1 on indoid_ao_t1(a) where a < 100;
create index i_aot2 on indoid_ao_t1(a) where a != 100;
create index i_aot3 on indoid_ao_t1(a, b) where a <= 100 and b >= 100;

select indexrelid::regclass, count(distinct indpred) from
(
  select indexrelid, indpred from pg_index
  where indrelid = 'indoid_ao_t1'::regclass
  union all
  select indexrelid, indpred from gp_dist_random('pg_index')
  where indrelid = 'indoid_ao_t1'::regclass
) as tmp
group by indexrelid;

-- Same for Column-Oriented AO tables
create table indoid_co_t1(a int, b int) with(appendonly=true, orientation=column);
create index i_cot1 on indoid_co_t1(a) where a < 100;
create index i_cot2 on indoid_co_t1(a) where a != 100;
create index i_cot3 on indoid_co_t1(a, b) where a <= 100 and b >= 100;

select indexrelid::regclass, count(distinct indpred) from
(
  select indexrelid, indpred from pg_index
  where indrelid = 'indoid_co_t1'::regclass
  union all
  select indexrelid, indpred from gp_dist_random('pg_index')
  where indrelid = 'indoid_co_t1'::regclass
) as tmp
group by indexrelid;

create function indoid_myfunc(integer) returns boolean as
$$
select $1 > 20;
$$ language sql immutable;

create table indoid_ao_t2(a int, b int) with (appendonly=true);
create index i_aot4 on indoid_ao_t2(b) where indoid_myfunc(b);
select count(foo.*) from
 ((select indpred from pg_index where indrelid = 'indoid_ao_t2'::regclass)
   union
  (select indpred from gp_dist_random('pg_index') where indrelid = 'indoid_ao_t2'::regclass)
 ) foo;

--
-- pg_language
--

-- start_ignore
create language plpython3u;
--end_ignore

select count(foo.*) 
from (
      select oid, lanname, lanplcallfoid, lanvalidator
      from pg_language
      where lanname='plpython3u'
      union
      select oid, lanname, lanplcallfoid, lanvalidator
      from gp_dist_random('pg_language')
      where lanname='plpython3u' ) foo;

--
-- pg_rewrite
--

create or replace function verify(varchar) returns bigint as
$$
select count(foo.*) from(
                    select oid, rulename, ev_class::regclass
                    from pg_rewrite
                    where ev_class=$1::regclass
                    union
                    select oid, rulename, ev_class::regclass
                    from gp_dist_random('pg_rewrite')
                    where ev_class=$1::regclass
                    ) foo;
$$ language sql;

-- copied from existing cdbfast tests:
-- (//cdbfast/Release-4_3-branch/oid_inconsistency/...)

create view rewrite_oid_bug as select 1;
select verify('rewrite_oid_bug');

CREATE table oid_consistency_foo_ao (a int) with (appendonly=true) distributed by (a);
CREATE table oid_consistency_bar_ao (a int) distributed by (a);
CREATE rule one as on insert to oid_consistency_bar_ao do instead update oid_consistency_foo_ao set a=1;
select verify('oid_consistency_bar_ao');

CREATE table oid_consistency_foo2 (a int) distributed by (a);
CREATE table oid_consistency_bar2 (a int) distributed by (a);
CREATE rule two as on insert to oid_consistency_bar2 do instead insert into oid_consistency_foo2(a) values(1);
select verify('oid_consistency_bar2');

CREATE table oid_consistency_tt1 (a int) distributed by (a);
CREATE table oid_consistency_tt2 (a int) distributed by (a);
CREATE rule "_RETURN" as on select to oid_consistency_tt1
        do instead select * from oid_consistency_tt2;
select verify('oid_consistency_tt1');

--
-- pg_trigger
--
create or replace function verify(varchar) returns bigint as
$$
select count(foo.*) from(
                    select oid, tgname, tgfoid::regclass
                    from pg_trigger
                    where tgrelid=$1::regclass
                    union
                    select oid, tgname, tgfoid::regclass
                    from gp_dist_random('pg_trigger')
                    where tgrelid=$1::regclass
                    ) foo;
$$ language sql;

create table trigger_oid(a int, b int) distributed by (a);
create or replace function trig_func() returns trigger as
$$
  begin
    return new;
  end
$$ language plpgsql no sql;
create trigger troid_trigger after insert on trigger_oid for each row execute procedure trig_func();

select verify('trigger_oid');

-- Case for Issue: https://github.com/greenplum-db/gpdb/issues/14465
create function func_fail_14465(int) returns int
        immutable language plpgsql as $$
begin
        perform unwanted_grant();
        raise warning 'owned';
        return 1;
exception when others then
        return 2;
end$$;
create materialized view mv_14465 as select 1 as c;
create index on mv_14465 (c) where func_fail_14465(1) > 0;
