--
-- SELECT_INTO
--

SELECT *
   INTO TABLE tmp1
   FROM onek
   WHERE onek.unique1 < 2;

DROP TABLE tmp1;

SELECT *
   INTO TABLE tmp1
   FROM onek2
   WHERE onek2.unique1 < 2;

DROP TABLE tmp1;

--
-- SELECT INTO and INSERT permission, if owner is not allowed to insert.
--
CREATE SCHEMA selinto_schema;
CREATE USER regress_selinto_user;
ALTER DEFAULT PRIVILEGES FOR ROLE regress_selinto_user
	  REVOKE INSERT ON TABLES FROM regress_selinto_user;
GRANT ALL ON SCHEMA selinto_schema TO public;

SET SESSION AUTHORIZATION regress_selinto_user;
SELECT * INTO TABLE selinto_schema.tmp1
	  FROM pg_class WHERE relname like '%a%';	-- Error
SELECT oid AS clsoid, relname, relnatts + 10 AS x
	  INTO selinto_schema.tmp2
	  FROM pg_class WHERE relname like '%b%';	-- Error
CREATE TABLE selinto_schema.tmp3 (a,b,c)
	   AS SELECT oid,relname,relacl FROM pg_class
	   WHERE relname like '%c%';	-- Error
RESET SESSION AUTHORIZATION;

ALTER DEFAULT PRIVILEGES FOR ROLE regress_selinto_user
	  GRANT INSERT ON TABLES TO regress_selinto_user;

SET SESSION AUTHORIZATION regress_selinto_user;
SELECT * INTO TABLE selinto_schema.tmp1
	  FROM pg_class WHERE relname like '%a%';	-- OK
SELECT oid AS clsoid, relname, relnatts + 10 AS x
	  INTO selinto_schema.tmp2
	  FROM pg_class WHERE relname like '%b%';	-- OK
CREATE TABLE selinto_schema.tmp3 (a,b,c)
	   AS SELECT oid,relname,relacl FROM pg_class
	   WHERE relname like '%c%';	-- OK
RESET SESSION AUTHORIZATION;

DROP SCHEMA selinto_schema CASCADE;
DROP USER regress_selinto_user;

-- Tests for WITH NO DATA and column name consistency
CREATE TABLE ctas_base (i int, j int);
INSERT INTO ctas_base VALUES (1, 2);
CREATE TABLE ctas_nodata (ii, jj, kk) AS SELECT i, j FROM ctas_base; -- Error
CREATE TABLE ctas_nodata (ii, jj, kk) AS SELECT i, j FROM ctas_base WITH NO DATA; -- Error
CREATE TABLE ctas_nodata (ii, jj) AS SELECT i, j FROM ctas_base; -- OK
CREATE TABLE ctas_nodata_2 (ii, jj) AS SELECT i, j FROM ctas_base WITH NO DATA; -- OK
CREATE TABLE ctas_nodata_3 (ii) AS SELECT i, j FROM ctas_base; -- OK
CREATE TABLE ctas_nodata_4 (ii) AS SELECT i, j FROM ctas_base WITH NO DATA; -- OK
SELECT * FROM ctas_nodata;
SELECT * FROM ctas_nodata_2;
SELECT * FROM ctas_nodata_3;
SELECT * FROM ctas_nodata_4;
DROP TABLE ctas_base;
DROP TABLE ctas_nodata;
DROP TABLE ctas_nodata_2;
DROP TABLE ctas_nodata_3;
DROP TABLE ctas_nodata_4;

-- Test for WITH NO DATA on toast column
CREATE TABLE ctas_base (i text);
INSERT INTO ctas_base VALUES ('a');
CREATE TABLE ctas_nodata AS SELECT i FROM ctas_base  WITH NO DATA; -- OK
SELECT * FROM ctas_nodata;
DROP TABLE ctas_base;
DROP TABLE ctas_nodata;

--
-- CREATE TABLE AS/SELECT INTO as last command in a SQL function
-- have been known to cause problems
--
CREATE FUNCTION make_table() RETURNS VOID
AS $$
  CREATE TABLE created_table AS SELECT * FROM int8_tbl;
$$ LANGUAGE SQL;

SELECT make_table();

SELECT * FROM created_table;

-- Try EXPLAIN ANALYZE SELECT INTO, but hide the output since it won't
-- be stable.
DO $$
BEGIN
	EXECUTE 'EXPLAIN ANALYZE SELECT * INTO TABLE easi FROM int8_tbl';
END$$;

DROP TABLE created_table;
DROP TABLE easi;

--
-- Disallowed uses of SELECT ... INTO.  All should fail
--
DECLARE foo CURSOR FOR SELECT 1 INTO b;
COPY (SELECT 1 INTO frak UNION SELECT 2) TO 'blob';
SELECT * FROM (SELECT 1 INTO f) bar;
CREATE VIEW foo AS SELECT 1 INTO b;
INSERT INTO b SELECT 1 INTO f;

-- Github Issue 9365: https://github.com/greenplum-db/gpdb/pull/9391
-- Previously, the following CTAS case miss dispatching OIDs to QEs, which leads to
-- errors.
CREATE OR REPLACE FUNCTION array_unnest_2d_to_1d(
  x ANYARRAY,
  OUT unnest_row_id INT,
  OUT unnest_result ANYARRAY
)
RETURNS SETOF RECORD
AS
$BODY$
  SELECT t2.r::int, array_agg($1[t2.r][t2.c] order by t2.c) FROM
  (
    SELECT generate_series(array_lower($1,2),array_upper($1,2)) as c, t1.r
    FROM
    (
      SELECT generate_series(array_lower($1,1),array_upper($1,1)) as r
    ) t1
  ) t2
GROUP BY t2.r
$BODY$ LANGUAGE SQL IMMUTABLE
;

DROP TABLE IF EXISTS unnest_2d_tbl01;
CREATE TABLE unnest_2d_tbl01 (id INT, val DOUBLE PRECISION[][]);
INSERT INTO unnest_2d_tbl01 VALUES
  (1, ARRAY[[1::float8,2],[3::float8,4],[5::float8,6],[7::float8,8]]),
  (2, ARRAY[[101::float8,202],[303::float8,404],[505::float8,606]])
;
DROP TABLE IF EXISTS unnest_2d_tbl01_out;
-- The following CTAS fails previously, see Github Issue 9365
CREATE TABLE unnest_2d_tbl01_out AS
  SELECT id, (array_unnest_2d_to_1d(val)).* FROM unnest_2d_tbl01;
