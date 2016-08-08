-- start_ignore
drop function if exists utilityfunc();
drop table if exists utilitytest cascade;
-- end_ignore

create function utilityfunc() returns integer as
$$
begin
    create table utilitytest(a int NOT NULL, b int, description text) distributed by (a);
    return 0;
end;
$$ language plpgsql;

select utilityfunc(); 

\d utilitytest;

drop table utilitytest;

select utilityfunc(); 

insert into utilitytest(a, b) values (1, 2);
insert into utilitytest(a, b) values (2, 3);

select *
from utilitytest
order by a,b;

