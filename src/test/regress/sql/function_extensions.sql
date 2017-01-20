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
