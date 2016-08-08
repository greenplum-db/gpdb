drop table if exists ao1;
create table ao1(a int, b int) with (appendonly=true);

-- test insert and select from within same transaction
begin;
    insert into ao1 select i, i from generate_series(1, 1000) i;
    select * from ao1;
abort;

insert into ao1 select i, i from generate_series(1, 1000) i;
select * from ao1;
