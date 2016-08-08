drop table if exists co1;
create table co1(a int, b int) with (appendonly=true, orientation=column);

-- test insert and select within same transaction
begin;
    insert into co1 select i, i from generate_series(1, 1000) i;
    select * from co1;
abort;

insert into co1 select i, i from generate_series(1, 1000) i;
select * from co1;
