-- start_ignore
drop function if exists utilityfunc();
drop table if exists utilitytest cascade;
-- end_ignore

create table utilitytest(a int NOT NULL, b int, description text) distributed by (a);
create index utilitytest_idx on utilitytest using bitmap(a);

insert into utilitytest(a, b) values (1, 2);
insert into utilitytest(a, b) values (2, 3);

select *
from utilitytest
order by a;

create function utilityfunc() returns integer as
$$
begin
    drop table if exists utilitytest cascade;
    return 0;
end;
$$ language plpgsql modifies sql data;

select 1 as one
where 0 = utilityfunc();

\d utilitytest;

create table utilitytest(a int NOT NULL, b int, description text) distributed by (a);
create index utilitytest_idx on utilitytest using bitmap(a);

insert into utilitytest(a, b) values (3, 4);

select *
from utilitytest
order by a;

select 1 as one
where 0 = utilityfunc();

\d utilitytest;
