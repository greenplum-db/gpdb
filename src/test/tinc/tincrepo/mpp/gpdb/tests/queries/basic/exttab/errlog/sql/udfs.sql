DROP EXTERNAL TABLE IF EXISTS exttab_udfs_1;
DROP EXTERNAL TABLE IF EXISTS exttab_udfs_2;

DROP TABLE IF EXISTS exttab_udfs_err;

-- Generate the file with very few errors
\! python @script@ 10 2 > @data_dir@/exttab_udfs_1.tbl

-- does not reach reject limit
CREATE EXTERNAL TABLE exttab_udfs_1( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_udfs_1.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 10;

-- Generate the file with lot of errors
\! python @script@ 200 50 > @data_dir@/exttab_udfs_2.tbl

-- reaches reject limit, use the same err table
CREATE EXTERNAL TABLE exttab_udfs_2( i int, j text ) 
LOCATION ('gpfdist://@host@:@port@/exttab_udfs_2.tbl') FORMAT 'TEXT' (DELIMITER '|') 
LOG ERRORS SEGMENT REJECT LIMIT 2;

-- Test: UDFS with segment reject limit reached

CREATE OR REPLACE FUNCTION exttab_udfs_func1 ()
RETURNS boolean
AS $$
BEGIN

  EXECUTE 'SELECT sum(distinct e1.i) as sum_i, sum(distinct e2.i) as sum_j, e1.j as j FROM

	   (SELECT i, j FROM exttab_udfs_1 WHERE i < 5 ) e1,
	   (SELECT i, j FROM exttab_udfs_2 WHERE i < 10) e2

	   group by e1.j';
  RETURN 1;
END;
$$
LANGUAGE plpgsql volatile;

SELECT gp_truncate_error_log('exttab_udfs_1');
SELECT gp_truncate_error_log('exttab_udfs_2');

-- Should fail
SELECT * FROM exttab_udfs_func1();

-- Should be populated
SELECT COUNT(*) > 0 FROM 
(
SELECT * FROM gp_read_error_log('exttab_udfs_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_udfs_2')
) FOO;

-- INSERT INTO from a udf
DROP TABLE IF EXISTS exttab_udfs_insert_1;

CREATE TABLE exttab_udfs_insert_1(a boolean);

SELECT gp_truncate_error_log('exttab_udfs_1');
SELECT gp_truncate_error_log('exttab_udfs_2');

-- Should fail
INSERT INTO exttab_udfs_insert_1 SELECT * FROM exttab_udfs_func1();

SELECT * FROM exttab_udfs_insert_1;

-- Error table should be populated correctly
SELECT COUNT(*) > 0 FROM 
(
SELECT * FROM gp_read_error_log('exttab_udfs_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_udfs_2')
) FOO;

-- Test: UDFs with INSERT INTO with segment reject limit reached
DROP TABLE IF EXISTS exttab_udfs_insert_2;

CREATE TABLE exttab_udfs_insert_2 (LIKE exttab_udfs_1);

CREATE OR REPLACE FUNCTION exttab_udfs_func2 ()
RETURNS boolean
AS $$
DECLARE
 r RECORD;
 cnt integer;
 result boolean;
BEGIN
  SELECT INTO result gp_truncate_error_log('exttab_udfs_1');
  SELECT INTO result gp_truncate_error_log('exttab_udfs_2');

  INSERT INTO exttab_udfs_insert_2
  SELECT e1.i, e2.j from exttab_udfs_1 e1 INNER JOIN exttab_udfs_1 e2 ON e1.i = e2.i
  UNION ALL
  SELECT e1.i, e2.j from exttab_udfs_1 e1 INNER JOIN exttab_udfs_1 e2 ON e1.i = e2.i
  UNION ALL
  SELECT e1.i, e2.j from exttab_udfs_1 e1 INNER JOIN exttab_udfs_1 e2 ON e1.i = e2.i;
  
  cnt := 0;
  FOR r in SELECT * FROM gp_read_error_log('exttab_udfs_1') LOOP
      -- just looping through the error log
      cnt := cnt + 1;
  END LOOP;
  
  IF cnt <= 0 THEN
      RAISE EXCEPTION 'Error log should not be empty';
  END IF;

  SELECT count(*) INTO cnt FROM exttab_udfs_insert_2;
  -- should be 24
  IF cnt <> 24 THEN
      RAISE EXCEPTION 'Unexpected number of rows inserted';
  END IF;
  
  -- Now make insert into fail
  INSERT INTO exttab_udfs_insert_2
  SELECT e1.i, e2.j from exttab_udfs_1 e1 INNER JOIN exttab_udfs_1 e2 ON e1.i = e2.i
  UNION ALL
  SELECT e1.i, e2.j from exttab_udfs_1 e1 INNER JOIN exttab_udfs_1 e2 ON e1.i = e2.i
  UNION ALL
  SELECT e1.i, e2.j from exttab_udfs_1 e1 INNER JOIN exttab_udfs_2 e2 ON e1.i = e2.i;
  
  -- Should not reach here
  cnt := 0;
  FOR r in SELECT * FROM gp_read_error_log('exttab_udfs_1') LOOP
      -- just looping through the error log
      cnt := cnt + 1;
  END LOOP;
  
  IF cnt <= 0 THEN
      RAISE EXCEPTION 'Error table should not be empty';
  END IF;
  
  RETURN 1;
END;
$$
LANGUAGE plpgsql volatile;

SELECT gp_truncate_error_log('exttab_udfs_1');
SELECT gp_truncate_error_log('exttab_udfs_2');

-- All this should fail, error logs should be populated even if the UDF gets aborted as we persist error rows written within aborted txs.
SELECT * FROM exttab_udfs_func2();

SELECT COUNT(*) > 0 FROM 
(
SELECT * FROM gp_read_error_log('exttab_udfs_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_udfs_2')
) FOO;

SELECT gp_truncate_error_log('exttab_udfs_1');
SELECT gp_truncate_error_log('exttab_udfs_2');
SELECT exttab_udfs_func2();
SELECT COUNT(*) > 0 FROM 
(
SELECT * FROM gp_read_error_log('exttab_udfs_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_udfs_2')
) FOO;

SELECT gp_truncate_error_log('exttab_udfs_1');
SELECT gp_truncate_error_log('exttab_udfs_2');
INSERT INTO exttab_udfs_insert_1 SELECT * FROM exttab_udfs_func2();
SELECT COUNT(*) > 0 FROM 
(
SELECT * FROM gp_read_error_log('exttab_udfs_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_udfs_2')
) FOO;

SELECT gp_truncate_error_log('exttab_udfs_1');
SELECT gp_truncate_error_log('exttab_udfs_2');
CREATE TABLE exttab_udfs_ctas_2 AS SELECT * FROM exttab_udfs_func2();
SELECT COUNT(*) > 0 FROM 
(
SELECT * FROM gp_read_error_log('exttab_udfs_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_udfs_2')
) FOO;

-- No rows should be inserted into exttab_udfs_insert_2
SELECT * FROM exttab_udfs_insert_2;

-- Test: UDFS with CTAS inside that will reach segment reject limits 

CREATE OR REPLACE FUNCTION exttab_udfs_func3 ()
RETURNS boolean
AS $$
DECLARE
 r RECORD;
 cnt integer;
 result boolean;
BEGIN
  SELECT INTO result gp_truncate_error_log('exttab_udfs_1');
  SELECT INTO result gp_truncate_error_log('exttab_udfs_2');

  CREATE TABLE exttab_udfs_ctas_2 AS
  SELECT e1.i, e2.j from exttab_udfs_1 e1 INNER JOIN exttab_udfs_1 e2 ON e1.i = e2.i
  UNION ALL
  SELECT e1.i, e2.j from exttab_udfs_1 e1 INNER JOIN exttab_udfs_1 e2 ON e1.i = e2.i
  UNION ALL
  SELECT e1.i, e2.j from exttab_udfs_1 e1 INNER JOIN exttab_udfs_1 e2 ON e1.i = e2.i;
  
  cnt := 0;
  FOR r in SELECT * FROM gp_read_error_log('exttab_udfs_1') LOOP
      -- just looping through the error log
      cnt := cnt + 1;
  END LOOP;
  
  IF cnt <= 0 THEN
      RAISE EXCEPTION 'table should not be empty';
  END IF;

  SELECT count(*) INTO cnt FROM exttab_udfs_ctas_2;
  -- should be 24
  IF cnt <> 24 THEN
      RAISE EXCEPTION 'Unexpected number of rows inserted';
  END IF;
  
  -- Now make insert into fail
  CREATE TABLE exttab_udfs_ctas_3 AS
  SELECT e1.i, e2.j from exttab_udfs_1 e1 INNER JOIN exttab_udfs_1 e2 ON e1.i = e2.i
  UNION ALL
  SELECT e1.i, e2.j from exttab_udfs_1 e1 INNER JOIN exttab_udfs_1 e2 ON e1.i = e2.i
  UNION ALL
  SELECT e1.i, e2.j from exttab_udfs_1 e1 INNER JOIN exttab_udfs_2 e2 ON e1.i = e2.i;
  
  -- Should not reach here
  cnt := 0;
  FOR r in SELECT * FROM gp_read_error_log('exttab_udfs_2') LOOP
      -- just looping through the error log
      cnt := cnt + 1;
  END LOOP;
  
  IF cnt <= 0 THEN
      RAISE EXCEPTION 'Error table should not be empty';
  END IF;
  
  RETURN 1;
END;
$$
LANGUAGE plpgsql volatile;


-- All this should fail, error logs should be populated even if the UDF gets aborted as we persist error rows written within aborted txs.
SELECT gp_truncate_error_log('exttab_udfs_1');
SELECT gp_truncate_error_log('exttab_udfs_2');
SELECT * FROM exttab_udfs_func3();
SELECT COUNT(*) > 0 FROM
(
SELECT * FROM gp_read_error_log('exttab_udfs_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_udfs_2')
) FOO;

SELECT gp_truncate_error_log('exttab_udfs_1');
SELECT gp_truncate_error_log('exttab_udfs_2');
SELECT exttab_udfs_func3();
SELECT COUNT(*) > 0 FROM
(
SELECT * FROM gp_read_error_log('exttab_udfs_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_udfs_2')
) FOO;

SELECT gp_truncate_error_log('exttab_udfs_1');
SELECT gp_truncate_error_log('exttab_udfs_2');
INSERT INTO exttab_udfs_insert_1 SELECT * FROM exttab_udfs_func3();
SELECT COUNT(*) > 0 FROM
(
SELECT * FROM gp_read_error_log('exttab_udfs_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_udfs_2')
) FOO;

SELECT gp_truncate_error_log('exttab_udfs_1');
SELECT gp_truncate_error_log('exttab_udfs_2');
CREATE TABLE exttab_udfs_ctas_4 AS SELECT * FROM exttab_udfs_func3();
SELECT COUNT(*) > 0 FROM
(
SELECT * FROM gp_read_error_log('exttab_udfs_1')
UNION ALL
SELECT * FROM gp_read_error_log('exttab_udfs_2')
) FOO;