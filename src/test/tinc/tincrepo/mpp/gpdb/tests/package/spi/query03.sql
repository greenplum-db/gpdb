create language plpgsql;
drop table test;
create table test (a integer, b integer, c integer);
insert into test select a, a%25, a%30 from generate_series(1, 100) a;

create or replace function refcursor(int, int) returns setof int as $$
declare
    c1 cursor (param1 int, param2 int) for select * from test where a > param1 and b > param2;
    nonsense record;
    i int;
begin
    open c1($1, $2);
    fetch c1 into nonsense;
    close c1;
    if found then
	for i in $1 .. $2 loop
     		return next i + 1;
        end loop;

        return;
    else
	for i in $1 .. $2 loop
     		return next i - 1;
        end loop;

        return;
    end if;
end
$$ language plpgsql;

select * from (select refcursor (1, 10)) t1, test;

drop function refcursor(int, int);
