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
$$ language "SQL";

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
  return next 'foo ' || current_setting('gp_segment');
  return next 'bar ' || current_setting('gp_segment');
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
  if current_setting('gp_segment')::integer < 2 then
    return next 'foo ' || current_setting('gp_segment');
    return next 'bar ' || current_setting('gp_segment');
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
