drop table if exists test cascade;
create table test (d integer, a integer, b integer);
insert into test select a, a, a%25 from generate_series(1,100) a;

create or replace function bu() returns trigger as '
begin
    if new.a != old.a then
        -- delete from test where a = old.a;
        -- insert into test (a,b) values (new.a, new.b);
        return null;
    end if;
    return new;
end;
' language plpgsql NO SQL;

create trigger tg_bu before update
   on test for each row execute procedure bu();

update test set a=200 where a=10;

drop trigger tg_bu on test;
drop function bu();
drop table test;
create table test (a integer, b integer, c integer);
insert into test select a, a%25, a%30 from generate_series(1, 100) a;
analyze test;

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
$$ language plpgsql READS SQL DATA ROWS 10;

select * from (select refcursor (1, 10)) t1, test;

drop function refcursor(int, int);
