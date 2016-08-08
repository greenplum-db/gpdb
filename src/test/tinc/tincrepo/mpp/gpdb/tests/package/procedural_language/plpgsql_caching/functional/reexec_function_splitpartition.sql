-- start_ignore
drop table if exists partitioned_table; 
drop function if exists partfunc();
-- end_ignore

create table partitioned_table
( a numeric, b int, c varchar )
partition by range(b)
(
   partition part1 start(1) end (3),
   default partition defpart
);

insert into partitioned_table(a, b, c) values (9.9999999999, 1, 'test');
insert into partitioned_table(a, b, c) values (8.888888, 2, 'test');
insert into partitioned_table(a, b, c) values (4.444444444444, 4, 'testdef');
insert into partitioned_table(a, b, c) values (7.7777, 10, 'testdef');
insert into partitioned_table(a, b, c) values (5.5555555555555, 1, 'test');

create function partfunc() returns text as 
$$
declare
   nm   numeric;
begin
   select a from partitioned_table into nm;
   return 'done';
end;
$$ LANGUAGE plpgsql;

select partfunc();

alter table partitioned_table split default partition
   start(4) inclusive end(10) exclusive into (partition part2, default partition);

select partfunc();

