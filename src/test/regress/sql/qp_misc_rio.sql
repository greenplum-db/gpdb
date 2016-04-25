-- All the tables, functions, etc. in this test file are created in
-- qp_misc_rio schema, so that they don't interfere with other tests
-- running in parallel.

-- start_ignore
create schema qp_misc_rio;

CREATE LANGUAGE plpythonu;
-- end_ignore

-- ----------------------------------------------------------------------
-- Test: 2 (schema drop)
-- ----------------------------------------------------------------------

select 
    table_schema, table_name, column_name, ordinal_position
from 
    information_schema.columns
where 
    table_schema ='test_schema'
    and ordinal_position =1;

create schema test_schema;
create table test_schema.test_table(c int);

-- EXPECT NO ERROR of 'violates check constraint' BEFORE drop schema
select table_schema, table_name,column_name,ordinal_position 
from information_schema.columns 
where table_schema ='test_schema' and ordinal_position =1;

drop table test_schema.test_table;
drop SCHEMA test_schema;

-- EXPECT NO ERROR of 'violates check constraint' AFTER drop schema
select table_schema, table_name,column_name,ordinal_position 
from information_schema.columns 
where table_schema ='test_schema' and ordinal_position =1;

-- EXPECT NO ERROR of 'violates check constraint'
select * 
FROM (
	select attnum::information_schema.cardinal_number 
	from pg_attribute 
	where attnum > 0) q 
where attnum = 4 limit 10;

-- ----------------------------------------------------------------------
-- Test: 3
-- ----------------------------------------------------------------------

--start_ignore
drop resource queue t3_test_q;
--end_ignore

-- Create resource queue with cost_overcommit=true
create resource queue t3_test_q with (active_statements = 6,max_cost=5e+06 ,cost_overcommit=true, min_cost=50000);

select * from pg_resqueue where rsqname='t3_test_q';

-- Increase cost threshold
alter resource queue t3_test_q with (max_cost=7e6);

select * from pg_resqueue where rsqname='t3_test_q';

-- Decrease cost threshold
alter resource queue t3_test_q with (max_cost=1e2);

select * from pg_resqueue where rsqname='t3_test_q';

drop resource queue t3_test_q;

-- ----------------------------------------------------------------------
-- Test: 4
-- ----------------------------------------------------------------------
SELECT to_date(to_char(20110521, '99999999'),'YYYYMMDD'), to_char(20110521,'99999999'), 20110521;

-- ----------------------------------------------------------------------
-- Test: 5
-- ----------------------------------------------------------------------
select mregr_pvalues(4, array[1,i]) from generate_series(1, 500) i;

-- ----------------------------------------------------------------------
-- Test: 6
-- ----------------------------------------------------------------------
select row();

-- ----------------------------------------------------------------------
-- Test: 8
-- ----------------------------------------------------------------------

set search_path to qp_misc_rio;

create table tbl_t8 (c1 int, c2 int) with (appendonly=true, compresstype=none, compresslevel=2) distributed by (c1);
insert into tbl_t8 values (1,2);

-- ----------------------------------------------------------------------
-- Test: 9
-- ----------------------------------------------------------------------
-- Expect NO ERROR like "ERROR:  Unexpected internal error (cdbsetop.c)"

set search_path to qp_misc_rio;

create table tb_function_test(a numeric,b numeric,c numeric,d character varying(20),e character varying(20)) distributed by (b,c);
select *,row_number() over(partition by a,b,c order by d),row_number() over(partition by a,b,c order by e) from tb_function_test where  b=1;

select *,row_number() over(partition by a,b,c order by d),row_number() over(partition by a,b,c order by e) from tb_function_test where  c=1;

select *,row_number() over(partition by a,b,c order by d),row_number() over(partition by a,b,c order by e) from tb_function_test where  a=1;

select *,row_number() over(partition by a,b,c order by d),row_number() over(partition by a,b,c order by e) from tb_function_test where b=(select a from tb_function_test limit 1);

-- ----------------------------------------------------------------------
-- Test: 10
-- ----------------------------------------------------------------------
-- Test \d+ after drop partition

set search_path to qp_misc_rio;

create table t10_t ( a int, b text) partition by range(a) (start (1) end (100) every(20));

insert into t10_t values ( generate_series(1,99),'t10_t_1');

create index t10_t_a on t10_t using bitmap(a);

create index t10_t_b on t10_t using bitmap(b);

\d+ t10_t

Alter table t10_t drop partition for (rank(1));

\d+ t10_t

-- ----------------------------------------------------------------------
-- Test: 11
-- ----------------------------------------------------------------------

set search_path to qp_misc_rio;

create table t11_t(a bigint, b bigint) distributed by (a);
insert into t11_t select a, a / 10 from generate_series(1, 100)a;

select sum((select count(*) from t11_t group by b having b = s.b)) as sum_col from (select * from t11_t order by a)s group by b order by sum_col;

-- ----------------------------------------------------------------------
-- Test: 14
-- ----------------------------------------------------------------------

set search_path to qp_misc_rio;

create table tbl_t14 as select * from gp_id DISTRIBUTED RANDOMLY;

select array(select dbid from gp_id);
select array(select dbid from tbl_t14);

-- ----------------------------------------------------------------------
-- Test: 15
-- ----------------------------------------------------------------------
-- aggregate over partition by
select state,
       sum(revenue) over (partition by state)
from
   (select 'A' as enc_email, 1 as revenue) b
   join (select 'A' as enc_email, 'B' as state ) c using(enc_email)
group by 1,b.revenue;

-- ----------------------------------------------------------------------
-- Test: 16
-- ----------------------------------------------------------------------

set search_path to qp_misc_rio;

CREATE TABLE testtable0000 AS SELECT spend, row_number() OVER (PARTITION BY 0) AS i, (spend % 2) AS r
FROM (select generate_series(1,10) as spend) x DISTRIBUTED RANDOMLY;

CREATE TABLE testtable0001 AS SELECT *, CASE WHEN (i % 6 = 0) THEN '00'
     WHEN (i % 6 = 1) THEN '11'
     WHEN (i % 6 = 2) THEN '22'
     WHEN (i % 6 = 3) THEN '33'
     WHEN (i % 6 = 4) THEN '44'
     WHEN (i % 6 = 5) THEN '55' END AS s1,
CASE WHEN (i % 6 = 0) THEN '00'
     WHEN (i % 6 = 1) THEN '11'
     WHEN (i % 6 = 2) THEN '22'
     WHEN (i % 6 = 3) THEN '33'
     WHEN (i % 6 = 4) THEN '44'
     WHEN (i % 6 = 5) THEN '55' END AS s2,
CASE WHEN (i % 6 = 0) THEN '00'
     WHEN (i % 6 = 1) THEN '11'
     WHEN (i % 6 = 2) THEN '22'
     WHEN (i % 6 = 3) THEN '33'
     WHEN (i % 6 = 4) THEN '44'
     WHEN (i % 6 = 5) THEN '55' END AS s3,
CASE WHEN (i % 6 = 0) THEN '00'
     WHEN (i % 6 = 1) THEN '11'
     WHEN (i % 6 = 2) THEN '22'
     WHEN (i % 6 = 3) THEN '33'
     WHEN (i % 6 = 4) THEN '44'
     WHEN (i % 6 = 5) THEN '55' END AS s4,
CASE WHEN (i % 6 = 0) THEN '00'
     WHEN (i % 6 = 1) THEN '11'
     WHEN (i % 6 = 2) THEN '22'
     WHEN (i % 6 = 3) THEN '33'
     WHEN (i % 6 = 4) THEN '44'
     WHEN (i % 6 = 5) THEN '55' END AS s5 FROM testtable0000;



CREATE VIEW testtable0002
AS SELECT testtable0001.*,
          miro_foo.s1_xform
FROM testtable0001
JOIN (SELECT s1,
                  COALESCE((AVG(CAST(r AS INT)) - 0.010000), 0)
		  AS s1_xform
           FROM testtable0001 GROUP BY s1)
	   AS miro_foo
ON testtable0001.s1 = miro_foo.s1;

SELECT MIN(s1_xform), MIN(s1_xform) FROM testtable0002;




SELECT s1,
       SUM(CAST(r AS INT)),
       COUNT(*) FILTER (WHERE r IS NOT NULL),
       COUNT(*)
FROM testtable0002 GROUP BY s1 order by s1;


CREATE VIEW testtable0003
AS SELECT testtable0002.*,
          miro_foo.s2_xform
FROM testtable0002
JOIN (SELECT s2,
                  COALESCE((AVG(CAST(r AS INT)) - 0.020000), 0)
		  AS s2_xform
           FROM testtable0002 GROUP BY s2)
	   AS miro_foo
ON testtable0002.s2 = miro_foo.s2;

SELECT MIN(s2_xform), MIN(s2_xform) FROM testtable0003;



SELECT s2,
       SUM(CAST(r AS INT)),
       COUNT(*) FILTER (WHERE r IS NOT NULL),
       COUNT(*)
FROM testtable0003 GROUP BY s2 order by s2;


CREATE VIEW testtable0004
AS SELECT testtable0003.*,
          miro_foo.s3_xform
FROM testtable0003
JOIN (SELECT s3,
                  COALESCE((AVG(CAST(r AS INT)) - 0.030000), 0)
		  AS s3_xform
           FROM testtable0003 GROUP BY s3)
	   AS miro_foo
ON testtable0003.s3 = miro_foo.s3;

SELECT MIN(s3_xform), MIN(s3_xform) FROM testtable0004;



SELECT s3,
       SUM(CAST(r AS INT)),
       COUNT(*) FILTER (WHERE r IS NOT NULL),
       COUNT(*)
FROM testtable0004 GROUP BY s3 order by s3;


CREATE VIEW testtable0005
AS SELECT testtable0004.*,
          miro_foo.s4_xform
FROM testtable0004
JOIN (SELECT s4,
                  COALESCE((AVG(CAST(r AS INT)) - 0.040000), 0)
		  AS s4_xform
           FROM testtable0004 GROUP BY s4)
	   AS miro_foo
ON testtable0004.s4 = miro_foo.s4;

SELECT MIN(s4_xform), MIN(s4_xform) FROM testtable0005;



SELECT s4,
       SUM(CAST(r AS INT)),
       COUNT(*) FILTER (WHERE r IS NOT NULL),
       COUNT(*)
FROM testtable0005 GROUP BY s4 order by s4;


CREATE VIEW testtable0006
AS SELECT testtable0005.*,
          miro_foo.s5_xform
FROM testtable0005
JOIN (SELECT s5,
                  COALESCE((AVG(CAST(r AS INT)) - 0.050000), 0)
		  AS s5_xform
           FROM testtable0005 GROUP BY s5)
	   AS miro_foo
ON testtable0005.s5 = miro_foo.s5;

SELECT MIN(s5_xform), MIN(s5_xform) FROM testtable0006;



SELECT s5,
       SUM(CAST(r AS INT)),
       COUNT(*) FILTER (WHERE r IS NOT NULL),
       COUNT(*)
FROM testtable0006 GROUP BY s5 order by s5;

-- ----------------------------------------------------------------------
-- Test: 17
-- ----------------------------------------------------------------------

-- series boundary is 8
CREATE TEMPORARY TABLE regtest_8 AS
SELECT 1::FLOAT8 AS y,
       ARRAY(SELECT no::FLOAT8
             FROM generate_series(1,8) AS no ) AS x
DISTRIBUTED BY (y);

SELECT mregr_coef(y, x) FROM regtest_8;

SELECT mregr_coef(y, x)
FROM (
      SELECT 1::FLOAT8 AS y,
      ARRAY( SELECT no::FLOAT8 FROM generate_series(1,8) AS no ) AS x
) AS one_row_subquery;

-- series boundary is 8191
CREATE TEMPORARY TABLE regtest_8191 AS
SELECT 1::FLOAT8 AS y,
       ARRAY(SELECT no::FLOAT8
             FROM generate_series(1,8191) AS no ) AS x
DISTRIBUTED BY (y);

SELECT mregr_coef(y, x) FROM regtest_8191;

SELECT mregr_coef(y, x)
FROM (
      SELECT 1::FLOAT8 AS y,
      ARRAY( SELECT no::FLOAT8 FROM generate_series(1,8191) AS no ) AS x
) AS one_row_subquery;

-- series boundary is 8192
CREATE TEMPORARY TABLE regtest_8192 AS
SELECT 1::FLOAT8 AS y,
       ARRAY(SELECT no::FLOAT8
             FROM generate_series(1,8192) AS no ) AS x
DISTRIBUTED BY (y);

SELECT mregr_coef(y, x) FROM regtest_8192;

SELECT mregr_coef(y, x)
FROM (
      SELECT 1::FLOAT8 AS y,
      ARRAY( SELECT no::FLOAT8 FROM generate_series(1,8192) AS no ) AS x
) AS one_row_subquery;

-- create temporary table with series 2^16 and select the table
CREATE TEMPORARY TABLE regtest_65536 AS
SELECT 1::FLOAT8 AS y,
      ARRAY(SELECT no::FLOAT8
            FROM generate_series(1,(2^16)::INTEGER) AS no ) AS x
DISTRIBUTED BY (y);

SELECT mregr_coef(y, x) FROM regtest_65536;

SELECT mregr_coef(y, x)
FROM (
      SELECT 1::FLOAT8 AS y,
      ARRAY( SELECT no::FLOAT8 FROM generate_series(1,(2^16)::INTEGER) AS no ) AS x
) AS one_row_subquery;


-- ----------------------------------------------------------------------
-- Test: 18
-- ----------------------------------------------------------------------

set search_path to qp_misc_rio;

CREATE FUNCTION t18_pytest() RETURNS VOID LANGUAGE plpythonu AS $$
  plpy.execute("SHOW client_min_messages")
$$;

SELECT t18_pytest();

DROP FUNCTION t18_pytest();

CREATE FUNCTION t18_pytest() RETURNS VARCHAR LANGUAGE plpythonu AS $$
  return plpy.execute("SELECT setting FROM pg_settings WHERE name='client_min_messages'")[0]['setting']
$$;

SELECT t18_pytest();

-- ----------------------------------------------------------------------
-- Test: 20
-- ----------------------------------------------------------------------
select distinct paramname
from gp_toolkit.gp_param_setting('allow_system_table_mods');

select distinct paramname
from gp_toolkit.gp_param_setting('max_resource_queues');

-- ----------------------------------------------------------------------
-- Test: 21
-- ----------------------------------------------------------------------
set search_path to qp_misc_rio;

create table parts (
    partnum     text,
    cost        float8
);

create table shipped (
    ttype       char(2),
    ordnum      int4,
    partnum     text,
    value       float8
);

create view shipped_view as
    select * from shipped where ttype = 'wt';


insert into parts (partnum, cost) values (1, 1234.56);

insert into shipped (ordnum, partnum, value)
    values (0, 1, (select cost from parts where partnum = 1));

select * from shipped_view ORDER BY 1,2;

-- ----------------------------------------------------------------------
-- Test: 22
-- ----------------------------------------------------------------------
-- SMALLINT: [-32768, 32767]
SELECT matrix_add(ARRAY[32767]::SMALLINT[], ARRAY[1]::SMALLINT[]);
SELECT matrix_add(ARRAY[32766]::SMALLINT[], ARRAY[1]::SMALLINT[]);
SELECT matrix_add(ARRAY[-32768]::SMALLINT[], ARRAY[-1]::SMALLINT[]);
SELECT matrix_add(ARRAY[-32767]::SMALLINT[], ARRAY[-1]::SMALLINT[]);
SELECT matrix_add(ARRAY[ARRAY[16000]]::SMALLINT[], ARRAY[ARRAY[32000]]::SMALLINT[]);

-- INT: [-2147483648, 2147483647]
SELECT matrix_add(ARRAY[2147483647]::INT[], ARRAY[1]::INT[]);
SELECT matrix_add(ARRAY[2147483646]::INT[], ARRAY[1]::INT[]);
SELECT matrix_add(ARRAY[-2147483648]::INT[], ARRAY[-1]::INT[]);
SELECT matrix_add(ARRAY[-2147483647]::INT[], ARRAY[-1]::INT[]);

-- BIGINT: [-9223372036854775808, 9223372036854775807]
SELECT matrix_add(ARRAY[9223372036854775807]::BIGINT[], ARRAY[1]::BIGINT[]);
SELECT matrix_add(ARRAY[9223372036854775806]::BIGINT[], ARRAY[1]::INT[]);
SELECT matrix_add(ARRAY[-9223372036854775808]::BIGINT[], ARRAY[-1]::BIGINT[]);
SELECT matrix_add(ARRAY[-9223372036854775807]::BIGINT[], ARRAY[-1]::INT[]);

-- matrix_multiply WILL PROMOTE RESULT TO INT64 OR FLOAT8 AUTOMATICALLY
SELECT matrix_multiply(ARRAY[ARRAY[9223372036854775807/3]]::BIGINT[], ARRAY[ARRAY[4]]::BIGINT[]);
SELECT matrix_multiply(ARRAY[ARRAY[-9223372036854775808]]::BIGINT[], ARRAY[ARRAY[-1]]::BIGINT[]);
SELECT matrix_multiply(ARRAY[ARRAY[10E200], ARRAY[10E200]]::FLOAT8[], ARRAY[ARRAY[10E200]]::FLOAT8[]);
SELECT matrix_multiply(ARRAY[ARRAY[16000000]]::INT[], ARRAY[ARRAY[32000000]]::INT[]);

-- ----------------------------------------------------------------------
-- Test: 23
-- ----------------------------------------------------------------------
set search_path to qp_misc_rio;

CREATE OR REPLACE FUNCTION func_array_argument_plpythonu(arg FLOAT8[])
RETURNS FLOAT8
AS $$
    return arg[0];
$$ LANGUAGE plpythonu;

SELECT func_array_argument_plpythonu('{1,2,3}');

-- ----------------------------------------------------------------------
-- Test: 24
-- ----------------------------------------------------------------------

set search_path to qp_misc_rio;

CREATE OR REPLACE FUNCTION func_setof_spi_in_iterator_plpythonu()
RETURNS SETOF text
AS $$
    for s in ('Hello', 'Brave', 'New', 'World'):
        plpy.execute('select 1')
        yield s
        plpy.execute('select 2')
$$ LANGUAGE plpythonu;

SELECT func_setof_spi_in_iterator_plpythonu();

-- ----------------------------------------------------------------------
-- Test: 25
-- ----------------------------------------------------------------------

set search_path to qp_misc_rio;
CREATE TYPE tuple_split AS (a INT8, b INT8);
CREATE OR REPLACE FUNCTION func_split_plpythonu(input INT8)
RETURNS SETOF tuple_split
AS $$
    yield [input, input];
    yield [input, input]
$$ LANGUAGE plpythonu;

SELECT * FROM func_split_plpythonu(10);
SELECT func_split_plpythonu(10);
SELECT (func_split_plpythonu(10)).*;


-- ----------------------------------------------------------------------
-- Test: 26
-- ----------------------------------------------------------------------

set search_path to qp_misc_rio;
CREATE OR REPLACE FUNCTION func_exec_query_plpythonu( query text )
RETURNS boolean
AS $$
    try:
        plan = plpy.prepare( query )
        rv = plpy.execute( plan )
    except:
        plpy.notice( 'Error trapped' )
        return 'false'

    for r in rv:
        plpy.notice( str( r ) )

    return 'true'
$$ LANGUAGE plpythonu;

SELECT func_exec_query_plpythonu( 'SELECT 1' );
SELECT func_exec_query_plpythonu( 'SELECT x' );

-- ----------------------------------------------------------------------
-- Test: 27
-- ----------------------------------------------------------------------

set search_path to qp_misc_rio;

set gp_autostats_mode to 'ON_NO_STATS';
set gp_autostats_mode_in_functions to 'NONE';

-- prepare function and table
CREATE OR REPLACE FUNCTION func_truncate_load_plpgsql()
RETURNS void
AS $$
    BEGIN
        EXECUTE 'TRUNCATE TABLE tbl_truncate_load;';
        EXECUTE 'INSERT INTO tbl_truncate_load SELECT i, i FROM generate_series(1, 10000000) i;';
    END;
$$ LANGUAGE plpgsql;

CREATE TABLE tbl_truncate_load (c1 int, c2 int) DISTRIBUTED BY (c1);

-- show default GUC value for gp_autostats_mode_in_functions
SELECT current_setting('gp_autostats_mode');
SELECT current_setting('gp_autostats_mode_in_functions');


-- test function with GUC value for gp_autostats_mode_in_functions as ON_NO_STATS
SELECT set_config('gp_autostats_mode_in_functions', 'ON_NO_STATS', False);

TRUNCATE TABLE tbl_truncate_load;
INSERT INTO tbl_truncate_load SELECT i, i FROM generate_series(1, 1000000) i;
-- check if the difference between reltuples and number of records for table tbl_truncate_load is within +-5%
SELECT CASE WHEN abs(reltuples-1000000)/1000000 < 0.05 THEN 'reltuples and number of records for table tbl_truncate_load are consistent'
            ELSE 'reltuples and number of records for table tbl_truncate_load are inconsistent'
       END AS remark
FROM pg_class WHERE oid='tbl_truncate_load'::regclass;

SELECT COUNT(*) FROM tbl_truncate_load;

SELECT func_truncate_load_plpgsql();
-- check if the difference between reltuples and number of records for table tbl_truncate_load is within +-5%
SELECT CASE WHEN abs(reltuples-10000000)/10000000 < 0.05 THEN 'reltuples and number of records for table tbl_truncate_load are consistent'
            ELSE 'reltuples and number of records for table tbl_truncate_load are inconsistent'
       END AS remark
FROM pg_class WHERE oid='tbl_truncate_load'::regclass;
SELECT count(*) FROM tbl_truncate_load;


-- test function with GUC value for gp_autostats_mode_in_functions as NONE
SELECT set_config('gp_autostats_mode_in_functions', 'NONE', False);

TRUNCATE TABLE tbl_truncate_load;
INSERT INTO tbl_truncate_load SELECT i, i FROM generate_series(1, 1000000) i;

-- check if the difference between reltuples and number of records for table tbl_truncate_load is within +-5%
SELECT CASE WHEN abs(reltuples-1000000)/1000000 < 0.05 THEN 'reltuples and number of records for table tbl_truncate_load are consistent'
            ELSE 'reltuples and number of records for table tbl_truncate_load are inconsistent'
       END AS remark
FROM pg_class WHERE oid='tbl_truncate_load'::regclass;

SELECT COUNT(*) FROM tbl_truncate_load;

SELECT func_truncate_load_plpgsql();

-- check if the difference between reltuples and number of records for table tbl_truncate_load is within +-5%
SELECT CASE WHEN abs(reltuples-10000000)/10000000 < 0.05 THEN 'reltuples and number of records for table tbl_truncate_load are consistent'
            ELSE 'reltuples and number of records for table tbl_truncate_load are inconsistent'
       END AS remark
FROM pg_class WHERE oid='tbl_truncate_load'::regclass;

SELECT count(*) FROM tbl_truncate_load;


-- clean up
RESET ALL;

-- ----------------------------------------------------------------------
-- Test: 28
-- ----------------------------------------------------------------------

set search_path to qp_misc_rio;

CREATE OR REPLACE FUNCTION func_oneline_plpythonu()
RETURNS text
AS $$
return "No spaces"
$$ LANGUAGE plpythonu;

CREATE OR REPLACE FUNCTION func_multiline1_plpythonu()
RETURNS text
AS $$
return """ One space
  Two spaces
   Three spaces
No spaces"""
$$ LANGUAGE plpythonu;

CREATE OR REPLACE FUNCTION func_multiline2_plpythonu()
RETURNS text
AS $$
# If there's something in my comment it can mess things up
return """
The ' in the comment should not cause this line to begin with a tab
""" + 'This is a rather long string containing\n\
    several lines of text just as you would do in C.\n\
     Note that whitespace at the beginning of the line is\
significant. The string can contain both \' and ".\n' + r"This is an another long string containing\n\
two lines of text and defined with the r\"...\" syntax."
$$ LANGUAGE plpythonu;

CREATE OR REPLACE FUNCTION func_multiline3_plpythonu()
RETURNS text
AS $$
# This is a comment
x = """
  # This is not a comment so the quotes at the end of the line do end the string """
return x
$$ LANGUAGE plpythonu;

SELECT func_oneline_plpythonu() UNION ALL
SELECT func_multiline1_plpythonu() UNION ALL
SELECT func_multiline2_plpythonu() UNION ALL
SELECT func_multiline3_plpythonu();

-- ----------------------------------------------------------------------
-- Test: 29
-- ----------------------------------------------------------------------
SELECT * FROM pg_pltemplate WHERE tmplname LIKE '%pljava%' ORDER BY tmplname;

-- ----------------------------------------------------------------------
-- Test: 30
-- ----------------------------------------------------------------------

set search_path to qp_misc_rio;

CREATE TABLE nt (i INT, j INT) DISTRIBUTED BY (j);
INSERT INTO nt SELECT i, i FROM generate_series(1,10) i;

SELECT lag(j) OVER (ORDER BY i ROWS BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING) FROM nt;
SELECT lag(j) OVER (w) FROM nt WINDOW w AS (ORDER BY i ROWS BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING);
SELECT lag(x) OVER (wx) FROM (SELECT 1 AS x, 2 AS y, 3 AS z) s WINDOW w AS (PARTITION BY y ORDER BY z), wx AS (w);

SELECT lead(j) OVER (ORDER BY i ROWS BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING) FROM nt;
SELECT lead(j) OVER (w) FROM nt WINDOW w AS (ORDER BY i ROWS BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING);
SELECT lead(x) OVER (wx) FROM (SELECT 1 AS x, 2 AS y, 3 AS z) s WINDOW w AS (PARTITION BY y ORDER BY z), wx AS (w);

-- ----------------------------------------------------------------------
-- Test: 32
-- ----------------------------------------------------------------------

set search_path to qp_misc_rio;

-- Infinity value
CREATE TABLE tbl_test_data(x float, y float) DISTRIBUTED BY (x);
INSERT INTO tbl_test_data VALUES(1,10);
INSERT INTO tbl_test_data VALUES('Infinity',20);

SELECT mregr_coef(y,array[x,1]::float[]) FROM tbl_test_data;
SELECT mregr_r2(y,array[x,1]::float[]) FROM tbl_test_data;
SELECT mregr_tstats(y,array[x,1]::float[]) FROM tbl_test_data;
SELECT mregr_pvalues(y,array[x,1]::float[]) FROM tbl_test_data;

-- -Infinity value
DELETE FROM tbl_test_data;
INSERT INTO tbl_test_data VALUES(1,10);
INSERT INTO tbl_test_data VALUES('-Infinity',20);

SELECT mregr_coef(y,array[x,1]::float[]) FROM tbl_test_data;
SELECT mregr_r2(y,array[x,1]::float[]) FROM tbl_test_data;
SELECT mregr_tstats(y,array[x,1]::float[]) FROM tbl_test_data;
SELECT mregr_pvalues(y,array[x,1]::float[]) FROM tbl_test_data;

-- NaN value
DELETE FROM tbl_test_data;
INSERT INTO tbl_test_data VALUES(1,10);
INSERT INTO tbl_test_data VALUES('NaN',20);

SELECT mregr_coef(y,array[x,1]::float[]) FROM tbl_test_data;
SELECT mregr_r2(y,array[x,1]::float[]) FROM tbl_test_data;
SELECT mregr_tstats(y,array[x,1]::float[]) FROM tbl_test_data;
SELECT mregr_pvalues(y,array[x,1]::float[]) FROM tbl_test_data;

-- NULL value
DELETE FROM tbl_test_data;
INSERT INTO tbl_test_data VALUES(1,10);
INSERT INTO tbl_test_data VALUES(NULL,20);

SELECT mregr_coef(y,array[x,1]::float[]) FROM tbl_test_data;
SELECT mregr_r2(y,array[x,1]::float[]) FROM tbl_test_data;
SELECT mregr_tstats(y,array[x,1]::float[]) FROM tbl_test_data;
SELECT mregr_pvalues(y,array[x,1]::float[]) FROM tbl_test_data;

-- ----------------------------------------------------------------------
-- Test: 33
-- ----------------------------------------------------------------------

set search_path to qp_misc_rio;

create table ccdd1 (a, b) as (select 1, 1 union select 1, 1 union select 1, 1);

select * from ccdd1;

-- ----------------------------------------------------------------------
-- Test: 34
-- ----------------------------------------------------------------------
-- This is expected to fail, with an error along the lines of:
-- function cannot execute on segment because it accesses relation "public.testdata_in"

set search_path to qp_misc_rio;

CREATE TABLE testdata_in ( c1 INT, c2 INT ) DISTRIBUTED BY (c1);
INSERT INTO testdata_in SELECT i, i FROM generate_series(1,100) i;
CREATE TABLE testdata_out ( c1 INT, c2 INT ) DISTRIBUTED BY (c1);

CREATE OR REPLACE FUNCTION func_plpythonu(n INT) RETURNS SETOF testdata_in
AS $$
        sqlstm = "SELECT * FROM testdata_in WHERE c1 <= %d ORDER BY c1;" % n
        return plpy.execute(sqlstm);
$$ LANGUAGE plpythonu;

INSERT INTO testdata_out SELECT * FROM func_plpythonu(10);

-- ----------------------------------------------------------------------
-- Test: 35
-- ----------------------------------------------------------------------

set search_path to qp_misc_rio;
CREATE OR REPLACE FUNCTION func_plpythonu2(x INT)
RETURNS INT
AS $$
     plpy.execute('DROP TABLE IF EXISTS tbl_plpythonu;')
     plpy.execute('CREATE TEMP TABLE tbl_plpythonu(col INT) DISTRIBUTED RANDOMLY;')
     for i in range(0, x):
         plpy.execute('INSERT INTO tbl_plpythonu VALUES(%d)' % i);
     return plpy.execute('SELECT COUNT(*) AS col FROM tbl_plpythonu;')[0]['col']
 $$ LANGUAGE plpythonu;


SELECT func_plpythonu2(200);

-- ----------------------------------------------------------------------
-- Test: 38
-- ----------------------------------------------------------------------

set search_path to qp_misc_rio;

-- start_ignore
drop role if exists triggertest_nopriv_a;
drop role if exists triggertest_nopriv_b;
-- end_ignore

-- Create a non-privileged user triggertest_nopriv_a
create role triggertest_nopriv_a with login ;

-- Create another non-privileged user triggertest_nopriv_b
create role triggertest_nopriv_b with login ;

GRANT ALL ON SCHEMA qp_misc_rio TO triggertest_nopriv_a;
GRANT ALL ON SCHEMA qp_misc_rio TO triggertest_nopriv_b;

-- Connect as non-privileged user "triggertest_nopriv_a"
SET ROLE triggertest_nopriv_a;
select user;

-- Create test table emp
CREATE TABLE emp (
    empname           text NOT NULL,
    salary            integer
);
-- Create a trigger function process_emp_audit()
begin;
create or replace function process_emp_audit() returns trigger as $$
begin
    raise notice '%', new.salary;
    return null;
end;
$$ language plpgsql security definer;
revoke all on function process_emp_audit() from public;
commit;

-- Create trigger using the trigger function

create trigger emp_audit
after insert on emp
    for each row execute procedure process_emp_audit();

-- Verified the trigger works correctly
insert into emp values ('Tammy', 100000);

-- connect as non-privileged user "triggertest_nopriv_b"
SET ROLE triggertest_nopriv_b;
select user;

-- Create test table emp
DROP TABLE IF EXISTS my_emp;
CREATE TABLE my_emp (
    empname           text NOT NULL,
    salary            integer
);

-- Create trigger using the trigger function process_emp_audit(),
-- which the current user does NOT have EXECUTE permission
create trigger my_emp_audit
after insert on my_emp
    for each row execute procedure process_emp_audit();

-- Verify that after grant trigger function's EXECUTE
-- permission, trigger can be created and executed correctly
-- Connect as trigger function's owner and grant EXECUTE permission
SET ROLE triggertest_nopriv_a;
grant execute on function process_emp_audit() to triggertest_nopriv_b;

-- connect as non-privileged user "triggertest_nopriv_b"
SET ROLE triggertest_nopriv_b;

-- Create trigger using the trigger function process_emp_audit(),
-- which the current user now has EXECUTE permission
-- the trigger should be created successfully
create trigger my_emp_audit
after insert on my_emp
    for each row execute procedure process_emp_audit();

-- Verified trigger can be run correctly
insert into my_emp values ('Tammy', 100000);

-- Now to confirm that we only check trigger function's EXECUTE
-- permission at trigger create time, but not at trigger run time
-- by revoking EXECUTE permission from triggertest_nopriv_b after
-- the trigger has been created
-- Connect as trigger function's owner and revoke EXECUTE permission
SET ROLE triggertest_nopriv_a;
revoke execute on function process_emp_audit() from triggertest_nopriv_b;

-- connect as non-privileged user "triggertest_nopriv_b"
SET ROLE triggertest_nopriv_b;

-- Verified that the existing trigger can still work
-- even the current user does NOT have the EXECUTE permission
-- on the trigger function.
insert into my_emp values ('Sammy', 100001);

-- Clean up
RESET ROLE;

-- ----------------------------------------------------------------------
-- Test: row_number() in subquery, with grouping in outer query
-- ----------------------------------------------------------------------

set search_path to qp_misc_rio;

create table bfv_legacy_mpp2(a int);

insert into bfv_legacy_mpp2 values (generate_series(1,10));

select  median(a), avg(a)
from
(
    select a,row_number() over (order by a)
    from bfv_legacy_mpp2
) sub1
group by a
order by a;

-- start_ignore
drop schema qp_misc_rio cascade;
-- end_ignore
