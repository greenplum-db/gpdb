-- test for subtransactions

drop table if exists ao1;
create table ao1(a int) with (appendonly=true);
begin;
    savepoint s1;
    insert into ao1 select * from generate_series(1, 1000);
    savepoint s2;
    select * from ao1;
    rollback to s1;
    select * from ao1;
commit;

select * from ao1;

drop table if exists ao1;
create table ao1(a int) with (appendonly=true, orientation=column);
begin;
    savepoint s1;
    insert into co1 select * from generate_series(1, 1000);
    savepoint s2;
    select * from co1;
    rollback to s1;
    select * from co1;
commit;

select * from co1;
