--start_ignore
drop table if exists prod_agg ;
drop table if exists cust_agg ;
--end_ignore

-- Test Aggregate Functions

-- Create product table 
create table prod_agg (sale integer, prod varchar);

-- inserting values into prod_agg table
insert into prod_agg values
  (100, 'shirts'),
  (200, 'pants'),
  (300, 't-shirts'),
  (400, 'caps'),
  (450, 'hats');

-- Create cust_agg table
create table cust_agg (cusname varchar, sale integer, prod varchar);

-- inserting values into customer table
insert into cust_agg values
  ('aryan', 100, 'shirts'),
  ('jay', 200, 'pants'),
  ('mahi', 300, 't-shirts'),
  ('nitu', 400, 'caps'),
  ('verru', 450, 'hats');

-- returning customer name from cust_agg name with count of prod_add table 
select cusname,(select count(*) from prod_agg) from cust_agg;

-- drop prod_agg and cust_agg table
drop table prod_agg;
drop table cust_agg;

-- Test Aggregate Functions

select x,y,count(*), grouping(x), grouping(y),grouping(x,y) from generate_series(1,1) x, generate_series(1,1) y group by cube(x,y);
select x,y,count(*), grouping(x,y) from generate_series(1,1) x, generate_series(1,1) y group by grouping sets((x,y),(x),(y),());
select x,y,count(*), grouping(x,y) from generate_series(1,2) x, generate_series(1,2) y group by cube(x,y);
--start_ignore
drop table if exists test;
--end_ignore
create table test (i int, n numeric);
insert into test values (0, 0), (0, 1), (0,2);
select i,n,count(*), grouping(i), grouping(n), grouping(i,n) from test group by grouping sets((), (i,n)) having n is null;

select x, y, count(*), grouping(x,y) from generate_series(1,1) x, generate_series(1,1) y group by grouping sets(x,y) having x is not null;
