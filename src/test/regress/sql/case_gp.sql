-- Additional CASE-WHEN tests


--
-- CASE ... WHEN IS NOT DISTINCT FROM ...
--
DROP TABLE IF EXISTS mytable CASCADE;
CREATE TABLE mytable (a int, b int, c varchar(1));
INSERT INTO mytable values (1,2,'t'),
  (2,3,'e'),
  (3,4,'o'),
  (4,5,'o'),
  (4,4,'o'),
  (5,5,'t'),
  (6,6,'t'),
  (7,6,'a'),
  (8,7,'t'),
  (9,8,'a');

DROP VIEW IF EXISTS notdisview;
CREATE OR REPLACE VIEW notdisview AS
SELECT
    CASE 'a'::text = 'test'::text
        WHEN 'test' IS NOT DISTINCT FROM ''::text THEN 'A'::text
        ELSE 'B'::text
        END AS t;
select pg_get_viewdef('notdisview',true);

DROP VIEW IF EXISTS notdisview2;
CREATE OR REPLACE VIEW notdisview2 AS
SELECT
    CASE
        WHEN c::text IS NOT DISTINCT FROM ''::text THEN 'A'::text
        ELSE 'B'::text
        END AS t
    FROM mytable;
select pg_get_viewdef('notdisview2',true);

CREATE TABLE mytable2 (
    key character varying(20) NOT NULL,
    key_value character varying(50)
) DISTRIBUTED BY (key);

DROP VIEW IF EXISTS notdisview3;
CREATE OR REPLACE VIEW notdisview3 AS
SELECT
    CASE mytable2.key_value
        WHEN IS NOT DISTINCT FROM 'NULL'::text THEN 'now'::text::date
        ELSE to_date(mytable2.key_value::text, 'YYYYMM'::text)
        END AS t
    FROM mytable2;
select pg_get_viewdef('notdisview3',false);

CREATE OR REPLACE FUNCTION negate(int) RETURNS int 
AS 'SELECT $1 * (-1)'
LANGUAGE sql CONTAINS SQL
IMMUTABLE
RETURNS null ON null input;

DROP VIEW IF EXISTS myview;
CREATE VIEW myview AS 
   SELECT a,b, CASE a WHEN IS NOT DISTINCT FROM b THEN b*10
                      WHEN IS NOT DISTINCT FROM b+1 THEN b*100 
                      WHEN b-1 THEN b*1000
                      WHEN b*10 THEN b*10000
                      WHEN negate(b) THEN b*(-1.0)
                      ELSE b END AS newb
     FROM mytable;
SELECT * FROM myview ORDER BY a,b;

-- Test deparse
select pg_get_viewdef('myview',true); 

DROP TABLE IF EXISTS products CASCADE;
CREATE TABLE products (id serial, name text, price numeric);
INSERT INTO products (name, price) values
  ('keyboard', 124.99),
  ('monitor', 299.99),
  ('mouse', 45.59);

SELECT id,name,price as old_price,
       CASE name WHEN IS NOT DISTINCT FROM 'keyboard' THEN products.price*1.5 
                 WHEN IS NOT DISTINCT FROM 'monitor' THEN price*1.2
                 WHEN 'keyboard tray' THEN price*.9 
                 END AS new_price
  FROM products;
                            
-- testexpr should be evaluated only once
DROP FUNCTION IF EXISTS blip(int);
DROP TABLE IF EXISTS calls_to_blip;

CREATE TABLE calls_to_blip (n serial, v int) DISTRIBUTED RANDOMLY;
CREATE OR REPLACE FUNCTION blip(int) RETURNS int
LANGUAGE plpgsql MODIFIES SQL DATA
VOLATILE
AS $$
DECLARE
	x alias for $1;
BEGIN
	INSERT INTO calls_to_blip(v) VALUES (x);
	RETURN x;
END;
$$;

SELECT CASE blip(1) 
			WHEN IS NOT DISTINCT FROM blip(2) THEN blip(20)
			WHEN IS NOT DISTINCT FROM blip(3) THEN blip(30)
			WHEN IS NOT DISTINCT FROM blip(4) THEN blip(40)
			ELSE blip(666)
			END AS answer;
SELECT * FROM calls_to_blip ORDER BY 1;

-- Negative test
--   1. wrong syntax
--   2. type mismatches
SELECT a,b,CASE WHEN IS NOT DISTINCT FROM b THEN b*100 ELSE b*1000 END FROM mytable;
SELECT a,b,c,CASE c WHEN IS NOT DISTINCT FROM b THEN a
                    WHEN IS NOT DISTINCT FROM b+1 THEN a*100
                    ELSE c END 
  FROM mytable; 

--
-- DECODE(): Oracle compatibility
--
SELECT decode(null,null,true,false);
SELECT decode(NULL::integer, 1, 100, NULL, 200, 300);
SELECT decode('1'::text, '1', 100, '2', 200);
SELECT decode(2, 1, 'ABC', 2, 'DEF');
SELECT decode('2009-02-05'::date, '2009-02-05', 'ok');
SELECT decode('2009-02-05 01:02:03'::timestamp, '2009-02-05 01:02:03', 'ok');

SELECT b,c,decode(c,'a',b*10,'e',b*100,'o',b*1000,'u',b*10000,'i',b*100000) as newb from mytable;
SELECT b,c,decode(c,'a',ARRAY[1,2],'e',ARRAY[3,4],'o',ARRAY[5,6],'u',ARRAY[7,8],'i',ARRAY[9,10],ARRAY[0]) as newb from mytable;

DROP VIEW IF EXISTS myview;
CREATE VIEW myview as
 SELECT id, name, price, DECODE(id, 1, 'Southlake',
                                    2, 'San Francisco',
                                    3, 'New Jersey',
                                    4, 'Seattle',
                                    5, 'Portland',
                                    6, 'San Francisco',
                                    7, 'Portland',
                                       'Non domestic') Location
  FROM products
 WHERE id < 100;

SELECT * FROM myview ORDER BY id, location;

-- Test deparse
select pg_get_viewdef('myview',true); 

-- User-defined DECODE function
CREATE OR REPLACE FUNCTION "decode"(int, int, int) RETURNS int
AS 'select $1 * $2 - $3;'
LANGUAGE sql CONTAINS SQL
IMMUTABLE
RETURNS null ON null input;

SELECT decode(11,8,11);
SELECT "decode"(11,8,11);
SELECT public.decode(11,8,11);

-- Test CASE x WHEN IS NOT DISTINCT FROM y with DECODE
SELECT a,b,decode(a,1,1), 
		CASE decode(a,1,1) WHEN IS NOT DISTINCT FROM 1 THEN b*100
                  		   WHEN IS NOT DISTINCT FROM 4 THEN b*1000 ELSE b END as newb
  FROM mytable ORDER BY a,b; 

-- Test CASE WHEN x IS NOT DISTINCT FROM y with DECODE
SELECT a,b,decode(a,1,1), 
		CASE WHEN decode(a,1,1) IS NOT DISTINCT FROM 1 THEN b*100
			 WHEN decode(a,1,1) IS NOT DISTINCT FROM 4 THEN b*1000 ELSE b END as newb
  FROM mytable ORDER BY a,b; 

SELECT a,b,"decode"(a,1,1), 
			CASE WHEN "decode"(a,1,1) IS NOT DISTINCT FROM 1 THEN b*100
                 WHEN "decode"(a,1,1) IS NOT DISTINCT FROM 4 THEN b*1000 ELSE b END as newb
  FROM mytable ORDER BY a,b; 

-- Negative test: type mismatches
SELECT b,c,decode(c,'a',ARRAY[1,2],'e',ARRAY[3,4],'o',ARRAY[5,6],'u',ARRAY[7,8],'i',ARRAY[9,10],0) as newb from mytable;

--
-- Clean up
--

DROP TABLE mytable CASCADE;
DROP TABLE products CASCADE;
DROP TABLE calls_to_blip;
DROP FUNCTION negate(int);
DROP FUNCTION "decode"(int, int, int);
DROP FUNCTION blip(int);

select CASE 'M'
    WHEN IS NOT DISTINCT FROM 'M' THEN 'Male'
    WHEN IS NOT DISTINCT FROM 'F' THEN 'Female'
    WHEN IS NOT DISTINCT FROM '' THEN 'Not Specified'
    WHEN IS NOT DISTINCT FROM null THEN 'Not Specified'
    END;
select CASE 'F'
    WHEN IS NOT DISTINCT FROM 'M' THEN 'Male'
    WHEN IS NOT DISTINCT FROM 'F' THEN 'Female'
    WHEN IS NOT DISTINCT FROM '' THEN 'Not Specified'
    WHEN IS NOT DISTINCT FROM null THEN 'Not Specified'
    END;
select CASE ''
    WHEN IS NOT DISTINCT FROM 'M' THEN 'Male'
    WHEN IS NOT DISTINCT FROM 'F' THEN 'Female'
    WHEN IS NOT DISTINCT FROM '' THEN 'Not Specified'
    WHEN IS NOT DISTINCT FROM null THEN 'Not Specified'
    END;
select CASE null
    WHEN IS NOT DISTINCT FROM 'M' THEN 'Male'
    WHEN IS NOT DISTINCT FROM 'F' THEN 'Female'
    WHEN IS NOT DISTINCT FROM '' THEN 'Not Specified'
    WHEN IS NOT DISTINCT FROM null THEN 'Not Specified'
    END;

create table case_genders (gid integer, gender char(1)) distributed by (gid);

insert into case_genders(gid, gender) values
  (1, 'F'),
  (2, 'M'),
  (3, 'Z'),
  (4, ''),
  (5, null),
  (6, 'G');

select gender, CASE gender
    WHEN IS NOT DISTINCT FROM 'M' THEN 'Male'
    WHEN IS NOT DISTINCT FROM 'F' THEN 'Female'
    WHEN IS NOT DISTINCT FROM '' THEN 'Not Specified'
    WHEN IS NOT DISTINCT FROM null THEN 'Not Specified'
    END
from case_genders
order by gid;

select CASE 'M'
    WHEN IS NOT DISTINCT FROM 'M' THEN 'Male'
    WHEN IS NOT DISTINCT FROM 'F' THEN 'Female'
    WHEN IS NOT DISTINCT FROM '' THEN 'Not Specified'
    WHEN IS NOT DISTINCT FROM null THEN 'Not Specified'
    ELSE 'Other' END;

select CASE null
    WHEN IS NOT DISTINCT FROM 'M' THEN 'Male'
    WHEN IS NOT DISTINCT FROM 'F' THEN 'Female'
    WHEN IS NOT DISTINCT FROM '' THEN 'Not Specified'
    ELSE 'Other' END;

select gender, CASE gender
    WHEN IS NOT DISTINCT FROM 'M' THEN 'Male'
    WHEN IS NOT DISTINCT FROM 'F' THEN 'Female'
    WHEN IS NOT DISTINCT FROM '' THEN 'Not Specified'
    WHEN IS NOT DISTINCT FROM null THEN 'Not Specified'
    ELSE 'Other' END
from case_genders
order by gid;
select 'a' as lhs, CASE 'a'
       WHEN 'f' THEN 'WHEN: f'
       WHEN IS NOT DISTINCT FROM 'f' THEN 'WHEN NEW: f'
       WHEN 'e' THEN 'WHEN: e'
       WHEN IS NOT DISTINCT FROM 'e' THEN 'WHEN NEW: e'
       WHEN 'd' THEN 'WHEN: d'
       WHEN IS NOT DISTINCT FROM 'd' THEN 'WHEN NEW: d'
       WHEN 'c' THEN 'WHEN: c'
       WHEN IS NOT DISTINCT FROM 'c' THEN 'WHEN NEW: c'
       WHEN 'b' THEN 'WHEN: b'
       WHEN IS NOT DISTINCT FROM 'b' THEN 'WHEN NEW: b'
       WHEN 'a' THEN 'WHEN: a'
       WHEN IS NOT DISTINCT FROM 'a' THEN 'WHEN NEW: a'
    ELSE 'NO MATCH' END as match; 


select 1 as lhs, CASE 1
       WHEN 4 THEN 'WHEN: 4'
       WHEN IS NOT DISTINCT FROM 4 THEN 'WHEN NEW: 4'
       WHEN 3 THEN 'WHEN: 3'
       WHEN IS NOT DISTINCT FROM 3 THEN 'WHEN NEW: 3'
       WHEN 2 THEN 'WHEN: 2'
       WHEN IS NOT DISTINCT FROM 2 THEN 'WHEN NEW: 2'
       WHEN 11 THEN 'WHEN: 11'
       WHEN IS NOT DISTINCT FROM 1 THEN 'WHEN NEW: 1'
    ELSE 'NO MATCH' END as match; 

select '2011-05-27'::date as lsh,  CASE '2011-05-27'::date
       WHEN '2011-07-25'::date THEN 'WHEN: 2011-07-25'
       WHEN IS NOT DISTINCT FROM '2011-05-27'::date THEN 'WHEN NEW: 2011-05-27'
    END as match; 


create table nomatch_case
(
   sid integer, 
   gender char(1) default 'F',
   name text,
   start_dt date
) distributed by (sid);

insert into nomatch_case(sid, gender, name, start_dt) values
  (1000, 'F', 'Jane Doe', '2011-01-15'::date),
  (2000, 'M', 'Ryan Goesling', '2011-02-01'::date),
  (3000, 'M', 'Tim Tebow', '2011-01-15'::date),
  (4000, 'F', 'Katy Perry', '2011-03-01'::date),
  (5000, 'F', 'Michael Scott', '2011-02-01'::date);

select sid,
       name,
       gender,
       start_dt,
       CASE upper(gender)
          WHEN 'MALE' THEN 'M'
          WHEN IS NOT DISTINCT FROM 'FEMALE' THEN 'F'
          WHEN trim('MALE ') THEN 'M'
       ELSE 'NO MATCH' END as case_gender
from nomatch_case
order by sid, name; 

select sid,
       name,
       gender,
       start_dt,
       CASE start_dt
           WHEN IS NOT DISTINCT FROM '2009-01-01'::date THEN 2009
           WHEN '2008-01-01'::date THEN 2008
           WHEN IS NOT DISTINCT FROM '2010-01-01'::date then 2010
           WHEN 2007 THEN 2007
           WHEN IS NOT DISTINCT FROM 2007 THEN 2007
           WHEN '2006-01-01'::date then 2006
       END as case_start_dt
from nomatch_case
order by sid, name;

select sid,
       name,
       gender,
       start_dt,
       CASE sid
           WHEN 100 THEN 'Dept 10' 
           WHEN 200 THEN 'Dept 20' 
           WHEN IS NOT DISTINCT FROM 300 then 'Dept 30'
           WHEN 400 THEN 'Dept 40'
           WHEN 500 THEN 'Dept 50'
           WHEN IS NOT DISTINCT FROM 600 then 'Dept 60'
           WHEN IS NOT DISTINCT FROM 700 then 'Dept 70'
       END as case_sid
from nomatch_case
order by sid, name;

create table combined_when 
(
   sid integer, 
   gender varchar(10) default 'F',
   name text,
   start_dt date
) distributed by (sid);

insert into combined_when(sid, gender, name, start_dt) values
  (1000, 'F', 'Jane Doe', '2011-01-15'::date),
  (2000, 'M', 'Ryan Goesling', '2011-02-01'::date),
  (3000, 'm', 'Tim Tebow', '2007-01-15'::date),
  (4000, 'F', 'Katy Perry', '2011-03-01'::date),
  (5000, 'f', 'Michael Scott', '2011-02-01'::date),
  (6000, 'Female  ', 'Mila Kunis', '2011-02-01'::date),
  (7000, ' Male ', 'Tom Brady', '2011-03-01'::date),
  (8000,  ' ', 'Lady Gaga', '2008-01-15'::date),
  (9000,  null, 'George Michael', '2011-01-15'::date),
  (10000,  'Male   ', 'Michael Jordan', null);

select case_yr_start_dt, count(sid)
from (select sid,
       name,
       gender,
       start_dt,
       CASE extract(year from start_dt)
           WHEN IS NOT DISTINCT FROM 2009 THEN 2009
           WHEN abs(2009-1) THEN 2008
           WHEN IS NOT DISTINCT FROM extract(year from '2010-01-01'::date) then 2010
           WHEN round(2007.05, 0) THEN 2007
           WHEN IS NOT DISTINCT FROM 2007 THEN 2007
           WHEN extract(year from '2006-01-01'::date) then 2006
           WHEN extract(year from '2011-01-01'::date) then 2011
       END as case_yr_start_dt
from combined_when
order by sid, name) a
group by case_yr_start_dt
order by 2 desc, 1;

create table case_expr
(
   sid integer, 
   gender char(1) default 'F',
   name text,
   start_dt date
) distributed by (sid);

insert into case_expr(sid, gender, name, start_dt) values
  (1000, 'F', 'Jane Doe', '2011-01-15'::date),
  (2000, 'M', 'Ryan Goesling', '2011-02-01'::date),
  (3000, 'M', 'Tim Tebow', '2011-01-15'::date),
  (4000, 'F', 'Katy Perry', '2011-03-01'::date),
  (5000, 'F', 'Michael Scott', '2011-02-01'::date);

select sid,
       name,
       gender,
       start_dt,
       CASE (gender is not null)
          WHEN (gender = 'MALE') THEN 'M'
          WHEN IS NOT DISTINCT FROM (gender = 'FEMALE') THEN 'F'
          WHEN (gender = trim('MALE ')) THEN 'M'
          WHEN IS NOT DISTINCT FROM (gender = 'M') THEN 'M'
          WHEN (gender = 'F') THEN 'F'
       ELSE 'NO MATCH' END as case_gender
from case_expr
order by sid, name; 

select sid,
       name,
       gender,
       start_dt,
       CASE (extract(year from start_dt) = 2011)
           WHEN IS NOT DISTINCT FROM (extract(month from start_dt) = 1) THEN 'January'
           WHEN (extract(month from start_dt) = 2) THEN 'February'
           WHEN (extract(month from start_dt) = 3) THEN 'March'
           WHEN IS NOT DISTINCT FROM (extract(month from start_dt) = 4) THEN 'April'
           WHEN (extract(month from start_dt) = 5) THEN 'May'
           WHEN IS NOT DISTINCT FROM (extract(month from start_dt) = 6) THEN 'June'
           WHEN IS NOT DISTINCT FROM (extract(month from start_dt) = 7) THEN 'July'
           WHEN (extract(month from start_dt) = 8) THEN 'August'
           WHEN IS NOT DISTINCT FROM (extract(month from start_dt) = 9) THEN 'September'
           WHEN IS NOT DISTINCT FROM (extract(month from start_dt) = 10) THEN 'October'
           WHEN IS NOT DISTINCT FROM (extract(month from start_dt) = 11) THEN 'November'
           WHEN (extract(month from start_dt) = 12) THEN 'December'
       END as case_start_month
from case_expr
order by sid, name;

select sid,
       name,
       gender,
       start_dt,
       CASE (sid > 100)
           WHEN (sid = 1000) THEN 'Dept 10' 
           WHEN (sid > 1000 and sid <= 2000) THEN 'Dept 20' 
           WHEN IS NOT DISTINCT FROM (sid = 3000) then 'Dept 30'
           WHEN (sid = 4000) THEN 'Dept 40'
           WHEN (sid = 5000) THEN 'Dept 50'
           WHEN IS NOT DISTINCT FROM (sid > 5000 and sid <= 6000) then 'Dept 60'
           WHEN IS NOT DISTINCT FROM (sid = 7000) then 'Dept 70'
       END as case_sid
from case_expr
order by sid, name;

drop table if exists combined_when;

create table combined_when 
(
   sid integer, 
   gender varchar(10) default 'F',
   name text,
   start_dt date
) distributed by (sid);

insert into combined_when(sid, gender, name, start_dt) values
  (1000, 'F', 'Jane Doe', '2011-01-15'::date),
  (2000, 'M', 'Ryan Goesling', '2011-02-01'::date),
  (3000, 'm', 'Tim Tebow', '2007-01-15'::date),
  (4000, 'F', 'Katy Perry', '2011-03-01'::date),
  (5000, 'f', 'Michael Scott', '2011-02-01'::date),
  (6000, 'Female  ', 'Mila Kunis', '2011-02-01'::date),
  (7000, ' Male ', 'Tom Brady', '2011-03-01'::date),
  (8000,  ' ', 'Lady Gaga', '2008-01-15'::date),
  (9000,  null, 'George Michael', '2011-01-15'::date),
  (10000,  'Male   ', 'Michael Jordan', null);

select sid,
       name,
       gender,
       start_dt,
       CASE upper(trim(gender))
          WHEN 'MALE' THEN 'M'
          WHEN IS NOT DISTINCT FROM 'FEMALE' THEN 'F'
          WHEN trim('MALE ') THEN 'M'
          WHEN trim(' FEMALE ') THEN 'F'
          WHEN IS NOT DISTINCT FROM 'M' THEN 'M'
          WHEN IS NOT DISTINCT FROM 'F' THEN 'F'
       ELSE 'NO MATCH' END as case_gender
from combined_when
order by sid, name; 

select sid,
       name,
       gender,
       start_dt,
       CASE extract(year from start_dt)
           WHEN IS NOT DISTINCT FROM 2009 THEN 2009
           WHEN abs(2009-1) THEN 2008
           WHEN IS NOT DISTINCT FROM extract(year from '2010-01-01'::date) then 2010
           WHEN round(2007.05, 0) THEN 2007
           WHEN IS NOT DISTINCT FROM 2007 THEN 2007
           WHEN extract(year from '2006-01-01'::date) then 2006
           WHEN extract(year from '2011-01-01'::date) then 2011
       END as case_yr_start_dt
from combined_when
order by sid, name;

select case_yr_start_dt, count(sid)
from (select sid,
       name,
       gender,
       start_dt,
       CASE extract(year from start_dt)
           WHEN IS NOT DISTINCT FROM 2009 THEN 2009
           WHEN abs(2009-1) THEN 2008
           WHEN IS NOT DISTINCT FROM extract(year from '2010-01-01'::date) then 2010
           WHEN round(2007.05, 0) THEN 2007
           WHEN IS NOT DISTINCT FROM 2007 THEN 2007
           WHEN extract(year from '2006-01-01'::date) then 2006
           WHEN extract(year from '2011-01-01'::date) then 2011
       END as case_yr_start_dt
from combined_when
order by sid, name) a
group by case_yr_start_dt
order by 2 desc, 1;

select CASE 'a'
       WHEN IS NOT DISTINCT FROM 'b' THEN 'a=b'
       WHEN NOT DISTINCT FROM 'a' THEN 'a=a'
END;

select CASE 'a'
       WHEN IS NOT DISTINCT 'b' THEN 'a=b'
       WHEN IS NOT DISTINCT FROM 'a' THEN 'a=a'
END;

select CASE 'a'
       WHEN IS NOT DISTINCT FROM 'b' THEN 'a=b'
       WHEN IS NOT DISTINCT FROM 'a' IS 'a=a'
END;

select CASE 'a'
       WHEN IS NOT DISTINCT FROM 'b' IS 'a=b'
       WHEN IS NOT DISTINCT FROM 'a' THEN 'a=a'
END;

select CASE 1
       WHEN IS NOT DISTINCT FROM 2 THEN '1=2'
       WHEN IS DISTINCT FROM 2 THEN '1<>2'
       WHEN IS NOT DISTINCT FROM 1 THEN '1=1'
END;

--
-- Case expression in group by
--
SELECT
	CASE t.field1
    	WHEN IS NOT DISTINCT FROM ''::text THEN 'Undefined'::character varying
        ELSE t.field1
	END AS field1
	FROM ( SELECT 'test value'::text AS field1) t
  	GROUP BY
	CASE t.field1
		WHEN IS NOT DISTINCT FROM ''::text THEN 'Undefined'::character varying
		ELSE t.field1
	END;
	
--
-- Variant of case expression in group by
--
SELECT
	CASE t.field1
    	WHEN IS NOT DISTINCT FROM ''::text THEN 'Undefined'::character varying
        ELSE t.field1
	END AS field1
	FROM ( SELECT 'test value'::text AS field1) t
  	GROUP BY 1;
	
--
-- decode in group by
--	
SELECT
	decode(t.field1, ''::text, 'Undefined'::character varying, t.field1) as field1
	FROM ( SELECT 'test value'::text AS field1) t
  	GROUP BY
	decode(t.field1, ''::text, 'Undefined'::character varying, t.field1);
	
--
-- variant of decode in group by
--
	SELECT
	decode(t.field1, ''::text, 'Undefined'::character varying, t.field1) as field1
	FROM ( SELECT 'test value'::text AS field1) t
  	GROUP BY 1;

