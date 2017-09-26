-- start_ignore
SET gp_create_table_random_default_distribution=off;
drop table if exists ggg;
-- end_ignore
create table ggg (a char(1), b varchar(2), d integer, e date)
distributed by (a)
partition by range(e)
( START (date '2001-01-01') INCLUSIVE
   END (date '2007-01-01') EXCLUSIVE
      EVERY (INTERVAL '2 years') );

insert into ggg values (1,1,1,date '2001-01-15');
insert into ggg values (2,2,1,date '2002-01-15');
insert into ggg values (1,3,1,date '2003-01-15');
insert into ggg values (2,2,3,date '2004-01-15');
insert into ggg values (1,4,5,date '2005-01-15');
insert into ggg values (2,2,4,date '2006-01-15');
insert into ggg values (1,5,6,date '2006-01-15');
insert into ggg values (2,7,3,date '2001-01-15');
insert into ggg values (1,'a',33,date '2002-01-15');
insert into ggg values (2,'c',44,date '2003-01-15');

select * from ggg;

select * from ggg_1_prt_1;
select * from ggg_1_prt_2;
select * from ggg_1_prt_3;
