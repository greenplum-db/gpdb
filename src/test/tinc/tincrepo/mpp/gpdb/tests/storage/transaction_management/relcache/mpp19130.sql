-- 
-- @description MPP-19130
-- @created 2013-02-26 00:00:00
-- @modified 2013-02-26 00:00:00
-- @tags plpython transaction echo

-- start_ignore
drop table if exists stran_foo, stran_tt;
-- end_ignore

create table stran_foo (a, b) as values(1, 10), (2, 20);

-- The PL/pgSQL EXCEPTION block opens a subtransaction.
-- If it's in reader, it was messing up relcache previously.
create table stran_foo(a, b) as values(1, 10), (2, 20);
create or replace function stran_func(a int) returns int as $$
declare
  x int;
begin
  begin
    select 1 + 2 into x;
  exception
    when division_by_zero then
      raise info 'except';
  end;
  return x;
end;
$$ language plpgsql;

create table stran_tt as select stran_func(b) from stran_foo;

-- start_ignore
drop table stran_foo;
drop table stran_tt;
drop function stran_func(int);
-- end_ignore
