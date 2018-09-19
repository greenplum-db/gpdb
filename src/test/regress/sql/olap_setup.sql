--
-- STANDARD DATA FOR olap_* TESTS.
--

-- start_ignore
drop table if exists customer cascade;
drop table if exists vendor cascade;
drop table if exists product cascade;
drop table if exists sale cascade;
drop table if exists sale_ord cascade;
-- end_ignore

create table customer 
(
	cn int not null,
	cname text not null,
	cloc text,
	
	primary key (cn)
	
) distributed by (cn);

create table vendor 
(
	vn int not null,
	vname text not null,
	vloc text,
	
	primary key (vn)
	
) distributed by (vn);

create table product 
(
	pn int not null,
	pname text not null,
	pcolor text,
	
	primary key (pn)
	
) distributed by (pn);

create table sale
(
	cn int not null,
	vn int not null,
	pn int not null,
	dt date not null,
	qty int not null,
	prc float not null,
	
	primary key (cn, vn, pn)
	
) distributed by (cn,vn,pn);

create table sale_ord
(
        ord int not null,
        cn int not null,
        vn int not null,
        pn int not null,
        dt date not null,
        qty int not null,
        prc float not null,

        primary key (cn, vn, pn)

) distributed by (cn,vn,pn);

-- Customers
insert into customer values 
  ( 1, 'Macbeth', 'Inverness'),
  ( 2, 'Duncan', 'Forres'),
  ( 3, 'Lady Macbeth', 'Inverness'),
  ( 4, 'Witches, Inc', 'Lonely Heath');

-- Vendors
insert into vendor values 
  ( 10, 'Witches, Inc', 'Lonely Heath'),
  ( 20, 'Lady Macbeth', 'Inverness'),
  ( 30, 'Duncan', 'Forres'),
  ( 40, 'Macbeth', 'Inverness'),
  ( 50, 'Macduff', 'Fife');

-- Products
insert into product values 
  ( 100, 'Sword', 'Black'),
  ( 200, 'Dream', 'Black'),
  ( 300, 'Castle', 'Grey'),
  ( 400, 'Justice', 'Clear'),
  ( 500, 'Donuts', 'Plain'),
  ( 600, 'Donuts', 'Chocolate'),
  ( 700, 'Hamburger', 'Grey'),
  ( 800, 'Fries', 'Grey');


-- Sales (transactions)
insert into sale values 
  ( 2, 40, 100, '1401-1-1', 1100, 2400),
  ( 1, 10, 200, '1401-3-1', 1, 0),
  ( 3, 40, 200, '1401-4-1', 1, 0),
  ( 1, 20, 100, '1401-5-1', 1, 0),
  ( 1, 30, 300, '1401-5-2', 1, 0),
  ( 1, 50, 400, '1401-6-1', 1, 0),
  ( 2, 50, 400, '1401-6-1', 1, 0),
  ( 1, 30, 500, '1401-6-1', 12, 5),
  ( 3, 30, 500, '1401-6-1', 12, 5),
  ( 3, 30, 600, '1401-6-1', 12, 5),
  ( 4, 40, 700, '1401-6-1', 1, 1),
  ( 4, 40, 800, '1401-6-1', 1, 1);

-- Sales_ord (the identical table as Sales, except that
--   a unique identifier is added for each row.)
insert into sale_ord values 
  ( 1,2, 40, 100, '1401-1-1', 1100, 2400),
  ( 2,1, 10, 200, '1401-3-1', 1, 0),
  ( 3,3, 40, 200, '1401-4-1', 1, 0),
  ( 4,1, 20, 100, '1401-5-1', 1, 0),
  ( 5,1, 30, 300, '1401-5-2', 1, 0),
  ( 6,1, 50, 400, '1401-6-1', 1, 0),
  ( 7,2, 50, 400, '1401-6-1', 1, 0),
  ( 8,1, 30, 500, '1401-6-1', 12, 5),
  ( 9,3, 30, 500, '1401-6-1', 12, 5),
  ( 10,3, 30, 600, '1401-6-1', 12, 5),
  ( 11,4, 40, 700, '1401-6-1', 1, 1),
  ( 12,4, 40, 800, '1401-6-1', 1, 1);
