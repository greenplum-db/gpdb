-- prepare a distributed table
drop table if exists t;
create table t as select i as id from generate_series(1,8) i; 

create or replace function foo() returns void as $$
begin
    EXECUTE 'SELECT row(id, 2) from t';
    RETURN;
end
$$ language 'plpgsql';

select foo(); 

-- cleanup
drop function foo();
drop table t;
