-- start_ignore
drop table if exists functest;
drop function if exists func1(int);
drop function if exists func2();
drop function if exists func3();
drop function if exists func4();
-- end_ignore

create table functest(a int, b int) distributed by (b);

create function func4() returns void as
$$
begin
    insert into functest values(1,2);
    insert into functest values(2,3);
    insert into functest values(3,4);
end; 
$$ language plpgsql;

create function func3() returns int as
$$
declare 
    rowcnt int;
begin
    perform func4();
    select count(*) into rowcnt from functest;
    return rowcnt;
end;
$$ language plpgsql;

create function func2() returns int as
$$
declare 
    newcnt int;
begin
    select func3() + 2 into newcnt;
    return newcnt;
end;
$$ language plpgsql;

create function func1(padding int) returns int as
$$
begin
    return func2() + $1;
end;
$$ language plpgsql;

select func1(1) as func1, 
       6 as expected;

select count(*) from functest;

drop function func4();

create function func4() returns void as
$$
begin
    insert into functest values(4,4);
end; 
$$ language plpgsql;

create or replace function func2() returns int as
$$
declare
    newcnt int;
begin
    select func3() + 4 into newcnt;
    return newcnt;
end;
$$ language plpgsql;

select func1(1) as func1,
       9 as expected;
