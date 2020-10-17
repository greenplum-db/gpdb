-- Simple test to verify accessibility of the OLD and NEW trigger variables

create table testtr (a int, b text);

-- GPDB: If 'a' is the distribution key, the UPDATE below fails:
-- ERROR:  UPDATE on distributed key column not allowed on relation with update triggers
alter table testtr set distributed randomly;

create function testtr_trigger() returns trigger language plpgsql as
$$begin
  raise notice 'tg_op = %', tg_op;
  raise notice 'old(%) = %', old.a, row(old.*);
  raise notice 'new(%) = %', new.a, row(new.*);
  if (tg_op = 'DELETE') then
    return old;
  else
    return new;
  end if;
end$$;

create trigger testtr_trigger before insert or delete or update on testtr
  for each row execute function testtr_trigger();

insert into testtr values (1, 'one'), (2, 'two');

update testtr set a = a + 1;

delete from testtr;
