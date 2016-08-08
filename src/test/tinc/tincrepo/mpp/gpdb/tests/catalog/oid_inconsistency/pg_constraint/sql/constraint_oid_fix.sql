DROP DATABASE IF EXISTS const_fix;
CREATE DATABASE const_fix;
\c const_fix
-- Verification
create or replace function verify(varchar) returns bigint as
$$
        select count(distinct(foo.oid)) from (
               (select oid from pg_constraint
               where conrelid = $1::regclass)
               union
               (select oid from gp_dist_random('pg_constraint')
               where conrelid = $1::regclass)) foo;
$$ language sql;

CREATE TABLE pt1 (id int, date date, amt decimal(10,2)) DISTRIBUTED BY (id)
PARTITION BY RANGE (date)
      (PARTITION Jan08 START (date '2008-01-01') INCLUSIVE ,
      PARTITION Feb08 START (date '2008-02-01') INCLUSIVE ,
      PARTITION Mar08 START (date '2008-03-01') INCLUSIVE
      END (date '2008-04-01') EXCLUSIVE);
CREATE TABLE t1 (id int, date date, amt decimal(10,2)) DISTRIBUTED BY (id);

INSERT INTO pt1 SELECT i, '2008-01-13', i FROM generate_series(1,5)i;
INSERT INTO pt1 SELECT i, '2008-02-13', i FROM generate_series(1,5)i;
INSERT INTO pt1 SELECT i, '2008-03-13', i FROM generate_series(1,5)i;
INSERT INTO t1 SELECT i, '2008-02-02', i FROM generate_series(11,15)i;

ALTER TABLE pt1 EXCHANGE PARTITION Feb08 WITH TABLE t1;

select verify('pt1_1_prt_feb08');
select verify('t1');

SELECT * FROM pt1 ORDER BY date, id;

ALTER TABLE pt1 ALTER COLUMN amt SET DEFAULT 67,
 EXCHANGE PARTITION Feb08 WITH TABLE t1;

select verify('pt1_1_prt_feb08');

SELECT * FROM t1 ORDER BY date, id;
SELECT * FROM pt1 ORDER BY date, id;

-- exchange with appendonly
CREATE TABLE pt2 (id int, date date, amt decimal(10,2)
       CHECK (amt > 0)) DISTRIBUTED BY (id)
PARTITION BY RANGE (date)
      (PARTITION Jan08 START (date '2008-01-01') INCLUSIVE,
      PARTITION Feb08 START (date '2008-02-01') INCLUSIVE,
      PARTITION Mar08 START (date '2008-03-01') INCLUSIVE
      END (date '2008-04-01') EXCLUSIVE);

CREATE TABLE t2 (id int, date date, amt decimal(10,2))
       WITH (appendonly=true, orientation=column) DISTRIBUTED BY (id);

INSERT INTO pt2 SELECT i, '2008-01-13', i FROM generate_series(1,5)i;
INSERT INTO pt2 SELECT i, '2008-01-20', i FROM generate_series(11,15)i;
INSERT INTO pt2 SELECT i, '2008-02-13', i FROM generate_series(1,5)i;
INSERT INTO pt2 SELECT i, '2008-03-13', i FROM generate_series(1,5)i;
INSERT INTO t2 SELECT i, '2008-02-02', i FROM generate_series(11,15)i;

-- split and exchange
ALTER TABLE pt2 EXCHANGE PARTITION Feb08 WITH TABLE t2,
      SPLIT PARTITION FOR ('2008-01-01') AT ('2008-01-16') INTO
       (PARTITION jan08_15, PARTITION jan08_31);

select verify('pt2_1_prt_feb08');
select verify('t2');

SELECT * FROM pt2 ORDER BY date, id;
SELECT * FROM t2 ORDER BY date, id;

CREATE TABLE t3 (id int, date date, amt decimal(10,2))
       WITH (appendonly=true, orientation=column) DISTRIBUTED BY (id);
INSERT INTO t3 SELECT i, '2008-03-02', i FROM generate_series(11,15)i;

-- add, rename and exchange
ALTER TABLE pt2 ADD PARTITION START (date '2009-02-01') INCLUSIVE
       END (date '2009-03-01') EXCLUSIVE,
       EXCHANGE PARTITION mar08 WITH TABLE t3,
       RENAME PARTITION FOR ('2008-01-16') TO jan2ndhalf;

select verify('pt2_1_prt_mar08');

-- truncate and exchage
ALTER TABLE pt2 TRUNCATE PARTITION feb08,
       EXCHANGE PARTITION feb08 WITH TABLE t2;

SELECT * FROM pt2 ORDER BY date, id;
SELECT * FROM t2;

select verify('pt2_1_prt_feb08');

-- exchange a subpartition
SET client_min_messages='warning';
CREATE TABLE pt3 (id int, year int, month int CHECK (month > 0),
       day int CHECK (day > 0), region text)
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
INSERT INTO pt3 SELECT i, 2001, 02, i, 'usa' FROM generate_series(1,5)i;
INSERT INTO pt3 SELECT i, 2001, 02, i, 'europe' FROM generate_series(1,5)i;
INSERT INTO pt3 SELECT i, 2002, 02, i, 'europe' FROM generate_series(1,5)i;
INSERT INTO pt3 SELECT i, 2002, 4, i, 'asia' FROM generate_series(1,5)i;
INSERT INTO pt3 SELECT i, 2002, 4, i, 'europe' FROM generate_series(1,5)i;

-- look at the constraints of the partition we plan to exchange
SELECT conname,consrc from pg_constraint where conrelid =
       'pt3_1_prt_2_2_prt_3_3_prt_europe'::regclass;

DROP TABLE t3;
CREATE TABLE t3 (id int, year int, month int, day int, region text)
       WITH (appendonly=true) DISTRIBUTED BY (id);

ALTER TABLE pt3 ALTER PARTITION FOR ('2001')
      ALTER PARTITION FOR ('2')
      EXCHANGE PARTITION FOR ('europe') WITH TABLE t3;

select verify('pt3_1_prt_2_2_prt_3_3_prt_europe');
select verify('t3');

INSERT INTO pt3 SELECT i, 2001, 02, i, 'europe' FROM generate_series(11,15)i;
SELECT * FROM pt3 ORDER BY year, month, region, id;
SELECT * FROM t3 ORDER BY year, month, region, id;
\d+ pt3_1_prt_2_2_prt_3_3_prt_europe

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
