-- start_ignore
drop view if exists v1 cascade;
drop function if exists viewfunc();
-- end_ignore

create temp view v1 as
   select 2+2 as f1;

create function viewfunc() returns int as 
$$
begin
    return f1 from v1; 
end
$$ language plpgsql;

select viewfunc(), '4' as expected;

create or replace temp view v1 as
    select 2+2+4 as f1; 

select viewfunc(), '8' as expected;

