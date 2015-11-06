--
-- CASE
-- Test the case statement
--

CREATE TABLE CASE_TBL (
  i integer,
  f double precision
) distributed by (f);

CREATE TABLE CASE2_TBL (
  i integer,
  j integer
);

INSERT INTO CASE_TBL VALUES (1, 10.1);
INSERT INTO CASE_TBL VALUES (2, 20.2);
INSERT INTO CASE_TBL VALUES (3, -30.3);
INSERT INTO CASE_TBL VALUES (4, NULL);

INSERT INTO CASE2_TBL VALUES (1, -1);
INSERT INTO CASE2_TBL VALUES (2, -2);
INSERT INTO CASE2_TBL VALUES (3, -3);
INSERT INTO CASE2_TBL VALUES (2, -4);
INSERT INTO CASE2_TBL VALUES (1, NULL);
INSERT INTO CASE2_TBL VALUES (NULL, -6);

--
-- Simplest examples without tables
--

SELECT '3' AS "One",
  CASE
    WHEN 1 < 2 THEN 3
  END AS "Simple WHEN";

SELECT '<NULL>' AS "One",
  CASE
    WHEN 1 > 2 THEN 3
  END AS "Simple default";

SELECT '3' AS "One",
  CASE
    WHEN 1 < 2 THEN 3
    ELSE 4
  END AS "Simple ELSE";

SELECT '4' AS "One",
  CASE
    WHEN 1 > 2 THEN 3
    ELSE 4
  END AS "ELSE default";

SELECT '6' AS "One",
  CASE
    WHEN 1 > 2 THEN 3
    WHEN 4 < 5 THEN 6
    ELSE 7
  END AS "Two WHEN with default";

-- Constant-expression folding shouldn't evaluate unreachable subexpressions
SELECT CASE WHEN 1=0 THEN 1/0 WHEN 1=1 THEN 1 ELSE 2/0 END;
SELECT CASE 1 WHEN 0 THEN 1/0 WHEN 1 THEN 1 ELSE 2/0 END;

-- However we do not currently suppress folding of potentially
-- reachable subexpressions  (but MPP does... So we get different answer from postgres).
--SELECT CASE WHEN i > 100 THEN 1/0 ELSE 0 END FROM case_tbl;

-- Test for cases involving untyped literals in test expression
SELECT CASE 'a' WHEN 'a' THEN 1 ELSE 2 END;

--
-- Examples of targets involving tables
--

SELECT '' AS "Five",
  CASE
    WHEN i >= 3 THEN i
  END AS ">= 3 or Null"
  FROM CASE_TBL;

SELECT '' AS "Five",
  CASE WHEN i >= 3 THEN (i + i)
       ELSE i
  END AS "Simplest Math"
  FROM CASE_TBL;

SELECT '' AS "Five", i AS "Value",
  CASE WHEN (i < 0) THEN 'small'
       WHEN (i = 0) THEN 'zero'
       WHEN (i = 1) THEN 'one'
       WHEN (i = 2) THEN 'two'
       ELSE 'big'
  END AS "Category"
  FROM CASE_TBL;

SELECT '' AS "Five",
  CASE WHEN ((i < 0) or (i < 0)) THEN 'small'
       WHEN ((i = 0) or (i = 0)) THEN 'zero'
       WHEN ((i = 1) or (i = 1)) THEN 'one'
       WHEN ((i = 2) or (i = 2)) THEN 'two'
       ELSE 'big'
  END AS "Category"
  FROM CASE_TBL;

--
-- Examples of qualifications involving tables
--

--
-- NULLIF() and COALESCE()
-- Shorthand forms for typical CASE constructs
--  defined in the SQL92 standard.
--

SELECT * FROM CASE_TBL WHERE COALESCE(f,i) = 4;

SELECT * FROM CASE_TBL WHERE NULLIF(f,i) = 2;

SELECT COALESCE(a.f, b.i, b.j)
  FROM CASE_TBL a, CASE2_TBL b;

SELECT *
  FROM CASE_TBL a, CASE2_TBL b
  WHERE COALESCE(a.f, b.i, b.j) = 2;

SELECT '' AS Five, NULLIF(a.i,b.i) AS "NULLIF(a.i,b.i)",
  NULLIF(b.i, 4) AS "NULLIF(b.i,4)"
  FROM CASE_TBL a, CASE2_TBL b;

SELECT '' AS "Two", *
  FROM CASE_TBL a, CASE2_TBL b
  WHERE COALESCE(f,b.i) = 2;

--
-- Examples of updates involving tables
--

UPDATE CASE_TBL
  SET i = CASE WHEN i >= 3 THEN (- i)
                ELSE (2 * i) END;

SELECT * FROM CASE_TBL;

UPDATE CASE_TBL
  SET i = CASE WHEN i >= 2 THEN (2 * i)
                ELSE (3 * i) END;

SELECT * FROM CASE_TBL;

UPDATE CASE_TBL
  SET i = CASE WHEN b.i >= 2 THEN (2 * j)
                ELSE (3 * j) END
  FROM CASE2_TBL b
  WHERE j = -CASE_TBL.i;

SELECT * FROM CASE_TBL;

--
-- CASE ... WHEN IS NOT DISTINCT FROM ...
--
DROP TABLE IF EXISTS mytable CASCADE;
CREATE TABLE mytable (a int, b int, c varchar(1));
INSERT INTO mytable values 	(1,2,'t'),
							(2,3,'e'),
							(3,4,'o'),
							(4,5,'o'),
							(4,4,'o'),
							(5,5,'t'),
							(6,6,'t'),
							(7,6,'a'),
							(8,7,'t'),
							(9,8,'a');

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
INSERT INTO products (name, price) values ('keyboard', 124.99);
INSERT INTO products (name, price) values ('monitor', 299.99);
INSERT INTO products (name, price) values ('mouse', 45.59);

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
SELECT decode(NULL, 1, 100, NULL, 200, 300);
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

DROP TABLE CASE_TBL;
DROP TABLE CASE2_TBL;
DROP TABLE mytable CASCADE;
DROP TABLE products CASCADE;
DROP TABLE calls_to_blip;
DROP FUNCTION negate(int);
DROP FUNCTION "decode"(int, int, int);
DROP FUNCTION blip(int);
