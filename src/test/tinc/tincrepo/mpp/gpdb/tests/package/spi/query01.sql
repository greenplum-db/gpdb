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
' language plpgsql;

create trigger tg_bu before update
   on test for each row execute procedure bu();

update test set a=200 where a=10;

drop trigger tg_bu on test;
drop function bu();
