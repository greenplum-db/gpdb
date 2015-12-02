--start_ignore
drop table if exists testemp;
drop table if exists product_agg;
drop table if exists quantity ;
drop table if exists product cascade ;
drop table if exists quantity ;
drop table if exists prod_agg ;
drop table if exists cust_agg ;
drop table if exists prod7 ;
drop table if exists cust7 ;
drop table if exists prod8 ;
drop table if exists prod9 ;
drop table if exists prod10 ;
--end_ignore
-- Test Aggregate Functions

-- Create testemp table
create table testemp(a integer, b integer);

-- insert values into table testemp
insert into testemp values(1,2);

-- returnning count of the actor able
select count(*) from testemp;

-- drop table testemp
drop table testemp;

-- Test Aggregate Functions

-- Create product_agg table 
create table product_agg(sale integer, product integer);

-- inserting values into product_agg table
insert into product_agg values
  (10, 20),
  (20, 25),
  (30, 30),
  (40, 35),
  (45, 40);

-- returning minimum of product_agg table
select min(sale) from product_agg;

-- dropt product_agg table

drop table product_agg;
-- Test Aggregate Functions

-- Create quantity table
create table quantity (qty integer, price integer);

-- inserting values into quantity table
insert into quantity values
  (100, 50),
  (200, 100),
  (300, 200),
  (400, 35),
  (500, 40);

-- returning maximum of quantity table
select max(price) from quantity;

-- drop quantity table
drop table quantity;-- Test Aggregate Functions

-- Create product table 
create table product (sale integer, prod integer);

-- inserting values into product table
insert into product values
  (10, 20),
  (20, 25),
  (30, 30),
  (40, 35),
  (45, 40);

-- returning minimum of product table
select min(sale*prod) from product;

-- drop product table
drop table product;-- Test Aggregate Functions

-- Create quantity table
create table quantity (qty integer, price integer, product character);

-- inserting values into quantity table
insert into quantity values (100, 50, p1);
insert into quantity values (200, 100, p2);
insert into quantity values (300, 200, p3);
insert into quantity values (400, 35, p4);
insert into quantity values (500, 40, p5);

-- returning maximum of quantity table
select max(qty*price) from quantity;

-- drop quantity table
drop table quantity;-- Test Aggregate Functions

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

-- create prod7 table 
create table prod7 (sale integer, prod varchar);

-- inserting values into prod7 table
insert into prod7 values
  (100, 'shirts'),
  (200, 'pants'),
  (300, 't-shirts'),
  (400, 'caps'),
  (450, 'hats');

-- create cust7 table
create table cust7 (cusname varchar, sale integer, prod varchar);

-- inserting values into cust7 table
insert into cust7 values
  ('jay', 100, 'shirts'),
  ('aryan', 200, 'pants'),
  ('mahi', 300, 't-shirts'),
  ('veeru', 400, 'caps'),
  ('jay', 450, 'hats');

-- returning customer name with count using order by customer name
\echo -- order 1
select cusname, (select count(*) from prod7) from cust7 order by cusname;

-- drop prod7 and cust7 table
drop table prod7;
drop table cust7;
-- Test Aggregate Functions

-- create prod8 table 
create table prod8 (sale integer, prodnm varchar,price integer);

-- inserting values into prod8 table
insert into prod8 values
  (100, 'shirts', 500),
  (200, 'pants',800),
  (300, 't-shirts', 300);

-- returning product and price using Havingand Group by clause

select prodnm, price, count(*) from prod8 GROUP BY prodnm, price;

-- drop prod8 table
drop table prod8;
-- Test Aggregate Functions

-- create prod9 table 
create table prod9 (sale integer, prodnm varchar,price integer);

-- inserting values into prod9 table
insert into prod9 values
  (100, 'shirts', 500),
  (200, 'pants',800),
  (300, 't-shirts', 300);

-- returning product and price using Havingand Group by clause

select prodnm, price from prod9 GROUP BY prodnm, price HAVING price !=300;

-- drop prod9 table
drop table prod9;-- Test Aggregate Functions

-- create prod10 table 
create table prod10 (sale integer, prodnm varchar,price integer);

-- inserting values into prod10 table
insert into prod10 values
  (100, 'shirts', 500),
  (200, 'pants',800),
  (300, 't-shirts', 300);

-- Returning product and price using HAVING and GROUP BY clauses.
-- Note: we can get away with ORDER BY prodnm because for this data set 
-- all prodnm values are unique.
\echo -- order 1
select prodnm, price from prod10 GROUP BY prodnm, price HAVING price !=400 ORDER BY prodnm;

-- drop prod10 table
drop table prod10;
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

--start_ignore
drop table if exists testemp;
drop table if exists product_agg;
drop table if exists quantity ;
drop table if exists product ;
drop table if exists quantity ;
drop table if exists prod_agg ;
drop table if exists cust_agg ;
drop table if exists prod7 ;
drop table if exists cust7 ;
drop table if exists prod8 ;
drop table if exists prod9 ;
drop table if exists prod10 ;
--end_ignore
