-- start_ignore
create schema qp_olap_mdqa;
set search_path = qp_olap_mdqa;
set datestyle = ISO, MDY;

--
-- STANDARD DATA FOR olap_* TESTS.
--

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

-- Sales (ord transactions)
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
-- end_ignore

SELECT CASE WHEN sale.vn < 10 THEN 1 ELSE 2 END as newalias1,
CASE WHEN sale.qty < 10 THEN 1 ELSE 2 END as newalias2,
CASE WHEN sale.vn < 10 THEN 1 ELSE 2 END as newalias3, 
TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.qty+sale.qty)),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.qty)),0),'99999999.9999999') 
FROM sale,product,vendor 
WHERE sale.pn=product.pn AND sale.vn=vendor.vn
GROUP BY ROLLUP((newalias3,sale.dt,sale.pn,sale.cn),(sale.cn,sale.dt,sale.cn,sale.dt)),sale.vn,sale.qty;

-- ###### Queries involving VARIANCE() function ###### --

SELECT CASE WHEN sale.vn < 10 THEN 1 ELSE 2 END as newalias1,CASE WHEN sale.pn < 10 THEN 1 ELSE 2 END as newalias2,CASE WHEN sale.pn < 10 THEN 1 ELSE 2 END as newalias3,CASE WHEN sale.cn < 10 THEN 1 ELSE 2 END as newalias4,GROUP_ID(), TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.prc+sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.vn*sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.prc+sale.vn)),0),'99999999.9999999') 
FROM sale,product
WHERE sale.pn=product.pn
GROUP BY ROLLUP((sale.cn,sale.prc,sale.prc),(newalias3,sale.vn),(sale.prc,newalias2),(newalias1,sale.pn,sale.cn,sale.qty,sale.cn),(sale.prc,newalias2,sale.cn,sale.cn,sale.qty),(sale.pn,sale.cn,sale.pn),(newalias2,newalias1,sale.qty,sale.prc,sale.qty),(sale.vn,sale.cn,sale.pn,sale.cn),(newalias4,sale.vn,newalias4,sale.dt)),GROUPING SETS(ROLLUP((sale.prc),(sale.vn,newalias3,newalias3,sale.vn),(newalias3,newalias3),(sale.qty),(sale.vn,newalias3),(newalias3,newalias1,sale.prc),(newalias2),(newalias4)),CUBE((sale.dt,sale.cn,newalias2),(newalias3,sale.cn,sale.cn,newalias2),(sale.vn,sale.vn,sale.cn,sale.pn,sale.vn),(sale.vn,sale.vn),(sale.prc,sale.cn,sale.pn,sale.qty),(newalias2,sale.qty,newalias2),(sale.vn,sale.prc,newalias1),(newalias2,sale.pn),(sale.cn,sale.prc,newalias1),(sale.vn,sale.prc,newalias1))),sale.vn,sale.pn,sale.cn;

-- ###### Queries involving AVG() function ###### --

SELECT sale.pn*2 as newalias1,sale.cn*2 as newalias2,sale.qty*2 as newalias3,sale.qty + sale.pn as newalias4,GROUP_ID(), TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.vn-sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.vn)),0),'99999999.9999999') 
FROM sale,product,vendor,customer
WHERE sale.pn=product.pn AND sale.vn=vendor.vn AND sale.cn=customer.cn
GROUP BY (),sale.pn,sale.cn,sale.qty;

-- ###### Queries involving COUNT() function ###### --

SELECT sale.cn as newalias1,CASE WHEN sale.vn < 10 THEN 1 ELSE 2 END as newalias2,sale.vn*2 as newalias3,CASE WHEN sale.prc < 10 THEN 1 ELSE 2 END as newalias4,sale.pn*2 as newalias5, TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.cn)),0),'99999999.9999999') 
FROM sale,vendor
WHERE sale.vn=vendor.vn
GROUP BY ROLLUP((sale.vn,newalias4),(sale.prc,sale.prc,sale.prc,sale.cn,newalias2),(sale.pn,sale.cn),(newalias2,newalias1,newalias3,sale.pn,sale.pn),(newalias4,newalias1,sale.pn,sale.qty)),CUBE((sale.qty,newalias4,sale.pn,newalias1,sale.dt),(sale.pn,newalias1,sale.vn,sale.vn,sale.vn)),sale.cn,sale.vn,sale.prc,sale.pn;

-- ###### Queries involving MAX() function ###### --

SELECT CASE WHEN sale.pn < 10 THEN 1 ELSE 2 END as newalias1,CASE WHEN sale.prc < 10 THEN 1 ELSE 2 END as newalias2,GROUPING(sale.pn), TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.vn+sale.pn)),0),'99999999.9999999') 
FROM sale,customer,product
WHERE sale.cn=customer.cn AND sale.pn=product.pn
GROUP BY (newalias2,sale.pn),(sale.qty),ROLLUP((sale.dt,sale.cn,sale.dt)),sale.pn,sale.prc;

-- ###### Queries involving MIN() function ###### --

SELECT sale.qty as newalias1,GROUPING(sale.qty), TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.qty+sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.pn/sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.prc*sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.prc+sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.vn)),0),'99999999.9999999') 
FROM sale
GROUP BY ROLLUP((newalias1),(newalias1,sale.cn,sale.cn),(sale.vn,sale.dt)),(),sale.qty;

-- ###### Queries involving STDDEV() function ###### --

SELECT CASE WHEN sale.pn < 10 THEN 1 ELSE 2 END as newalias1,CASE WHEN sale.vn < 10 THEN 1 ELSE 2 END as newalias2,CASE WHEN sale.dt::text < 10::text THEN 1 ELSE 2 END as newalias3,CASE WHEN sale.dt::text < 10::text THEN 1 ELSE 2 END as newalias4, TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.cn-sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.qty*sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.prc-sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.pn+sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.prc-sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.vn)),0),'99999999.9999999') 
FROM sale,customer,vendor
WHERE sale.cn=customer.cn AND sale.vn=vendor.vn
GROUP BY ROLLUP((sale.prc,newalias1),(sale.prc,newalias4,sale.prc,sale.dt,newalias4),(newalias4,sale.pn,sale.dt)),sale.pn,sale.vn,sale.dt; 

-- ###### Queries involving SUM() function ###### --

SELECT CASE WHEN sale.cn < 10 THEN 1 ELSE 2 END as newalias1,CASE WHEN sale.qty < 10 THEN 1 ELSE 2 END as newalias2,CASE WHEN sale.vn < 10 THEN 1 ELSE 2 END as newalias3,CASE WHEN sale.qty < 10 THEN 1 ELSE 2 END as newalias4,GROUPING(sale.cn,sale.cn,sale.vn), TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.cn+sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.pn+sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.pn+sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.vn-sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.qty-sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.prc/sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.pn*sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.cn+sale.prc)),0),'99999999.9999999') 
FROM sale,vendor
WHERE sale.vn=vendor.vn
GROUP BY ROLLUP((newalias4,sale.cn,sale.qty),(sale.vn,newalias1,sale.dt,newalias1,sale.pn),(newalias4,newalias4,sale.cn),(newalias3,sale.prc,newalias1),(newalias1,newalias4),(sale.qty,sale.pn,sale.dt),(sale.vn,sale.dt,newalias2,newalias3),(sale.cn)),sale.cn,sale.qty,sale.vn;

-- ###### Queries involving VARIANCE() function ###### --

SELECT CASE WHEN sale.prc < 10 THEN 1 ELSE 2 END as newalias1,sale.qty + sale.prc as newalias2,sale.dt+interval '2 months' as newalias3,CASE WHEN sale.pn < 10 THEN 1 ELSE 2 END as newalias4,sale.pn as newalias5,GROUPING(sale.prc,sale.dt), TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.pn+sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.vn+sale.cn)),0),'99999999.9999999') 
FROM sale,product
WHERE sale.pn=product.pn
GROUP BY (newalias1,newalias2,sale.pn,newalias3,sale.qty),(sale.dt,sale.pn,newalias1,newalias1),(newalias2,newalias1,newalias4,sale.qty,sale.dt),sale.prc,sale.qty,sale.dt,sale.pn;

-- ###### Queries involving AVG() function ###### --

SELECT CASE WHEN sale.vn < 10 THEN 1 ELSE 2 END as newalias1,CASE WHEN sale.cn < 10 THEN 1 ELSE 2 END as newalias2,CASE WHEN sale.pn < 10 THEN 1 ELSE 2 END as newalias3,GROUPING(sale.pn), TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.prc+sale.cn)),0),'99999999.9999999') 
FROM sale,product,vendor
WHERE sale.pn=product.pn AND sale.vn=vendor.vn
GROUP BY CUBE((sale.qty,newalias1,newalias1,newalias2),(sale.prc,newalias3,sale.qty,newalias1),(sale.pn,sale.cn,sale.cn,sale.qty,sale.cn),(newalias1,sale.qty,newalias2),(sale.qty,sale.cn,sale.cn,sale.cn),(sale.dt)),(newalias3,newalias2,sale.cn),(sale.qty,sale.cn,newalias1,sale.vn),(sale.prc),sale.vn,sale.cn,sale.pn;

-- ###### Queries involving COUNT() function ###### --

SELECT CASE WHEN sale.cn < 10 THEN 1 ELSE 2 END as newalias1,CASE WHEN sale.pn < 10 THEN 1 ELSE 2 END as newalias2,GROUPING(sale.cn,sale.pn),GROUP_ID(), TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.vn-sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.prc*sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.vn-sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.qty+sale.cn)),0),'99999999.9999999') 
FROM sale
GROUP BY CUBE((sale.dt,newalias2,sale.prc),(sale.vn)),sale.cn,sale.pn;

-- NOTE: this query suffers from the issue discussed at:
-- https://www.postgresql.org/message-id/flat/7dbdcf5c-b5a6-ef89-4958-da212fe10176%40iki.fi
SELECT CASE WHEN sale.qty < 10 THEN 1 ELSE 2 END as newalias1,
               CASE WHEN sale.qty < 10 THEN 1 ELSE 2 END as newalias4,
               GROUPING(sale.qty,sale.cn,sale.qty)
FROM sale 
GROUP BY GROUPING SETS((sale.vn,newalias1,sale.qty),(sale.prc,newalias4,newalias1,newalias1),(sale.vn,sale.vn,sale.dt)),
                    sale.qty,sale.cn,sale.vn,sale.pn;
-- ###### Queries involving MAX() function ###### --

SELECT sale.vn*2 as newalias1,sale.qty as newalias2,sale.vn + sale.vn as newalias3,GROUPING(sale.vn),GROUP_ID(), TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.qty/sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.cn*sale.cn)),0),'99999999.9999999') 
FROM sale,product
WHERE sale.pn=product.pn
GROUP BY ROLLUP((sale.dt),(sale.vn,sale.pn,newalias1,newalias1,newalias1)),sale.vn,sale.qty HAVING GROUP_ID() > 1;

-- ###### Queries involving MIN() function ###### --

SELECT CASE WHEN sale.vn < 10 THEN 1 ELSE 2 END as newalias1,sale.cn + sale.vn as newalias2,sale.qty as newalias3,CASE WHEN sale.pn < 10 THEN 1 ELSE 2 END as newalias4,GROUP_ID(), TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.vn+sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.prc+sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.qty/sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.prc+sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.qty)),0),'99999999.9999999') 
FROM sale,product
WHERE sale.pn=product.pn
GROUP BY CUBE((newalias2,sale.prc,sale.dt),(sale.qty,sale.qty,newalias2,newalias3),(sale.cn,sale.cn,newalias2,sale.cn)),sale.vn,sale.cn,sale.qty,sale.pn;

-- ###### Queries involving STDDEV() function ###### --

SELECT CASE WHEN sale.cn < 10 THEN 1 ELSE 2 END as newalias1,CASE WHEN sale.qty < 10 THEN 1 ELSE 2 END as newalias2,CASE WHEN sale.prc < 10 THEN 1 ELSE 2 END as newalias3,GROUPING(sale.qty,sale.cn), TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.prc+sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.prc-sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.pn+sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.vn+sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.vn-sale.cn)),0),'99999999.9999999') 
FROM sale
GROUP BY CUBE((sale.cn,newalias2)),CUBE((newalias3)),sale.cn,sale.qty,sale.prc;

-- ###### Queries involving SUM() function ###### --

SELECT CASE WHEN sale.pn < 10 THEN 1 ELSE 2 END as newalias1,GROUPING(sale.pn), TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.prc+sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.qty/sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.vn+sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.pn+sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.pn-sale.vn)),0),'99999999.9999999') 
FROM sale,product,vendor,customer
WHERE sale.pn=product.pn AND sale.vn=vendor.vn AND sale.cn=customer.cn
GROUP BY CUBE((sale.prc,sale.prc),(newalias1),(sale.cn,sale.vn),(newalias1,sale.dt,sale.pn,newalias1),(sale.pn)),sale.pn HAVING COALESCE(COUNT(DISTINCT sale.pn),0) < 52.8275232558546;

-- ###### Queries involving VARIANCE() function ###### --

SELECT CASE WHEN sale.qty < 10 THEN 1 ELSE 2 END as newalias1,CASE WHEN sale.cn < 10 THEN 1 ELSE 2 END as newalias2,CASE WHEN sale.vn < 10 THEN 1 ELSE 2 END as newalias3, TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.cn/sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.prc)),0),'99999999.9999999') 
FROM sale,product
WHERE sale.pn=product.pn
GROUP BY ROLLUP((newalias1,sale.prc,newalias3,sale.cn),(sale.cn,sale.dt,newalias2,sale.pn),(sale.vn,sale.dt),(sale.cn,sale.cn,sale.cn,newalias1),(sale.qty,sale.prc,newalias2,sale.prc,newalias3),(sale.pn,newalias1),(newalias1,sale.dt,sale.vn,sale.qty),(newalias2,newalias2)),ROLLUP((sale.dt),(sale.dt,sale.prc,sale.dt,newalias1),(newalias1,newalias2,sale.prc),(sale.prc),(sale.qty),(sale.qty,sale.cn)),sale.qty,sale.cn,sale.vn;

-- ###### Queries involving AVG() function ###### --

SELECT sale.qty + sale.qty as newalias1,sale.cn as newalias2,sale.qty*2 as newalias3,GROUPING(sale.cn),GROUP_ID(), TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.qty-sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.vn-sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.cn-sale.prc)),0),'99999999.9999999') 
FROM sale,vendor
WHERE sale.vn=vendor.vn
GROUP BY CUBE((newalias3,sale.cn,sale.dt),(sale.qty,sale.cn,sale.cn,sale.dt),(sale.pn,newalias3),(sale.pn,sale.pn,sale.vn),(sale.prc,newalias3,sale.qty,sale.cn)),sale.qty,sale.cn;

-- ###### Queries involving COUNT() function ###### --

SELECT CASE WHEN sale.vn < 10 THEN 1 ELSE 2 END as newalias1,sale.pn + sale.vn as newalias2,sale.cn + sale.vn as newalias3,CASE WHEN sale.prc < 10 THEN 1 ELSE 2 END as newalias4,CASE WHEN sale.prc < 10 THEN 1 ELSE 2 END as newalias5, TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.vn+sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.qty+sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.qty*sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.vn/sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.vn-sale.qty)),0),'99999999.9999999') 
FROM sale,vendor
WHERE sale.vn=vendor.vn
GROUP BY CUBE((sale.pn,sale.pn,sale.pn,sale.prc,sale.dt),(sale.prc,sale.pn,sale.cn,sale.cn),(newalias2,sale.cn,sale.cn,sale.cn,newalias1),(newalias2,newalias1,newalias1,newalias1,sale.vn)),sale.vn,sale.pn,sale.cn,sale.prc HAVING COALESCE(COUNT(DISTINCT sale.vn),0) <> 25.6885699096937;

-- ###### Queries involving MAX() function ###### --

SELECT CASE WHEN sale.pn < 10 THEN 1 ELSE 2 END as newalias1,CASE WHEN sale.cn < 10 THEN 1 ELSE 2 END as newalias2,CASE WHEN sale.dt::text < 10::text THEN 1 ELSE 2 END as newalias3,GROUP_ID(), TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.pn*sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.cn/sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.cn/sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.vn*sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.pn/sale.pn)),0),'99999999.9999999') 
FROM sale,customer
WHERE sale.cn=customer.cn
GROUP BY (),sale.pn,sale.cn,sale.dt;

-- ###### Queries involving MIN() function ###### --

SELECT CASE WHEN sale.cn < 10 THEN 1 ELSE 2 END as newalias1,GROUP_ID(), TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.vn-sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.vn/sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.qty+sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.prc*sale.pn)),0),'99999999.9999999') 
FROM sale,customer,product
WHERE sale.cn=customer.cn AND sale.pn=product.pn
GROUP BY CUBE((sale.cn,sale.pn),(sale.vn,sale.vn),(sale.cn)),ROLLUP((sale.cn),(sale.dt,sale.pn),(newalias1,sale.cn,newalias1)),sale.cn;

-- ###### Queries involving STDDEV() function ###### --

SELECT sale.cn*2 as newalias1,CASE WHEN sale.dt::text < 10::text THEN 1 ELSE 2 END as newalias2,sale.prc as newalias3,GROUP_ID(), TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.prc*sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.vn)),0),'99999999.9999999') 
FROM sale
GROUP BY (),sale.cn,sale.dt,sale.prc;

SELECT CASE WHEN sale.pn < 10 THEN 1 ELSE 2 END as newalias1,sale.pn*2 as newalias2,
sale.qty as newalias3,CASE WHEN sale.pn < 10 THEN 1 ELSE 2 END as newalias4,GROUPING(sale.pn), 
TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.cn)),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.pn-sale.cn)),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.vn)),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.prc-sale.vn)),0),'99999999.9999999'),
TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.vn+sale.prc)),0),'99999999.9999999'),
TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.vn)),0),'99999999.9999999'),
TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.pn)),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.pn-sale.qty)),0),'99999999.9999999'),
TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.vn)),0),'99999999.9999999'),
TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.vn)),0),'99999999.9999999') 
FROM sale,vendor
WHERE sale.vn=vendor.vn
GROUP BY GROUPING SETS(CUBE((newalias2,sale.prc,newalias4,newalias4),
(sale.qty,sale.pn,newalias2),(sale.pn),(sale.qty,sale.cn),(newalias2,newalias3),
(newalias1,sale.vn,newalias3,sale.prc,newalias3),(newalias1,newalias1,sale.dt,newalias3,newalias4),
(sale.vn,newalias4,newalias2,newalias2))),sale.pn,sale.qty HAVING GROUP_ID() <= 9;

-- ###### Queries involving SUM() function ###### --

SELECT sale.pn as newalias1,GROUPING(sale.pn), TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.qty/sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.pn*sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.vn/sale.pn)),0),'99999999.9999999') 
FROM sale,customer,vendor
WHERE sale.cn=customer.cn AND sale.vn=vendor.vn
GROUP BY ROLLUP((sale.dt,sale.pn,sale.pn,newalias1),(sale.cn,sale.dt,sale.prc),(sale.pn,newalias1,sale.cn,newalias1),(sale.vn,sale.cn,sale.pn,sale.pn),(sale.prc),(sale.prc)),sale.pn;

-- ###### Queries involving VARIANCE() function ###### --

SELECT sale.pn*2 as newalias1, TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.qty+sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.vn-sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.vn*sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.vn)),0),'99999999.9999999') 
FROM sale
GROUP BY ROLLUP((sale.pn),(newalias1)),GROUPING SETS(ROLLUP((sale.qty,sale.prc,sale.pn),(sale.dt)),ROLLUP((sale.prc),(sale.qty,sale.vn),(sale.prc))),sale.pn HAVING COALESCE(COUNT(DISTINCT sale.pn),0) < 33.6499974602656;

-- ###### Queries involving AVG() function ###### --

SELECT sale.cn as newalias1,sale.dt as newalias2,sale.cn as newalias3,GROUP_ID(), TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.qty)),0),'99999999.9999999') 
FROM sale,product,vendor
WHERE sale.pn=product.pn AND sale.vn=vendor.vn
GROUP BY ROLLUP((sale.prc,newalias2,sale.pn,sale.dt),(newalias3),(sale.pn,sale.prc,sale.dt,sale.prc)),sale.cn,sale.dt;

-- ###### Queries involving COUNT() function ###### --

SELECT CASE WHEN sale.pn < 10 THEN 1 ELSE 2 END as newalias1,CASE WHEN sale.cn < 10 THEN 1 ELSE 2 END as newalias2,sale.cn as newalias3,sale.pn + sale.cn as newalias4,CASE WHEN sale.qty < 10 THEN 1 ELSE 2 END as newalias5,sale.prc as newalias6,GROUPING(sale.cn,sale.pn,sale.prc),GROUP_ID(), TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.vn)),0),'99999999.9999999') 
FROM sale,product
WHERE sale.pn=product.pn
GROUP BY ROLLUP((sale.prc),(sale.prc,newalias1),(sale.pn,newalias4,newalias2,sale.dt,sale.cn),(newalias1,sale.dt,sale.dt,sale.qty),(sale.pn,sale.pn,sale.pn,newalias2)),sale.pn,sale.cn,sale.qty,sale.prc HAVING GROUPING(sale.cn,sale.cn,sale.pn,sale.cn,sale.cn,sale.prc) <= 5;

-- ###### Queries involving MAX() function ###### --

SELECT sale.dt + interval '5 days' as newalias1, TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.prc-sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.prc-sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.vn/sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.prc)),0),'99999999.9999999') 
FROM sale,customer,product
WHERE sale.cn=customer.cn AND sale.pn=product.pn
GROUP BY (sale.vn),sale.dt;

-- ###### Queries involving MIN() function ###### --

SELECT sale.dt as newalias1,sale.pn*2 as newalias2,CASE WHEN sale.cn < 10 THEN 1 ELSE 2 END as newalias3,CASE WHEN sale.vn < 10 THEN 1 ELSE 2 END as newalias4, TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.cn/sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.qty-sale.pn)),0),'99999999.9999999') 
FROM sale,product
WHERE sale.pn=product.pn
GROUP BY CUBE((sale.dt),(sale.pn,newalias3,newalias1,newalias1),(newalias3,sale.pn),(newalias2,newalias4,sale.vn,sale.qty,newalias4),(newalias4,newalias2,sale.dt,sale.cn),(newalias1,sale.vn),(newalias4,sale.prc,newalias1,sale.dt),(sale.cn)),ROLLUP((sale.vn,newalias4,newalias1)),sale.dt,sale.pn,sale.cn,sale.vn HAVING GROUP_ID() <> 9;

-- ###### Queries involving STDDEV() function ###### --

SELECT sale.qty*2 as newalias1,CASE WHEN sale.pn < 10 THEN 1 ELSE 2 END as newalias2,CASE WHEN sale.cn < 10 THEN 1 ELSE 2 END as newalias3,sale.cn as newalias4,sale.pn + sale.pn as newalias5,sale.vn as newalias6,GROUPING(sale.cn,sale.qty,sale.cn),GROUP_ID(), TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.qty-sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.qty)),0),'99999999.9999999') 
FROM sale
GROUP BY GROUPING SETS((),()),ROLLUP((sale.cn),(newalias2,sale.pn),(newalias2,newalias4,sale.qty,sale.cn,sale.qty),(newalias3,sale.dt),(sale.dt,sale.cn),(sale.pn,sale.prc,newalias4),(newalias3,newalias1,newalias3,newalias4,newalias4)),sale.qty,sale.pn,sale.cn,sale.vn;

-- ###### Queries involving SUM() function ###### --

SELECT sale.cn + sale.cn as newalias1, TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.prc*sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.prc+sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.prc-sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.qty*sale.cn)),0),'99999999.9999999') 
FROM sale,product,vendor,customer
WHERE sale.pn=product.pn AND sale.vn=vendor.vn AND sale.cn=customer.cn
GROUP BY (sale.vn,sale.qty),(sale.dt,sale.vn,sale.pn),(sale.qty,sale.pn),sale.cn;

-- ###### Queries involving VARIANCE() function ###### --

SELECT sale.dt as newalias1,sale.cn + sale.cn as newalias2,sale.vn*2 as newalias3,CASE WHEN sale.vn < 10 THEN 1 ELSE 2 END as newalias4,GROUPING(sale.vn,sale.cn), TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.pn+sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.qty-sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.vn*sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.vn-sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.qty-sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.cn)),0),'99999999.9999999') 
FROM sale
GROUP BY (),sale.dt,sale.cn,sale.vn;

-- ###### Queries involving AVG() function ###### --

SELECT sale.dt as newalias1,sale.vn*2 as newalias2,CASE WHEN sale.cn < 10 THEN 1 ELSE 2 END as newalias3,GROUPING(sale.dt),GROUP_ID(), TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.vn+sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.vn+sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.prc+sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.qty*sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.qty)),0),'99999999.9999999') 
FROM sale,vendor
WHERE sale.vn=vendor.vn
GROUP BY ROLLUP((sale.cn,sale.prc,sale.dt,sale.vn),(newalias1),(sale.qty),(sale.prc),(sale.vn,newalias1,sale.cn),(newalias3,sale.qty),(newalias2),(sale.cn,newalias3),(sale.pn)),CUBE((sale.cn,newalias3,sale.cn,sale.pn),(sale.pn),(newalias2,newalias1,sale.cn,sale.cn),(newalias2,newalias3,sale.cn,sale.prc)),sale.dt,sale.vn,sale.cn;

-- ###### Queries involving AVG() function ###### --

SELECT sale.qty*2 as newalias1,GROUP_ID(), TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.prc/sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.cn-sale.vn)),0),'99999999.9999999') 
FROM sale
GROUP BY (sale.vn),(sale.prc),ROLLUP((sale.vn,sale.pn,sale.cn),(sale.vn,sale.pn,sale.vn,sale.pn)),sale.qty;

-- ###### Queries involving COUNT() function ###### --

SELECT CASE WHEN sale.qty < 10 THEN 1 ELSE 2 END as newalias1,GROUPING(sale.qty),GROUP_ID(), TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.qty+sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.cn)),0),'99999999.9999999') 
FROM sale
GROUP BY (sale.prc,sale.vn),(sale.cn),(sale.cn),(sale.pn),(sale.prc,sale.qty,newalias1),(sale.prc),(sale.prc),(newalias1),sale.qty;

-- ###### Queries involving MAX() function ###### --

SELECT sale.qty*2 as newalias1,CASE WHEN sale.cn < 10 THEN 1 ELSE 2 END as newalias2,CASE WHEN sale.dt::text < 10::text THEN 1 ELSE 2 END as newalias3,sale.vn as newalias4, TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.pn*sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.qty/sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.pn+sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.qty-sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.qty/sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.cn+sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.pn/sale.pn)),0),'99999999.9999999') 
FROM sale,customer,vendor
WHERE sale.cn=customer.cn AND sale.vn=vendor.vn
GROUP BY (newalias3,newalias1,sale.pn),(newalias1),(newalias4,newalias1,newalias4,newalias3,sale.prc),(sale.qty,sale.dt),(newalias3,sale.prc,sale.pn,sale.dt),(sale.dt,newalias3,sale.qty),(newalias1,newalias4,newalias3,newalias1,newalias3),(sale.prc,sale.pn,sale.dt,sale.qty,sale.vn),(newalias2),sale.qty,sale.cn,sale.dt,sale.vn;

-- ###### Queries involving MIN() function ###### --

SELECT CASE WHEN sale.pn < 10 THEN 1 ELSE 2 END as newalias1,GROUP_ID(), TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.vn+sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.prc-sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.prc+sale.prc)),0),'99999999.9999999') 
FROM sale,vendor
WHERE sale.vn=vendor.vn
GROUP BY (),CUBE((sale.prc,sale.cn,sale.cn,sale.cn),(sale.qty,sale.pn),(sale.dt,sale.dt),(sale.pn,sale.cn,sale.cn),(sale.vn,sale.cn,newalias1),(sale.dt,sale.dt),(sale.cn,sale.cn,sale.pn)),sale.pn;

-- ###### Queries involving STDDEV() function ###### --

SELECT sale.pn as newalias1,CASE WHEN sale.cn < 10 THEN 1 ELSE 2 END as newalias2,CASE WHEN sale.qty < 10 THEN 1 ELSE 2 END as newalias3,CASE WHEN sale.qty < 10 THEN 1 ELSE 2 END as newalias4,GROUP_ID(), TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.qty/sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.vn-sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.vn+sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.pn*sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.pn-sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.pn+sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.pn/sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.prc*sale.qty)),0),'99999999.9999999') 
FROM sale
GROUP BY (),GROUPING SETS(CUBE((sale.qty,newalias4,sale.qty,sale.qty,sale.dt),(sale.cn,sale.vn,newalias4),(newalias3,sale.prc,sale.prc),(newalias1,sale.qty,newalias1,newalias1),(newalias1,newalias4,sale.pn,newalias1,sale.dt),(newalias1))),sale.pn,sale.cn,sale.qty;

-- ###### Queries involving SUM() function ###### --

SELECT CASE WHEN sale.qty < 10 THEN 1 ELSE 2 END as newalias1,GROUPING(sale.qty),GROUP_ID(), TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.pn+sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.vn+sale.qty)),0),'99999999.9999999') 
FROM sale,product
WHERE sale.pn=product.pn
GROUP BY GROUPING SETS(CUBE((sale.qty,sale.dt,sale.qty),(sale.prc,sale.qty),(sale.prc),(sale.qty),(sale.pn,sale.pn,sale.qty),(sale.vn))),sale.qty;

-- ###### Queries involving VARIANCE() function ###### --

SELECT CASE WHEN sale.qty < 10 THEN 1 ELSE 2 END as newalias1,sale.cn + sale.qty as newalias2,sale.vn + sale.cn as newalias3,sale.cn as newalias4,GROUPING(sale.qty,sale.qty,sale.cn,sale.cn), TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.qty+sale.prc)),0),'99999999.9999999') 
FROM sale
GROUP BY CUBE((newalias1),(sale.prc),(sale.prc,newalias4),(newalias3,sale.vn,newalias3,sale.dt),(newalias1,newalias2),(sale.dt,sale.pn,sale.cn),(sale.vn,newalias2,sale.cn),(sale.prc,sale.dt,sale.pn,sale.cn),(newalias2,sale.pn),(sale.qty,newalias4,sale.prc,sale.prc)),sale.qty,sale.cn,sale.vn;

-- ###### Queries involving AVG() function ###### --

SELECT sale.qty as newalias1,sale.cn as newalias2,CASE WHEN sale.cn < 10 THEN 1 ELSE 2 END as newalias3,CASE WHEN sale.dt::text < 10::text THEN 1 ELSE 2 END as newalias4,GROUPING(sale.dt,sale.cn), TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.qty+sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.pn+sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.vn)),0),'99999999.9999999') 
FROM sale
GROUP BY ROLLUP((newalias4,sale.pn),(newalias2,newalias3),(sale.cn,sale.qty,sale.cn,sale.pn,sale.dt),(sale.qty,newalias1,sale.prc),(sale.qty,sale.qty,newalias4,newalias2,sale.cn),(sale.qty,sale.pn),(sale.prc,newalias4,newalias2,sale.pn,newalias3)),(newalias3,newalias1,newalias4),(sale.cn),(sale.pn,sale.cn,newalias3,sale.cn),(sale.prc,newalias4,sale.vn),(newalias1,newalias1,newalias1),(sale.dt,newalias4,newalias2,sale.vn),(sale.cn,sale.vn,sale.qty,newalias4,sale.qty),(newalias3,newalias1),(sale.dt,newalias3,newalias4,sale.vn,sale.dt),(sale.dt,sale.dt),sale.qty,sale.cn,sale.dt;

-- ###### Queries involving COUNT() function ###### --

SELECT CASE WHEN sale.prc < 10 THEN 1 ELSE 2 END as newalias1,sale.pn*2 as newalias2,GROUPING(sale.pn,sale.prc), TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.prc*sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.pn-sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.pn)),0),'99999999.9999999') 
FROM sale,customer
WHERE sale.cn=customer.cn
GROUP BY CUBE((sale.vn),(sale.qty,sale.pn,sale.pn),(sale.qty,sale.vn,sale.cn)),sale.prc,sale.pn;

-- ###### Queries involving MAX() function ###### --

SELECT sale.cn as newalias1,CASE WHEN sale.dt::text < 10::text THEN 1 ELSE 2 END as newalias2,GROUPING(sale.cn,sale.dt),GROUP_ID(), TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.qty+sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.pn*sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.cn+sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.qty+sale.qty)),0),'99999999.9999999') 
FROM sale,product
WHERE sale.pn=product.pn
GROUP BY CUBE((sale.pn,sale.pn),(newalias1,sale.cn)),sale.cn,sale.dt HAVING COALESCE(COUNT(DISTINCT sale.dt),0) < 64.620392683825;

-- ###### Queries involving MIN() function ###### --

SELECT sale.dt + interval '2 years' as newalias1,sale.vn + sale.dt as newalias2,CASE WHEN sale.vn < 10 THEN 1 ELSE 2 END as newalias3,sale.vn as newalias4,sale.vn*2 as newalias5,CASE WHEN sale.prc < 10 THEN 1 ELSE 2 END as newalias6,GROUP_ID(), TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.qty/sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.vn-sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.prc+sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.qty)),0),'99999999.9999999') 
FROM sale,customer,vendor
WHERE sale.cn=customer.cn AND sale.vn=vendor.vn
GROUP BY GROUPING SETS(ROLLUP((sale.qty,newalias4,sale.pn,newalias2),(sale.vn,newalias1)),()),ROLLUP((sale.pn)),sale.dt,sale.vn,sale.prc;

-- ###### Queries involving COUNT() function ###### --

SELECT CASE WHEN sale.dt::text < 10::text THEN 1 ELSE 2 END as newalias1,sale.cn as newalias2,CASE WHEN sale.dt::text < 10::text THEN 1 ELSE 2 END as newalias3,sale.prc + sale.cn as newalias4,CASE WHEN sale.cn < 10 THEN 1 ELSE 2 END as newalias5,GROUPING(sale.cn),GROUP_ID(), TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.cn*sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.cn*sale.vn)),0),'99999999.9999999') 
FROM sale,vendor
WHERE sale.vn=vendor.vn
GROUP BY CUBE((newalias3),(newalias1,sale.prc),(sale.cn,newalias2),(sale.vn,newalias4),(sale.qty,newalias4,sale.cn,newalias3,newalias2),(newalias1)),sale.dt,sale.cn,sale.prc HAVING GROUPING(sale.dt,sale.dt,sale.prc) = 6;

-- ###### Queries involving STDDEV() function ###### --

SELECT CASE WHEN sale.qty < 10 THEN 1 ELSE 2 END as newalias1,sale.pn as newalias2,GROUPING(sale.qty,sale.qty),GROUP_ID(), TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.prc-sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.cn*sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.pn+sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.qty*sale.prc)),0),'99999999.9999999') 
FROM sale,product
WHERE sale.pn=product.pn
GROUP BY CUBE((sale.cn),(sale.cn)),sale.qty,sale.pn HAVING COALESCE(COUNT(DISTINCT sale.pn),0) >= 34.8977780303152;

-- ###### Queries involving SUM() function ###### --

SELECT sale.cn + sale.cn as newalias1,GROUP_ID(), TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.cn-sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.prc*sale.vn)),0),'99999999.9999999') 
FROM sale
GROUP BY GROUPING SETS(CUBE((sale.vn),(sale.dt),(newalias1,sale.pn,sale.qty),(sale.pn,newalias1,sale.vn),(sale.cn,sale.prc,sale.prc)),ROLLUP((sale.cn,sale.dt))),ROLLUP((sale.vn,sale.prc),(newalias1),(sale.prc,sale.pn,sale.cn)),sale.cn;

-- ###### Queries involving VARIANCE() function ###### --

SELECT CASE WHEN sale.pn < 10 THEN 1 ELSE 2 END as newalias1,CASE WHEN sale.qty < 10 THEN 1 ELSE 2 END as newalias2,GROUPING(sale.pn),GROUP_ID(), TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.vn-sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.pn-sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.qty-sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.prc)),0),'99999999.9999999') 
FROM sale,product,vendor,customer
WHERE sale.pn=product.pn AND sale.vn=vendor.vn AND sale.cn=customer.cn
GROUP BY (newalias2,sale.qty,sale.dt,sale.cn),sale.pn,sale.qty;

-- ###### Queries involving AVG() function ###### --

SELECT sale.pn + sale.pn as newalias1,CASE WHEN sale.qty < 10 THEN 1 ELSE 2 END as newalias2,sale.qty + sale.qty as newalias3,CASE WHEN sale.pn < 10 THEN 1 ELSE 2 END as newalias4,GROUPING(sale.qty), TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.cn)),0),'99999999.9999999') 
FROM sale,customer,product
WHERE sale.cn=customer.cn AND sale.pn=product.pn
GROUP BY ROLLUP((sale.cn,newalias3,newalias1),(sale.dt,newalias1,newalias3),(newalias3,newalias2,sale.prc),(newalias3,sale.pn,newalias3,newalias1)),ROLLUP((newalias1,sale.dt,newalias2,newalias4),(newalias4,sale.dt,newalias1),(newalias3,newalias1,sale.vn),(newalias3,sale.prc,sale.vn,sale.vn)),sale.pn,sale.qty;

-- ###### Queries involving COUNT() function ###### --

SELECT CASE WHEN sale.dt::text < 10::text THEN 1 ELSE 2 END as newalias1,sale.pn*2 as newalias2,sale.prc + sale.prc as newalias3,GROUPING(sale.pn,sale.prc), TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.cn+sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.cn+sale.cn)),0),'99999999.9999999') 
FROM sale,customer,product
WHERE sale.cn=customer.cn AND sale.pn=product.pn
GROUP BY CUBE((sale.cn,sale.dt,sale.cn,sale.cn,sale.dt),(sale.qty,sale.qty),(sale.pn,newalias1,sale.prc,newalias1)),sale.dt,sale.pn,sale.prc;

-- ###### Queries involving MAX() function ###### --

SELECT sale.qty as newalias1,sale.qty + sale.qty as newalias2,sale.qty as newalias3, TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.vn*sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.qty+sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.cn-sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.qty-sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.pn*sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.pn+sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.prc+sale.qty)),0),'99999999.9999999') 
FROM sale,product
WHERE sale.pn=product.pn
GROUP BY (sale.pn,newalias2),(sale.vn,sale.qty,sale.prc,sale.pn),(newalias1,newalias1,newalias1),(newalias3,sale.vn,sale.vn),(sale.prc,sale.cn),(sale.cn),(newalias3),(newalias3,sale.qty,sale.qty),sale.qty HAVING COALESCE(SUM(DISTINCT sale.qty),0) = 71.702580143165;

-- ###### Queries involving MIN() function ###### --

SELECT sale.cn + sale.cn as newalias1,sale.cn*2 as newalias2,sale.qty*2 as newalias3,GROUPING(sale.cn),GROUP_ID(), TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.vn-sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.pn/sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.cn+sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.vn-sale.cn)),0),'99999999.9999999') 
FROM sale,product,vendor
WHERE sale.pn=product.pn AND sale.vn=vendor.vn
GROUP BY GROUPING SETS((),CUBE((sale.cn,sale.pn,sale.qty,sale.pn,sale.pn),(newalias2),(newalias1,sale.qty,sale.dt,newalias3),(sale.cn,sale.qty),(sale.prc,sale.prc,sale.prc),(newalias2,sale.pn,sale.dt,sale.prc),(newalias2),(sale.prc))),sale.cn,sale.qty;

-- ###### Queries involving STDDEV() function ###### --

SELECT CASE WHEN sale.qty < 10 THEN 1 ELSE 2 END as newalias1, TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.vn*sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.vn-sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.pn/sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.qty-sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.pn-sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.qty*sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.cn)),0),'99999999.9999999') 
FROM sale,vendor
WHERE sale.vn=vendor.vn
GROUP BY CUBE((sale.cn,sale.dt,sale.dt),(sale.pn,sale.prc),(sale.qty,sale.vn)),sale.qty;

-- ###### Queries involving SUM() function ###### --

SELECT sale.vn*2 as newalias1,sale.prc as newalias2,GROUPING(sale.prc,sale.vn), TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.qty)),0),'99999999.9999999') 
FROM sale,customer,product
WHERE sale.cn=customer.cn AND sale.pn=product.pn
GROUP BY (),(sale.prc,sale.qty),(sale.qty,newalias1,sale.cn,newalias1),(sale.vn),(sale.qty,newalias2,sale.pn),sale.vn,sale.prc;

-- ###### Queries involving VARIANCE() function ###### --

SELECT sale.vn*2 as newalias1,sale.vn as newalias2,CASE WHEN sale.vn < 10 THEN 1 ELSE 2 END as newalias3, TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.vn*sale.pn)),0),'99999999.9999999') 
FROM sale,customer,product
WHERE sale.cn=customer.cn AND sale.pn=product.pn
GROUP BY CUBE((newalias2,sale.pn,newalias2),(sale.prc),(newalias3),(sale.pn,newalias3,sale.cn,newalias1),(sale.pn,sale.cn,sale.pn),(sale.dt)),sale.vn HAVING COALESCE(AVG(DISTINCT sale.vn),0) <> 64.0393457794417;

-- ###### Queries involving MAX() function ###### --

SELECT sale.pn*2 as newalias1,CASE WHEN sale.pn < 10 THEN 1 ELSE 2 END as newalias2,CASE WHEN sale.cn < 10 THEN 1 ELSE 2 END as newalias3,sale.vn as newalias4,CASE WHEN sale.qty < 10 THEN 1 ELSE 2 END as newalias5, TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.prc/sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.pn+sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.vn)),0),'99999999.9999999') 
FROM sale,customer,product
WHERE sale.cn=customer.cn AND sale.pn=product.pn
GROUP BY GROUPING SETS((),CUBE((sale.pn,newalias3),(sale.cn,newalias1,sale.qty),(sale.prc,sale.qty,sale.vn),(sale.qty),(sale.prc,newalias3,newalias2),(sale.prc,sale.pn,sale.qty,sale.cn),(newalias4,newalias2,sale.vn),(sale.cn,newalias1,sale.cn,sale.vn,sale.cn),(sale.prc,sale.cn,sale.qty,newalias1),(newalias4,newalias2,sale.dt))),ROLLUP((sale.prc,newalias3,newalias4,sale.qty),(sale.cn,sale.vn,sale.cn,newalias2),(sale.prc,newalias2,newalias3,sale.cn)),sale.pn,sale.cn,sale.vn,sale.qty;

-- ###### Queries involving AVG() function ###### --

SELECT sale.pn*2 as newalias1,CASE WHEN sale.pn < 10 THEN 1 ELSE 2 END as newalias2,GROUPING(sale.pn),GROUP_ID(), TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.vn+sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.vn/sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.pn-sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.qty+sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.cn/sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.pn+sale.pn)),0),'99999999.9999999') 
FROM sale,customer,vendor
WHERE sale.cn=customer.cn AND sale.vn=vendor.vn
GROUP BY GROUPING SETS((),ROLLUP((sale.pn),(sale.vn),(sale.vn),(sale.prc,sale.pn,sale.dt,sale.vn),(sale.cn))),sale.pn HAVING GROUPING(sale.pn,sale.pn) = 1;

-- ###### Queries involving COUNT() function ###### --

SELECT CASE WHEN sale.pn < 10 THEN 1 ELSE 2 END as newalias1,sale.cn*2 as newalias2,sale.cn*2 as newalias3,GROUPING(sale.pn,sale.cn,sale.cn), TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.cn-sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.vn)),0),'99999999.9999999') 
FROM sale,customer,product
WHERE sale.cn=customer.cn AND sale.pn=product.pn
GROUP BY ROLLUP((sale.vn,sale.qty,newalias1),(newalias2)),sale.pn,sale.cn HAVING GROUPING(sale.cn,sale.cn,sale.cn) <= 0;

-- ###### Queries involving MAX() function ###### --

SELECT CASE WHEN sale.prc < 10 THEN 1 ELSE 2 END as newalias1, TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.cn+sale.cn)),0),'99999999.9999999') 
FROM sale,customer
WHERE sale.cn=customer.cn
GROUP BY ROLLUP((sale.pn,sale.prc),(sale.qty,sale.dt,sale.prc)),(sale.cn,sale.cn),sale.prc HAVING GROUPING(sale.prc) > 1;

-- ###### Queries involving MIN() function ###### --

SELECT sale.qty*2 as newalias1,CASE WHEN sale.pn < 10 THEN 1 ELSE 2 END as newalias2,sale.vn*2 as newalias3, TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.pn-sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.cn/sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.qty+sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.cn)),0),'99999999.9999999') 
FROM sale,product,vendor
WHERE sale.pn=product.pn AND sale.vn=vendor.vn
GROUP BY ROLLUP((sale.cn,newalias3,sale.pn,sale.pn,sale.cn),(sale.vn,newalias3,sale.cn),(sale.qty,sale.cn,sale.cn)),sale.qty,sale.pn,sale.vn;

-- ###### Queries involving STDDEV() function ###### --

SELECT CASE WHEN sale.qty < 10 THEN 1 ELSE 2 END as newalias1, TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.vn*sale.cn)),0),'99999999.9999999') 
FROM sale
GROUP BY ROLLUP((sale.cn,sale.cn),(sale.cn,sale.vn),(sale.vn)),(),sale.qty;

-- ###### Queries involving SUM() function ###### --

SELECT CASE WHEN sale.prc < 10 THEN 1 ELSE 2 END as newalias1,sale.cn + sale.prc as newalias2,CASE WHEN sale.vn < 10 THEN 1 ELSE 2 END as newalias3,GROUPING(sale.prc),GROUP_ID(), TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.prc/sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.vn/sale.pn)),0),'99999999.9999999') 
FROM sale,product
WHERE sale.pn=product.pn
GROUP BY (sale.qty,sale.qty),(sale.cn),(newalias1,sale.dt),(sale.prc),GROUPING SETS(()),sale.prc,sale.cn,sale.vn;

-- ###### Queries involving VARIANCE() function ###### --

SELECT sale.dt as newalias1,GROUPING(sale.dt),GROUP_ID(), TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.vn+sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.vn*sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.vn+sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.vn*sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.qty*sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.prc*sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.prc-sale.prc)),0),'99999999.9999999') 
FROM sale,product
WHERE sale.pn=product.pn
GROUP BY (sale.cn,sale.prc),(sale.vn,sale.cn,sale.qty),(sale.cn,newalias1,sale.cn),(sale.pn),sale.dt HAVING GROUP_ID() = 0;

-- ###### Queries involving AVG() function ###### --

SELECT sale.qty + sale.qty as newalias1,sale.qty as newalias2,sale.cn as newalias3,sale.prc as newalias4,sale.cn as newalias5, TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.pn-sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.qty-sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.qty+sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.pn-sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.vn+sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.pn*sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.prc-sale.prc)),0),'99999999.9999999') 
FROM sale,product,vendor
WHERE sale.pn=product.pn AND sale.vn=vendor.vn
GROUP BY ROLLUP((sale.prc,sale.qty,newalias2,sale.dt),(newalias2,sale.pn),(sale.dt,newalias2,newalias1,newalias2),(sale.prc),(newalias2,sale.pn,newalias3,sale.prc),(newalias4,sale.qty),(newalias4,sale.prc)),(),sale.qty,sale.cn,sale.prc;

-- ###### Queries involving COUNT() function ###### --

SELECT sale.qty as newalias1,CASE WHEN sale.qty < 10 THEN 1 ELSE 2 END as newalias2, TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.prc/sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.pn+sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.prc)),0),'99999999.9999999') 
FROM sale
GROUP BY ROLLUP((sale.vn,sale.qty,newalias2,sale.vn),(sale.prc),(sale.dt,sale.qty,sale.vn,newalias2),(sale.cn),(sale.vn),(newalias2),(sale.pn,sale.vn),(newalias1)),(),sale.qty HAVING GROUPING(sale.qty,sale.qty) = 3;

-- ###### Queries involving MAX() function ###### --

SELECT sale.vn as newalias1,sale.cn as newalias2,CASE WHEN sale.pn < 10 THEN 1 ELSE 2 END as newalias3,GROUPING(sale.cn,sale.pn), TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.qty/sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.pn-sale.prc)),0),'99999999.9999999') 
FROM sale,product,vendor,customer
WHERE sale.pn=product.pn AND sale.vn=vendor.vn AND sale.cn=customer.cn
GROUP BY (sale.vn),(newalias2,sale.vn,newalias3,sale.pn,newalias1),GROUPING SETS(CUBE((newalias3,sale.vn,sale.qty,sale.qty),(sale.vn,sale.cn),(sale.pn))),sale.vn,sale.cn,sale.pn HAVING COALESCE(COUNT(DISTINCT sale.cn),0) > 22.6345604094671;

-- ###### Queries involving MIN() function ###### --

SELECT sale.qty*2 as newalias1,CASE WHEN sale.prc < 10 THEN 1 ELSE 2 END as newalias2,GROUPING(sale.prc), TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.prc*sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.prc-sale.prc)),0),'99999999.9999999') 
FROM sale,product,vendor,customer
WHERE sale.pn=product.pn AND sale.vn=vendor.vn AND sale.cn=customer.cn
GROUP BY CUBE((sale.qty),(newalias2,sale.dt,sale.dt),(sale.qty),(sale.vn,sale.vn,newalias2)),sale.qty,sale.prc HAVING COALESCE(COUNT(DISTINCT sale.prc),0) = 41.6214176436572;

-- ###### Queries involving MIN() function ###### --

SELECT sale.cn as newalias1,sale.pn as newalias2, TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.cn-sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.qty/sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.qty-sale.prc)),0),'99999999.9999999') 
FROM sale,vendor
WHERE sale.vn=vendor.vn
GROUP BY CUBE((sale.cn),(sale.cn),(newalias2,newalias2,sale.pn,sale.vn),(newalias2,newalias1,sale.cn)),ROLLUP((sale.pn,sale.cn),(sale.prc),(sale.cn,sale.vn),(sale.prc,newalias2,sale.vn,sale.vn),(sale.cn,sale.cn,sale.qty),(sale.cn)),sale.cn,sale.pn HAVING GROUPING(sale.cn,sale.cn) < 1;

-- ###### Queries involving STDDEV() function ###### --

SELECT sale.cn*2 as newalias1,CASE WHEN sale.vn < 10 THEN 1 ELSE 2 END as newalias2, TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.cn/sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.qty*sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.vn/sale.qty)),0),'99999999.9999999') 
FROM sale,product,vendor,customer
WHERE sale.pn=product.pn AND sale.vn=vendor.vn AND sale.cn=customer.cn
GROUP BY GROUPING SETS((),CUBE((sale.dt,sale.dt,sale.cn,sale.qty),(sale.prc),(newalias1,sale.cn,sale.cn,sale.prc),(sale.pn,sale.dt,sale.prc))),(),sale.cn,sale.vn HAVING GROUP_ID() > 0;

-- ###### Queries involving SUM() function ###### --

SELECT CASE WHEN sale.qty < 10 THEN 1 ELSE 2 END as newalias1,CASE WHEN sale.vn < 10 THEN 1 ELSE 2 END as newalias2,CASE WHEN sale.dt::text < 10::text THEN 1 ELSE 2 END as newalias3,sale.qty + sale.qty as newalias4,CASE WHEN sale.pn < 10 THEN 1 ELSE 2 END as newalias5,GROUP_ID(), TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.cn-sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.vn+sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.qty-sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.cn+sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.cn-sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.pn+sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.pn*sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.pn)),0),'99999999.9999999') 
FROM sale,product,vendor
WHERE sale.pn=product.pn AND sale.vn=vendor.vn
GROUP BY (newalias2,newalias2,sale.vn,newalias3),sale.qty,sale.vn,sale.dt,sale.pn;

-- ###### Queries involving VARIANCE() function ###### --

SELECT CASE WHEN sale.qty < 10 THEN 1 ELSE 2 END as newalias1,sale.dt + interval '2 mins' as newalias2,CASE WHEN sale.pn < 10 THEN 1 ELSE 2 END as newalias3,CASE WHEN sale.qty < 10 THEN 1 ELSE 2 END as newalias4,GROUPING(sale.qty,sale.dt),GROUP_ID(), TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.qty-sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.prc+sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(sale.vn+sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.pn)),0),'99999999.9999999') 
FROM sale,customer,product
WHERE sale.cn=customer.cn AND sale.pn=product.pn
GROUP BY ROLLUP((newalias3,sale.pn,sale.dt),(sale.vn),(sale.dt,sale.dt,sale.prc,sale.prc,sale.cn),(newalias3,sale.prc,sale.prc),(newalias4,newalias4,sale.prc),(sale.dt,sale.pn,sale.dt),(newalias1,newalias2,newalias1,newalias1,sale.prc),(sale.cn)),sale.qty,sale.dt,sale.pn;

-- ###### Queries involving STDDEV() function ###### --

SELECT sale.vn as newalias1,GROUPING(sale.vn),GROUP_ID(), TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.vn+sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.pn)),0),'99999999.9999999') 
FROM sale,customer,vendor
WHERE sale.cn=customer.cn AND sale.vn=vendor.vn
GROUP BY CUBE((sale.pn),(sale.pn,sale.cn)),CUBE((newalias1)),sale.vn;

-- ###### Queries involving SUM() function ###### --

SELECT sale.cn as newalias1,sale.qty*2 as newalias2,CASE WHEN sale.cn < 10 THEN 1 ELSE 2 END as newalias3,GROUPING(sale.cn), TO_CHAR(COALESCE(SUM(DISTINCT floor(sale.cn+sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(sale.cn+sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV(DISTINCT floor(sale.prc+sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(sale.qty/sale.cn)),0),'99999999.9999999') 
FROM sale,product,vendor
WHERE sale.pn=product.pn AND sale.vn=vendor.vn
GROUP BY GROUPING SETS(ROLLUP((sale.qty,sale.vn,sale.vn,sale.pn,sale.dt))),sale.cn,sale.qty;

-- start_ignore
drop schema qp_olap_mdqa cascade;
-- end_ignore
