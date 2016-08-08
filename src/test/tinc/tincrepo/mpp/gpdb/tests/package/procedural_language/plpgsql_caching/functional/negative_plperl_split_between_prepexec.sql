-- start_ignore
drop table if exists partitioned_t; 
drop function if exists plperl_prep();
drop function if exists plperl_run();
-- end_ignore

create table partitioned_t
( a numeric, b int, c varchar )
partition by range(b)
(
   partition part1 start(1) end (3),
   default partition defpart
);

insert into partitioned_t(a, b, c) values (9.9999999999, 1, 'test');
insert into partitioned_t(a, b, c) values (8.888888, 2, 'test');
insert into partitioned_t(a, b, c) values (4.444444444444, 4, 'testdef');
insert into partitioned_t(a, b, c) values (7.7777, 10, 'testdef');
insert into partitioned_t(a, b, c) values (5.5555555555555, 1, 'test');

create function plperl_prep() returns integer as $$
   $_SHARED{my_plan} = spi_prepare('select b from partitioned_t as tempb');
   return 0;
$$ LANGUAGE plperl;

create function plperl_run() returns SETOF partitioned_t as $$
   return spi_exec_prepared($_SHARED{my_plan})->{rows}->[0]->{tempb};
$$ LANGUAGE plperl;

select plperl_prep();

select plperl_run();

alter table partitioned_t split default partition start(4) inclusive end(10) exclusive into (partition part2, default partition);

select plperl_run();
