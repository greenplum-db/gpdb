--
-- STANDARD DATA FOR olap_* TESTS.
--
-- start_ignore

drop table cf_olap_windowerr_customer;
drop table cf_olap_windowerr_vendor;
drop table cf_olap_windowerr_product;
drop table cf_olap_windowerr_sale;
drop table cf_olap_windowerr_sale_ord;
drop table cf_olap_windowerr_util;


create table cf_olap_windowerr_customer 
(
	cn int not null,
	cname text not null,
	cloc text,
	
	primary key (cn)
	
) distributed by (cn);

create table cf_olap_windowerr_vendor 
(
	vn int not null,
	vname text not null,
	vloc text,
	
	primary key (vn)
	
) distributed by (vn);

create table cf_olap_windowerr_product 
(
	pn int not null,
	pname text not null,
	pcolor text,
	
	primary key (pn)
	
) distributed by (pn);

create table cf_olap_windowerr_sale
(
	cn int not null,
	vn int not null,
	pn int not null,
	dt date not null,
	qty int not null,
	prc float not null,
	
	primary key (cn, vn, pn)
	
) distributed by (cn,vn,pn);

create table cf_olap_windowerr_sale_ord
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

create table cf_olap_windowerr_util
(
	xn int not null,
	
	primary key (xn)
	
) distributed by (xn);

-- cf_olap_windowerr_customers
insert into cf_olap_windowerr_customer values 
  ( 1, 'Macbeth', 'Inverness'),
  ( 2, 'Duncan', 'Forres'),
  ( 3, 'Lady Macbeth', 'Inverness'),
  ( 4, 'Witches, Inc', 'Lonely Heath');

-- cf_olap_windowerr_vendors
insert into cf_olap_windowerr_vendor values 
  ( 10, 'Witches, Inc', 'Lonely Heath'),
  ( 20, 'Lady Macbeth', 'Inverness'),
  ( 30, 'Duncan', 'Forres'),
  ( 40, 'Macbeth', 'Inverness'),
  ( 50, 'Macduff', 'Fife');

-- cf_olap_windowerr_products
insert into cf_olap_windowerr_product values 
  ( 100, 'Sword', 'Black'),
  ( 200, 'Dream', 'Black'),
  ( 300, 'Castle', 'Grey'),
  ( 400, 'Justice', 'Clear'),
  ( 500, 'Donuts', 'Plain'),
  ( 600, 'Donuts', 'Chocolate'),
  ( 700, 'Hamburger', 'Grey'),
  ( 800, 'Fries', 'Grey');


-- cf_olap_windowerr_sales (transactions)
insert into cf_olap_windowerr_sale values 
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

-- cf_olap_windowerr_sales (ord transactions)
insert into cf_olap_windowerr_sale_ord values 
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

-- cf_olap_windowerr_util

insert into cf_olap_windowerr_util values 
  (1),
  (20),
  (300);

-- end_ignore

-- LEAD() function with OVER() clause having ONLY ORDER BY ASC/DESC (without framing) --

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.pn*cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.pn) as int),NULL) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc);

-- COUNT() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(DENSE_RANK() OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn desc range 3 preceding ),
win2 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.pn desc),
win3 as (order by cf_olap_windowerr_sale.pn asc);

-- COUNT() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.qty)) OVER(order by cf_olap_windowerr_sale.vn desc range between floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.qty) preceding and 2 preceding ),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.qty,
TO_CHAR(COALESCE(RANK() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.qty) as int),NULL) OVER(win4),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.vn desc range between floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.qty) preceding and 2 preceding ),
win2 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.pn desc),
win3 as (order by cf_olap_windowerr_sale.cn desc),
win4 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc);

-- COUNT() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(CUME_DIST() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(order by cf_olap_windowerr_sale.pn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.prc)) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.qty) as int),NULL) OVER(win4),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.vn desc range between floor(cf_olap_windowerr_sale.prc) preceding and current row ),
win2 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.cn desc),
win3 as (order by cf_olap_windowerr_sale.pn asc),
win4 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc);

-- COUNT() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(order by cf_olap_windowerr_sale.vn asc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.vn desc range between floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.vn) preceding and floor(cf_olap_windowerr_sale.cn) following ),
win2 as (order by cf_olap_windowerr_sale.vn asc);

-- COUNT() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn desc range between floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.prc) preceding and unbounded following ),
win2 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.vn asc);

-- COUNT() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn asc range between current row and unbounded following ),
win2 as (order by cf_olap_windowerr_sale.vn asc);

-- COUNT() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.cn) as int),NULL) OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.cn asc rows between floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.qty) preceding and 4 following ),
win2 as (order by cf_olap_windowerr_sale.pn asc),
win3 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc);

-- COUNT() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.qty*cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(DENSE_RANK() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.prc) as int),NULL) OVER(win4),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.pn)) OVER(partition by cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.vn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.cn) as int),NULL) OVER(win5),0),'99999999.9999999'),cf_olap_windowerr_sale.qty
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc rows between current row and floor(cf_olap_windowerr_sale.vn) following ),
win2 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.vn desc),
win3 as (partition by cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.vn asc),
win4 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),
win5 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc);

-- COUNT() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.vn)) OVER(order by cf_olap_windowerr_sale.vn desc),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.pn) as int),NULL) OVER(win4),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win5),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.cn asc range between unbounded preceding and floor(cf_olap_windowerr_sale.cn) following ),
win2 as (order by cf_olap_windowerr_sale.vn desc),
win3 as (order by cf_olap_windowerr_sale.pn asc),
win4 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc),
win5 as (order by cf_olap_windowerr_sale.vn asc);

-- COUNT() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.cn desc range between floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.prc) preceding and current row );

-- COUNT() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.pn)) OVER(partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn desc),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.pn) as int),NULL) OVER(win4),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win5),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.pn desc,cf_olap_windowerr_sale.pn desc rows between 0 preceding and floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.qty) following ),
win2 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn desc),
win3 as (order by cf_olap_windowerr_sale.vn desc),
win4 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc),
win5 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.cn desc);

-- MAX() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc) as int),NULL) OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(partition by cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn desc),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.pn*cf_olap_windowerr_sale.cn)) OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty*cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.cn) as int),NULL) OVER(win4),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.pn asc range between unbounded preceding and current row ),
win2 as (partition by cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn desc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),
win4 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc);

-- MAX() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.vn asc range between 3 following and floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.qty) following ),
win2 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.pn asc),
win3 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.cn desc);

-- MAX() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.pn)) OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.pn asc rows between unbounded preceding and 1 preceding ),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.prc) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.prc)) OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.pn asc rows between unbounded preceding and 1 preceding ),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc)) OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.pn asc rows between unbounded preceding and 1 preceding ),
win2 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);

-- MAX() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.prc
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.cn asc rows between floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.prc) preceding and current row ),
win2 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.vn asc);

-- MAX() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.pn*cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.vn) as int),NULL) OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.prc) as int),NULL) OVER(win4),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.vn) as int),NULL) OVER(win5),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc rows between current row and current row ),
win2 as (order by cf_olap_windowerr_sale.vn desc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc),
win4 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc),
win5 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc);

-- MAX() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.cn)) OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc,cf_olap_windowerr_sale.vn asc rows between current row and unbounded following ),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.qty) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(ROW_NUMBER() OVER(partition by cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc,cf_olap_windowerr_sale.vn asc rows between current row and unbounded following ),
win2 as (partition by cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc);

-- MAX() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.cn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.qty,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.pn) as int),NULL) OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc rows between floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.cn) following and floor(cf_olap_windowerr_sale.pn*cf_olap_windowerr_sale.prc) following ),
win2 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc);

-- MAX() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc)) OVER(partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.cn desc range between unbounded preceding and 3 following ),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.cn) as int),NULL) OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.cn) as int),NULL) OVER(win4),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.cn desc range between unbounded preceding and 3 following ),
win2 as (partition by cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.pn desc),
win3 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc),
win4 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc);

-- MAX() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc rows between 7 preceding and unbounded following );

-- MAX() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.vn) as int),NULL) OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.vn) as int),NULL) OVER(win4),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.vn)) OVER(partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.pn desc,cf_olap_windowerr_sale.pn asc rows between current row and unbounded following ),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.pn desc,cf_olap_windowerr_sale.pn asc rows between current row and unbounded following ),
win2 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.pn desc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc),
win4 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc);

-- MAX() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.qty*cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.cn desc rows between floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.pn) following and unbounded following );

-- MAX() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.cn)) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.pn) as int),NULL) OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc rows between floor(cf_olap_windowerr_sale.prc) following and unbounded following ),
win2 as (order by cf_olap_windowerr_sale.vn desc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc);

-- MIN() function with NULL OVER() clause in combination with other window functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999')
,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.pn) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc);

-- MIN() function with OVER() clause having ONLY PARTITION BY --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn);

-- MIN() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.cn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn desc range between unbounded preceding and floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.vn) preceding ),
win2 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc);

-- MIN() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn asc range between floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.cn) preceding and floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.pn) following );

-- MIN() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.qty)) OVER(order by cf_olap_windowerr_sale.cn desc range between floor(cf_olap_windowerr_sale.pn) preceding and floor(cf_olap_windowerr_sale.qty) following ),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.qty) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.qty
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn desc range between floor(cf_olap_windowerr_sale.pn) preceding and floor(cf_olap_windowerr_sale.qty) following ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc),
win3 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.vn desc);

-- MIN() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.vn desc range between current row and floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.qty) following );

-- MIN() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.pn desc rows unbounded preceding );

-- MIN() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc rows current row );

-- MIN() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.qty*cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.vn) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.cn asc rows between unbounded preceding and unbounded following ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc);

-- MIN() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.pn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.qty)) OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc rows between floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.pn) preceding and current row ),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc rows between floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.pn) preceding and current row ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc),
win3 as (order by cf_olap_windowerr_sale.cn asc);

-- MIN() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.cn) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.pn)) OVER(order by cf_olap_windowerr_sale.vn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win4),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.vn desc rows between 1 preceding and 2 following ),
win2 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),
win3 as (order by cf_olap_windowerr_sale.vn asc),
win4 as (order by cf_olap_windowerr_sale.cn asc);

-- MIN() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.vn) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.pn*cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.cn desc range unbounded preceding ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);

-- MIN() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.qty)) OVER(partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.cn desc range between floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.cn) preceding and floor(cf_olap_windowerr_sale.pn*cf_olap_windowerr_sale.prc) preceding ),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.cn desc range between floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.cn) preceding and floor(cf_olap_windowerr_sale.pn*cf_olap_windowerr_sale.prc) preceding );


-- MIN() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.pn) as int),NULL) OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.pn*cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.cn) as int),NULL) OVER(win4),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.vn) as int),NULL) OVER(win5),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.cn desc rows unbounded preceding ),
win2 as (order by cf_olap_windowerr_sale.pn desc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc),
win4 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc),
win5 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);

-- MIN() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.qty,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.prc) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win4),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.qty*cf_olap_windowerr_sale.prc) as int),NULL) OVER(win5),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.pn asc rows between unbounded preceding and unbounded following ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),
win3 as (order by cf_olap_windowerr_sale.vn desc),
win4 as (order by cf_olap_windowerr_sale.cn desc),
win5 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);

-- MIN() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc rows between current row and floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc) following );

-- MIN() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.vn)) OVER(order by cf_olap_windowerr_sale.vn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.cn) as int),NULL) OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc rows between current row and 0 following ),
win2 as (order by cf_olap_windowerr_sale.vn asc),
win3 as (partition by cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc);

-- STDDEV() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(STDDEV(floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.pn desc range unbounded preceding );

-- STDDEV() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(STDDEV(floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.vn asc range floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.cn) preceding );

-- STDDEV() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(STDDEV(floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(RANK() OVER(partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.pn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.cn)) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win4),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.pn asc range between unbounded preceding and current row ),
win2 as (order by cf_olap_windowerr_sale.pn asc),
win3 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.pn asc),
win4 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.cn desc);

-- STDDEV() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(STDDEV(floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn desc range between floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.pn) preceding and 3 following );

-- STDDEV() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(STDDEV(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.vn asc range between floor(cf_olap_windowerr_sale.vn) preceding and unbounded following ),
win2 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.pn asc);

-- STDDEV() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(STDDEV(floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc) as int),NULL) OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc,cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.cn desc rows between current row and floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.cn) following ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc);

-- STDDEV() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(STDDEV(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc)) OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc rows between 3 following and floor(cf_olap_windowerr_sale.cn) following ),
win2 as (order by cf_olap_windowerr_sale.cn asc);

-- STDDEV() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(STDDEV(floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(order by cf_olap_windowerr_sale.pn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.cn)) OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.cn desc rows between floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.pn) following and unbounded following ),
win2 as (order by cf_olap_windowerr_sale.pn asc);

-- STDDEV() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(STDDEV(floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.qty) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.qty) as int),NULL) OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.cn asc range between unbounded preceding and current row ),
win2 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc);

-- STDDEV() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(STDDEV(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.cn asc range between unbounded preceding and 2 following );

-- STDDEV() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(STDDEV(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.cn asc rows between unbounded preceding and floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.pn) preceding ),
win2 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.vn asc),
win3 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn asc);

-- STDDEV() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(STDDEV(floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.qty) as int),NULL) OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc rows between 4 preceding and current row ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc);

-- STDDEV() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(STDDEV(floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc rows between current row and floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc) following ),
win2 as (order by cf_olap_windowerr_sale.cn desc);

-- STDDEV_POP() function with NULL OVER() clause in combination with other window functions --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(STDDEV_POP(floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999')
,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.pn) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.cn)) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.pn) as int),NULL) OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (),
win2 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),
win3 as (partition by cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc);

-- STDDEV_POP() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(STDDEV_POP(floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(partition by cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.vn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.qty*cf_olap_windowerr_sale.prc) as int),NULL) OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.dt
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.pn desc range between unbounded preceding and current row ),
win2 as (partition by cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.vn asc),
win3 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc);

-- STDDEV_POP() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(STDDEV_POP(floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn desc range between floor(cf_olap_windowerr_sale.vn) preceding and floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.cn) following );

-- STDDEV_POP() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(STDDEV_POP(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.qty) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,
TO_CHAR(COALESCE(DENSE_RANK() OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.vn asc range between current row and floor(cf_olap_windowerr_sale.cn) following ),
win2 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc),
win3 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.pn asc);

-- STDDEV_POP() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(STDDEV_POP(floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.pn) as int),NULL) OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win4),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc rows between unbounded preceding and unbounded following ),
win2 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn desc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc),
win4 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn desc);

-- STDDEV_POP() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(STDDEV_POP(floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.vn desc range between unbounded preceding and floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.qty) preceding );

-- STDDEV_POP() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(STDDEV_POP(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.qty*cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.pn)) OVER(order by cf_olap_windowerr_sale.vn desc),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.vn) as int),NULL) OVER(win4),0),'99999999.9999999'),cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.vn asc range between floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.vn) preceding and 1 preceding ),
win2 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.cn desc),
win3 as (order by cf_olap_windowerr_sale.vn desc),
win4 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc);

-- STDDEV_POP() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(STDDEV_POP(floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.cn asc range between floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.cn) preceding and floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.vn) following );

-- STDDEV_POP() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(STDDEV_POP(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc rows between unbounded preceding and floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.qty) preceding );

-- STDDEV_POP() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(STDDEV_POP(floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.cn desc rows between floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc) preceding and unbounded following );

-- STDDEV_POP() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(STDDEV_POP(floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.prc) as int),NULL) OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win4),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.cn)) OVER(partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.cn asc rows between current row and current row ),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.cn asc rows between current row and current row ),
win2 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.cn desc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc),
win4 as (order by cf_olap_windowerr_sale.pn desc);

-- STDDEV_SAMP() function with NULL OVER() clause in combination with other window functions --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(STDDEV_SAMP(floor(cf_olap_windowerr_sale.prc*cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999')
,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.vn) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.prc) as int),NULL) OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(RANK() OVER(win4),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.vn)) OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc),
win3 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc),
win4 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.vn desc);

-- STDDEV_SAMP() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(STDDEV_SAMP(floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.qty) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.vn) as int),NULL) OVER(win4),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.pn desc range between unbounded preceding and current row ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc),
win3 as (order by cf_olap_windowerr_sale.pn asc),
win4 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc);

-- STDDEV_SAMP() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(STDDEV_SAMP(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.cn)) OVER(order by cf_olap_windowerr_sale.cn asc range between current row and current row ),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.vn)) OVER(order by cf_olap_windowerr_sale.cn asc range between current row and current row ),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn asc range between current row and current row );

-- STDDEV_SAMP() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(STDDEV_SAMP(floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.pn*cf_olap_windowerr_sale.prc)) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.prc) as int),NULL) OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(DENSE_RANK() OVER(win4),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.cn)) OVER(order by cf_olap_windowerr_sale.cn asc range between 2 following and floor(cf_olap_windowerr_sale.vn) following ),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn asc range between 2 following and floor(cf_olap_windowerr_sale.vn) following ),
win2 as (order by cf_olap_windowerr_sale.cn desc),
win3 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc),
win4 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.pn asc);

-- STDDEV_SAMP() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(STDDEV_SAMP(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc rows floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.vn) preceding );

-- STDDEV_SAMP() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(STDDEV_SAMP(floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.prc) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.prc*cf_olap_windowerr_sale.pn)) OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.vn desc rows between unbounded preceding and floor(cf_olap_windowerr_sale.vn) preceding ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);

-- STDDEV_SAMP() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(STDDEV_SAMP(floor(cf_olap_windowerr_sale.prc*cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc rows between floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.prc) preceding and current row );

-- STDDEV_SAMP() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(STDDEV_SAMP(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc rows between 7 following and floor(cf_olap_windowerr_sale.vn) following );

-- STDDEV_SAMP() function with OVER() clause having PARTITION BY and ORDER BY (without framing) in combination with other window functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(STDDEV_SAMP(floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn
,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.vn) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.vn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.vn asc),
win2 as (partition by cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc);

-- STDDEV_SAMP() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(STDDEV_SAMP(floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.qty)) OVER(partition by cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn asc range unbounded preceding ),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.cn) as int),NULL) OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn asc range unbounded preceding ),
win2 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.cn asc),
win3 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc);

-- STDDEV_SAMP() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(STDDEV_SAMP(floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.pn) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win4),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.cn desc range between unbounded preceding and current row ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc),
win3 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.cn asc),
win4 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.vn desc);

-- STDDEV_SAMP() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(STDDEV_SAMP(floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,
TO_CHAR(COALESCE(RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.pn) as int),NULL) OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.vn desc range between floor(cf_olap_windowerr_sale.qty) following and floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.vn) following ),
win2 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.cn desc),
win3 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc);

-- STDDEV_SAMP() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(STDDEV_SAMP(floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.pn) as int),NULL) OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.pn asc range between 4 following and unbounded following ),
win2 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc);

-- STDDEV_SAMP() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(STDDEV_SAMP(floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.cn desc rows floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.pn) preceding );

-- STDDEV_SAMP() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(STDDEV_SAMP(floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.vn desc rows between unbounded preceding and floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.vn) following ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc);

-- STDDEV_SAMP() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(STDDEV_SAMP(floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.vn)) OVER(partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc rows between 3 preceding and current row ),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.prc*cf_olap_windowerr_sale.pn) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.vn)) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc)) OVER(partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc rows between 3 preceding and current row ),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc rows between 3 preceding and current row ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc),
win3 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.pn asc);

-- STDDEV_SAMP() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(STDDEV_SAMP(floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.vn asc rows between floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.cn) following and floor(cf_olap_windowerr_sale.vn) following );

-- SUM() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(SUM(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(RANK() OVER(win4),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.pn asc range current row ),
win2 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.pn asc),
win3 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.cn desc),
win4 as (order by cf_olap_windowerr_sale.vn asc);

-- SUM() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(SUM(floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.prc) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn asc range between unbounded preceding and current row ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc);

-- SUM() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(SUM(floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.prc*cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.vn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.vn)) OVER(partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.qty) as int),NULL) OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn desc range between 2 preceding and floor(cf_olap_windowerr_sale.cn) following ),
win2 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc);

-- SUM() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(SUM(floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,
TO_CHAR(COALESCE(RANK() OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.cn)) OVER(partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn desc range between floor(cf_olap_windowerr_sale.vn) preceding and unbounded following ),
win2 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),
win3 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.vn desc);

-- SUM() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(SUM(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.vn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.qty)) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.pn) as int),NULL) OVER(win4),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.pn asc range between 4 following and unbounded following ),
win2 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc),
win3 as (order by cf_olap_windowerr_sale.vn desc),
win4 as (partition by cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);

-- SUM() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(SUM(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc rows between unbounded preceding and unbounded following );

-- SUM() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(SUM(floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc rows between floor(cf_olap_windowerr_sale.qty) preceding and floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc) preceding );

-- SUM() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(SUM(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.qty) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.vn) as int),NULL) OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win4),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win5),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.vn desc rows between floor(cf_olap_windowerr_sale.prc) preceding and floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.pn) following ),
win2 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc),
win3 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc),
win4 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.cn desc, cf_olap_windowerr_sale.pn),
win5 as (order by cf_olap_windowerr_sale.pn asc);

-- SUM() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(SUM(floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.pn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.cn asc range unbounded preceding ),
win2 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc);

-- SUM() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(SUM(floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.pn asc range between floor(cf_olap_windowerr_sale.qty) preceding and current row );

-- SUM() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(SUM(floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.pn*cf_olap_windowerr_sale.qty)) OVER(partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.vn asc range between current row and current row ),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.cn) as int),NULL) OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.vn asc range between current row and current row ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc);

-- SUM() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(SUM(floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.pn asc range between floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc) following and unbounded following );

-- SUM() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(SUM(floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.pn) as int),NULL) OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.pn asc rows floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.cn) preceding ),
win2 as (order by cf_olap_windowerr_sale.cn desc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc);

-- SUM() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(SUM(floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.vn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.qty) as int),NULL) OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.vn)) OVER(partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc,cf_olap_windowerr_sale.vn asc rows between unbounded preceding and 2 following ),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc,cf_olap_windowerr_sale.vn asc rows between unbounded preceding and 2 following ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc);

-- SUM() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(SUM(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc rows between floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.cn) preceding and current row );

-- SUM() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(SUM(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc rows between current row and current row );

-- VAR_POP() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(VAR_POP(floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn desc range 0 preceding );

-- VAR_POP() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(VAR_POP(floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn desc range between floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc) preceding and floor(cf_olap_windowerr_sale.cn) preceding );
-- VAR_POP() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(VAR_POP(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.pn) as int),NULL) OVER(win4),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn desc range between floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc) preceding and 4 preceding ),
win2 as (order by cf_olap_windowerr_sale.vn asc),
win3 as (order by cf_olap_windowerr_sale.pn asc),
win4 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc);
-- VAR_POP() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(VAR_POP(floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.cn) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.prc) as int),NULL) OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn desc range between floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.vn) preceding and 4 following ),
win2 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc),
win3 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);
-- VAR_POP() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(VAR_POP(floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn desc range between floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc) preceding and floor(cf_olap_windowerr_sale.cn) preceding );

-- VAR_POP() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(VAR_POP(floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.pn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.prc)) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.cn)) OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc rows between unbounded preceding and floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.qty) preceding ),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc rows between unbounded preceding and floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.qty) preceding ),
win2 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);

-- VAR_POP() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(VAR_POP(floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.qty)) OVER(partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.vn desc),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.qty) as int),NULL) OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc rows between floor(cf_olap_windowerr_sale.pn) preceding and current row ),
win2 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.vn desc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc);

-- VAR_POP() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(VAR_POP(floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.cn) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc) as int),NULL) OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win4),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.qty) as int),NULL) OVER(win5),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,
TO_CHAR(COALESCE(RANK() OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.vn asc rows between 2 preceding and unbounded following ),
win2 as (partition by cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc),
win4 as (order by cf_olap_windowerr_sale.pn asc),
win5 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc);

-- VAR_POP() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(VAR_POP(floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.prc) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.qty)) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.pn desc range between 0 preceding and floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.qty) following ),
win2 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc);

-- VAR_POP() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(VAR_POP(floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.vn desc range between current row and floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.qty) following );

-- VAR_POP() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(VAR_POP(floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.qty)) OVER(partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.cn asc range between current row and 1 following ),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.vn) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.cn asc range between current row and 1 following ),
win2 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc);

-- VAR_POP() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(VAR_POP(floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc rows between unbounded preceding and floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.cn) following );

-- VAR_POP() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(VAR_POP(floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.vn) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc rows between 4 preceding and current row ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc);

-- VAR_POP() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(VAR_POP(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.qty)) OVER(partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc rows between floor(cf_olap_windowerr_sale.pn) preceding and floor(cf_olap_windowerr_sale.cn) following ),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc rows between floor(cf_olap_windowerr_sale.pn) preceding and floor(cf_olap_windowerr_sale.cn) following ),
win2 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn asc),
win3 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.pn asc);

-- VAR_POP() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(VAR_POP(floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.cn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.vn) as int),NULL) OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.qty,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.qty)) OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.vn asc rows between current row and unbounded following ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),
win3 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc);

-- VAR_SAMP() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(VAR_SAMP(floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.qty*cf_olap_windowerr_sale.cn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.pn desc range floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.qty) preceding ),
win2 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc);

-- VAR_SAMP() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(VAR_SAMP(floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.qty*cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.cn) as int),NULL) OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.pn asc range between unbounded preceding and unbounded following ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc);

-- VAR_SAMP() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(VAR_SAMP(floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.prc)) OVER(order by cf_olap_windowerr_sale.cn desc range between floor(cf_olap_windowerr_sale.cn) preceding and floor(cf_olap_windowerr_sale.vn) preceding ),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn desc range between floor(cf_olap_windowerr_sale.cn) preceding and floor(cf_olap_windowerr_sale.vn) preceding ),
win2 as (order by cf_olap_windowerr_sale.cn asc);

-- VAR_SAMP() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(VAR_SAMP(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.vn desc range between floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.prc) preceding and unbounded following );

-- VAR_SAMP() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(VAR_SAMP(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc,cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.pn desc rows between floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.qty) preceding and floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.pn) preceding );

-- VAR_SAMP() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(VAR_SAMP(floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(DENSE_RANK() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.vn) as int),NULL) OVER(win4),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc rows between floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.cn) preceding and 1 preceding ),
win2 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.cn asc),
win3 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.vn asc),
win4 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc);

-- VAR_SAMP() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(VAR_SAMP(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc rows between 8 following and unbounded following );

-- VAR_SAMP() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(VAR_SAMP(floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.prc) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc) as int),NULL) OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.vn desc rows between 4 following and unbounded following ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc),
win3 as (partition by cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc);

-- VAR_SAMP() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(VAR_SAMP(floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(order by cf_olap_windowerr_sale.cn desc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.cn asc range floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.pn) preceding ),
win2 as (order by cf_olap_windowerr_sale.cn desc);

-- VAR_SAMP() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(VAR_SAMP(floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(order by cf_olap_windowerr_sale.pn asc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.vn desc range between floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc) preceding and current row ),
win2 as (order by cf_olap_windowerr_sale.pn asc);

-- VAR_SAMP() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(VAR_SAMP(floor(cf_olap_windowerr_sale.pn*cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.qty)) OVER(partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.pn desc range between current row and unbounded following ),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc) as int),NULL) OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.pn desc range between current row and unbounded following ),
win2 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.pn asc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc);

-- VAR_SAMP() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(VAR_SAMP(floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.qty)) OVER(partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.vn desc rows unbounded preceding ),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.qty) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.qty)) OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.vn desc rows unbounded preceding ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc),
win3 as (order by cf_olap_windowerr_sale.pn asc);

-- VAR_SAMP() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(VAR_SAMP(floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.pn desc rows floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.vn) preceding );

-- VAR_SAMP() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(VAR_SAMP(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.qty) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.pn)) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.prc)) OVER(order by cf_olap_windowerr_sale.vn desc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.pn desc rows between 0 preceding and current row ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc),
win3 as (order by cf_olap_windowerr_sale.vn desc);

-- VAR_SAMP() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(VAR_SAMP(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.pn desc rows between current row and 9 following );

-- VAR_SAMP() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(VAR_SAMP(floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc rows between floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.qty) following and unbounded following );

-- VARIANCE() function with NULL OVER() clause in combination with other window functions --

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(VARIANCE(floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999')
,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.prc*cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.prc)) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.vn) as int),NULL) OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (),
win2 as (order by cf_olap_windowerr_sale.cn desc),
win3 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc);

-- VARIANCE() function with OVER() clause having ONLY PARTITION BY in combination with other window functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(VARIANCE(floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc
,
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.qty) as int),NULL) OVER(win4),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(DENSE_RANK() OVER(win5),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win4),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.prc),
win2 as (order by cf_olap_windowerr_sale.pn asc),
win3 as (order by cf_olap_windowerr_sale.pn asc),
win4 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc),
win5 as (order by cf_olap_windowerr_sale.pn desc);

-- VARIANCE() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(VARIANCE(floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.cn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.vn desc range between floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.qty) preceding and floor(cf_olap_windowerr_sale.vn) preceding ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);

-- VARIANCE() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(VARIANCE(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.pn desc range between 2 preceding and current row );

-- VARIANCE() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(VARIANCE(floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.vn)) OVER(order by cf_olap_windowerr_sale.pn asc range between floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.prc) preceding and current row ),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.pn asc range between floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.prc) preceding and current row );

-- VARIANCE() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(VARIANCE(floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.cn)) OVER(order by cf_olap_windowerr_sale.pn desc),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.pn) as int),NULL) OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.vn asc range between floor(cf_olap_windowerr_sale.pn) following and floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.pn) following ),
win2 as (order by cf_olap_windowerr_sale.pn desc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);

-- VARIANCE() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(VARIANCE(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc rows between unbounded preceding and 1 preceding );

-- VARIANCE() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(VARIANCE(floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(RANK() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.qty) as int),NULL) OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win4),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.vn desc rows between unbounded preceding and floor(cf_olap_windowerr_sale.vn) preceding ),
win2 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.pn desc),
win3 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),
win4 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.vn asc);

-- VARIANCE() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(VARIANCE(floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.vn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.qty,
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.qty)) OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.vn asc rows between unbounded preceding and unbounded following ),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win4),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.pn) as int),NULL) OVER(win5),0),'99999999.9999999'),cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.vn asc rows between unbounded preceding and unbounded following ),
win2 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc),
win3 as (order by cf_olap_windowerr_sale.vn asc),
win4 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.cn asc),
win5 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc);

-- VARIANCE() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(VARIANCE(floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.vn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.vn)) OVER(partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.qty*cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc rows between 1 following and floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.vn) following ),
win2 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc),
win3 as (order by cf_olap_windowerr_sale.cn desc);

-- VARIANCE() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(VARIANCE(floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.cn desc rows between floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc) following and unbounded following );

-- VARIANCE() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(VARIANCE(floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.qty*cf_olap_windowerr_sale.prc) as int),NULL) OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc,cf_olap_windowerr_sale.cn desc rows between 2 following and unbounded following ),
win2 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc);

-- VARIANCE() function with OVER() clause having PARTITION BY and ORDER BY (without framing) in combination with other window functions --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(VARIANCE(floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty
,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.pn) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.cn)) OVER(partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.prc) as int),NULL) OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.cn)) OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.cn asc),
win2 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc);

-- VARIANCE() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(VARIANCE(floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.qty) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn asc range unbounded preceding ),
win2 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc);

-- VARIANCE() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(VARIANCE(floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.vn desc range floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.pn) preceding );

-- VARIANCE() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(VARIANCE(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.cn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.pn) as int),NULL) OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.cn) as int),NULL) OVER(win4),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn desc range between floor(cf_olap_windowerr_sale.vn) preceding and 3 preceding ),
win2 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),
win4 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc);

-- VARIANCE() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(VARIANCE(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc rows between unbounded preceding and 4 preceding ),
win2 as (order by cf_olap_windowerr_sale.cn desc);

-- VARIANCE() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(VARIANCE(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc rows between 9 preceding and unbounded following );

-- VARIANCE() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(VARIANCE(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.pn)) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc rows between current row and floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.pn) following ),
win2 as (partition by cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.vn asc),
win3 as (order by cf_olap_windowerr_sale.pn desc);

-- VARIANCE() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(VARIANCE(floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.prc) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(DENSE_RANK() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win4),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win4),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc rows between floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.prc) following and 1 following ),
win2 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),
win3 as (order by cf_olap_windowerr_sale.cn desc),
win4 as (order by cf_olap_windowerr_sale.pn asc);

-- CORR() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(CORR(floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.vn)) OVER(order by cf_olap_windowerr_sale.cn asc range unbounded preceding ),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.pn*cf_olap_windowerr_sale.pn)) OVER(order by cf_olap_windowerr_sale.cn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.prc) as int),NULL) OVER(win4),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn asc range unbounded preceding ),
win2 as (order by cf_olap_windowerr_sale.cn asc),
win3 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.vn asc),
win4 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);

-- CORR() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(CORR(floor(cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.vn) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.pn) as int),NULL) OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.pn*cf_olap_windowerr_sale.prc) as int),NULL) OVER(win4),0),'99999999.9999999'),cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.vn desc range between unbounded preceding and current row ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc),
win4 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);

-- CORR() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(CORR(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.vn asc rows floor(cf_olap_windowerr_sale.vn) preceding );

-- CORR() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(CORR(floor(cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.prc) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.pn)) OVER(partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.prc) as int),NULL) OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.vn asc rows between unbounded preceding and floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.vn) preceding ),
win2 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc);

-- CORR() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(CORR(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.cn)) OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.cn desc rows between unbounded preceding and unbounded following ),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(DENSE_RANK() OVER(partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.pn desc),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.pn desc),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.qty) as int),NULL) OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.cn desc rows between unbounded preceding and unbounded following ),
win2 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.pn desc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc);

-- CORR() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(CORR(floor(cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.qty) as int),NULL) OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.pn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win4),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc rows between 1 preceding and floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc) following ),
win2 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.pn asc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc),
win4 as (order by cf_olap_windowerr_sale.pn desc);

-- CORR() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(CORR(floor(cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.prc*cf_olap_windowerr_sale.prc)) OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc rows between floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.vn) preceding and unbounded following ),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc rows between floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.vn) preceding and unbounded following );

-- CORR() function with OVER() clause having PARTITION BY and ORDER BY (without framing) in combination with other window functions --

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(CORR(floor(cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
,
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.vn)) OVER(partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.cn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.cn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.vn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.cn asc),
win2 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc);

-- CORR() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(CORR(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.cn asc range between 1 preceding and floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc) following );

-- CORR() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(CORR(floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.pn) as int),NULL) OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(partition by cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.pn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.qty*cf_olap_windowerr_sale.qty) as int),NULL) OVER(win4),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.cn desc range between floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.prc) preceding and unbounded following ),
win2 as (partition by cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.pn asc),
win3 as (partition by cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc),
win4 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc);

-- CORR() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(CORR(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.prc)) OVER(partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.pn desc),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.prc) as int),NULL) OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.vn asc range between current row and current row ),
win2 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.pn desc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc);

-- CORR() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(CORR(floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc*cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.qty) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,
TO_CHAR(COALESCE(DENSE_RANK() OVER(partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.prc)) OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.vn desc range between floor(cf_olap_windowerr_sale.qty) following and 4 following ),
win2 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);

-- CORR() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(CORR(floor(cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc*cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.qty) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.pn) as int),NULL) OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(DENSE_RANK() OVER(partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.cn desc range between 2 following and unbounded following ),
win2 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc),
win3 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc);

-- CORR() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(CORR(floor(cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc,cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.vn asc rows between unbounded preceding and floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.qty) preceding );

-- CORR() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(CORR(floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win4),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win5),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.cn)) OVER(win4),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc rows between floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc) preceding and unbounded following ),
win2 as (order by cf_olap_windowerr_sale.vn desc),
win3 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.vn desc),
win4 as (partition by cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.pn desc),
win5 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.cn desc);

-- COVAR_POP() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(COVAR_POP(floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.pn asc range between unbounded preceding and floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.cn) preceding );

-- COVAR_POP() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(COVAR_POP(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn desc range between current row and floor(cf_olap_windowerr_sale.qty) following );

-- COVAR_POP() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(COVAR_POP(floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc rows between floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.prc) preceding and floor(cf_olap_windowerr_sale.cn) preceding );

-- COVAR_POP() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(COVAR_POP(floor(cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.prc)) OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.pn asc rows between 3 preceding and floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.vn) following ),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.pn asc rows between 3 preceding and floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.vn) following ),
win2 as (order by cf_olap_windowerr_sale.cn desc);

-- COVAR_POP() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(COVAR_POP(floor(cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.cn) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.vn) as int),NULL) OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc rows between floor(cf_olap_windowerr_sale.prc) following and unbounded following ),
win2 as (partition by cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc),
win3 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc);

-- COVAR_POP() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(COVAR_POP(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.pn*cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.vn desc range between floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.pn) preceding and floor(cf_olap_windowerr_sale.vn) preceding );

-- COVAR_POP() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(COVAR_POP(floor(cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.prc) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.pn) as int),NULL) OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.pn) as int),NULL) OVER(win4),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.vn asc range between floor(cf_olap_windowerr_sale.vn) preceding and 2 preceding ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc),
win4 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);

-- COVAR_POP() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(COVAR_POP(floor(cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.qty)) OVER(partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.cn desc range between floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.cn) preceding and unbounded following ),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.vn)) OVER(partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.cn desc range between floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.cn) preceding and unbounded following ),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.qty) as int),NULL) OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.cn desc range between floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.cn) preceding and unbounded following ),
win2 as (order by cf_olap_windowerr_sale.vn asc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc);

-- COVAR_POP() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(COVAR_POP(floor(cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.cn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.pn)) OVER(partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.pn asc range between floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.prc) following and floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.prc) following ),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.pn asc range between floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.prc) following and floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.prc) following ),
win2 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc);

-- COVAR_POP() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(COVAR_POP(floor(cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.pn asc range between floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.prc) following and unbounded following );

-- COVAR_POP() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(COVAR_POP(floor(cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.pn) as int),NULL) OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.pn desc range between floor(cf_olap_windowerr_sale.prc) following and unbounded following ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);

-- COVAR_POP() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(COVAR_POP(floor(cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.prc) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.cn) as int),NULL) OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.cn)) OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.cn desc rows between unbounded preceding and current row ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc);

-- COVAR_POP() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(COVAR_POP(floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.cn asc rows between unbounded preceding and floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.prc) following );

-- COVAR_POP() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(COVAR_POP(floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.prc) as int),NULL) OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc rows between unbounded preceding and 0 following ),
win2 as (order by cf_olap_windowerr_sale.pn desc),
win3 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc);

-- COVAR_POP() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(COVAR_POP(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.cn desc rows between floor(cf_olap_windowerr_sale.vn) preceding and floor(cf_olap_windowerr_sale.pn) following );

-- COVAR_POP() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(COVAR_POP(floor(cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.qty) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc rows between current row and floor(cf_olap_windowerr_sale.vn) following ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc);

-- COVAR_SAMP() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(COVAR_SAMP(floor(cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.vn desc range between unbounded preceding and current row ),
win2 as (order by cf_olap_windowerr_sale.pn desc);

-- COVAR_SAMP() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(COVAR_SAMP(floor(cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.qty)) OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc rows floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.qty) preceding ),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.prc*cf_olap_windowerr_sale.prc)) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.vn) as int),NULL) OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc rows floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.qty) preceding ),
win2 as (order by cf_olap_windowerr_sale.vn desc),
win3 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc);

-- COVAR_SAMP() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(COVAR_SAMP(floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.vn desc rows between unbounded preceding and floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.prc) preceding );

-- COVAR_SAMP() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(COVAR_SAMP(floor(cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.prc)) OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.cn desc rows between floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.qty) preceding and current row ),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.vn) as int),NULL) OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.cn desc rows between floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.qty) preceding and current row ),
win2 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.pn asc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc);

-- COVAR_SAMP() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(COVAR_SAMP(floor(cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(RANK() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.vn asc rows between current row and floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.pn) following ),
win2 as (order by cf_olap_windowerr_sale.cn asc);

-- COVAR_SAMP() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(COVAR_SAMP(floor(cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(RANK() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.cn desc range between unbounded preceding and floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.pn) preceding ),
win2 as (order by cf_olap_windowerr_sale.cn asc);

-- COVAR_SAMP() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(COVAR_SAMP(floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.cn asc range between unbounded preceding and floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.prc) following );

-- COVAR_SAMP() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(COVAR_SAMP(floor(cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.pn asc range between floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.pn) preceding and current row );

-- COVAR_SAMP() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(COVAR_SAMP(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.vn) as int),NULL) OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.qty,
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.prc)) OVER(partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.pn)) OVER(partition by cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn desc range between 3 preceding and current row ),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win4),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn desc range between 3 preceding and current row ),
win2 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.cn desc),
win3 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc),
win4 as (order by cf_olap_windowerr_sale.vn asc);

-- COVAR_SAMP() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(COVAR_SAMP(floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.pn/(cf_olap_windowerr_sale.prc+1))) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.vn asc range between floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.prc) preceding and unbounded following );

-- COVAR_SAMP() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(COVAR_SAMP(floor(cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.pn desc range between current row and floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.qty) following );

-- COVAR_SAMP() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(COVAR_SAMP(floor(cf_olap_windowerr_sale.qty*cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.vn asc range between floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.qty) following and 0 following );

-- COVAR_SAMP() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(COVAR_SAMP(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.prc)) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.vn)) OVER(partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc rows between unbounded preceding and floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.qty) preceding ),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc rows between unbounded preceding and floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.qty) preceding ),
win2 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.vn desc);

-- COVAR_SAMP() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(COVAR_SAMP(floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(CUME_DIST() OVER(partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.pn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.prc)) OVER(partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc rows between unbounded preceding and current row ),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.pn) as int),NULL) OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc rows between unbounded preceding and current row ),
win2 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.pn asc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc);

-- COVAR_SAMP() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(COVAR_SAMP(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.pn asc rows between floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.pn) preceding and 2 preceding );

-- COVAR_SAMP() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(COVAR_SAMP(floor(cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.prc*cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.prc)) OVER(partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc rows between floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.pn) preceding and current row ),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.cn) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.pn)) OVER(partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc rows between floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.pn) preceding and current row ),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc rows between floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.pn) preceding and current row ),
win2 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc),
win3 as (order by cf_olap_windowerr_sale.cn asc);

-- REGR_AVGX() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_AVGX(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.qty*cf_olap_windowerr_sale.vn)) OVER(order by cf_olap_windowerr_sale.vn desc),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(order by cf_olap_windowerr_sale.vn desc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.vn asc range between 3 following and 4 following ),
win2 as (order by cf_olap_windowerr_sale.vn desc);

-- REGR_AVGX() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_AVGX(floor(cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.vn desc rows between unbounded preceding and floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.vn) preceding );

-- REGR_AVGX() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_AVGX(floor(cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.cn) as int),NULL) OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.pn asc rows between unbounded preceding and unbounded following ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc);

-- REGR_AVGX() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_AVGX(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc,cf_olap_windowerr_sale.vn desc rows between floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.cn) preceding and current row );

-- REGR_AVGX() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(REGR_AVGX(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.pn) as int),NULL) OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win4),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.pn asc range current row ),
win2 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.cn desc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc),
win4 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.cn asc);

-- REGR_AVGX() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(REGR_AVGX(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.pn*cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.qty)) OVER(partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.vn desc range between unbounded preceding and current row ),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.cn)) OVER(partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.vn desc range between unbounded preceding and current row ),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.vn desc range between unbounded preceding and current row );

-- REGR_AVGX() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(REGR_AVGX(floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc)) OVER(partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn desc range between unbounded preceding and 3 following ),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn desc range between unbounded preceding and 3 following ),
win2 as (order by cf_olap_windowerr_sale.cn asc);

-- REGR_AVGX() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(REGR_AVGX(floor(cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.pn) as int),NULL) OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.cn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.cn desc range between 3 following and 0 following ),
win2 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.cn asc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc);

-- REGR_AVGX() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_AVGX(floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.prc) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.qty,
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.vn asc range between floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.pn) following and unbounded following ),
win2 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);

-- REGR_AVGX() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_AVGX(floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.prc*cf_olap_windowerr_sale.cn)) OVER(partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.pn desc rows between unbounded preceding and 1 preceding ),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.qty) as int),NULL) OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.pn desc rows between unbounded preceding and 1 preceding ),
win2 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.pn asc),
win3 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc);

-- REGR_AVGX() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_AVGX(floor(cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.pn) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(partition by cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(partition by cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc rows between floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.vn) preceding and floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.prc) preceding ),
win2 as (partition by cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);

-- REGR_AVGX() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_AVGX(floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.pn)) OVER(partition by cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.cn asc rows between floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.cn) following and 1 following ),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(DENSE_RANK() OVER(partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.cn desc),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.qty) as int),NULL) OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.cn asc rows between floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.cn) following and 1 following ),
win2 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.cn desc),
win3 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);

-- REGR_AVGX() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_AVGX(floor(cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.vn) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.vn)) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.pn*cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.pn) as int),NULL) OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc)) OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win4),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc rows between floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.pn) following and unbounded following ),
win2 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc),
win4 as (order by cf_olap_windowerr_sale.pn desc);

-- REGR_AVGY() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_AVGY(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn desc range unbounded preceding );

-- REGR_AVGY() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_AVGY(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.prc)) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.vn asc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn desc range unbounded preceding ),
win2 as (order by cf_olap_windowerr_sale.vn asc),
win3 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.vn asc);

-- REGR_AVGY() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_AVGY(floor(cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.prc) as int),NULL) OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win4),0),'99999999.9999999'),cf_olap_windowerr_sale.dt
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.pn desc range 4 preceding ),
win2 as (order by cf_olap_windowerr_sale.vn desc),
win3 as (partition by cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),
win4 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.vn desc);

-- REGR_AVGY() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_AVGY(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.vn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.vn asc range between unbounded preceding and floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.prc) preceding ),
win2 as (partition by cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc);

-- REGR_AVGY() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_AVGY(floor(cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.pn asc range between unbounded preceding and floor(cf_olap_windowerr_sale.qty) following );

-- REGR_AVGY() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_AVGY(floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn asc range between unbounded preceding and floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.pn) following ),
win2 as (order by cf_olap_windowerr_sale.pn desc);

-- REGR_AVGY() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(REGR_AVGY(floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn asc range between floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.vn) following and unbounded following );

-- REGR_AVGY() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_AVGY(floor(cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.qty) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.vn)) OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc rows between unbounded preceding and floor(cf_olap_windowerr_sale.prc) following ),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc rows between unbounded preceding and floor(cf_olap_windowerr_sale.prc) following ),
win2 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),
win3 as (partition by cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.pn asc);

-- REGR_AVGY() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(REGR_AVGY(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.pn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.pn asc rows between floor(cf_olap_windowerr_sale.pn) preceding and 4 preceding ),
win2 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc),
win3 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.pn desc);

-- REGR_AVGY() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_AVGY(floor(cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.prc) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.qty*cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc) as int),NULL) OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc,cf_olap_windowerr_sale.cn asc rows between floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.pn) preceding and current row ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc);

-- REGR_AVGY() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_AVGY(floor(cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc rows between 4 preceding and floor(cf_olap_windowerr_sale.pn) following );

-- REGR_AVGY() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_AVGY(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.prc) as int),NULL) OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc rows between floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.vn) preceding and unbounded following ),
win2 as (partition by cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc);

-- REGR_AVGY() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_AVGY(floor(cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.cn desc range between floor(cf_olap_windowerr_sale.pn) preceding and floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.vn) preceding );

-- REGR_AVGY() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(REGR_AVGY(floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.cn asc range between current row and unbounded following );

-- REGR_AVGY() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_AVGY(floor(cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.vn desc range between floor(cf_olap_windowerr_sale.vn) following and floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc) following );

-- REGR_AVGY() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_AVGY(floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.prc)) OVER(partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.vn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.cn asc range between 4 following and unbounded following ),
win2 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.vn asc);

-- REGR_AVGY() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_AVGY(floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.pn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.prc*cf_olap_windowerr_sale.prc) as int),NULL) OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,
TO_CHAR(COALESCE(DENSE_RANK() OVER(win4),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc)) OVER(partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc rows 2 preceding ),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc rows 2 preceding ),
win2 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc),
win3 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc),
win4 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn asc);

-- REGR_AVGY() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_AVGY(floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.cn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,
TO_CHAR(COALESCE(ROW_NUMBER() OVER(partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.qty) as int),NULL) OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.cn desc rows between unbounded preceding and floor(cf_olap_windowerr_sale.pn) preceding ),
win2 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);

-- REGR_AVGY() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_AVGY(floor(cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc,cf_olap_windowerr_sale.cn desc rows between unbounded preceding and unbounded following );

-- REGR_AVGY() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_AVGY(floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.prc*cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc rows between floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.qty) preceding and unbounded following ),
win2 as (order by cf_olap_windowerr_sale.pn desc),
win3 as (partition by cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn desc);

-- REGR_AVGY() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(REGR_AVGY(floor(cf_olap_windowerr_sale.pn*cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc rows between current row and floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.cn) following );

-- REGR_COUNT() function with OVER() clause having ONLY PARTITION BY in combination with other window functions --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_COUNT(floor(cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn
,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.pn) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win4),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc),
win3 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn asc),
win4 as (order by cf_olap_windowerr_sale.vn asc);

-- REGR_COUNT() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_COUNT(floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.vn asc range floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.cn) preceding );

-- REGR_COUNT() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(REGR_COUNT(floor(cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.cn) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc)) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(order by cf_olap_windowerr_sale.cn desc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn asc range current row ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc),
win3 as (order by cf_olap_windowerr_sale.cn desc);

-- REGR_COUNT() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_COUNT(floor(cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn asc range between unbounded preceding and current row );

-- REGR_COUNT() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_COUNT(floor(cf_olap_windowerr_sale.qty*cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.prc*cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn asc range between unbounded preceding and floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.vn) following );

-- REGR_COUNT() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(REGR_COUNT(floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.vn) as int),NULL) OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn asc range between 1 following and 3 following ),
win2 as (order by cf_olap_windowerr_sale.vn desc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc);

-- REGR_COUNT() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(REGR_COUNT(floor(cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn desc range between floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.cn) following and unbounded following );

-- REGR_COUNT() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_COUNT(floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.prc*cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc)) OVER(order by cf_olap_windowerr_sale.vn desc range between floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.qty) following and unbounded following ),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.vn desc range between floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.qty) following and unbounded following ),
win2 as (order by cf_olap_windowerr_sale.cn desc);

-- REGR_COUNT() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_COUNT(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc rows between 5 preceding and current row );

-- REGR_COUNT() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_COUNT(floor(cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc,cf_olap_windowerr_sale.pn desc,cf_olap_windowerr_sale.cn desc rows between floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.prc) preceding and unbounded following );

-- REGR_COUNT() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_COUNT(floor(cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc rows between current row and floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.pn) following );

-- REGR_COUNT() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_COUNT(floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.vn asc range between unbounded preceding and unbounded following );

-- REGR_COUNT() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_COUNT(floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.pn) as int),NULL) OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win4),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.pn)) OVER(order by cf_olap_windowerr_sale.vn asc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.vn asc range between current row and current row ),
win2 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.vn desc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc),
win4 as (order by cf_olap_windowerr_sale.vn asc);

-- REGR_COUNT() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_COUNT(floor(cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.vn asc range between 3 following and unbounded following );

-- REGR_COUNT() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_COUNT(floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.vn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(CUME_DIST() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win4),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.cn) as int),NULL) OVER(win5),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.vn desc rows unbounded preceding ),
win2 as (partition by cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc),
win3 as (order by cf_olap_windowerr_sale.vn desc),
win4 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn desc),
win5 as (partition by cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);

-- REGR_COUNT() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(REGR_COUNT(floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc rows between unbounded preceding and floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc) following );

-- REGR_COUNT() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(REGR_COUNT(floor(cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.qty) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(PERCENT_RANK() OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.vn) as int),NULL) OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.qty) as int),NULL) OVER(win4),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win4),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc rows between unbounded preceding and 1 following ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc),
win4 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc);

-- REGR_COUNT() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_COUNT(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc rows between floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.cn) preceding and floor(cf_olap_windowerr_sale.vn) preceding );

-- REGR_COUNT() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_COUNT(floor(cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.qty) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.vn) as int),NULL) OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.qty,
TO_CHAR(COALESCE(RANK() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.pn)) OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.pn)) OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.pn asc rows between floor(cf_olap_windowerr_sale.qty) preceding and floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.vn) following ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),
win3 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc);

-- REGR_INTERCEPT() function with NULL OVER() clause --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_INTERCEPT(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as ();

-- REGR_INTERCEPT() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_INTERCEPT(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.pn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(RANK() OVER(partition by cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.prc)) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.vn)) OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.pn asc range unbounded preceding ),
win2 as (partition by cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);

-- REGR_INTERCEPT() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_INTERCEPT(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn asc range between current row and unbounded following );

-- REGR_INTERCEPT() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_INTERCEPT(floor(cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn desc range between floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.vn) following and floor(cf_olap_windowerr_sale.pn) following );

-- REGR_INTERCEPT() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_INTERCEPT(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc rows unbounded preceding );

-- REGR_INTERCEPT() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_INTERCEPT(floor(cf_olap_windowerr_sale.pn*cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.vn asc rows current row );

-- REGR_INTERCEPT() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(REGR_INTERCEPT(floor(cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.vn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.prc*cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.prc*cf_olap_windowerr_sale.prc) as int),NULL) OVER(win4),0),'99999999.9999999'),cf_olap_windowerr_sale.dt
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.cn asc rows between unbounded preceding and current row ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc),
win3 as (order by cf_olap_windowerr_sale.cn desc),
win4 as (partition by cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);

-- REGR_INTERCEPT() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_INTERCEPT(floor(cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc)) OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.vn asc rows between unbounded preceding and 4 following ),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.cn)) OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.vn asc rows between unbounded preceding and 4 following ),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.vn asc rows between unbounded preceding and 4 following ),
win2 as (order by cf_olap_windowerr_sale.cn desc);

-- REGR_INTERCEPT() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_INTERCEPT(floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.cn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.cn)) OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc rows between floor(cf_olap_windowerr_sale.prc) preceding and floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.qty) preceding ),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc rows between floor(cf_olap_windowerr_sale.prc) preceding and floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.qty) preceding ),
win2 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc),
win3 as (order by cf_olap_windowerr_sale.vn desc);

-- REGR_INTERCEPT() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_INTERCEPT(floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.vn) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.pn) as int),NULL) OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.cn)) OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc rows between 1 preceding and current row ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc);

-- REGR_INTERCEPT() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_INTERCEPT(floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc rows between floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc) preceding and unbounded following ),
win2 as (partition by cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.pn desc);

-- REGR_INTERCEPT() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_INTERCEPT(floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.cn) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc rows between floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.pn) following and floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.cn) following ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc);

-- REGR_INTERCEPT() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(REGR_INTERCEPT(floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.pn)) OVER(partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.cn desc range unbounded preceding ),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.cn desc range unbounded preceding ),
win2 as (order by cf_olap_windowerr_sale.vn asc),
win3 as (order by cf_olap_windowerr_sale.cn asc);

-- REGR_INTERCEPT() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_INTERCEPT(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.qty)) OVER(partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.pn asc range between unbounded preceding and floor(cf_olap_windowerr_sale.vn) preceding ),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.vn)) OVER(partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.pn asc range between unbounded preceding and floor(cf_olap_windowerr_sale.vn) preceding ),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.pn*cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.pn) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(partition by cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.pn asc range between unbounded preceding and floor(cf_olap_windowerr_sale.vn) preceding ),
win2 as (partition by cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc);

-- REGR_INTERCEPT() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_INTERCEPT(floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.vn asc range between floor(cf_olap_windowerr_sale.cn) preceding and current row );

-- REGR_INTERCEPT() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_INTERCEPT(floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.vn desc range between current row and current row );

-- REGR_INTERCEPT() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_INTERCEPT(floor(cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.prc)) OVER(partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn desc range between current row and unbounded following ),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.cn) as int),NULL) OVER(win4),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.vn)) OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn desc range between current row and unbounded following ),
win2 as (order by cf_olap_windowerr_sale.vn desc),
win3 as (order by cf_olap_windowerr_sale.vn asc),
win4 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc);

-- REGR_INTERCEPT() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_INTERCEPT(floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.pn)) OVER(partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.vn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.vn) as int),NULL) OVER(win4),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc rows current row ),
win2 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn asc),
win3 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.vn asc),
win4 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);

-- REGR_INTERCEPT() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_INTERCEPT(floor(cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.prc)) OVER(partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc rows between unbounded preceding and floor(cf_olap_windowerr_sale.prc) preceding ),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.qty) as int),NULL) OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win4),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(partition by cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc rows between unbounded preceding and floor(cf_olap_windowerr_sale.prc) preceding ),
win2 as (order by cf_olap_windowerr_sale.pn desc),
win3 as (partition by cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),
win4 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.pn desc);

-- REGR_INTERCEPT() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_INTERCEPT(floor(cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.vn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(partition by cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.cn)) OVER(partition by cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.cn desc rows between unbounded preceding and current row ),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.cn desc rows between unbounded preceding and current row ),
win2 as (partition by cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc),
win3 as (order by cf_olap_windowerr_sale.pn desc);

-- REGR_INTERCEPT() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_INTERCEPT(floor(cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.prc*cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.vn)) OVER(partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc rows between floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.vn) preceding and current row ),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.qty)) OVER(partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc rows between floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.vn) preceding and current row ),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc rows between floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.vn) preceding and current row ),
win2 as (order by cf_olap_windowerr_sale.pn asc);

-- REGR_INTERCEPT() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_INTERCEPT(floor(cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc,cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.cn asc rows between floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.qty) following and floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.cn) following );

-- REGR_R2() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_R2(floor(cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn asc range floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.qty) preceding );

-- REGR_R2() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(REGR_R2(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.qty) as int),NULL) OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn desc range current row ),
win2 as (order by cf_olap_windowerr_sale.cn desc),
win3 as (partition by cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);

-- REGR_R2() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_R2(floor(cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.vn asc range between floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.qty) preceding and 2 preceding );

-- REGR_R2() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_R2(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.cn) as int),NULL) OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.prc*cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.pn) as int),NULL) OVER(win4),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.qty)) OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn desc range between floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.pn) preceding and current row ),
win2 as (order by cf_olap_windowerr_sale.vn asc),
win3 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc),
win4 as (partition by cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc);

-- REGR_R2() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_R2(floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(DENSE_RANK() OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.pn) as int),NULL) OVER(win4),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win5),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc rows floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.cn) preceding ),
win2 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.vn desc),
win3 as (order by cf_olap_windowerr_sale.pn desc),
win4 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc),
win5 as (order by cf_olap_windowerr_sale.cn desc);

-- REGR_R2() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_R2(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc rows between unbounded preceding and floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.vn) preceding );

-- REGR_R2() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_R2(floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.qty*cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.qty)) OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.pn desc rows between floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.prc) preceding and current row ),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.prc)) OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.pn desc rows between floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.prc) preceding and current row ),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.pn) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.prc/(cf_olap_windowerr_sale.prc+1)) as int),NULL) OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.pn desc rows between floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.prc) preceding and current row ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc);

-- REGR_R2() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_R2(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.pn desc,cf_olap_windowerr_sale.vn asc rows between floor(cf_olap_windowerr_sale.pn) preceding and unbounded following );

-- REGR_R2() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_R2(floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.cn)) OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.cn desc rows between 2 preceding and unbounded following ),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.cn desc rows between 2 preceding and unbounded following ),
win2 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.pn desc);

-- REGR_R2() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_R2(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.pn) as int),NULL) OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.cn desc range between unbounded preceding and 0 preceding ),
win2 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.cn desc),
win3 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc);

-- REGR_R2() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(REGR_R2(floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.cn asc range between unbounded preceding and unbounded following );

-- REGR_R2() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(REGR_R2(floor(cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.prc) as int),NULL) OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.cn desc range between floor(cf_olap_windowerr_sale.pn) preceding and floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.prc) preceding ),
win2 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc);

-- REGR_R2() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(REGR_R2(floor(cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.qty*cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.vn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.pn asc range between current row and floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc) following ),
win2 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc);

-- REGR_R2() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(REGR_R2(floor(cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.prc) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn asc range between floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.qty) following and unbounded following ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc);

-- REGR_R2() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_R2(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.pn*cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.vn asc rows floor(cf_olap_windowerr_sale.vn) preceding );

-- REGR_R2() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_R2(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.cn)) OVER(partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.cn asc rows current row ),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.qty) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.cn asc rows current row ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),
win3 as (order by cf_olap_windowerr_sale.pn asc);

-- REGR_R2() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_R2(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.pn asc rows between unbounded preceding and floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.pn) preceding );

-- REGR_R2() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(REGR_R2(floor(cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.qty) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.qty*cf_olap_windowerr_sale.qty)) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.pn desc,cf_olap_windowerr_sale.cn desc rows between floor(cf_olap_windowerr_sale.prc) preceding and floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.vn) preceding ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc);

-- REGR_R2() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_R2(floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc rows between 4 preceding and 4 following );

-- REGR_SLOPE() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_SLOPE(floor(cf_olap_windowerr_sale.qty*cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc)) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn desc range between unbounded preceding and 4 preceding ),
win2 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.pn desc),
win3 as (partition by cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.vn asc);

-- REGR_SLOPE() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_SLOPE(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.qty)) OVER(partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.cn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.pn)) OVER(partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.cn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn asc range between unbounded preceding and 2 following ),
win2 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.cn asc);

-- REGR_SLOPE() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_SLOPE(floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.qty)) OVER(order by cf_olap_windowerr_sale.pn asc range between floor(cf_olap_windowerr_sale.vn) following and floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.vn) following ),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.pn asc range between floor(cf_olap_windowerr_sale.vn) following and floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.vn) following );

-- REGR_SLOPE() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_SLOPE(floor(cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.cn desc rows floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.pn) preceding );

-- REGR_SLOPE() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(REGR_SLOPE(floor(cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc rows current row );

-- REGR_SLOPE() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_SLOPE(floor(cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.vn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(DENSE_RANK() OVER(partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.pn asc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc rows between unbounded preceding and current row ),
win2 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc),
win3 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.pn asc);

-- REGR_SLOPE() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_SLOPE(floor(cf_olap_windowerr_sale.prc*cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(RANK() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.cn) as int),NULL) OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.dt
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.pn desc rows between 0 preceding and floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.prc) preceding ),
win2 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.cn desc),
win3 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc);

-- REGR_SLOPE() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_SLOPE(floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.prc)) OVER(partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.cn desc range current row ),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.cn desc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.cn desc range current row ),
win2 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.cn desc);

-- REGR_SLOPE() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(REGR_SLOPE(floor(cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.qty,
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.vn) as int),NULL) OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.prc) as int),NULL) OVER(win4),0),'99999999.9999999'),cf_olap_windowerr_sale.dt
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.pn desc range between floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.prc) preceding and 4 preceding ),
win2 as (order by cf_olap_windowerr_sale.vn asc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),
win4 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc);

-- REGR_SLOPE() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_SLOPE(floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.vn desc range between current row and floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.pn) following );

-- REGR_SLOPE() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_SLOPE(floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.qty)) OVER(partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn desc range between current row and floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.prc) following ),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn desc range between current row and floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.prc) following ),
win2 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.vn desc);

-- REGR_SLOPE() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_SLOPE(floor(cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.qty) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.vn) as int),NULL) OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.cn desc range between current row and unbounded following ),
win2 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc);

-- REGR_SLOPE() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_SLOPE(floor(cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.qty) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win4),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.qty) as int),NULL) OVER(win5),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.pn desc range between 0 following and unbounded following ),
win2 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc),
win3 as (order by cf_olap_windowerr_sale.cn asc),
win4 as (order by cf_olap_windowerr_sale.pn asc),
win5 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc);

-- REGR_SLOPE() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_SLOPE(floor(cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.vn)) OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.cn desc rows unbounded preceding ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc);

-- REGR_SLOPE() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_SLOPE(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.cn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.prc)) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty*cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.prc) as int),NULL) OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc rows between floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.cn) preceding and floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.prc) preceding ),
win2 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);

-- REGR_SLOPE() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_SLOPE(floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.cn desc rows between current row and floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.pn) following ),
win2 as (order by cf_olap_windowerr_sale.cn desc);

-- REGR_SLOPE() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_SLOPE(floor(cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.cn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(order by cf_olap_windowerr_sale.pn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.pn asc rows between 4 following and 2 following ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),
win3 as (order by cf_olap_windowerr_sale.pn asc);

-- REGR_SXX() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_SXX(floor(cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn desc range floor(cf_olap_windowerr_sale.cn) preceding );

-- REGR_SXX() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(REGR_SXX(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.vn)) OVER(order by cf_olap_windowerr_sale.cn asc range between 0 following and 3 following ),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(RANK() OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn asc range between 0 following and 3 following ),
win2 as (order by cf_olap_windowerr_sale.pn asc),
win3 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.cn asc);

-- REGR_SXX() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_SXX(floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc,cf_olap_windowerr_sale.cn asc rows 9 preceding );

-- REGR_SXX() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_SXX(floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.cn) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win4),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win5),0),'99999999.9999999'),cf_olap_windowerr_sale.prc
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc rows between floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.vn) preceding and 4 preceding ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc),
win3 as (order by cf_olap_windowerr_sale.pn desc),
win4 as (order by cf_olap_windowerr_sale.cn asc),
win5 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.cn desc);

-- REGR_SXX() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_SXX(floor(cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.cn asc rows between floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.pn) preceding and current row );

-- REGR_SXX() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_SXX(floor(cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.cn)) OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.cn desc rows between floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.qty) preceding and 2 following ),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.prc) as int),NULL) OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.pn) as int),NULL) OVER(win4),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.cn desc rows between floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.qty) preceding and 2 following ),
win2 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.pn asc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc),
win4 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc);

-- REGR_SXX() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_SXX(floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.pn asc rows between floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.prc) preceding and unbounded following );

-- REGR_SXX() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_SXX(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.vn)) OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.vn desc rows between floor(cf_olap_windowerr_sale.qty) preceding and unbounded following ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc);

-- REGR_SXX() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(REGR_SXX(floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.pn desc rows between current row and current row ),
win2 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc);

-- REGR_SXX() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_SXX(floor(cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.cn)) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.pn)) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.vn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.cn)) OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.vn desc rows between current row and floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.qty) following ),
win2 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.vn asc);

-- REGR_SXX() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_SXX(floor(cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.prc*cf_olap_windowerr_sale.vn)) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.cn)) OVER(order by cf_olap_windowerr_sale.vn desc),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.pn) as int),NULL) OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win4),0),'99999999.9999999'),cf_olap_windowerr_sale.dt
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.cn asc rows between floor(cf_olap_windowerr_sale.pn) following and unbounded following ),
win2 as (order by cf_olap_windowerr_sale.vn desc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc),
win4 as (partition by cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.vn desc);

-- REGR_SXX() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_SXX(floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.cn asc range floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.pn) preceding );

-- REGR_SXX() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(REGR_SXX(floor(cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.cn asc range between unbounded preceding and floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.cn) preceding );

-- REGR_SXX() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(REGR_SXX(floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.cn asc range between floor(cf_olap_windowerr_sale.qty) preceding and floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc) preceding );

-- REGR_SXX() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_SXX(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.prc) as int),NULL) OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.vn desc range between 4 preceding and current row ),
win2 as (partition by cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.vn asc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc);

-- REGR_SXX() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_SXX(floor(cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty/(cf_olap_windowerr_sale.prc+1)) as int),cast (floor(cf_olap_windowerr_sale.qty) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.pn) as int),NULL) OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.qty) as int),NULL) OVER(win4),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn asc range between 3 following and 1 following ),
win2 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc),
win4 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc);

-- REGR_SXX() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(REGR_SXX(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc rows floor(cf_olap_windowerr_sale.cn) preceding );

-- REGR_SXX() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_SXX(floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.pn desc rows current row );

-- REGR_SXX() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_SXX(floor(cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc rows between unbounded preceding and floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.cn) following );

-- REGR_SXX() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_SXX(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.cn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.qty,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.prc) as int),NULL) OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.prc*cf_olap_windowerr_sale.vn) as int),NULL) OVER(win4),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.cn desc rows between 3 preceding and current row ),
win2 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc),
win3 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),
win4 as (partition by cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);

-- REGR_SXX() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_SXX(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc rows between current row and 0 following );

-- REGR_SXX() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_SXX(floor(cf_olap_windowerr_sale.qty*cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc rows between floor(cf_olap_windowerr_sale.prc) following and floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.pn) following );

-- REGR_SXY() function with OVER() clause having ONLY PARTITION BY in combination with other window functions --

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_SXY(floor(cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn
,
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc) as int),NULL) OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,
TO_CHAR(COALESCE(RANK() OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn),
win2 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn desc),
win3 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc);

-- REGR_SXY() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_SXY(floor(cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.vn)) OVER(order by cf_olap_windowerr_sale.cn asc range floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.pn) preceding ),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn asc range floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.pn) preceding ),
win2 as (order by cf_olap_windowerr_sale.pn desc);

-- REGR_SXY() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_SXY(floor(cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.cn)) OVER(order by cf_olap_windowerr_sale.cn asc range current row ),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.prc) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.vn) as int),NULL) OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn asc range current row ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc),
win3 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc);

-- REGR_SXY() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_SXY(floor(cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.vn desc range between floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.qty) preceding and floor(cf_olap_windowerr_sale.prc) preceding );

-- REGR_SXY() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_SXY(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.qty)) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.prc) as int),NULL) OVER(win4),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.pn) as int),NULL) OVER(win5),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn desc range between current row and floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.prc) following ),
win2 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.cn asc),
win3 as (order by cf_olap_windowerr_sale.cn asc),
win4 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),
win5 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc);

-- REGR_SXY() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_SXY(floor(cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.cn desc rows unbounded preceding );

-- REGR_SXY() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_SXY(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.vn)) OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.pn desc rows unbounded preceding ),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc)) OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.qty) as int),NULL) OVER(win4),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.pn desc rows unbounded preceding ),
win2 as (order by cf_olap_windowerr_sale.vn asc),
win3 as (order by cf_olap_windowerr_sale.cn desc),
win4 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc);

-- REGR_SXY() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_SXY(floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.qty) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.vn) as int),NULL) OVER(win4),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.prc) as int),NULL) OVER(win5),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.cn asc rows 3 preceding ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc),
win3 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.vn desc),
win4 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc),
win5 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);

-- REGR_SXY() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_SXY(floor(cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.cn desc rows between unbounded preceding and current row );

-- REGR_SXY() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_SXY(floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.pn desc rows between current row and floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.qty) following );

-- REGR_SXY() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_SXY(floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.cn asc range unbounded preceding );

-- REGR_SXY() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(REGR_SXY(floor(cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.qty) as int),NULL) OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.pn) as int),NULL) OVER(win4),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.pn asc range 1 preceding ),
win2 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.vn asc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc),
win4 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc);

-- REGR_SXY() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_SXY(floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.qty) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(DENSE_RANK() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(partition by cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.pn asc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.cn desc range between unbounded preceding and current row ),
win2 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),
win3 as (partition by cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.pn asc);

-- REGR_SXY() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_SXY(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.vn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.qty,
TO_CHAR(COALESCE(CUME_DIST() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win4),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.qty)) OVER(order by cf_olap_windowerr_sale.pn asc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.pn desc range between floor(cf_olap_windowerr_sale.cn) preceding and 1 following ),
win2 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc),
win3 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.vn asc),
win4 as (order by cf_olap_windowerr_sale.pn asc);

-- REGR_SXY() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(REGR_SXY(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.qty)) OVER(partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.pn desc range between current row and current row ),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.cn)) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.prc) as int),NULL) OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.pn desc range between current row and current row ),
win2 as (order by cf_olap_windowerr_sale.pn asc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc);

-- REGR_SXY() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_SXY(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.pn desc range between floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.qty) following and floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.prc) following );

-- REGR_SXY() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(REGR_SXY(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.vn) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.cn desc rows current row ),
win2 as (partition by cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc),
win3 as (order by cf_olap_windowerr_sale.cn desc);

-- REGR_SXY() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_SXY(floor(cf_olap_windowerr_sale.qty*cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.prc) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.vn)) OVER(partition by cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.vn) as int),NULL) OVER(win4),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.pn*cf_olap_windowerr_sale.cn)) OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.vn desc rows between unbounded preceding and floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.pn) preceding ),
win2 as (partition by cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc),
win3 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.cn desc),
win4 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);

-- REGR_SXY() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_SXY(floor(cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.cn)) OVER(partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc rows between current row and current row ),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc rows between current row and current row );

-- REGR_SYY() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_SYY(floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.qty) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.pn asc range between unbounded preceding and 0 preceding ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc),
win3 as (order by cf_olap_windowerr_sale.vn asc);

-- REGR_SYY() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_SYY(floor(cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.qty) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.qty)) OVER(partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.qty*cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.pn) as int),NULL) OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.pn desc range between floor(cf_olap_windowerr_sale.prc) preceding and current row ),
win2 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc);

-- REGR_SYY() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_SYY(floor(cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.pn) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.cn) as int),NULL) OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win4),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn asc range between current row and current row ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),
win3 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc),
win4 as (order by cf_olap_windowerr_sale.cn desc);

-- REGR_SYY() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(REGR_SYY(floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn desc range between current row and unbounded following );

-- REGR_SYY() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_SYY(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.vn asc range between floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.prc) following and floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.pn) following );

-- REGR_SYY() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_SYY(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.vn asc range between 1 following and floor(cf_olap_windowerr_sale.qty) following ),
win2 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.vn asc);

-- REGR_SYY() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_SYY(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.cn)) OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.cn desc rows between unbounded preceding and floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.vn) preceding ),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.qty) as int),NULL) OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.cn desc rows between unbounded preceding and floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.vn) preceding ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc);

-- REGR_SYY() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_SYY(floor(cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.vn asc rows between floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc) preceding and 8 following );

-- REGR_SYY() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_SYY(floor(cf_olap_windowerr_sale.qty*cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc rows between floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.qty) preceding and unbounded following );

-- REGR_SYY() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(REGR_SYY(floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.cn)) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.prc) as int),NULL) OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.pn*cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.qty) as int),NULL) OVER(win4),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.pn*cf_olap_windowerr_sale.cn)) OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc rows between current row and 1 following ),
win2 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.pn asc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc),
win4 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc);

-- REGR_SYY() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_SYY(floor(cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.pn) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.cn/(cf_olap_windowerr_sale.prc+1)) as int),NULL) OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.vn desc range unbounded preceding ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc),
win3 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);

-- REGR_SYY() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(REGR_SYY(floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.pn)) OVER(partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn desc range between unbounded preceding and 3 following ),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn desc range between unbounded preceding and 3 following );

-- REGR_SYY() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_SYY(floor(cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.vn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.qty,
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.vn asc range between floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.vn) preceding and 1 following ),
win2 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc);

-- REGR_SYY() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(REGR_SYY(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc rows between unbounded preceding and unbounded following );

-- REGR_SYY() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(REGR_SYY(floor(cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc rows between 8 preceding and current row );

-- REGR_SYY() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_SYY(floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc rows between floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.pn) preceding and unbounded following );


-- The PRECEDING/FOLLOWING clauses cannot be NULL.
SELECT sum(g) OVER (ORDER BY g ROWS BETWEEN NULL::int4 PRECEDING AND UNBOUNDED FOLLOWING) from generate_series(1, 5) g;
SELECT sum(g) OVER (ORDER BY g ROWS BETWEEN UNBOUNDED PRECEDING AND NULL::int4 FOLLOWING) from generate_series(1, 5) g;

-- Not even an expression that yields NULL at runtime.
create function retnull() returns int4 as $$ select NULL::int4 $$ language sql volatile;
SELECT sum(g) OVER (ORDER BY g ROWS BETWEEN retnull() PRECEDING AND UNBOUNDED FOLLOWING) from generate_series(1, 5) g;
SELECT sum(g) OVER (ORDER BY g ROWS BETWEEN UNBOUNDED PRECEDING AND retnull() FOLLOWING) from generate_series(1, 5) g;
drop function retnull();

-- But an expression that contains a NULL, but doesn't return NULL, is OK. (May seem like
-- an odd case to test, but we used to incorrectly reject expressions that contained
-- any NULL constants anywhere.)
SELECT sum(g) OVER (ORDER BY g ROWS BETWEEN (array[1,2, NULL])[1] PRECEDING AND UNBOUNDED FOLLOWING) from generate_series(1, 5) g;
SELECT sum(g) OVER (ORDER BY g ROWS BETWEEN UNBOUNDED PRECEDING AND (array[1,2, NULL])[1] FOLLOWING) from generate_series(1, 5) g;


-- The PRECEDING/FOLLOWING expression must be an integer (or coercible to
-- an integer).
SELECT sum(g) OVER (ORDER BY g  ROWS BETWEEN UNBOUNDED PRECEDING AND ('2'::text) FOLLOWING) from generate_series(1, 5) g;
SELECT sum(g) OVER (ORDER BY g  ROWS BETWEEN UNBOUNDED PRECEDING AND '2' FOLLOWING) from generate_series(1, 5) g;
SELECT sum(g) OVER (ORDER BY g  ROWS BETWEEN UNBOUNDED PRECEDING AND '2' || '' FOLLOWING) from generate_series(1, 5) g;
SELECT sum(g) OVER (ORDER BY g  ROWS BETWEEN UNBOUNDED PRECEDING AND ('2' || '')::integer FOLLOWING) from generate_series(1, 5) g;
SELECT sum(g) OVER (ORDER BY g  ROWS BETWEEN now() PRECEDING AND UNBOUNDED FOLLOWING) from generate_series(1, 5) g;

-- And it must not be negative. Check with a constant, a stable expression that
-- can be evaluated at planning time, and with a volatile expression.
create function retneg() returns int4 as $$ begin return -1::int4; end; $$ language plpgsql volatile;

SELECT sum(g) OVER (ORDER BY g  ROWS BETWEEN UNBOUNDED PRECEDING AND -123 FOLLOWING) from generate_series(1, 5) g;
SELECT sum(g) OVER (ORDER BY g  ROWS BETWEEN UNBOUNDED PRECEDING AND '-123' FOLLOWING) from generate_series(1, 5) g;
SELECT sum(g) OVER (ORDER BY g  ROWS BETWEEN UNBOUNDED PRECEDING AND ('-123' || '')::integer FOLLOWING) from generate_series(1, 5) g;
SELECT sum(g) OVER (ORDER BY g  ROWS BETWEEN UNBOUNDED PRECEDING AND retneg() FOLLOWING) from generate_series(1, 5) g;

drop function retneg();


-- Overriding a window frame (i.e. ROWS BETWEEN ...) in the OVER clause,
-- when a frame already given in the WINDOW definition, is not allowed.
select avg(a) over (w rows between 1 preceding and 1 following)
from generate_series(1,5) a
window w as (order by a rows between unbounded preceding and unbounded following);


CREATE TABLE qp_filter_test (i int, j int) DISTRIBUTED BY (i);
INSERT INTO qp_filter_test VALUES (1, 1);
INSERT INTO qp_filter_test VALUES (2, 1);
INSERT INTO qp_filter_test VALUES (3, 1);
INSERT INTO qp_filter_test VALUES (4, 2);
INSERT INTO qp_filter_test VALUES (NULL, 2);
INSERT INTO qp_filter_test VALUES (6, 2);
INSERT INTO qp_filter_test VALUES (7, 3);
INSERT INTO qp_filter_test VALUES (8, NULL);
INSERT INTO qp_filter_test VALUES (9, 3);
INSERT INTO qp_filter_test VALUES (10, NULL);

select j, i, ntile(j) over (partition by j order by i) FROM qp_filter_test;

SELECT ntile(-1) over (order by i) FROM qp_filter_test;

SELECT ntile(0) over (order by i) FROM qp_filter_test;

SELECT ntile(0) over (order by i) FROM qp_filter_test;

DROP TABLE qp_filter_test;


-- Test use of window functions in places they shouldn't be allowed: MPP-2382
-- CHECK constraints
CREATE TABLE wintest_for_window_seq (i int check (i < count(*) over (order by i)));

CREATE TABLE wintest_for_window_seq (i int default count(*) over (order by i));

-- index expression and function
CREATE TABLE wintest_for_window_seq (i int);
insert into wintest_for_window_seq values(1);
CREATE INDEX wintest_idx_for_window_seq on wintest_for_window_seq (i) where i < count(*) over (order by i);
CREATE INDEX wintest_idx_for_window_seq on wintest_for_window_seq (sum(i) over (order by i));
CREATE INDEX wintest_idx_for_window_seq on wintest_for_window_seq ((sum(i) over (order by i)));
-- alter table
ALTER TABLE wintest_for_window_seq alter i set default count(*) over (order by i);
alter table wintest_for_window_seq alter column i type float using count(*) over (order by
i)::float;

-- select
select * from wintest_for_window_seq where count(*) over (order by i) > 0;

-- update
update wintest_for_window_seq set i = count(*) over (order by i);
update wintest_for_window_seq set i = 1 where count(*) over (order by i) > 0;

-- delete
delete from wintest_for_window_seq where count(*) over (order by i) > 0;

drop table wintest_for_window_seq;

--
-- Test "window does not exist" error in window parsing code
--
select row_number() over (bogus) FROM generate_series(1,10) i;
select row_number() over bogus FROM generate_series(1,10) i;

--
-- STANDARD DATA FOR olap_* TESTS.
--
-- start_ignore
drop table cf_olap_windowerr_customer;
drop table cf_olap_windowerr_vendor;
drop table cf_olap_windowerr_product;
drop table cf_olap_windowerr_sale;
drop table cf_olap_windowerr_sale_ord;
drop table cf_olap_windowerr_util;
-- end_ignore
