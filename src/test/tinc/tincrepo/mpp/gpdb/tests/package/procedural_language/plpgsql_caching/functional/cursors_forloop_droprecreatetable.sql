-- start_ignore
drop function if exists forloop();
drop table if exists fordata;
-- end_ignore

create table fordata(a int) distributed by (a);

create function forloop() returns int as $$
declare
   rowcnt integer;
begin
   for i IN 1..200 LOOP
      insert into fordata(a) values(i);
      drop table fordata;
      create table fordata(a int) distributed by (a);
   end loop;
   select count(*) into rowcnt from fordata; 
   return rowcnt;
end;
$$ language plpgsql;

set gp_plpgsql_clear_cache_always=on;
select forloop();

