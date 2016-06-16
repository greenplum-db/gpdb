-- start_ignore
create schema dml_functional_no_setup_2;
set search_path to dml_functional_no_setup_2;
-- end_ignore

-- start_ignore
DROP FUNCTION IF EXISTS insert_correct();
DROP LANGUAGE IF EXISTS plpythonu; 
CREATE LANGUAGE plpythonu;
-- end_ignore
CREATE or REPLACE FUNCTION  insert_correct () RETURNS void as $$
    plpy.execute('INSERT INTO  dml_plperl_t1 VALUES (1)');
    plpy.execute('INSERT INTO  dml_plperl_t1 VALUES (2)');
    plpy.execute('INSERT INTO  dml_plperl_t1 VALUES (4)');
    return;
$$ language plpythonu;
DROP FUNCTION IF EXISTS dml_plperl_fn1 (int,int);
CREATE or REPLACE FUNCTION dml_plperl_fn1 (st int,en int) returns void as $$
DECLARE
i integer;
begin
  i=st;
  while i <= en LOOP
    perform insert_correct();
    i = i + 1;
  END LOOP;
end;
$$ LANGUAGE 'plpgsql';
DROP TABLE IF EXISTS dml_plperl_t1;
CREATE TABLE dml_plperl_t1 ( i int) distributed by (i);
SELECT dml_plperl_fn1(1,10);
SELECT COUNT(*) FROM dml_plperl_t1;
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test2: UDF with Insert within transaction



DROP FUNCTION IF EXISTS dml_insertdata(integer);
CREATE OR REPLACE FUNCTION dml_insertdata (startvalue INTEGER) RETURNS VOID
AS
$$
DECLARE
   i INTEGER;
BEGIN
   i = startvalue;
    EXECUTE 'INSERT INTO dml_plpgsql_t2(a) VALUES (' || i || ')';
END;
$$
LANGUAGE PLPGSQL;
DROP TABLE IF EXISTS dml_plpgsql_t2;
CREATE TABLE dml_plpgsql_t2( a int ) distributed by (a);
BEGIN;
select dml_insertdata(1);
select dml_insertdata(2);
select dml_insertdata(3);
select dml_insertdata(4);
select dml_insertdata(5);
select dml_insertdata(6);
select dml_insertdata(7);
select dml_insertdata(8);
select dml_insertdata(9);
select dml_insertdata(10);
COMMIT;
SELECT COUNT(*) FROM dml_plpgsql_t2;
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test3: UDF with Insert within transaction



DROP FUNCTION IF EXISTS dml_fn2(int);
CREATE OR REPLACE FUNCTION dml_fn2(x int) RETURNS INT as $$
for i in range(0, x):
  plpy.execute('INSERT INTO dml_plpython_t2 values(%d)' % i);
return plpy.execute('SELECT COUNT(*) as a FROM dml_plpython_t2')[0]["a"]
$$ language plpythonu;
DROP TABLE IF EXISTS dml_plpython_t2;
CREATE TABLE dml_plpython_t2(a int) DISTRIBUTED randomly;
BEGIN;
SELECT dml_fn2(20);
ROLLBACK;
BEGIN;
SELECT dml_fn2(10);
COMMIT;
SELECT COUNT(*) FROM dml_plpython_t2;
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test4: Negative test - UDF with Insert



-- start_ignore
DROP FUNCTION IF EXISTS insert_wrong();
-- end_ignore
CREATE or REPLACE FUNCTION insert_wrong() RETURNS void as $$
BEGIN
INSERT INTO  errorhandlingtmpTABLE VALUES ('fjdk');
END;
$$ language plpgsql;
DROP FUNCTION IF EXISTS dml_plperl_fn2 (int,int);
CREATE or REPLACE FUNCTION dml_plperl_fn2 (st int,en int) returns void as $$
DECLARE
i integer;
begin
  i=st;
  while i <= en LOOP
    perform insert_wrong();
    i = i + 1;
  END LOOP;
end;
$$ LANGUAGE 'plpgsql';
DROP TABLE IF EXISTS dml_plperl_t2;
CREATE TABLE dml_plperl_t2(i int) distributed by (i);
SELECT dml_plperl_fn2(1,10);
SELECT COUNT(*) FROM dml_plperl_t2;
-- @author prabhd 
-- @created 2012-12-05 12:00:00 
-- @modified 2012-12-05 12:00:00 
-- @tags dml 
-- @db_name dmldb
-- @description test5: Negative test - UDF with Insert. Different data type



DROP FUNCTION IF EXISTS dml_insertvalue(integer);
CREATE OR REPLACE FUNCTION dml_insertvalue (inp integer) RETURNS VOID
AS
$$
DECLARE
BEGIN
     EXECUTE 'INSERT INTO dml_plpgsql_t1(a) VALUES (%)' , i;
EXCEPTION
    WHEN others THEN
    RAISE NOTICE 'Error in data';
END;
$$
LANGUAGE PLPGSQL;
DROP FUNCTION IF EXISTS dml_indata(integer,integer);
CREATE OR REPLACE FUNCTION dml_indata (startvalue integer, endvalue integer) RETURNS VOID
AS
$$
DECLARE
   i INTEGER;
BEGIN
   i = startvalue;
   WHILE i <= endvalue LOOP
       PERFORM dml_insertvalue(100);
       i = i + 1;
   END LOOP;
END;
$$
LANGUAGE PLPGSQL;
DROP TABLE IF EXISTS dml_plpgsql_t1;
CREATE TABLE dml_plpgsql_t1(a char) distributed by (a);
SELECT dml_indata(1,10);
SELECT COUNT(*) FROM dml_plpgsql_t1;
-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS volatilefn_dml_char;
CREATE TABLE volatilefn_dml_char
(
    col1 char DEFAULT 'a',
    col2 char DEFAULT 'a',
    col3 int,
    col4 char DEFAULT 'a'
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def 
);
DROP TABLE IF EXISTS volatilefn_dml_char_candidate;
CREATE TABLE volatilefn_dml_char_candidate
(
    col1 char DEFAULT 'a',
    col2 char DEFAULT 'a',
    col3 int,
    col4 char 
) 
DISTRIBUTED by (col2);
INSERT INTO volatilefn_dml_char_candidate(col3,col4) VALUES(10,'g');

-- Create volatile UDF
CREATE OR REPLACE FUNCTION func(x int) RETURNS int AS $$
BEGIN

DROP TABLE IF EXISTS foo;
CREATE TABLE foo (a int, b int) distributed by (a);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar (c int, d int) distributed by (c);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;

UPDATE bar SET d = d+1 WHERE c = $1;
RETURN $1 + 1;
END
$$ LANGUAGE plpgsql VOLATILE MODIFIES SQL DATA;

INSERT INTO volatilefn_dml_char(col1,col3) SELECT col2,func(14) FROM volatilefn_dml_char_candidate;
SELECT * FROM volatilefn_dml_char ORDER BY 1,2,3;

UPDATE volatilefn_dml_char SET col3 = (SELECT func(1));
SELECT * FROM volatilefn_dml_char ORDER BY 1,2,3;

DELETE FROM volatilefn_dml_char WHERE col3 = (SELECT func(1));
SELECT * FROM volatilefn_dml_char ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS volatilefn_dml_date;
CREATE TABLE volatilefn_dml_date
(
    col1 date DEFAULT '2014-01-01',
    col2 date DEFAULT '2014-01-01',
    col3 int,
    col4 date DEFAULT '2014-01-01'
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def 
);
DROP TABLE IF EXISTS volatilefn_dml_date_candidate;
CREATE TABLE volatilefn_dml_date_candidate
(
    col1 date DEFAULT '2014-01-01',
    col2 date DEFAULT '2014-01-01',
    col3 int,
    col4 date 
) 
DISTRIBUTED by (col2);
INSERT INTO volatilefn_dml_date_candidate(col3,col4) VALUES(10,'2013-12-30');

-- Create volatile UDF
CREATE OR REPLACE FUNCTION func(x int) RETURNS int AS $$
BEGIN

DROP TABLE IF EXISTS foo;
CREATE TABLE foo (a int, b int) distributed by (a);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar (c int, d int) distributed by (c);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;

UPDATE bar SET d = d+1 WHERE c = $1;
RETURN $1 + 1;
END
$$ LANGUAGE plpgsql VOLATILE MODIFIES SQL DATA;

INSERT INTO volatilefn_dml_date(col1,col3) SELECT col2,func(14) FROM volatilefn_dml_date_candidate;
SELECT * FROM volatilefn_dml_date ORDER BY 1,2,3;

UPDATE volatilefn_dml_date SET col3 = (SELECT func(1));
SELECT * FROM volatilefn_dml_date ORDER BY 1,2,3;

DELETE FROM volatilefn_dml_date WHERE col3 = (SELECT func(1));
SELECT * FROM volatilefn_dml_date ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS volatilefn_dml_decimal;
CREATE TABLE volatilefn_dml_decimal
(
    col1 decimal DEFAULT 1.00,
    col2 decimal DEFAULT 1.00,
    col3 int,
    col4 decimal DEFAULT 1.00
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def 
);
DROP TABLE IF EXISTS volatilefn_dml_decimal_candidate;
CREATE TABLE volatilefn_dml_decimal_candidate
(
    col1 decimal DEFAULT 1.00,
    col2 decimal DEFAULT 1.00,
    col3 int,
    col4 decimal 
) 
DISTRIBUTED by (col2);
INSERT INTO volatilefn_dml_decimal_candidate(col3,col4) VALUES(10,2.00);

-- Create volatile UDF
CREATE OR REPLACE FUNCTION func(x int) RETURNS int AS $$
BEGIN

DROP TABLE IF EXISTS foo;
CREATE TABLE foo (a int, b int) distributed by (a);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar (c int, d int) distributed by (c);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;

UPDATE bar SET d = d+1 WHERE c = $1;
RETURN $1 + 1;
END
$$ LANGUAGE plpgsql VOLATILE MODIFIES SQL DATA;

INSERT INTO volatilefn_dml_decimal(col1,col3) SELECT col2,func(14) FROM volatilefn_dml_decimal_candidate;
SELECT * FROM volatilefn_dml_decimal ORDER BY 1,2,3;

UPDATE volatilefn_dml_decimal SET col3 = (SELECT func(1));
SELECT * FROM volatilefn_dml_decimal ORDER BY 1,2,3;

DELETE FROM volatilefn_dml_decimal WHERE col3 = (SELECT func(1));
SELECT * FROM volatilefn_dml_decimal ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS volatilefn_dml_float;
CREATE TABLE volatilefn_dml_float
(
    col1 float DEFAULT 1.00,
    col2 float DEFAULT 1.00,
    col3 int,
    col4 float DEFAULT 1.00
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def 
);
DROP TABLE IF EXISTS volatilefn_dml_float_candidate;
CREATE TABLE volatilefn_dml_float_candidate
(
    col1 float DEFAULT 1.00,
    col2 float DEFAULT 1.00,
    col3 int,
    col4 float 
) 
DISTRIBUTED by (col2);
INSERT INTO volatilefn_dml_float_candidate(col3,col4) VALUES(10,2.00);

-- Create volatile UDF
CREATE OR REPLACE FUNCTION func(x int) RETURNS int AS $$
BEGIN

DROP TABLE IF EXISTS foo;
CREATE TABLE foo (a int, b int) distributed by (a);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar (c int, d int) distributed by (c);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;

UPDATE bar SET d = d+1 WHERE c = $1;
RETURN $1 + 1;
END
$$ LANGUAGE plpgsql VOLATILE MODIFIES SQL DATA;

INSERT INTO volatilefn_dml_float(col1,col3) SELECT col2,func(14) FROM volatilefn_dml_float_candidate;
SELECT * FROM volatilefn_dml_float ORDER BY 1,2,3;

UPDATE volatilefn_dml_float SET col3 = (SELECT func(1));
SELECT * FROM volatilefn_dml_float ORDER BY 1,2,3;

DELETE FROM volatilefn_dml_float WHERE col3 = (SELECT func(1));
SELECT * FROM volatilefn_dml_float ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS volatilefn_dml_int;
CREATE TABLE volatilefn_dml_int
(
    col1 int DEFAULT 20000,
    col2 int DEFAULT 20000,
    col3 int,
    col4 int DEFAULT 20000
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def 
);
DROP TABLE IF EXISTS volatilefn_dml_int_candidate;
CREATE TABLE volatilefn_dml_int_candidate
(
    col1 int DEFAULT 20000,
    col2 int DEFAULT 20000,
    col3 int,
    col4 int 
) 
DISTRIBUTED by (col2);
INSERT INTO volatilefn_dml_int_candidate(col3,col4) VALUES(10,10000);

-- Create volatile UDF
CREATE OR REPLACE FUNCTION func(x int) RETURNS int AS $$
BEGIN

DROP TABLE IF EXISTS foo;
CREATE TABLE foo (a int, b int) distributed by (a);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar (c int, d int) distributed by (c);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;

UPDATE bar SET d = d+1 WHERE c = $1;
RETURN $1 + 1;
END
$$ LANGUAGE plpgsql VOLATILE MODIFIES SQL DATA;

INSERT INTO volatilefn_dml_int(col1,col3) SELECT col2,func(14) FROM volatilefn_dml_int_candidate;
SELECT * FROM volatilefn_dml_int ORDER BY 1,2,3;

UPDATE volatilefn_dml_int SET col3 = (SELECT func(1));
SELECT * FROM volatilefn_dml_int ORDER BY 1,2,3;

DELETE FROM volatilefn_dml_int WHERE col3 = (SELECT func(1));
SELECT * FROM volatilefn_dml_int ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS volatilefn_dml_int4;
CREATE TABLE volatilefn_dml_int4
(
    col1 int4 DEFAULT 10000000,
    col2 int4 DEFAULT 10000000,
    col3 int,
    col4 int4 DEFAULT 10000000
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def 
);
DROP TABLE IF EXISTS volatilefn_dml_int4_candidate;
CREATE TABLE volatilefn_dml_int4_candidate
(
    col1 int4 DEFAULT 10000000,
    col2 int4 DEFAULT 10000000,
    col3 int,
    col4 int4 
) 
DISTRIBUTED by (col2);
INSERT INTO volatilefn_dml_int4_candidate(col3,col4) VALUES(10,20000000);

-- Create volatile UDF
CREATE OR REPLACE FUNCTION func(x int) RETURNS int AS $$
BEGIN

DROP TABLE IF EXISTS foo;
CREATE TABLE foo (a int, b int) distributed by (a);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar (c int, d int) distributed by (c);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;

UPDATE bar SET d = d+1 WHERE c = $1;
RETURN $1 + 1;
END
$$ LANGUAGE plpgsql VOLATILE MODIFIES SQL DATA;

INSERT INTO volatilefn_dml_int4(col1,col3) SELECT col2,func(14) FROM volatilefn_dml_int4_candidate;
SELECT * FROM volatilefn_dml_int4 ORDER BY 1,2,3;

UPDATE volatilefn_dml_int4 SET col3 = (SELECT func(1));
SELECT * FROM volatilefn_dml_int4 ORDER BY 1,2,3;

DELETE FROM volatilefn_dml_int4 WHERE col3 = (SELECT func(1));
SELECT * FROM volatilefn_dml_int4 ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS volatilefn_dml_int8;
CREATE TABLE volatilefn_dml_int8
(
    col1 int8 DEFAULT 1000000000000000000,
    col2 int8 DEFAULT 1000000000000000000,
    col3 int,
    col4 int8 DEFAULT 1000000000000000000
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def 
);
DROP TABLE IF EXISTS volatilefn_dml_int8_candidate;
CREATE TABLE volatilefn_dml_int8_candidate
(
    col1 int8 DEFAULT 1000000000000000000,
    col2 int8 DEFAULT 1000000000000000000,
    col3 int,
    col4 int8 
) 
DISTRIBUTED by (col2);
INSERT INTO volatilefn_dml_int8_candidate(col3,col4) VALUES(10,200000000000000000);

-- Create volatile UDF
CREATE OR REPLACE FUNCTION func(x int) RETURNS int AS $$
BEGIN

DROP TABLE IF EXISTS foo;
CREATE TABLE foo (a int, b int) distributed by (a);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar (c int, d int) distributed by (c);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;

UPDATE bar SET d = d+1 WHERE c = $1;
RETURN $1 + 1;
END
$$ LANGUAGE plpgsql VOLATILE MODIFIES SQL DATA;

INSERT INTO volatilefn_dml_int8(col1,col3) SELECT col2,func(14) FROM volatilefn_dml_int8_candidate;
SELECT * FROM volatilefn_dml_int8 ORDER BY 1,2,3;

UPDATE volatilefn_dml_int8 SET col3 = (SELECT func(1));
SELECT * FROM volatilefn_dml_int8 ORDER BY 1,2,3;

DELETE FROM volatilefn_dml_int8 WHERE col3 = (SELECT func(1));
SELECT * FROM volatilefn_dml_int8 ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS volatilefn_dml_interval;
CREATE TABLE volatilefn_dml_interval
(
    col1 interval DEFAULT '11 hours',
    col2 interval DEFAULT '11 hours',
    col3 int,
    col4 interval DEFAULT '11 hours'
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def 
);
DROP TABLE IF EXISTS volatilefn_dml_interval_candidate;
CREATE TABLE volatilefn_dml_interval_candidate
(
    col1 interval DEFAULT '11 hours',
    col2 interval DEFAULT '11 hours',
    col3 int,
    col4 interval 
) 
DISTRIBUTED by (col2);
INSERT INTO volatilefn_dml_interval_candidate(col3,col4) VALUES(10,'10 secs');

-- Create volatile UDF
CREATE OR REPLACE FUNCTION func(x int) RETURNS int AS $$
BEGIN

DROP TABLE IF EXISTS foo;
CREATE TABLE foo (a int, b int) distributed by (a);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar (c int, d int) distributed by (c);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;

UPDATE bar SET d = d+1 WHERE c = $1;
RETURN $1 + 1;
END
$$ LANGUAGE plpgsql VOLATILE MODIFIES SQL DATA;

INSERT INTO volatilefn_dml_interval(col1,col3) SELECT col2,func(14) FROM volatilefn_dml_interval_candidate;
SELECT * FROM volatilefn_dml_interval ORDER BY 1,2,3;

UPDATE volatilefn_dml_interval SET col3 = (SELECT func(1));
SELECT * FROM volatilefn_dml_interval ORDER BY 1,2,3;

DELETE FROM volatilefn_dml_interval WHERE col3 = (SELECT func(1));
SELECT * FROM volatilefn_dml_interval ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS volatilefn_dml_numeric;
CREATE TABLE volatilefn_dml_numeric
(
    col1 numeric DEFAULT 1.000000,
    col2 numeric DEFAULT 1.000000,
    col3 int,
    col4 numeric DEFAULT 1.000000
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def 
);
DROP TABLE IF EXISTS volatilefn_dml_numeric_candidate;
CREATE TABLE volatilefn_dml_numeric_candidate
(
    col1 numeric DEFAULT 1.000000,
    col2 numeric DEFAULT 1.000000,
    col3 int,
    col4 numeric 
) 
DISTRIBUTED by (col2);
INSERT INTO volatilefn_dml_numeric_candidate(col3,col4) VALUES(10,2.000000);

-- Create volatile UDF
CREATE OR REPLACE FUNCTION func(x int) RETURNS int AS $$
BEGIN

DROP TABLE IF EXISTS foo;
CREATE TABLE foo (a int, b int) distributed by (a);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar (c int, d int) distributed by (c);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;

UPDATE bar SET d = d+1 WHERE c = $1;
RETURN $1 + 1;
END
$$ LANGUAGE plpgsql VOLATILE MODIFIES SQL DATA;

INSERT INTO volatilefn_dml_numeric(col1,col3) SELECT col2,func(14) FROM volatilefn_dml_numeric_candidate;
SELECT * FROM volatilefn_dml_numeric ORDER BY 1,2,3;

UPDATE volatilefn_dml_numeric SET col3 = (SELECT func(1));
SELECT * FROM volatilefn_dml_numeric ORDER BY 1,2,3;

DELETE FROM volatilefn_dml_numeric WHERE col3 = (SELECT func(1));
SELECT * FROM volatilefn_dml_numeric ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS volatilefn_dml_time;
CREATE TABLE volatilefn_dml_time
(
    col1 time DEFAULT '1:00:00',
    col2 time DEFAULT '1:00:00',
    col3 int,
    col4 time DEFAULT '1:00:00'
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def 
);
DROP TABLE IF EXISTS volatilefn_dml_time_candidate;
CREATE TABLE volatilefn_dml_time_candidate
(
    col1 time DEFAULT '1:00:00',
    col2 time DEFAULT '1:00:00',
    col3 int,
    col4 time 
) 
DISTRIBUTED by (col2);
INSERT INTO volatilefn_dml_time_candidate(col3,col4) VALUES(10,'12:00:00');

-- Create volatile UDF
CREATE OR REPLACE FUNCTION func(x int) RETURNS int AS $$
BEGIN

DROP TABLE IF EXISTS foo;
CREATE TABLE foo (a int, b int) distributed by (a);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar (c int, d int) distributed by (c);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;

UPDATE bar SET d = d+1 WHERE c = $1;
RETURN $1 + 1;
END
$$ LANGUAGE plpgsql VOLATILE MODIFIES SQL DATA;

INSERT INTO volatilefn_dml_time(col1,col3) SELECT col2,func(14) FROM volatilefn_dml_time_candidate;
SELECT * FROM volatilefn_dml_time ORDER BY 1,2,3;

UPDATE volatilefn_dml_time SET col3 = (SELECT func(1));
SELECT * FROM volatilefn_dml_time ORDER BY 1,2,3;

DELETE FROM volatilefn_dml_time WHERE col3 = (SELECT func(1));
SELECT * FROM volatilefn_dml_time ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS volatilefn_dml_timestamp;
CREATE TABLE volatilefn_dml_timestamp
(
    col1 timestamp DEFAULT '2014-01-01 12:00:00',
    col2 timestamp DEFAULT '2014-01-01 12:00:00',
    col3 int,
    col4 timestamp DEFAULT '2014-01-01 12:00:00'
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def 
);
DROP TABLE IF EXISTS volatilefn_dml_timestamp_candidate;
CREATE TABLE volatilefn_dml_timestamp_candidate
(
    col1 timestamp DEFAULT '2014-01-01 12:00:00',
    col2 timestamp DEFAULT '2014-01-01 12:00:00',
    col3 int,
    col4 timestamp 
) 
DISTRIBUTED by (col2);
INSERT INTO volatilefn_dml_timestamp_candidate(col3,col4) VALUES(10,'2013-12-30 12:00:00');

-- Create volatile UDF
CREATE OR REPLACE FUNCTION func(x int) RETURNS int AS $$
BEGIN

DROP TABLE IF EXISTS foo;
CREATE TABLE foo (a int, b int) distributed by (a);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar (c int, d int) distributed by (c);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;

UPDATE bar SET d = d+1 WHERE c = $1;
RETURN $1 + 1;
END
$$ LANGUAGE plpgsql VOLATILE MODIFIES SQL DATA;

INSERT INTO volatilefn_dml_timestamp(col1,col3) SELECT col2,func(14) FROM volatilefn_dml_timestamp_candidate;
SELECT * FROM volatilefn_dml_timestamp ORDER BY 1,2,3;

UPDATE volatilefn_dml_timestamp SET col3 = (SELECT func(1));
SELECT * FROM volatilefn_dml_timestamp ORDER BY 1,2,3;

DELETE FROM volatilefn_dml_timestamp WHERE col3 = (SELECT func(1));
SELECT * FROM volatilefn_dml_timestamp ORDER BY 1,2,3;

-- @author prabhd 
-- @created 2014-04-01 12:00:00
-- @modified 2012-04-01 12:00:00
-- @tags dml MPP-21090 ORCA
-- @product_version gpdb: [4.3-]
-- @optimizer_mode on	
-- @description Tests for MPP-21090



DROP TABLE IF EXISTS volatilefn_dml_timestamptz;
CREATE TABLE volatilefn_dml_timestamptz
(
    col1 timestamptz DEFAULT '2014-01-01 12:00:00 PST',
    col2 timestamptz DEFAULT '2014-01-01 12:00:00 PST',
    col3 int,
    col4 timestamptz DEFAULT '2014-01-01 12:00:00 PST'
) 
DISTRIBUTED by (col1)
PARTITION BY LIST(col2)
(
default partition def 
);
DROP TABLE IF EXISTS volatilefn_dml_timestamptz_candidate;
CREATE TABLE volatilefn_dml_timestamptz_candidate
(
    col1 timestamptz DEFAULT '2014-01-01 12:00:00 PST',
    col2 timestamptz DEFAULT '2014-01-01 12:00:00 PST',
    col3 int,
    col4 timestamptz 
) 
DISTRIBUTED by (col2);
INSERT INTO volatilefn_dml_timestamptz_candidate(col3,col4) VALUES(10,'2013-12-30 12:00:00 PST');

-- Create volatile UDF
CREATE OR REPLACE FUNCTION func(x int) RETURNS int AS $$
BEGIN

DROP TABLE IF EXISTS foo;
CREATE TABLE foo (a int, b int) distributed by (a);
INSERT INTO foo select i, i+1 from generate_series(1,10) i;

DROP TABLE IF EXISTS bar;
CREATE TABLE bar (c int, d int) distributed by (c);
INSERT INTO bar select i, i+1 from generate_series(1,10) i;

UPDATE bar SET d = d+1 WHERE c = $1;
RETURN $1 + 1;
END
$$ LANGUAGE plpgsql VOLATILE MODIFIES SQL DATA;

INSERT INTO volatilefn_dml_timestamptz(col1,col3) SELECT col2,func(14) FROM volatilefn_dml_timestamptz_candidate;
SELECT * FROM volatilefn_dml_timestamptz ORDER BY 1,2,3;

UPDATE volatilefn_dml_timestamptz SET col3 = (SELECT func(1));
SELECT * FROM volatilefn_dml_timestamptz ORDER BY 1,2,3;

DELETE FROM volatilefn_dml_timestamptz WHERE col3 = (SELECT func(1));
SELECT * FROM volatilefn_dml_timestamptz ORDER BY 1,2,3;


-- start_ignore
drop schema dml_functional_no_setup_2 cascade;
-- end_ignore
