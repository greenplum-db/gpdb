-- start_ignore
drop function if exists func1();
drop function if exists func2();
drop table if exists functest;
-- end_ignore

create table functest(a int, b int) distributed by (b);

create function func2()
returns integer
as
$$
declare 
    rowcnt integer;
begin
    insert into functest values(1,2);
    select count(*) into rowcnt from functest;
    return rowcnt;
end;
$$ language plpgsql;

create function func1()
returns integer
as
$$
begin
    return func2();
end;
$$ language plpgsql;

select func1();

drop function func2();

create function func2()
returns integer
as
$$
declare
    rowcnt integer;
begin
    insert into functest values(1,2);
    select count(*) into rowcnt from functest;
    return rowcnt;
end;
$$ language plpgsql;

select func1();

