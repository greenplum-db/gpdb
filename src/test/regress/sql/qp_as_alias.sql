-- start_ignore
create schema qp_as_alias;
set search_path = qp_as_alias;
-- end_ignore

create table xyz ("B B" int, "C" int, "D+1" int) DISTRIBUTED RANDOMLY;
-- start_equiv
create table xyz_ctas1 as (select "B B" AS "B+1", "C" AS "_%A", "D+1" AS "D" from xyz) DISTRIBUTED RANDOMLY;
create table xyz_ctas2 as (select "B B" "B+1", "C" "_%A", "D+1" "D" from xyz) DISTRIBUTED RANDOMLY;
-- end_equiv
CREATE TEMP TABLE disttable (f1 integer) DISTRIBUTED RANDOMLY;
INSERT INTO DISTTABLE VALUES(1);
INSERT INTO DISTTABLE VALUES(2);
INSERT INTO DISTTABLE VALUES(3);
INSERT INTO DISTTABLE VALUES(NULL);

-- start_equiv
SELECT f1, f1 IS DISTINCT FROM 2 AS "not 2" FROM disttable ORDER BY 1;
SELECT f1, f1 IS DISTINCT FROM 2 "not 2" FROM disttable ORDER BY 1;
-- end_equiv

-- start_equiv
SELECT f1, f1 IS DISTINCT FROM NULL AS "not null" FROM disttable ORDER BY 1;
SELECT f1, f1 IS DISTINCT FROM NULL "not null" FROM disttable ORDER BY 1;
-- end_equiv

-- start_equiv
SELECT f1, f1 IS DISTINCT FROM f1 AS "false" FROM disttable ORDER BY 1;
SELECT f1, f1 IS DISTINCT FROM f1 "false" FROM disttable ORDER BY 1;
-- end_equiv

-- start_equiv
SELECT f1, f1 IS DISTINCT FROM f1+1 AS "not null" FROM disttable ORDER BY 1;
SELECT f1, f1 IS DISTINCT FROM f1+1 "not null" FROM disttable ORDER BY 1;
-- end_equiv

-- start_equiv
SELECT 1 IS DISTINCT FROM 2 AS "yes";
SELECT 1 IS DISTINCT FROM 2 "yes";
-- end_equiv

-- start_equiv
SELECT 2 IS DISTINCT FROM 2 AS "no";
SELECT 2 IS DISTINCT FROM 2 "no";
-- end_equiv

-- start_equiv
SELECT 2 IS DISTINCT FROM null AS "yes";
SELECT 2 IS DISTINCT FROM null "yes";
-- end_equiv

-- start_equiv
SELECT null IS DISTINCT FROM null AS "no";
SELECT null IS DISTINCT FROM null "no";
-- end_equiv

CREATE TABLE test_having (a int, b int, c char(8), d char) DISTRIBUTED RANDOMLY;
INSERT INTO test_having VALUES (0, 1, 'XXXX', 'A');
INSERT INTO test_having VALUES (1, 2, 'AAAA', 'b');
INSERT INTO test_having VALUES (2, 2, 'AAAA', 'c');
INSERT INTO test_having VALUES (3, 3, 'BBBB', 'D');
INSERT INTO test_having VALUES (4, 3, 'BBBB', 'e');
INSERT INTO test_having VALUES (5, 3, 'bbbb', 'F');
INSERT INTO test_having VALUES (6, 4, 'cccc', 'g');
INSERT INTO test_having VALUES (7, 4, 'cccc', 'h');
INSERT INTO test_having VALUES (8, 4, 'CCCC', 'I');
INSERT INTO test_having VALUES (9, 4, 'CCCC', 'j');

-- start_equiv
SELECT 1 AS one FROM test_having WHERE 1/a = 1 HAVING 1 < 2;
SELECT 1 one FROM test_having WHERE 1/a = 1 HAVING 1 < 2;
-- end_equiv

-- start_equiv
SELECT 1 AS "one_%^" FROM test_having WHERE 1/a = 1 HAVING 1 < 2;
SELECT 1 "one_%^" FROM test_having WHERE 1/a = 1 HAVING 1 < 2;
-- end_equiv

-- start_equiv
SELECT c AS "C+1", max(a) AS MAX FROM test_having GROUP BY c HAVING count(*) > 2 OR min(a) = max(a)  ORDER BY c;
SELECT c "C+1", max(a) AS MAX FROM test_having GROUP BY c HAVING count(*) > 2 OR min(a) = max(a)  ORDER BY c;
-- end_equiv
create table xyz1 ("B B" text, "C" text, "D+1" text) DISTRIBUTED RANDOMLY;
insert into xyz1 values ('0_zero','1_one','2_two');
insert into xyz1 values ('3_three','4_four','5_five');
insert into xyz1 values ('6_six','7_seven','8_eight');

-- start_equiv
select upper("B B") AS "B_B", upper ("C") AS C, substr("D+1",1,1) AS "D" from xyz1;
select upper("B B") "B_B", upper ("C") C, substr("D+1",1,1) "D" from xyz1;
-- end_equiv

-- start_equiv
select lower("B B") AS "B_B", lower ("C") AS C, substr("D+1",1,1) AS "D+1" from xyz1;
select lower("B B") "B_B", lower ("C") C, substr("D+1",1,1) "D+1" from xyz1;
-- end_equiv

-- start_equiv
select upper("B B") AS "B_B", lower ("C") AS C, substr("D+1",1,1) AS "D+1%" from xyz1;
select upper("B B") "B_B", lower ("C") C, substr("D+1",1,1) "D+1%" from xyz1;
-- end_equiv

-- start_equiv
select lower("B B") AS "B_B", upper ("C") AS C, ("D+1") AS "D+1" from xyz1;
select lower("B B") "B_B", upper ("C") C, ("D+1") "D+1" from xyz1;
-- end_equiv

-- start_equiv
select upper("C") AS "C", lower ("B B") AS "%B_B%", "D+1" AS "D+1" from xyz1;
select upper("C") "C", lower ("B B") "%B_B%", "D+1" "D+1" from xyz1;
-- end_equiv

-- start_equiv
select lower("C") AS "%C%", upper ("B B") AS "BB", "D+1"||'9_nine' AS "D+1" from xyz1;
select lower("C") "%C%", upper ("B B") "BB", "D+1"||'9_nine' "D+1" from xyz1;
-- end_equiv

-- start_equiv
select upper("B B") AS "B_B", lower ("C") AS C, ("D+1"||'9_nine') AS "D+1" from xyz1;
select upper("B B") "B_B", lower ("C") C, ("D+1"||'9_nine') "D+1" from xyz1;
-- end_equiv
CREATE TABLE test (a integer, b integer) DISTRIBUTED RANDOMLY;

-- start_equiv
CREATE OR REPLACE FUNCTION one() returns integer as $$ select 1 AS result; $$ language sql;
CREATE OR REPLACE FUNCTION one() returns integer as $$ select 1 result; $$ language sql;
-- end_equiv

-- start_equiv
CREATE OR REPLACE FUNCTION add_em(integer, integer) RETURNS integer as $$ SELECT $1 + $2 AS sum; $$ LANGUAGE SQL;
CREATE OR REPLACE FUNCTION add_em(integer, integer) RETURNS integer as $$ SELECT $1 + $2 sum; $$ LANGUAGE SQL;
-- end_equiv

-- start_equiv
INSERT INTO test select a, a%25 from generate_series(1,100) AS a;
INSERT INTO test select a, a%25 from generate_series(1,100) a;
-- end_equiv

-- OLAP
create table customer 
(
	cn int not null,
	cname text not null,
	cloc text,
	
	primary key (cn)
	
) distributed by (cn);

create table vendor 
(
	vn int not null,
	vname text not null,
	vloc text,
	
	primary key (vn)
	
) distributed by (vn);

create table product 
(
	pn int not null,
	pname text not null,
	pcolor text,
	
	primary key (pn)
	
) distributed by (pn);

create table sale
(
	cn int not null,
	vn int not null,
	pn int not null,
	dt date not null,
	qty int not null,
	prc float not null,
	
	primary key (cn, vn, pn)
	
) distributed by (cn,vn,pn);

create table sale_ord
(
        ord int not null,
	cn int not null,
	vn int not null,
	pn int not null,
	dt date not null,
	qty int not null,
	prc float not null,
	
	primary key (cn, vn, pn)
	
) distributed by (cn,vn,pn);

create table util
(
	xn int not null,
	
	primary key (xn)
	
) distributed by (xn);

-- Customers
insert into customer values 
  ( 1, 'Macbeth', 'Inverness'),
  ( 2, 'Duncan', 'Forres'),
  ( 3, 'Lady Macbeth', 'Inverness'),
  ( 4, 'Witches, Inc', 'Lonely Heath');

-- Vendors
insert into vendor values 
  ( 10, 'Witches, Inc', 'Lonely Heath'),
  ( 20, 'Lady Macbeth', 'Inverness'),
  ( 30, 'Duncan', 'Forres'),
  ( 40, 'Macbeth', 'Inverness'),
  ( 50, 'Macduff', 'Fife');

-- Products
insert into product values 
  ( 100, 'Sword', 'Black'),
  ( 200, 'Dream', 'Black'),
  ( 300, 'Castle', 'Grey'),
  ( 400, 'Justice', 'Clear'),
  ( 500, 'Donuts', 'Plain'),
  ( 600, 'Donuts', 'Chocolate'),
  ( 700, 'Hamburger', 'Grey'),
  ( 800, 'Fries', 'Grey');


-- Sales (transactions)
insert into sale values 
  ( 2, 40, 100, '1401-1-1', 1100, 2400),
  ( 1, 10, 200, '1401-3-1', 1, 0),
  ( 3, 40, 200, '1401-4-1', 1, 0),
  ( 1, 20, 100, '1401-5-1', 1, 0),
  ( 1, 30, 300, '1401-5-2', 1, 0),
  ( 1, 50, 400, '1401-6-1', 1, 0),
  ( 2, 50, 400, '1401-6-1', 1, 0),
  ( 1, 30, 500, '1401-6-1', 12, 5),
  ( 3, 30, 500, '1401-6-1', 12, 5),
  ( 3, 30, 600, '1401-6-1', 12, 5),
  ( 4, 40, 700, '1401-6-1', 1, 1),
  ( 4, 40, 800, '1401-6-1', 1, 1);

-- Sales (ord transactions)
insert into sale_ord values 
  ( 1,2, 40, 100, '1401-1-1', 1100, 2400),
  ( 2,1, 10, 200, '1401-3-1', 1, 0),
  ( 3,3, 40, 200, '1401-4-1', 1, 0),
  ( 4,1, 20, 100, '1401-5-1', 1, 0),
  ( 5,1, 30, 300, '1401-5-2', 1, 0),
  ( 6,1, 50, 400, '1401-6-1', 1, 0),
  ( 7,2, 50, 400, '1401-6-1', 1, 0),
  ( 8,1, 30, 500, '1401-6-1', 12, 5),
  ( 9,3, 30, 500, '1401-6-1', 12, 5),
  ( 10,3, 30, 600, '1401-6-1', 12, 5),
  ( 11,4, 40, 700, '1401-6-1', 1, 1),
  ( 12,4, 40, 800, '1401-6-1', 1, 1);

-- Util

insert into util values 
  (1),
  (20),
  (300);

-- start_equiv
SELECT sale.cn,sale.dt,sale.pn,sale.cn,sale.pn,sale.pn,GROUP_ID() AS "group id fn", TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.qty*sale.prc)),0),'99999999.9999999') AS "coalese avg fn"
FROM "public"."sale" AS "sale", "public"."product" AS product
WHERE sale.pn=product.pn
GROUP BY ROLLUP((sale.qty,sale.pn,sale.prc),(sale.qty),(sale.prc,sale.vn,sale.pn),(sale.dt,sale.cn)),ROLLUP((sale.vn,sale.cn,sale.qty),(sale.cn));

SELECT sale.cn,sale.dt,sale.pn,sale.cn,sale.pn,sale.pn,GROUP_ID() "group id fn", TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.qty*sale.prc)),0),'99999999.9999999') "coalese avg fn"
FROM "public"."sale" "sale", "public"."product" product
WHERE sale.pn=product.pn
GROUP BY ROLLUP((sale.qty,sale.pn,sale.prc),(sale.qty),(sale.prc,sale.vn,sale.pn),(sale.dt,sale.cn)),ROLLUP((sale.vn,sale.cn,sale.qty),(sale.cn));
-- end_equiv

-- start_equiv
SELECT sale.pn,sale.vn,sale.cn,sale.qty,sale.qty,sale.pn,
GROUPING(sale.qty,sale.pn),GROUP_ID() AS "grouping fn", 
TO_CHAR(COALESCE(VAR_SAMP(floor(sale.pn)),0),'99999999.9999999') AS "var_samp fn",
TO_CHAR(COALESCE(STDDEV_SAMP(floor(sale.cn+sale.pn)),0),'99999999.9999999') AS "stddev_samp fn",
TO_CHAR(COALESCE(AVG(floor(sale.pn+sale.qty)),0),'99999999.9999999') AS "average floor fn",
TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.pn-sale.cn)),0),'99999999.9999999') AS "max distinct floor fn",
TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.prc+sale.prc)),0),'99999999.9999999') AS "sum distinct floor fn"
FROM "public"."sale" sale, "public"."customer" "customer"
WHERE "sale"."cn" = customer.cn
GROUP BY (),(),sale.pn,sale.vn,sale.cn,sale.qty HAVING GROUP_ID() < 5;

SELECT sale.pn,sale.vn,sale.cn,sale.qty,sale.qty,sale.pn,
GROUPING(sale.qty,sale.pn),GROUP_ID() "grouping fn",
TO_CHAR(COALESCE(VAR_SAMP(floor(sale.pn)),0),'99999999.9999999') "var_samp fn",
TO_CHAR(COALESCE(STDDEV_SAMP(floor(sale.cn+sale.pn)),0),'99999999.9999999') "stddev_samp fn",
TO_CHAR(COALESCE(AVG(floor(sale.pn+sale.qty)),0),'99999999.9999999') "average floor fn",
TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.pn-sale.cn)),0),'99999999.9999999') "max distinct floor fn",
TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.prc+sale.prc)),0),'99999999.9999999') "sum distinct floor fn"
FROM "public"."sale" sale, "public"."customer" "customer"
WHERE "sale"."cn" = customer.cn
GROUP BY (),(),sale.pn,sale.vn,sale.cn,sale.qty HAVING GROUP_ID() < 5;
-- end_equiv
create table xyz2 ("B B" int, "C" int, "D+1" int) DISTRIBUTED RANDOMLY;
insert into xyz2 values (generate_series(1,3),generate_series(4,6),generate_series(7,9));

-- start_equiv
select "B B" AS "%_B B","C" AS _c, "D+1" AS "D" from xyz2;
select "B B" "%_B B","C" _c, "D+1" "D" from xyz2;
-- end_equiv

-- start_equiv
select "B B" AS "%_B()","C" AS "_c&--", "D+1" AS "D+1" from xyz2;
select "B B" "%_B()","C" "_c&--", "D+1" "D+1" from xyz2;
-- end_equiv

-- start_equiv
select "B B" AS "## B B ##","C" AS "##_C_##", "D+1" AS "## D+1 ##" from xyz2;
select "B B" "## B B ##","C" "##_C_##", "D+1" "## D+1 ##" from xyz2;
-- end_equiv

-- start_equiv
select "B B" AS "!@ B @!","C" AS "&* C *&", "D+1" AS "^~ D ~^" from xyz2;
select "B B" "!@ B @!","C" "&* C *&", "D+1" "^~ D ~^" from xyz2;
-- end_equiv

-- start_equiv
select "B B"+1 "(B B)","C" "[C]", "D+1" "{D+1}" from xyz2;
select "B B"+1 "(B B)","C" "[C]", "D+1" "{D+1}" from xyz2;
-- end_equiv
-- start_equiv
SELECT 1 AS one WHERE 1 IN (SELECT 1);
SELECT 1 one WHERE 1 IN (SELECT 1);
-- end_equiv

-- start_equiv
SELECT 1 AS zero WHERE 1 NOT IN (SELECT 1);
SELECT 1 zero WHERE 1 NOT IN (SELECT 1);
-- end_equiv

-- start_equiv
SELECT 1 AS zero WHERE 1 IN (SELECT 2);
SELECT 1 zero WHERE 1 IN (SELECT 2);
-- end_equiv

CREATE TABLE SUBSELECT_TBL (
   f1 integer,
   f2 integer,
   f3 float
 ) DISTRIBUTED RANDOMLY;
INSERT INTO SUBSELECT_TBL VALUES (1, 2, 3);
INSERT INTO SUBSELECT_TBL VALUES (2, 3, 4);
INSERT INTO SUBSELECT_TBL VALUES (3, 4, 5);
INSERT INTO SUBSELECT_TBL VALUES (1, 1, 1);
INSERT INTO SUBSELECT_TBL VALUES (2, 2, 2);
INSERT INTO SUBSELECT_TBL VALUES (3, 3, 3);
INSERT INTO SUBSELECT_TBL VALUES (6, 7, 8);
INSERT INTO SUBSELECT_TBL VALUES (8, 9, NULL);

-- start_equiv
SELECT '' AS eight, * FROM SUBSELECT_TBL ORDER BY 2,3,4;
SELECT '' eight, * FROM SUBSELECT_TBL ORDER BY 2,3,4;
-- end_equiv

-- start_equiv
SELECT '' AS six, f1 AS "Uncorrelated Field" FROM SUBSELECT_TBL  WHERE f1 IN (SELECT f2 FROM SUBSELECT_TBL) ORDER BY 2;
SELECT '' six, f1 "Uncorrelated Field" FROM SUBSELECT_TBL  WHERE f1 IN (SELECT f2 FROM SUBSELECT_TBL) ORDER BY 2;
-- end_equiv

-- start_equiv
SELECT '' AS six, f1 AS "Uncorrelated Field" FROM SUBSELECT_TBL  WHERE f1 IN (SELECT f2 FROM SUBSELECT_TBL WHERE f2 IN (SELECT f1 FROM SUBSELECT_TBL)) ORDER BY 2;
SELECT '' six, f1 "Uncorrelated Field" FROM SUBSELECT_TBL  WHERE f1 IN (SELECT f2 FROM SUBSELECT_TBL WHERE f2 IN (SELECT f1 FROM SUBSELECT_TBL)) ORDER BY 
2;
-- end_equiv

-- start_equiv
SELECT '' AS six, f1 AS "Correlated Field", f2 AS "Second Field"  FROM SUBSELECT_TBL upper  WHERE f1 IN (SELECT f2 FROM SUBSELECT_TBL WHERE f1 = upper.f1) ORDER BY 2,3;
SELECT '' six, f1 "Correlated Field", f2 "Second Field"  FROM SUBSELECT_TBL upper  WHERE f1 IN (SELECT f2 FROM SUBSELECT_TBL WHERE f1 = upper.f1) ORDER
 BY 2,3;
-- end_equiv

 
CREATE TEMP TABLE foo (id integer) DISTRIBUTED RANDOMLY;
CREATE TEMP TABLE bar (id1 integer, id2 integer) DISTRIBUTED RANDOMLY;
INSERT INTO foo VALUES (1);
INSERT INTO bar VALUES (1, 1);
INSERT INTO bar VALUES (2, 2);
INSERT INTO bar VALUES (3, 1);

-- start_equiv
SELECT * FROM foo WHERE id IN (SELECT id2 FROM (SELECT DISTINCT id1, id2 FROM bar) AS s) ORDER BY 1;
SELECT * FROM foo WHERE id IN (SELECT id2 FROM (SELECT DISTINCT id1, id2 FROM bar) s) ORDER BY 1;
-- end_equiv

-- start_equiv
SELECT * FROM foo WHERE id IN (SELECT id2 FROM (SELECT id1, id2 FROM bar UNION SELECT id1, id2 FROM bar) AS s) ORDER BY 1;
SELECT * FROM foo WHERE id IN (SELECT id2 FROM (SELECT id1, id2 FROM bar UNION SELECT id1, id2 FROM bar) s) ORDER BY 1;
-- end_equiv

-- start_equiv
SELECT * FROM foo WHERE id IN  (SELECT id2 FROM (SELECT id2 FROM bar GROUP BY id2) AS s) ORDER BY 1;
SELECT * FROM foo WHERE id IN  (SELECT id2 FROM (SELECT id2 FROM bar GROUP BY id2) s) ORDER BY 1;
-- end_equiv

-- start_equiv
SELECT * FROM foo WHERE id IN (SELECT id2 FROM (SELECT id2 FROM bar UNION SELECT id2 FROM bar) AS s) ORDER BY 1;
SELECT * FROM foo WHERE id IN (SELECT id2 FROM (SELECT id2 FROM bar UNION SELECT id2 FROM bar) s) ORDER BY 1;
-- end_equiv
CREATE TABLE TIMESTAMP_TBL (d1 timestamp(2) without time zone) DISTRIBUTED RANDOMLY;
INSERT INTO TIMESTAMP_TBL VALUES ('now');
INSERT INTO TIMESTAMP_TBL VALUES ('now');
INSERT INTO TIMESTAMP_TBL VALUES ('today');
INSERT INTO TIMESTAMP_TBL VALUES ('yesterday');
INSERT INTO TIMESTAMP_TBL VALUES ('tomorrow');
INSERT INTO TIMESTAMP_TBL VALUES ('tomorrow EST');
INSERT INTO TIMESTAMP_TBL VALUES ('tomorrow zulu');

-- start_equiv
SELECT count(*) AS One FROM TIMESTAMP_TBL WHERE d1 = timestamp without time zone 'today';
SELECT count(*) One FROM TIMESTAMP_TBL WHERE d1 = timestamp without time zone 'today';
-- end_equiv

-- start_equiv
SELECT count(*) AS Three FROM TIMESTAMP_TBL WHERE d1 = timestamp without time zone 'tomorrow';
SELECT count(*) Three FROM TIMESTAMP_TBL WHERE d1 = timestamp without time zone 'tomorrow';
-- end_equiv

-- start_equiv
SELECT count(*) AS One FROM TIMESTAMP_TBL WHERE d1 = timestamp without time zone 'yesterday';
SELECT count(*) One FROM TIMESTAMP_TBL WHERE d1 = timestamp without time zone 'yesterday';
-- end_equiv

-- start_equiv
SELECT count(*) AS One FROM TIMESTAMP_TBL WHERE d1 = timestamp(2) without time zone 'now';
SELECT count(*) One FROM TIMESTAMP_TBL WHERE d1 = timestamp(2) without time zone 'now';
-- end_equiv

DELETE FROM TIMESTAMP_TBL;

INSERT INTO TIMESTAMP_TBL VALUES ('2009-09-09 00:16:07');
INSERT INTO TIMESTAMP_TBL VALUES ('2009-03-28 01:09:00');
INSERT INTO TIMESTAMP_TBL VALUES ('2009-03-27 15:31:50.06');
INSERT INTO TIMESTAMP_TBL VALUES ('2010-05-15');
INSERT INTO TIMESTAMP_TBL VALUES ('Mon Feb 10 17:32:01 1997 PST');

-- start_equiv
SELECT '' AS "day", date_trunc('day',d1) AS date_trunc  FROM TIMESTAMP_TBL WHERE d1 > timestamp without time zone '2009-03-27';
SELECT '' "day", date_trunc('day',d1) date_trunc  FROM TIMESTAMP_TBL WHERE d1 > timestamp without time zone '2009-03-27';
-- end_equiv

-- start_equiv
SELECT '' AS "5", d1 FROM TIMESTAMP_TBL  WHERE d1 <= timestamp without time zone '2010-05-15';
SELECT '' "5", d1 FROM TIMESTAMP_TBL  WHERE d1 <= timestamp without time zone '2010-05-15';
-- end_equiv

-- start_equiv
SELECT '' AS date_trunc_week, date_trunc( 'week', timestamp '2004-02-29 15:44:17.71393' ) AS week_trunc;
SELECT '' date_trunc_week, date_trunc( 'week', timestamp '2004-02-29 15:44:17.71393' )  week_trunc;
-- end_equiv

-- start_equiv
SELECT '' AS to_char_1, to_char(d1, 'DAY Day day DY Dy dy MONTH Month month RM MON Mon mon')  FROM TIMESTAMP_TBL;
SELECT '' to_char_1, to_char(d1, 'DAY Day day DY Dy dy MONTH Month month RM MON Mon mon')  FROM TIMESTAMP_TBL;
-- end_equiv

-- start_ignore
drop schema qp_as_alias cascade;
-- end_ignore