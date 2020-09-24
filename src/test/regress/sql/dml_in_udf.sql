-- Test various cases where a user-defined function modifies tables.

-- start_ignore
create language plpython3u;
-- end_ignore

--
-- test1: UDF with Insert
--
CREATE or REPLACE FUNCTION  insert_correct () RETURNS void as $$
  plpy.execute('INSERT INTO  dml_plperl_t1 VALUES (1)');
  plpy.execute('INSERT INTO  dml_plperl_t1 VALUES (2)');
  plpy.execute('INSERT INTO  dml_plperl_t1 VALUES (4)');
  return;
$$ language plpython3u;

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

CREATE TABLE dml_plperl_t1 ( i int) distributed by (i);
SELECT dml_plperl_fn1(1,10);
SELECT COUNT(*) FROM dml_plperl_t1;


--
-- test2: UDF with Insert within transaction
--
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

--
-- test3: UDF with Insert within transaction
--
CREATE OR REPLACE FUNCTION dml_fn2(x int) RETURNS INT as $$
  for i in range(0, x):
    plpy.execute('INSERT INTO dml_plpython_t2 values(%d)' % i);
  return plpy.execute('SELECT COUNT(*) as a FROM dml_plpython_t2')[0]["a"]
$$ language plpython3u;

CREATE TABLE dml_plpython_t2(a int) DISTRIBUTED randomly;
BEGIN;
SELECT dml_fn2(20);
ROLLBACK;
BEGIN;
SELECT dml_fn2(10);
COMMIT;
SELECT COUNT(*) FROM dml_plpython_t2;

--
-- test4: Negative test - UDF with Insert
--
CREATE or REPLACE FUNCTION insert_wrong() RETURNS void as $$
BEGIN
  INSERT INTO  errorhandlingtmpTABLE VALUES ('fjdk');
END;
$$ language plpgsql;
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

CREATE TABLE dml_plperl_t2(i int) distributed by (i);
SELECT dml_plperl_fn2(1,10);
SELECT COUNT(*) FROM dml_plperl_t2;

--
-- test5: Negative test - UDF with Insert. Different data type
--
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

CREATE TABLE dml_plpgsql_t1(a char) distributed by (a);
SELECT dml_indata(1,10);
SELECT COUNT(*) FROM dml_plpgsql_t1;


--
-- This test has of a fairly complicated UDF, which drops and creates
-- tables. The UDF is used in an INSERT/UPDATE/DELETE statement.
--
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
CREATE FUNCTION dml_in_udf_func(x int) RETURNS int AS $$
BEGIN
  DROP TABLE IF EXISTS udftest_foo;
  CREATE TABLE udftest_foo (a int, b int) distributed by (a);
  INSERT INTO udftest_foo select i, i+1 from generate_series(1,10) i;

  DROP TABLE IF EXISTS udftest_bar;
  CREATE TABLE udftest_bar (c int, d int) distributed by (c);
  INSERT INTO udftest_bar select i, i+1 from generate_series(1,10) i;

  UPDATE udftest_bar SET d = d+1 WHERE c = $1;
  RETURN $1 + 1;
END
$$ LANGUAGE plpgsql VOLATILE MODIFIES SQL DATA;

INSERT INTO volatilefn_dml_int8(col1,col3) SELECT col2,dml_in_udf_func(14) FROM volatilefn_dml_int8_candidate;
SELECT * FROM volatilefn_dml_int8 ORDER BY 1,2,3;

UPDATE volatilefn_dml_int8 SET col3 = (SELECT dml_in_udf_func(1));
SELECT * FROM volatilefn_dml_int8 ORDER BY 1,2,3;

DELETE FROM volatilefn_dml_int8 WHERE col3 = (SELECT dml_in_udf_func(1));
SELECT * FROM volatilefn_dml_int8 ORDER BY 1,2,3;
