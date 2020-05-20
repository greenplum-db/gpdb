-- -----------------------------------------------------------------
-- Test extensions to functions (MPP-16060)
-- 	1. data access indicators
-- -----------------------------------------------------------------

-- test prodataaccess
create function func1(int, int) returns int as
$$
  select $1 + $2;
$$ language sql immutable contains sql;

-- check prodataaccess column in pg_proc
select proname, prodataaccess from pg_proc where proname = 'func1';

-- check prodataaccess in pg_attribute
select relname, attname, attlen from pg_class c, pg_attribute
where attname = 'prodataaccess' and attrelid = c.oid and c.relname = 'pg_proc';

create function func2(a anyelement, b anyelement, flag bool)
returns anyelement as
$$
  select $1 + $2;
$$ language sql reads sql data;

-- check prodataaccess column in pg_proc
select proname, prodataaccess from pg_proc where proname = 'func2';

create function func3() returns oid as
$$
  select oid from pg_class where relname = 'pg_type';
$$ language sql modifies sql data volatile;

-- check prodataaccess column in pg_proc
select proname, prodataaccess from pg_proc where proname = 'func3';

-- check default value of prodataaccess
drop function func1(int, int);
create function func1(int, int) returns varchar as $$
declare
	v_name varchar(20) DEFAULT 'zzzzz';
begin
	select relname from pg_class into v_name where oid=$1;
	return v_name;
end;
$$ language plpgsql READS SQL DATA;

select proname, proargnames, prodataaccess from pg_proc where proname = 'func1';

create function func4(int, int) returns int as
$$
  select $1 + $2;
$$ language sql CONTAINS SQL;

-- check prodataaccess column
select proname, proargnames, prodataaccess from pg_proc where proname = 'func4';

-- change prodataaccess option
create or replace function func4(int, int) returns int as
$$
  select $1 + $2;
$$ language sql modifies sql data;

select proname, proargnames, prodataaccess from pg_proc where proname = 'func4';

-- upper case language name
create or replace function func5(int) returns int as
$$
  select $1;
$$ language SQL;

-- check prodataaccess column
select proname, proargnames, prodataaccess from pg_proc where proname = 'func5';

-- alter function with data access
alter function func5(int) reads sql data;

-- check prodataaccess column
select proname, proargnames, prodataaccess from pg_proc where proname = 'func5';

-- alter function with data access
alter function func5(int) modifies sql data;

-- check prodataaccess column
select proname, proargnames, prodataaccess from pg_proc where proname = 'func5';

-- alter function with data access
alter function func5(int) no sql;

-- alter function with data access
alter function func5(int) volatile contains sql;

alter function func5(int) immutable reads sql data;
alter function func5(int) immutable modifies sql data;

-- data_access indicators for plpgsql
drop function func1(int, int);
create or replace function func1(int, int) returns varchar as $$
declare
	v_name varchar(20) DEFAULT 'zzzzz';
begin
	select relname from pg_class into v_name where oid=$1;
	return v_name;
end;
$$ language plpgsql reads sql data;

select proname, proargnames, prodataaccess from pg_proc where proname = 'func1';

-- check conflicts
drop function func1(int, int);
create function func1(int, int) returns int as
$$
  select $1 + $2;
$$ language sql immutable no sql;

create function func1(int, int) returns int as
$$
  select $1 + $2;
$$ language sql immutable reads sql data;

-- stable function with modifies data_access
create table bar (c int, d int);
create function func1_mod_int_stb(x int) returns int as $$
begin
	update bar set d = d+1 where c = $1;
	return $1 + 1;
end
$$ language plpgsql stable modifies sql data;
select * from func1_mod_int_stb(5) order by 1;

drop function func2(anyelement, anyelement, bool);
drop function func3();
drop function func4(int, int);
drop function func5(int);
drop function func1_mod_int_stb(int);


-- Test EXECUTE ON [ANY | MASTER | ALL SEGMENTS ]

CREATE TABLE srf_testtab (t text) DISTRIBUTED BY (t);
INSERT INTO srf_testtab VALUES ('foo 0');
INSERT INTO srf_testtab VALUES ('foo 1');
INSERT INTO srf_testtab VALUES ('foo -1');

create function srf_on_master () returns setof text as $$
begin
  return next 'foo ' || current_setting('gp_contentid');
  return next 'bar ' || current_setting('gp_contentid');
end;
$$ language plpgsql EXECUTE ON MASTER;

-- A function with ON MASTER or ON ALL SEGMENTS is only allowed in the target list
-- in the simple case with no FROM.
select srf_on_master();
select srf_on_master() FROM srf_testtab;

-- In both these cases, the function should run on master and hence return
-- ('foo -1'), ('bar -1')
select * from srf_on_master();
select * from srf_testtab, srf_on_master();

-- Should run on master, even when used in a join. (With EXECUTE ON ANY,
-- it would be pushed to segments.)
select * from srf_testtab, srf_on_master() where srf_on_master = srf_testtab.t;

-- Repeat, with EXECUTE ON ALL SEGMENTS

create function srf_on_all_segments () returns setof text as $$
begin

  -- To make the output reproducible, regardless of the number of segments in
  -- the cluster, only return rows on segments 0 and 1
  if current_setting('gp_contentid')::integer < 2 then
    return next 'foo ' || current_setting('gp_contentid');
    return next 'bar ' || current_setting('gp_contentid');
  end if;
end;
$$ language plpgsql EXECUTE ON ALL SEGMENTS;

select srf_on_all_segments();
select srf_on_all_segments() FROM srf_testtab;
select * from srf_on_all_segments();
select * from srf_testtab, srf_on_all_segments();

select * from srf_testtab, srf_on_all_segments() where srf_on_all_segments = srf_testtab.t;

-- And with EXEUCTE ON ANY.

create function test_srf () returns setof text as $$
begin
  return next 'foo';
end;
$$ language plpgsql EXECUTE ON ANY IMMUTABLE;

-- Set the owner, to make the output of the \df+ tests below repeatable,
-- regardless of the name of the current user.
CREATE ROLE srftestuser;
ALTER FUNCTION test_srf() OWNER TO srftestuser;

select test_srf();
select test_srf() FROM srf_testtab;
select * from test_srf();

-- Since the function is marked as EXECUTE ON ANY, and IMMUTABLE, the planner
-- can choose to run it anywhere.
explain select * from srf_testtab, test_srf();
explain select * from srf_testtab, test_srf() where test_srf = srf_testtab.t;

-- Test ALTER FUNCTION, and that \df displays the EXECUTE ON correctly

\df+ test_srf

alter function test_srf() EXECUTE ON MASTER;
\df+ test_srf

alter function test_srf() EXECUTE ON ALL SEGMENTS;
\df+ test_srf

alter function test_srf() EXECUTE ON ANY;
\df+ test_srf

DROP FUNCTION test_srf();
DROP ROLE srftestuser;

-- Test error propagation from segments
CREATE TABLE uniq_test(id int primary key);
CREATE OR REPLACE FUNCTION trigger_unique() RETURNS void AS $$
BEGIN
	INSERT INTO uniq_test VALUES (1);
	INSERT INTO uniq_test VALUES (1);
	EXCEPTION WHEN unique_violation THEN
		raise notice 'unique_violation';
END;
$$ LANGUAGE plpgsql volatile;
SELECT trigger_unique();

-- Test CTAS select * from f()
-- Above query will fail in past in f() contains DDLs.
-- Since CTAS is write gang and f() could only be run at EntryDB(QE)
-- But EntryDB and QEs cannot run DDLs which needs to do dispatch.
-- We introduce new function location 'EXECUTE ON INITPLAN' to run
-- the function on initplan to overcome the above issue.
CREATE or replace FUNCTION get_temp_file_num() returns text as
$$
import os
fileNum = len([name for name in os.listdir(os.environ['MASTER_DATA_DIRECTORY'] + '/base/pgsql_tmp') if name.startswith('FUNCTION_SCAN')])
return fileNum
$$ language plpythonu;

CREATE OR REPLACE FUNCTION get_country()
 RETURNS TABLE (
  country_id integer,
  country character varying(50)
  )

AS $$
  begin
  drop table if exists public.country;
  create table public.country( country_id integer,
    country character varying(50));
  insert into public.country
  (country_id, country)
  select 111,'INDIA'
  union all select 222,'CANADA'
  union all select 333,'USA' ;
  RETURN QUERY
  SELECT
  c.country_id,
  c.country
  FROM
  public.country c order by country_id;
  end; $$
LANGUAGE 'plpgsql' EXECUTE ON INITPLAN;

-- Temp file number before running INITPLAN function
SELECT get_temp_file_num();
SELECT * FROM get_country();
SELECT get_country();

DROP TABLE IF EXISTS t1_function_scan;
EXPLAIN CREATE TABLE t1_function_scan AS SELECT * FROM get_country();
CREATE TABLE t1_function_scan AS SELECT * FROM get_country();
INSERT INTO t1_function_scan SELECT * FROM get_country();
INSERT INTO t1_function_scan SELECT * FROM get_country();
SELECT count(*) FROM t1_function_scan;


-- test with limit clause
DROP TABLE IF EXISTS t1_function_scan_limit;
CREATE TABLE t1_function_scan_limit AS SELECT * FROM get_country() limit 2;
SELECT count(*) FROM t1_function_scan_limit;

-- test with order by clause
DROP TABLE IF EXISTS t1_function_scan_order_by;
CREATE TABLE t1_function_scan_order_by AS SELECT * FROM get_country() f1 ORDER BY f1.country_id DESC limit 1;
SELECT * FROM t1_function_scan_order_by;

-- test with group by clause
DROP TABLE IF EXISTS t1_function_scan_group_by;
CREATE TABLE t1_function_scan_group_by AS SELECT f1.country_id, count(*) FROM get_country() f1 GROUP BY f1.country_id;
SELECT count(*) FROM t1_function_scan_group_by;

-- test join table
DROP TABLE IF EXISTS t1_function_scan_join;
CREATE TABLE t1_function_scan_join AS SELECT f1.country_id, f1.country FROM get_country() f1, t1_function_scan_limit;
SELECT count(*) FROM t1_function_scan_join;

DROP TABLE IF EXISTS t2_function_scan;
CREATE TABLE t2_function_scan (id int, val int);
INSERT INTO t2_function_scan SELECT k, k+1 FROM generate_series(1,100000) AS k;

CREATE OR REPLACE FUNCTION get_id()
 RETURNS TABLE (
  id integer,
  val integer
  )
AS $$
  begin
  RETURN QUERY
  SELECT * FROM t2_function_scan;
  END; $$
LANGUAGE 'plpgsql' EXECUTE ON INITPLAN;

DROP TABLE IF EXISTS t3_function_scan;
CREATE TABLE t3_function_scan AS SELECT * FROM get_id();
SELECT count(*) FROM t3_function_scan;

-- abort case 1: abort before entrydb run the function scan
DROP TABLE IF EXISTS t4_function_scan;
CREATE TABLE t4_function_scan AS SELECT 444, (1 / (0* random()))::text UNION ALL SELECT * FROM get_country();

-- Temp file number after running INITPLAN function, number should not changed.
SELECT get_temp_file_num();

-- test join case with two INITPLAN functions
DROP TABLE IF EXISTS t5_function_scan;
CREATE TABLE t5_function_scan AS SELECT * FROM get_id(), get_country();
SELECT count(*) FROM t5_function_scan;

-- test union all 
DROP TABLE IF EXISTS t6_function_scan;
CREATE TABLE t6_function_scan AS SELECT 100/(1+ 1* random())::int id, 'cc'::text cc UNION ALL SELECT * FROM  get_country();
SELECT count(*) FROM t6_function_scan;

DROP TABLE IF EXISTS t7_function_scan;
CREATE TABLE t7_function_scan AS SELECT * FROM  get_country() UNION ALL SELECT 100/(1+ 1* random())::int, 'cc'::text;
SELECT count(*) FROM t7_function_scan;
