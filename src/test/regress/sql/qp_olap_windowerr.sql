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
-- mvd 6->5; 4,8,6,9,1->7; 1->10; 

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
-- mvd 2->5; 2->6; 4,3,8,1->7; 3->9; 1->10; 

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
-- mvd 6->5; 2,8,9->7; 9->10; 9->11; 2,8,9->12; 1,6->13; 

-- COUNT() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(order by cf_olap_windowerr_sale.vn asc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.vn desc range between floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.vn) preceding and floor(cf_olap_windowerr_sale.cn) following ),
win2 as (order by cf_olap_windowerr_sale.vn asc);
-- mvd 3->6; 3->7; 3->8; 

-- COUNT() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn desc range between floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.prc) preceding and unbounded following ),
win2 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.vn asc);
-- mvd 4->3; 6,4,7,1,2->5; 

-- COUNT() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn asc range between current row and unbounded following ),
win2 as (order by cf_olap_windowerr_sale.vn asc);
-- mvd 1->6; 2->7; 

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
-- mvd 1,3->5; 4->6; 4->7; 1,3,4->8; 4->9; 

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
-- mvd 8->7; 8,10,2,1->9; 10,2->11; 8->12; 10,2->13; 10,8,2,15,1->14; 

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
-- mvd 5,6,3,1->4; 3->7; 3->8; 1->9; 6,3->10; 3->11; 

-- COUNT() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.cn desc range between floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.prc) preceding and current row );
-- mvd 6,4,7,3->5; 

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
-- mvd 5,6,7->4; 5,9,7->8; 5,9,7->10; 9->11; 7->12; 5,9,7->13; 

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
-- mvd 3->2; 1,3->4; 1->5; 1,3->6; 1->7; 3->8; 

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
-- mvd 2->4; 2,1->5; 2,1->6; 2,1->7; 9,10,2->8; 

-- MAX() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.pn)) OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.pn asc rows between unbounded preceding and 1 preceding ),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.prc) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.prc)) OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.pn asc rows between unbounded preceding and 1 preceding ),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc)) OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.pn asc rows between unbounded preceding and 1 preceding ),
win2 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);
-- mvd 6,7->5; 6,7->8; 2,1,6,7->9; 6,7->10; 2,1,6,7->11; 

-- MAX() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.prc
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.cn asc rows between floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.prc) preceding and current row ),
win2 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.vn asc);
-- mvd 3->5; 7,3,1,2->6; 

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
-- mvd 3->7; 9->8; 11->10; 9->12; 2,1,3,11->13; 

-- MAX() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.cn)) OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc,cf_olap_windowerr_sale.vn asc rows between current row and unbounded following ),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.qty) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(ROW_NUMBER() OVER(partition by cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc,cf_olap_windowerr_sale.vn asc rows between current row and unbounded following ),
win2 as (partition by cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc);
-- mvd 2,6->5; 2,6->7; 9,2->8; 9,2->10; 9,2->11; 

-- MAX() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.cn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.qty,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.pn) as int),NULL) OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc rows between floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.cn) following and floor(cf_olap_windowerr_sale.pn*cf_olap_windowerr_sale.prc) following ),
win2 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc);
-- mvd 3->2; 1,3,5->4; 7->6; 

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
-- mvd 3,1->2; 3,1->4; 3,1->5; 7,8->6; 7,3,10,8->9; 7,3,1,10->11; 

-- MAX() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(order by cf_olap_windowerr_sale.vn desc),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.vn)) OVER(order by cf_olap_windowerr_sale.cn asc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.pn desc rows between unbounded preceding and 2 preceding ),
win2 as (order by cf_olap_windowerr_sale.vn desc),
win3 as (order by cf_olap_windowerr_sale.cn asc);
-- mvd 5,3,2->4; 3->6; 5->7; 3->8; 3->9; 5->10; 

-- MAX() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc rows between 7 preceding and unbounded following );
-- mvd 4,3,1->6; 

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
-- mvd 3,4,5,6->2; 4,1,6->7; 5->8; 6->9; 3,4,5,6->10; 

-- MAX() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.qty*cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.cn desc rows between floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.pn) following and unbounded following );
-- mvd 3,4,5,6->2; 

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
-- mvd 6,1,3,7->5; 7->8; 7->9; 7->10; 7->11; 

-- MIN() function with NULL OVER() clause in combination with other window functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999')
,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.pn) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc);
-- mvd 7->7; 3->8; 3->9; 

-- MIN() function with OVER() clause having ONLY PARTITION BY --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn);
-- mvd 4->3; 

-- MIN() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.cn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn desc range between unbounded preceding and floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.vn) preceding ),
win2 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc);
-- mvd 6->5; 6,8,4->7; 6,8,4->9; 6,8,4->10; 

-- MIN() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn asc range between floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.cn) preceding and floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.pn) following );
-- mvd 1->6; 

-- MIN() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.qty)) OVER(order by cf_olap_windowerr_sale.cn desc range between floor(cf_olap_windowerr_sale.pn) preceding and floor(cf_olap_windowerr_sale.qty) following ),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.qty) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.qty
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn desc range between floor(cf_olap_windowerr_sale.pn) preceding and floor(cf_olap_windowerr_sale.qty) following ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc),
win3 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.vn desc);
-- mvd 1->5; 1->6; 1->7; 3,4,9->8; 

-- MIN() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.vn desc range between current row and floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.qty) following );
-- mvd 2->3; 

-- MIN() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.pn desc rows unbounded preceding );
-- mvd 3,1->2; 

-- MIN() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc rows current row );
-- mvd 5->4; 

-- MIN() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.qty*cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.vn) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.cn asc rows between unbounded preceding and unbounded following ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc);
-- mvd 3,4->2; 4->5; 4->6; 

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
-- mvd 4->3; 6->5; 4->7; 6->8; 4->9; 6->10; 

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
-- mvd 5->4; 5->6; 3,2,5,1->7; 5->8; 5->9; 3->10; 

-- MIN() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.vn) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.pn*cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.cn desc range unbounded preceding ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);
-- mvd 3,4->2; 3->5; 3,4->6; 

-- MIN() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.qty)) OVER(partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.cn desc range between floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.cn) preceding and floor(cf_olap_windowerr_sale.pn*cf_olap_windowerr_sale.prc) preceding ),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.cn desc range between floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.cn) preceding and floor(cf_olap_windowerr_sale.pn*cf_olap_windowerr_sale.prc) preceding );
-- mvd 3,4,5->2; 3,4,5->6; 

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
-- mvd 6,3,7,1->5; 1->8; 1->9; 1->10; 6->11; 

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
-- mvd 3,8,2->7; 3->9; 1->10; 3->11; 3->12; 

-- MIN() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc rows between current row and floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc) following );
-- mvd 4,1->3; 

-- MIN() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.vn)) OVER(order by cf_olap_windowerr_sale.vn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.cn) as int),NULL) OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc rows between current row and 0 following ),
win2 as (order by cf_olap_windowerr_sale.vn asc),
win3 as (partition by cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc);
-- mvd 5,6,7->4; 3->8; 3->9; 7,1->10; 

-- STDDEV() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(STDDEV(floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.pn desc range unbounded preceding );
-- mvd 8->7; 

-- STDDEV() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(STDDEV(floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.vn asc range floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.cn) preceding );
-- mvd 4->6; 

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
-- mvd 2->4; 2->5; 7,8,9,10,2->6; 7,8,9,10,2->11; 2->12; 8,9,10,2->13; 

-- STDDEV() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(STDDEV(floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn desc range between floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.pn) preceding and 3 following );
-- mvd 5->6; 

-- STDDEV() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(STDDEV(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.vn asc range between floor(cf_olap_windowerr_sale.vn) preceding and unbounded following ),
win2 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.pn asc);
-- mvd 1->6; 2,1,8->7; 

-- STDDEV() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(STDDEV(floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc) as int),NULL) OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc,cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.cn desc rows between current row and floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.cn) following ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc);
-- mvd 3,1->2; 1->4; 

-- STDDEV() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(STDDEV(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc)) OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc rows between 3 following and floor(cf_olap_windowerr_sale.cn) following ),
win2 as (order by cf_olap_windowerr_sale.cn asc);
-- mvd 1->3; 5->4; 5->6; 

-- STDDEV() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(STDDEV(floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(order by cf_olap_windowerr_sale.pn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.cn)) OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.cn desc rows between floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.pn) following and unbounded following ),
win2 as (order by cf_olap_windowerr_sale.pn asc);
-- mvd 3,6->5; 6->7; 6->8; 6->9; 6->10; 

-- STDDEV() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(STDDEV(floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.qty) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.qty) as int),NULL) OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.cn asc range between unbounded preceding and current row ),
win2 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc);
-- mvd 3,1,2->7; 1,2->8; 1->9; 1->10; 

-- STDDEV() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(STDDEV(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.cn asc range between unbounded preceding and 2 following );
-- mvd 1,6,7->5; 

-- STDDEV() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(STDDEV(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.cn asc rows between unbounded preceding and floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.pn) preceding ),
win2 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.vn asc),
win3 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn asc);
-- mvd 3,4,1,5->2; 3,4,1,5->6; 4,1->7; 3,4,5->8; 

-- STDDEV() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(STDDEV(floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.qty) as int),NULL) OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc rows between 4 preceding and current row ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc);
-- mvd 1,5,8->7; 2->9; 

-- STDDEV() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(STDDEV(floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc rows between current row and floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc) following ),
win2 as (order by cf_olap_windowerr_sale.cn desc);
-- mvd 8,5,1->7; 8->9; 8,5,1->10; 

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
-- mvd 6->6; 1,2,5->7; 1,2,5->8; 1,2,5->9; 1,4->10; 

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
-- mvd 1->5; 3,2->6; 3,2->7; 3,2->8; 10,2,1->9; 

-- STDDEV_POP() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(STDDEV_POP(floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn desc range between floor(cf_olap_windowerr_sale.vn) preceding and floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.cn) following );
-- mvd 3->7; 

-- STDDEV_POP() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(STDDEV_POP(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.qty) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,
TO_CHAR(COALESCE(DENSE_RANK() OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.vn asc range between current row and floor(cf_olap_windowerr_sale.cn) following ),
win2 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc),
win3 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.pn asc);
-- mvd 3->2; 5,6,7,1->4; 6,3,1->8; 

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
-- mvd 4->3; 6,7,4,8->5; 8->9; 7,6,8->10; 

-- STDDEV_POP() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(STDDEV_POP(floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.vn desc range between unbounded preceding and floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.qty) preceding );
-- mvd 3,4,5,6->2; 

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
-- mvd 3,4->2; 3,4->5; 7,1,3->6; 3->8; 3->9; 7,1,11->10; 

-- STDDEV_POP() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(STDDEV_POP(floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.cn asc range between floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.cn) preceding and floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.vn) following );
-- mvd 6,7->5; 

-- STDDEV_POP() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(STDDEV_POP(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc rows between unbounded preceding and floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.qty) preceding );
-- mvd 1,3,4->2; 

-- STDDEV_POP() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(STDDEV_POP(floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.cn desc rows between floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc) preceding and unbounded following );
-- mvd 5,1,2->4; 

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
-- mvd 1,6,2,3->7; 1,2->8; 2->9; 3->10; 1,6,2,3->11; 

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
-- mvd 3->3; 3->4; 1->5; 7,8,1->6; 10,1->9; 1->11; 

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
-- mvd 3->2; 3->4; 3->5; 3->6; 3->7; 1->8; 

-- STDDEV_SAMP() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(STDDEV_SAMP(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.cn)) OVER(order by cf_olap_windowerr_sale.cn asc range between current row and current row ),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.vn)) OVER(order by cf_olap_windowerr_sale.cn asc range between current row and current row ),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn asc range between current row and current row );
-- mvd 2->5; 2->6; 2->7; 

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
-- mvd 7->6; 7->8; 7->9; 7,11,2,1->10; 5,11,13,2,1->12; 7->14; 

-- STDDEV_SAMP() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(STDDEV_SAMP(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc rows floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.vn) preceding );
-- mvd 4->3; 

-- STDDEV_SAMP() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(STDDEV_SAMP(floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.prc) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.prc*cf_olap_windowerr_sale.pn)) OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.vn desc rows between unbounded preceding and floor(cf_olap_windowerr_sale.vn) preceding ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);
-- mvd 2,3->6; 2->7; 2->8; 2->9; 2->10; 

-- STDDEV_SAMP() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(STDDEV_SAMP(floor(cf_olap_windowerr_sale.prc*cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc rows between floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.prc) preceding and current row );
-- mvd 2->4; 

-- STDDEV_SAMP() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(STDDEV_SAMP(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc rows between 7 following and floor(cf_olap_windowerr_sale.vn) following );
-- mvd 4->6; 

-- STDDEV_SAMP() function with OVER() clause having PARTITION BY and ORDER BY (without framing) in combination with other window functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(STDDEV_SAMP(floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn
,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.vn) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.vn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.vn asc),
win2 as (partition by cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc);
-- mvd 1,4,5,6->3; 5,6->7; 1,4,5,6->8; 5,6->9; 

-- STDDEV_SAMP() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(STDDEV_SAMP(floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.qty)) OVER(partition by cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn asc range unbounded preceding ),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.cn) as int),NULL) OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn asc range unbounded preceding ),
win2 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.cn asc),
win3 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc);
-- mvd 2,1->5; 2,1->6; 2,1->7; 4,2,3,1->8; 

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
-- mvd 4,5,2,1->3; 1->6; 1->7; 4,5,2,1->8; 1->9; 4,11,5,2,12->10; 

-- STDDEV_SAMP() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(STDDEV_SAMP(floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,
TO_CHAR(COALESCE(RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.pn) as int),NULL) OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.vn desc range between floor(cf_olap_windowerr_sale.qty) following and floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.vn) following ),
win2 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.cn desc),
win3 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc);
-- mvd 5,6,3->4; 2,5->7; 2,1,5,3->8; 5,6,3->9; 

-- STDDEV_SAMP() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(STDDEV_SAMP(floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.pn) as int),NULL) OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.pn asc range between 4 following and unbounded following ),
win2 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc);
-- mvd 3,6,7->5; 1,4->8; 

-- STDDEV_SAMP() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(STDDEV_SAMP(floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.cn desc rows floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.pn) preceding );
-- mvd 3,5,6->4; 

-- STDDEV_SAMP() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(STDDEV_SAMP(floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.vn desc rows between unbounded preceding and floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.vn) following ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc);
-- mvd 3,4,5,1,6->2; 6->7; 6->8; 

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
-- mvd 8,1,2->7; 8,1,2->9; 4->10; 4->11; 2,13->12; 8,1,2->14; 

-- STDDEV_SAMP() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(STDDEV_SAMP(floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.vn asc rows between floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.cn) following and floor(cf_olap_windowerr_sale.vn) following );
-- mvd 5,8,4,6,1,3->7; 

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
-- mvd 2->4; 1,3,2->5; 2->6; 1,3,8,9,2->7; 9->10; 1,3,2->11; 

-- SUM() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(SUM(floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.prc) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn asc range between unbounded preceding and current row ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc);
-- mvd 3->2; 5->4; 

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
-- mvd 1->3; 1,5,6,7->4; 1,5,6,7->8; 1,5,6,7->9; 1,5,6,7->10; 7->11; 

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
-- mvd 3->6; 3->7; 1,9,3,10->8; 3,12->11; 1,9,3,10->13; 

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
-- mvd 7->6; 4,9,1,7->8; 4,9,1,7->10; 12->11; 4,9,1,7->13; 4,7->14; 

-- SUM() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(SUM(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc rows between unbounded preceding and unbounded following );
-- mvd 5->4; 

-- SUM() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(SUM(floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc rows between floor(cf_olap_windowerr_sale.qty) preceding and floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc) preceding );
-- mvd 1->5; 

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
win4 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.cn desc),
win5 as (order by cf_olap_windowerr_sale.pn asc);
-- mvd 3,2->4; 6,3,7,2,8->5; 7,3,2,1->9; 6,3,7,2->10; 1->11; 7,3,2,1->12; 

-- SUM() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(SUM(floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.pn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.cn asc range unbounded preceding ),
win2 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc);
-- mvd 3,4,5->2; 3,4,7->6; 3,4,5->8; 

-- SUM() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(SUM(floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.pn asc range between floor(cf_olap_windowerr_sale.qty) preceding and current row );
-- mvd 3,4,5->2; 

-- SUM() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(SUM(floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.pn*cf_olap_windowerr_sale.qty)) OVER(partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.vn asc range between current row and current row ),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.cn) as int),NULL) OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.vn asc range between current row and current row ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc);
-- mvd 2,5->4; 2,5->6; 5->7; 

-- SUM() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(SUM(floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.pn asc range between floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc) following and unbounded following );
-- mvd 1,6,3,7->5; 

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
-- mvd 3,4,1,5->2; 4->6; 4->7; 5->8; 3,4,1,5->9; 

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
-- mvd 1,5,2->4; 7->6; 7->8; 1,5,2->9; 7->10; 

-- SUM() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(SUM(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc rows between floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.cn) preceding and current row );
-- mvd 4,5->3; 

-- SUM() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(SUM(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc rows between current row and current row );
-- mvd 6,2,7,1,8,9->5; 6,2,7,1,8,9->10; 

-- VAR_POP() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(VAR_POP(floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn desc range 0 preceding );
-- mvd 3->2; 

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
-- mvd 1->2; 4->3; 6->5; 1->7; 
-- VAR_POP() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(VAR_POP(floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.cn) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.prc) as int),NULL) OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn desc range between floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.vn) preceding and 4 following ),
win2 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc),
win3 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);
-- mvd 2->7; 2,5,3,1->8; 10,2,3,5->9; 2->11; 
-- VAR_POP() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(VAR_POP(floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn desc range between floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc) preceding and floor(cf_olap_windowerr_sale.cn) preceding );
-- mvd 2->5; 

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
-- mvd 4->3; 4->5; 7,1,8,2,4->6; 4->9; 7,1,8,2,4->10; 4->11; 

-- VAR_POP() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(VAR_POP(floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.qty)) OVER(partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.vn desc),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.qty) as int),NULL) OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc rows between floor(cf_olap_windowerr_sale.pn) preceding and current row ),
win2 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.vn desc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc);
-- mvd 1->6; 8,3,1->7; 8,3,1->9; 3->10; 

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
-- mvd 2,1,3->5; 1,3->6; 3->7; 3->8; 10,2,1,3->9; 3->11; 

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
-- mvd 3,4->2; 3,6,4->5; 3,4->7; 3,6,4->8; 3,6,4->9; 3,6,4->10; 

-- VAR_POP() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(VAR_POP(floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.vn desc range between current row and floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.qty) following );
-- mvd 3,4,5->2; 

-- VAR_POP() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(VAR_POP(floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.qty)) OVER(partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.cn asc range between current row and 1 following ),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.vn) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.cn asc range between current row and 1 following ),
win2 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc);
-- mvd 2,6,7,4->5; 2,6,7,4->8; 2,6,7,4->9; 2,6,7->10; 2,6,7,4->11; 

-- VAR_POP() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(VAR_POP(floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc rows between unbounded preceding and floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.cn) following );
-- mvd 5->4; 

-- VAR_POP() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(VAR_POP(floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.vn) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc rows between 4 preceding and current row ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc);
-- mvd 4,5,6,7->3; 6->8; 6->9; 6->10; 

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
-- mvd 4,5,6,7->3; 4,5,6,7->8; 1,6,7->9; 1,5,6,7->10; 4,5,6,7->11; 4,5,6,7->12; 

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
-- mvd 4,1->3; 4,1->5; 7->6; 4,7,9,2->8; 4,1->10; 7->11; 

-- VAR_SAMP() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(VAR_SAMP(floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.qty*cf_olap_windowerr_sale.cn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.pn desc range floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.qty) preceding ),
win2 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc);
-- mvd 3->2; 5,1,3->4; 

-- VAR_SAMP() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(VAR_SAMP(floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.qty*cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.cn) as int),NULL) OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.pn asc range between unbounded preceding and unbounded following ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc);
-- mvd 2->5; 2->6; 

-- VAR_SAMP() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(VAR_SAMP(floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.prc)) OVER(order by cf_olap_windowerr_sale.cn desc range between floor(cf_olap_windowerr_sale.cn) preceding and floor(cf_olap_windowerr_sale.vn) preceding ),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn desc range between floor(cf_olap_windowerr_sale.cn) preceding and floor(cf_olap_windowerr_sale.vn) preceding ),
win2 as (order by cf_olap_windowerr_sale.cn asc);
-- mvd 1->4; 1->5; 1->6; 1->7; 1->8; 

-- VAR_SAMP() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(VAR_SAMP(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.vn desc range between floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.prc) preceding and unbounded following );
-- mvd 5->4; 

-- VAR_SAMP() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(VAR_SAMP(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc,cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.pn desc rows between floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.qty) preceding and floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.pn) preceding );
-- mvd 5,1->4; 

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
-- mvd 6->5; 6,8,4,1->7; 8,4,2->9; 6->10; 6,8,4,2->11; 

-- VAR_SAMP() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(VAR_SAMP(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc rows between 8 following and unbounded following );
-- mvd 7->6; 

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
-- mvd 6,2->5; 6,2->7; 2->8; 2->9; 4,2->10; 6,2->11; 

-- VAR_SAMP() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(VAR_SAMP(floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(order by cf_olap_windowerr_sale.cn desc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.cn asc range floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.pn) preceding ),
win2 as (order by cf_olap_windowerr_sale.cn desc);
-- mvd 1->5; 1->6; 1->7; 

-- VAR_SAMP() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(VAR_SAMP(floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(order by cf_olap_windowerr_sale.pn asc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.vn desc range between floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc) preceding and current row ),
win2 as (order by cf_olap_windowerr_sale.pn asc);
-- mvd 7,1->6; 1->8; 1->9; 

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
-- mvd 1,3,4->2; 1,3,4->5; 3,4->6; 8->7; 3,4->9; 

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
-- mvd 1,8,9->7; 1,8,9->10; 8->11; 8->12; 9->13; 8->14; 

-- VAR_SAMP() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(VAR_SAMP(floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.pn desc rows floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.vn) preceding );
-- mvd 5,1,6->4; 

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
-- mvd 3,4,6->5; 3->7; 4->8; 3->9; 4->10; 

-- VAR_SAMP() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(VAR_SAMP(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.pn desc rows between current row and 9 following );
-- mvd 7,3,4,8->6; 

-- VAR_SAMP() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(VAR_SAMP(floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc rows between floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.qty) following and unbounded following );
-- mvd 1,2,3->7; 

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
-- mvd 3->3; 3->4; 6->5; 6->7; 6,1->8; 

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
-- mvd 8,4,1->7; 1->9; 1->10; 4,12,1->11; 1->13; 4,12,1->14; 

-- VARIANCE() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(VARIANCE(floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.cn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.vn desc range between floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.qty) preceding and floor(cf_olap_windowerr_sale.vn) preceding ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);
-- mvd 3->2; 3->4; 6->5; 

-- VARIANCE() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(VARIANCE(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.pn desc range between 2 preceding and current row );
-- mvd 1->5; 

-- VARIANCE() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(VARIANCE(floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.vn)) OVER(order by cf_olap_windowerr_sale.pn asc range between floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.prc) preceding and current row ),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.pn asc range between floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.prc) preceding and current row );
-- mvd 2->4; 2->5; 2->6; 

-- VARIANCE() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(VARIANCE(floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.cn)) OVER(order by cf_olap_windowerr_sale.pn desc),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.pn) as int),NULL) OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.vn asc range between floor(cf_olap_windowerr_sale.pn) following and floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.pn) following ),
win2 as (order by cf_olap_windowerr_sale.pn desc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);
-- mvd 4->3; 6->5; 6->7; 1->8; 

-- VARIANCE() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(VARIANCE(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc rows between unbounded preceding and 1 preceding );
-- mvd 8->7; 

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
-- mvd 3->2; 5,1,6->4; 1,3,6->7; 9,5,10,3->8; 

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
-- mvd 1,2->3; 2,5->4; 1,2->6; 2->7; 1,5->8; 10->9; 

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
-- mvd 4->3; 1,4,6->5; 1,4,6->7; 1,4,6->8; 4->9; 4->10; 

-- VARIANCE() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(VARIANCE(floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.cn desc rows between floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc) following and unbounded following );
-- mvd 3,2->4; 

-- VARIANCE() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(VARIANCE(floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.qty*cf_olap_windowerr_sale.prc) as int),NULL) OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc,cf_olap_windowerr_sale.cn desc rows between 2 following and unbounded following ),
win2 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc);
-- mvd 1,6->5; 1,2,3->7; 

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
-- mvd 5,1,6,3->4; 5,2,1,6,3->7; 5,2,1,6,3->8; 5->9; 5->10; 5,2,1,6,3->11; 

-- VARIANCE() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(VARIANCE(floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.qty) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn asc range unbounded preceding ),
win2 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc);
-- mvd 2,4->3; 2,6,7,4->5; 

-- VARIANCE() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(VARIANCE(floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.vn desc range floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.pn) preceding );
-- mvd 2,4,1,5->3; 

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
-- mvd 6,3->5; 8,6,1->7; 6->9; 8,6,1->10; 6,3->11; 3->12; 

-- VARIANCE() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(VARIANCE(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc rows between unbounded preceding and 4 preceding ),
win2 as (order by cf_olap_windowerr_sale.cn desc);
-- mvd 2,4->3; 2,4->5; 2->6; 

-- VARIANCE() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(VARIANCE(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc rows between 9 preceding and unbounded following );
-- mvd 2,6->5; 

-- VARIANCE() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(VARIANCE(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.pn)) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc rows between current row and floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.pn) following ),
win2 as (partition by cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.vn asc),
win3 as (order by cf_olap_windowerr_sale.pn desc);
-- mvd 7,8,9,4->6; 7,9->10; 7,9->11; 4->12; 

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
-- mvd 1,3,4,5->2; 3,4,7->6; 3->8; 7->9; 3,4,7->10; 7->11; 

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
-- mvd 1->4; 1->5; 1->6; 1,8,3,9->7; 1->10; 1->11; 

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
-- mvd 5->4; 5->6; 5->7; 5->8; 5->9; 2,5,11->10; 

-- CORR() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(CORR(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.vn asc rows floor(cf_olap_windowerr_sale.vn) preceding );
-- mvd 3->7; 

-- CORR() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(CORR(floor(cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.prc) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.pn)) OVER(partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.prc) as int),NULL) OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.vn asc rows between unbounded preceding and floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.vn) preceding ),
win2 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc);
-- mvd 3,4->2; 6,3,4->5; 6,3,4->7; 3->8; 

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
-- mvd 1->3; 1->4; 1,6->5; 1,6->7; 1,6->8; 10->9; 

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
-- mvd 3->2; 5,1,3->4; 3->6; 5->7; 5,1,3->8; 3->9; 

-- CORR() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(CORR(floor(cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.prc*cf_olap_windowerr_sale.prc)) OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc rows between floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.vn) preceding and unbounded following ),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc rows between floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.vn) preceding and unbounded following );
-- mvd 1->5; 1->6; 

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
-- mvd 3,6,1->5; 3,6,1->7; 3,6,1->8; 1,10,11->9; 1,10,11->12; 

-- CORR() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(CORR(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.cn asc range between 1 preceding and floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc) following );
-- mvd 2->5; 

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
-- mvd 5,2,6->4; 5,2,6->7; 5,6->8; 5,6->9; 5,6->10; 12,13,5,2,6->11; 

-- CORR() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(CORR(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.prc)) OVER(partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.pn desc),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.prc) as int),NULL) OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.vn asc range between current row and current row ),
win2 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.pn desc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc);
-- mvd 3,4->2; 1,6,3,7,8->5; 1,6,3,7,8->9; 8->10; 

-- CORR() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(CORR(floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc*cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.qty) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,
TO_CHAR(COALESCE(DENSE_RANK() OVER(partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.prc)) OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.vn desc range between floor(cf_olap_windowerr_sale.qty) following and 4 following ),
win2 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);
-- mvd 6,7,2->5; 9,1,6->8; 9,1,6->10; 9,1,6->11; 9,1,6->12; 

-- CORR() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(CORR(floor(cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc*cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.qty) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.pn) as int),NULL) OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(DENSE_RANK() OVER(partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.cn desc range between 2 following and unbounded following ),
win2 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc),
win3 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc);
-- mvd 1,4->3; 6,4,2,7->5; 1,4,9->8; 6,4,2,7->10; 

-- CORR() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(CORR(floor(cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc,cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.vn asc rows between unbounded preceding and floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.qty) preceding );
-- mvd 1,3,4,5->2; 

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
-- mvd 4,5->3; 1->6; 1,2,5->7; 4,5->8; 10,11->9; 4,5->12; 

-- COVAR_POP() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(COVAR_POP(floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.pn asc range between unbounded preceding and floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.cn) preceding );
-- mvd 3->2; 

-- COVAR_POP() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(COVAR_POP(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn desc range between current row and floor(cf_olap_windowerr_sale.qty) following );
-- mvd 5->4; 

-- COVAR_POP() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(COVAR_POP(floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc rows between floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.prc) preceding and floor(cf_olap_windowerr_sale.cn) preceding );
-- mvd 3->2; 

-- COVAR_POP() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(COVAR_POP(floor(cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.prc)) OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.pn asc rows between 3 preceding and floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.vn) following ),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.pn asc rows between 3 preceding and floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.vn) following ),
win2 as (order by cf_olap_windowerr_sale.cn desc);
-- mvd 1,4->3; 1,4->5; 1->6; 

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
-- mvd 8->7; 5,8->9; 5,4,3,8->10; 5,4,3,8->11; 5,4,3,8->12; 

-- COVAR_POP() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(COVAR_POP(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.pn*cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.vn desc range between floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.pn) preceding and floor(cf_olap_windowerr_sale.vn) preceding );
-- mvd 7,1->6; 

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
-- mvd 1,4->7; 9->8; 9->10; 9->11; 9->12; 

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
-- mvd 5,6,7,1->4; 5,6,7,1->8; 5,6,7,1->9; 11->10; 2->12; 

-- COVAR_POP() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(COVAR_POP(floor(cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.cn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.pn)) OVER(partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.pn asc range between floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.prc) following and floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.prc) following ),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.pn asc range between floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.prc) following and floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.prc) following ),
win2 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc);
-- mvd 3,4->2; 6,4->5; 3,4->7; 6,4->8; 

-- COVAR_POP() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(COVAR_POP(floor(cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.pn asc range between floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.prc) following and unbounded following );
-- mvd 2,1,4->3; 

-- COVAR_POP() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(COVAR_POP(floor(cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.pn) as int),NULL) OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.pn desc range between floor(cf_olap_windowerr_sale.prc) following and unbounded following ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);
-- mvd 4,1->3; 2->5; 

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
-- mvd 2,4,5,1->3; 5->6; 1->7; 1->8; 1->9; 

-- COVAR_POP() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(COVAR_POP(floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.cn asc rows between unbounded preceding and floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.prc) following );
-- mvd 2,7,8,3->6; 

-- COVAR_POP() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(COVAR_POP(floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.prc) as int),NULL) OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc rows between unbounded preceding and 0 following ),
win2 as (order by cf_olap_windowerr_sale.pn desc),
win3 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc);
-- mvd 2,4,6,8->7; 8->9; 5,2,1->10; 

-- COVAR_POP() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(COVAR_POP(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.cn desc rows between floor(cf_olap_windowerr_sale.vn) preceding and floor(cf_olap_windowerr_sale.pn) following );
-- mvd 6,7,8,4->5; 

-- COVAR_POP() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(COVAR_POP(floor(cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.qty) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc rows between current row and floor(cf_olap_windowerr_sale.vn) following ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc);
-- mvd 6,7->5; 7->8; 7->9; 7->10; 

-- COVAR_SAMP() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(COVAR_SAMP(floor(cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.vn desc range between unbounded preceding and current row ),
win2 as (order by cf_olap_windowerr_sale.pn desc);
-- mvd 7->6; 9->8; 

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
-- mvd 3->5; 3->6; 3->7; 3->8; 3->9; 11,2,3,12,13->10; 

-- COVAR_SAMP() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(COVAR_SAMP(floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.vn desc rows between unbounded preceding and floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.prc) preceding );
-- mvd 3,4,5->2; 

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
-- mvd 4,1->3; 4,1->5; 2,4,7,1->6; 2,4,7,1->8; 1->9; 

-- COVAR_SAMP() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(COVAR_SAMP(floor(cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(RANK() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.vn asc rows between current row and floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.pn) following ),
win2 as (order by cf_olap_windowerr_sale.cn asc);
-- mvd 3,4->2; 3->5; 

-- COVAR_SAMP() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(COVAR_SAMP(floor(cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(RANK() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.cn desc range between unbounded preceding and floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.pn) preceding ),
win2 as (order by cf_olap_windowerr_sale.cn asc);
-- mvd 1,2,6->5; 1->7; 

-- COVAR_SAMP() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(COVAR_SAMP(floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.cn asc range between unbounded preceding and floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.prc) following );
-- mvd 3,4,1->2; 

-- COVAR_SAMP() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(COVAR_SAMP(floor(cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.pn asc range between floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.pn) preceding and current row );
-- mvd 3,4,5,6->2; 

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
-- mvd 4,1->3; 6,4->5; 4,8,1->7; 4,8,1->9; 4,1->10; 2->11; 

-- COVAR_SAMP() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(COVAR_SAMP(floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.pn/(cf_olap_windowerr_sale.prc+1))) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.vn asc range between floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.prc) preceding and unbounded following );
-- mvd 6,7,8->5; 

-- COVAR_SAMP() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(COVAR_SAMP(floor(cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.pn desc range between current row and floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.qty) following );
-- mvd 4,5->3; 

-- COVAR_SAMP() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(COVAR_SAMP(floor(cf_olap_windowerr_sale.qty*cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.vn asc range between floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.qty) following and 0 following );
-- mvd 2,3,1->5; 

-- COVAR_SAMP() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(COVAR_SAMP(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.prc)) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.vn)) OVER(partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc rows between unbounded preceding and floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.qty) preceding ),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc rows between unbounded preceding and floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.qty) preceding ),
win2 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.vn desc);
-- mvd 1,3,4->2; 6,7->5; 6,7->8; 1,3,4->9; 1,3,4->10; 

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
-- mvd 4,5->3; 1,7,4,8->6; 1,7,4,8->9; 4,5->10; 5->11; 1,7,4,8->12; 

-- COVAR_SAMP() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(COVAR_SAMP(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.pn asc rows between floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.pn) preceding and 2 preceding );
-- mvd 5,1,6,7,2->4; 

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
-- mvd 3,5,6,1->4; 3,5,6,1->7; 3,6,5->8; 3,5,6,1->9; 2->10; 

-- REGR_AVGX() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_AVGX(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.qty*cf_olap_windowerr_sale.vn)) OVER(order by cf_olap_windowerr_sale.vn desc),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(order by cf_olap_windowerr_sale.vn desc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.vn asc range between 3 following and 4 following ),
win2 as (order by cf_olap_windowerr_sale.vn desc);
-- mvd 1->5; 1->6; 1->7; 1->8; 

-- REGR_AVGX() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_AVGX(floor(cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.vn desc rows between unbounded preceding and floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.vn) preceding );
-- mvd 3,6,1->5; 

-- REGR_AVGX() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_AVGX(floor(cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.cn) as int),NULL) OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.pn asc rows between unbounded preceding and unbounded following ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc);
-- mvd 4,5->3; 2->6; 

-- REGR_AVGX() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_AVGX(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc,cf_olap_windowerr_sale.vn desc rows between floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.cn) preceding and current row );
-- mvd 6,1->5; 

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
-- mvd 1,4->3; 1,6,7->5; 4->8; 1,6,4->9; 

-- REGR_AVGX() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(REGR_AVGX(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.pn*cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.qty)) OVER(partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.vn desc range between unbounded preceding and current row ),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.cn)) OVER(partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.vn desc range between unbounded preceding and current row ),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.vn desc range between unbounded preceding and current row );
-- mvd 1,2,4,5,6->3; 1,2,4,5,6->7; 1,2,4,5,6->8; 

-- REGR_AVGX() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(REGR_AVGX(floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc)) OVER(partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn desc range between unbounded preceding and 3 following ),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn desc range between unbounded preceding and 3 following ),
win2 as (order by cf_olap_windowerr_sale.cn asc);
-- mvd 3,4,5,6->2; 3,4,5,6->7; 3->8; 

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
-- mvd 3,6,1,7->5; 6,3->8; 6,3->9; 7->10; 6,3->11; 7->12; 

-- REGR_AVGX() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_AVGX(floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.prc) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.qty,
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.vn asc range between floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.pn) following and unbounded following ),
win2 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);
-- mvd 5,1,3,6->4; 1,5,8->7; 1,5,8->9; 

-- REGR_AVGX() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_AVGX(floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.prc*cf_olap_windowerr_sale.cn)) OVER(partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.pn desc rows between unbounded preceding and 1 preceding ),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.qty) as int),NULL) OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.pn desc rows between unbounded preceding and 1 preceding ),
win2 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.pn asc),
win3 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc);
-- mvd 5,2,6->4; 5,2,6->7; 9,5,6->8; 5,9,2,6->10; 

-- REGR_AVGX() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_AVGX(floor(cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.pn) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(partition by cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(partition by cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc rows between floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.vn) preceding and floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.prc) preceding ),
win2 as (partition by cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);
-- mvd 6,1,7,8->5; 1,7->9; 1,7->10; 1,7->11; 1,7->12; 

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
-- mvd 3,4->2; 3,4->5; 3,7->6; 3,7->8; 3,7,10->9; 

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
-- mvd 6,1,3,7,8,9->5; 6,3,8,7,9->10; 6,3,8,7,9->11; 8->12; 8->13; 9->14; 

-- REGR_AVGY() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_AVGY(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn desc range unbounded preceding );
-- mvd 3->2; 

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
-- mvd 2->6; 4->7; 4->8; 2,4->9; 2,4->10; 

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
-- mvd 6->5; 1->7; 9,6->8; 1->10; 9,12,1,6->11; 

-- REGR_AVGY() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_AVGY(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.vn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.vn asc range between unbounded preceding and floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.prc) preceding ),
win2 as (partition by cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc);
-- mvd 4->3; 6,1->5; 

-- REGR_AVGY() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_AVGY(floor(cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.pn asc range between unbounded preceding and floor(cf_olap_windowerr_sale.qty) following );
-- mvd 3->5; 

-- REGR_AVGY() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_AVGY(floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn asc range between unbounded preceding and floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.pn) following ),
win2 as (order by cf_olap_windowerr_sale.pn desc);
-- mvd 2->7; 2->8; 1->9; 

-- REGR_AVGY() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(REGR_AVGY(floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn asc range between floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.vn) following and unbounded following );
-- mvd 4->3; 

-- REGR_AVGY() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_AVGY(floor(cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.qty) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.vn)) OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc rows between unbounded preceding and floor(cf_olap_windowerr_sale.prc) following ),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc rows between unbounded preceding and floor(cf_olap_windowerr_sale.prc) following ),
win2 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),
win3 as (partition by cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.pn asc);
-- mvd 3->2; 5,6,7,3->4; 3->8; 7,3->9; 

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
-- mvd 5,1->4; 7,8,1->6; 5,2,8,1->9; 5,1->10; 5,1->11; 

-- REGR_AVGY() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_AVGY(floor(cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.prc) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.qty*cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc) as int),NULL) OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc,cf_olap_windowerr_sale.cn asc rows between floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.pn) preceding and current row ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc);
-- mvd 2,1->4; 1->5; 2,1->6; 8->7; 

-- REGR_AVGY() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_AVGY(floor(cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc rows between 4 preceding and floor(cf_olap_windowerr_sale.pn) following );
-- mvd 8->7; 

-- REGR_AVGY() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_AVGY(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.prc) as int),NULL) OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc rows between floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.vn) preceding and unbounded following ),
win2 as (partition by cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc);
-- mvd 2->5; 2,3->6; 

-- REGR_AVGY() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_AVGY(floor(cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.cn desc range between floor(cf_olap_windowerr_sale.pn) preceding and floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.vn) preceding );
-- mvd 5->4; 

-- REGR_AVGY() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(REGR_AVGY(floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.cn asc range between current row and unbounded following );
-- mvd 7,8,9->6; 

-- REGR_AVGY() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_AVGY(floor(cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.vn desc range between floor(cf_olap_windowerr_sale.vn) following and floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc) following );
-- mvd 8,1,9->7; 

-- REGR_AVGY() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_AVGY(floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.prc)) OVER(partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.vn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.cn asc range between 4 following and unbounded following ),
win2 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.vn asc);
-- mvd 3,4,1,5->2; 3,4,1,5->6; 8,3,1->7; 8,3,1->9; 3,4,1,5->10; 

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
-- mvd 5,1->4; 5,1->6; 8,1,9->7; 11,1->10; 8,2,9->12; 5,1->13; 

-- REGR_AVGY() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_AVGY(floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.cn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,
TO_CHAR(COALESCE(ROW_NUMBER() OVER(partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.qty) as int),NULL) OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.cn desc rows between unbounded preceding and floor(cf_olap_windowerr_sale.pn) preceding ),
win2 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);
-- mvd 4,5,2->3; 7,5,4,1->6; 7,5,4,1->8; 4->9; 

-- REGR_AVGY() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_AVGY(floor(cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc,cf_olap_windowerr_sale.cn desc rows between unbounded preceding and unbounded following );
-- mvd 3,1,4->2; 

-- REGR_AVGY() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_AVGY(floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.prc*cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc rows between floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.qty) preceding and unbounded following ),
win2 as (order by cf_olap_windowerr_sale.pn desc),
win3 as (partition by cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn desc);
-- mvd 3,2,7,8->6; 3,2,7,8->9; 8->10; 2,8->11; 

-- REGR_AVGY() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(REGR_AVGY(floor(cf_olap_windowerr_sale.pn*cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc rows between current row and floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.cn) following );
-- mvd 1,3,2->4; 

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
-- mvd 5,6,1->4; 1->7; 1->8; 10,6,1,2->9; 1->11; 

-- REGR_COUNT() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_COUNT(floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.vn asc range floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.cn) preceding );
-- mvd 2->7; 

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
-- mvd 1->7; 2->8; 2->9; 2->10; 1->11; 1->12; 

-- REGR_COUNT() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_COUNT(floor(cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn asc range between unbounded preceding and current row );
-- mvd 2->4; 

-- REGR_COUNT() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_COUNT(floor(cf_olap_windowerr_sale.qty*cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.prc*cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn asc range between unbounded preceding and floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.vn) following );
-- mvd 5->4; 

-- REGR_COUNT() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(REGR_COUNT(floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.vn) as int),NULL) OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn asc range between 1 following and 3 following ),
win2 as (order by cf_olap_windowerr_sale.vn desc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc);
-- mvd 1->3; 5->4; 5->6; 

-- REGR_COUNT() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(REGR_COUNT(floor(cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn desc range between floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.cn) following and unbounded following );
-- mvd 3->2; 

-- REGR_COUNT() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_COUNT(floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.prc*cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc)) OVER(order by cf_olap_windowerr_sale.vn desc range between floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.qty) following and unbounded following ),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.vn desc range between floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.qty) following and unbounded following ),
win2 as (order by cf_olap_windowerr_sale.cn desc);
-- mvd 2->7; 2->8; 1->9; 2->10; 

-- REGR_COUNT() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_COUNT(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc rows between 5 preceding and current row );
-- mvd 2->3; 

-- REGR_COUNT() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_COUNT(floor(cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc,cf_olap_windowerr_sale.pn desc,cf_olap_windowerr_sale.cn desc rows between floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.prc) preceding and unbounded following );
-- mvd 1,5->6; 

-- REGR_COUNT() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_COUNT(floor(cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc rows between current row and floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.pn) following );
-- mvd 4->5; 

-- REGR_COUNT() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_COUNT(floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.vn asc range between unbounded preceding and unbounded following );
-- mvd 5,1,6->4; 

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
-- mvd 5,6,1->4; 5,8,6->7; 1->9; 6->10; 6->11; 

-- REGR_COUNT() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_COUNT(floor(cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.vn asc range between 3 following and unbounded following );
-- mvd 4,5,6->3; 

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
-- mvd 5,1,6,7->4; 9,7->8; 1->10; 5,7->11; 5->12; 

-- REGR_COUNT() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(REGR_COUNT(floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc rows between unbounded preceding and floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc) following );
-- mvd 6,4,2->5; 

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
-- mvd 4,1,2->3; 6->5; 6->7; 6->8; 6->9; 6->10; 

-- REGR_COUNT() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_COUNT(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc rows between floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.cn) preceding and floor(cf_olap_windowerr_sale.vn) preceding );
-- mvd 5,3->7; 

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
-- mvd 3,4,5->2; 1->6; 3,1,8,4,5->7; 3,1,8,4,5->9; 1->10; 1->11; 

-- REGR_INTERCEPT() function with NULL OVER() clause --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_INTERCEPT(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as ();
-- mvd 6->6; 

-- REGR_INTERCEPT() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_INTERCEPT(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.pn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(RANK() OVER(partition by cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.prc)) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.vn)) OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.pn asc range unbounded preceding ),
win2 as (partition by cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);
-- mvd 2->3; 5,2->4; 5,2->6; 5,2->7; 5,2->8; 

-- REGR_INTERCEPT() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_INTERCEPT(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn asc range between current row and unbounded following );
-- mvd 3->2; 

-- REGR_INTERCEPT() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_INTERCEPT(floor(cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn desc range between floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.vn) following and floor(cf_olap_windowerr_sale.pn) following );
-- mvd 4->7; 

-- REGR_INTERCEPT() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_INTERCEPT(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc rows unbounded preceding );
-- mvd 1->5; 

-- REGR_INTERCEPT() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_INTERCEPT(floor(cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.prc) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(RANK() OVER(win4),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc rows floor(cf_olap_windowerr_sale.qty) preceding ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc),
win3 as (partition by cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.pn desc),
win4 as (partition by cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.vn desc);
-- mvd 1->5; 7->6; 7->8; 10,3->9; 1,7->11; 

-- REGR_INTERCEPT() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_INTERCEPT(floor(cf_olap_windowerr_sale.pn*cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.vn asc rows current row );
-- mvd 3->4; 

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
-- mvd 3->2; 5->4; 5->6; 5->7; 3->8; 3,10->9; 

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
-- mvd 5,6->4; 5,6->7; 5,6->8; 5->9; 5->10; 5,6->11; 

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
-- mvd 6->5; 8,9,6,1->7; 8,9,6,1->10; 6->11; 6->12; 

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
-- mvd 1->5; 1->6; 4->7; 4->8; 4->9; 

-- REGR_INTERCEPT() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_INTERCEPT(floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc rows between floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc) preceding and unbounded following ),
win2 as (partition by cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.pn desc);
-- mvd 3->2; 1->4; 

-- REGR_INTERCEPT() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_INTERCEPT(floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.cn) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc rows between floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.pn) following and floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.cn) following ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc);
-- mvd 3->2; 3->4; 3->5; 3->6; 3->7; 

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
-- mvd 3,2,5,6->4; 6->7; 3,2,5,6->8; 2->9; 3,2,5,6->10; 

-- REGR_INTERCEPT() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_INTERCEPT(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.qty)) OVER(partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.pn asc range between unbounded preceding and floor(cf_olap_windowerr_sale.vn) preceding ),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.vn)) OVER(partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.pn asc range between unbounded preceding and floor(cf_olap_windowerr_sale.vn) preceding ),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.pn*cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.pn) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(partition by cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.pn asc range between unbounded preceding and floor(cf_olap_windowerr_sale.vn) preceding ),
win2 as (partition by cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc);
-- mvd 4,3->6; 4,3->7; 4,3->8; 2,3->9; 2,3->10; 

-- REGR_INTERCEPT() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_INTERCEPT(floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.vn asc range between floor(cf_olap_windowerr_sale.cn) preceding and current row );
-- mvd 8,3->7; 

-- REGR_INTERCEPT() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_INTERCEPT(floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.vn desc range between current row and current row );
-- mvd 6,1,3,7->5; 

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
-- mvd 7,3,8,5->6; 8->9; 7,3,8,5->10; 8->11; 5->12; 8->13; 

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
-- mvd 4,2->3; 4,6,2->5; 4,2->7; 9,4,10->8; 9,4,10->11; 4->12; 

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
-- mvd 4,3,1->7; 1->8; 4,3,1->9; 11->10; 11,4,1->12; 11->13; 

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
-- mvd 5,2,3->4; 7,1->6; 3->8; 7,1->9; 5,2,3->10; 5,2,3->11; 

-- REGR_INTERCEPT() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_INTERCEPT(floor(cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.prc*cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.vn)) OVER(partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc rows between floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.vn) preceding and current row ),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.qty)) OVER(partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc rows between floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.vn) preceding and current row ),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc rows between floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.vn) preceding and current row ),
win2 as (order by cf_olap_windowerr_sale.pn asc);
-- mvd 5,1,2,3,6,7->4; 5,1,2,3,6,7->8; 5,1,2,3,6,7->9; 7->10; 

-- REGR_INTERCEPT() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_INTERCEPT(floor(cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc,cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.cn asc rows between floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.qty) following and floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.cn) following );
-- mvd 4,5->3; 

-- REGR_R2() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_R2(floor(cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn asc range floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.qty) preceding );
-- mvd 4->3; 

-- REGR_R2() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(REGR_R2(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.qty) as int),NULL) OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn desc range current row ),
win2 as (order by cf_olap_windowerr_sale.cn desc),
win3 as (partition by cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);
-- mvd 2->6; 2->7; 2->8; 2,1->9; 

-- REGR_R2() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_R2(floor(cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.vn asc range between floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.qty) preceding and 2 preceding );
-- mvd 5->4; 

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
-- mvd 5->4; 7->6; 5,9,3->8; 11,1->10; 7->12; 

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
-- mvd 3->2; 5,3,6->4; 8->7; 8->9; 5,3,6->10; 3->11; 

-- REGR_R2() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_R2(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc rows between unbounded preceding and floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.vn) preceding );
-- mvd 5->4; 

-- REGR_R2() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_R2(floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.qty*cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.qty)) OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.pn desc rows between floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.prc) preceding and current row ),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.prc)) OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.pn desc rows between floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.prc) preceding and current row ),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.pn) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc) as int),NULL) OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.pn desc rows between floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.prc) preceding and current row ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc);
-- mvd 3,4->2; 3,4->5; 3,4->6; 1->7; 3->8; 

-- REGR_R2() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_R2(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.pn desc,cf_olap_windowerr_sale.vn asc rows between floor(cf_olap_windowerr_sale.pn) preceding and unbounded following );
-- mvd 3,4->2; 

-- REGR_R2() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_R2(floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.cn)) OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.cn desc rows between 2 preceding and unbounded following ),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.cn desc rows between 2 preceding and unbounded following ),
win2 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.pn desc);
-- mvd 3,1->2; 3,5,6,1->4; 3,1->7; 

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
-- mvd 4,1,5,6->3; 4,1,5,6->7; 4,1,5,6->8; 4,5,1->9; 5,6,11->10; 

-- REGR_R2() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(REGR_R2(floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.cn asc range between unbounded preceding and unbounded following );
-- mvd 5,2,6->4; 

-- REGR_R2() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(REGR_R2(floor(cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.prc) as int),NULL) OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.cn desc range between floor(cf_olap_windowerr_sale.pn) preceding and floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.prc) preceding ),
win2 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc);
-- mvd 3,2,1,5->4; 2,5->6; 

-- REGR_R2() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(REGR_R2(floor(cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.qty*cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.vn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.pn asc range between current row and floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc) following ),
win2 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc);
-- mvd 5,2->6; 8,9,2->7; 

-- REGR_R2() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(REGR_R2(floor(cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.prc) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn asc range between floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.qty) following and unbounded following ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc);
-- mvd 1,7->6; 1,7->8; 7->9; 7->10; 

-- REGR_R2() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_R2(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.pn*cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.vn asc rows floor(cf_olap_windowerr_sale.vn) preceding );
-- mvd 3,1,4->2; 

-- REGR_R2() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_R2(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.cn)) OVER(partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.cn asc rows current row ),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.qty) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.cn asc rows current row ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),
win3 as (order by cf_olap_windowerr_sale.pn asc);
-- mvd 1,4,6,3,2->5; 1,4,6,3,2->7; 4->8; 2->9; 

-- REGR_R2() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_R2(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.pn asc rows between unbounded preceding and floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.pn) preceding );
-- mvd 6,7,8->5; 

-- REGR_R2() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(REGR_R2(floor(cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.qty) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.qty*cf_olap_windowerr_sale.qty)) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.pn desc,cf_olap_windowerr_sale.cn desc rows between floor(cf_olap_windowerr_sale.prc) preceding and floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.vn) preceding ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc);
-- mvd 5,2,6,7->4; 6->8; 6->9; 6->10; 

-- REGR_R2() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_R2(floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc rows between 4 preceding and 4 following );
-- mvd 6,5->7; 6,5->8; 

-- REGR_R2() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_R2(floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.pn*cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.prc) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.prc) as int),NULL) OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.qty*cf_olap_windowerr_sale.vn)) OVER(partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc rows between 3 preceding and unbounded following ),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc rows between 3 preceding and unbounded following ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc),
win3 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc);
-- mvd 3,1,4->2; 3,1,4->5; 4->6; 8,9,4,10->7; 3,1,4->11; 3,1,4->12; 

-- REGR_SLOPE() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_SLOPE(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.pn) as int),NULL) OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(ROW_NUMBER() OVER(partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.pn desc range unbounded preceding ),
win2 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.vn asc),
win3 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc);
-- mvd 4->3; 6,1,7,4->5; 9,2,1,4->8; 9,2,1,4->10; 

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
-- mvd 3->5; 3,7->6; 3,7->8; 2,1->9; 3,7->10; 

-- REGR_SLOPE() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_SLOPE(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.qty)) OVER(partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.cn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.pn)) OVER(partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.cn asc),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn asc range between unbounded preceding and 2 following ),
win2 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.cn asc);
-- mvd 1->3; 1,5->4; 1,5->6; 1,5->7; 1,5->8; 

-- REGR_SLOPE() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_SLOPE(floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.qty)) OVER(order by cf_olap_windowerr_sale.pn asc range between floor(cf_olap_windowerr_sale.vn) following and floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.vn) following ),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.pn asc range between floor(cf_olap_windowerr_sale.vn) following and floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.vn) following );
-- mvd 1->3; 1->4; 

-- REGR_SLOPE() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_SLOPE(floor(cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.cn desc rows floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.pn) preceding );
-- mvd 1,4->3; 

-- REGR_SLOPE() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(REGR_SLOPE(floor(cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc rows current row );
-- mvd 1->6; 

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
-- mvd 5->4; 7,8,9,2,5->6; 7,8,9,2,5->10; 9,1,2,5->11; 9,1,2,5->12; 

-- REGR_SLOPE() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_SLOPE(floor(cf_olap_windowerr_sale.prc*cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(RANK() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.cn) as int),NULL) OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.dt
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.pn desc rows between 0 preceding and floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.prc) preceding ),
win2 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.cn desc),
win3 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc);
-- mvd 1,2,4->3; 6,1,2->5; 8,4->7; 

-- REGR_SLOPE() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_SLOPE(floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.prc)) OVER(partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.cn desc range current row ),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.cn desc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.cn desc range current row ),
win2 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.cn desc);
-- mvd 6,3,2,7,1->5; 3,9->8; 6,3,2,7,1->10; 3,9->11; 3,9->12; 

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
-- mvd 1,6,2->5; 1->7; 9->8; 9->10; 12,9,1,6->11; 

-- REGR_SLOPE() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_SLOPE(floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.vn desc range between current row and floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.pn) following );
-- mvd 2->3; 

-- REGR_SLOPE() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_SLOPE(floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.qty)) OVER(partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn desc range between current row and floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.prc) following ),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn desc range between current row and floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.prc) following ),
win2 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.vn desc);
-- mvd 5,6,7,1->4; 5,6,7,1->8; 6,7,1->9; 

-- REGR_SLOPE() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_SLOPE(floor(cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.qty) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.vn) as int),NULL) OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.cn desc range between current row and unbounded following ),
win2 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc);
-- mvd 4,5,6->3; 8,5,6,2->7; 6->9; 

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
-- mvd 1,4->5; 2,3,7,1,8->6; 3->9; 4->10; 3,8,4->11; 

-- REGR_SLOPE() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_SLOPE(floor(cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(DENSE_RANK() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.vn)) OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.cn desc rows unbounded preceding ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc);
-- mvd 1,4,5->3; 1->6; 1->7; 1->8; 1->9; 

-- REGR_SLOPE() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_SLOPE(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.cn) as int),cast (floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.cn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.prc)) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty*cf_olap_windowerr_sale.pn) as int),cast (floor(cf_olap_windowerr_sale.prc) as int),NULL) OVER(win3),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc rows between floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.cn) preceding and floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.prc) preceding ),
win2 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc);
-- mvd 2,1->3; 5,2,1->4; 5,2,1->6; 8->7; 

-- REGR_SLOPE() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_SLOPE(floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.cn desc rows between current row and floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.pn) following ),
win2 as (order by cf_olap_windowerr_sale.cn desc);
-- mvd 2,1,5,6,3->4; 2,1,5,6,3->7; 1->8; 

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
-- mvd 4,2,5->3; 7->6; 5->8; 5->9; 5->10; 

-- REGR_SXX() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_SXX(floor(cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn desc range floor(cf_olap_windowerr_sale.cn) preceding );
-- mvd 5->4; 

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
-- mvd 4->3; 4->5; 4->6; 4->7; 9->8; 2,4,1,9->10; 

-- REGR_SXX() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_SXX(floor(cf_olap_windowerr_sale.cn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc,cf_olap_windowerr_sale.cn asc rows 9 preceding );
-- mvd 1,3->2; 

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
-- mvd 4->6; 4->7; 4->8; 1->9; 4->10; 12,4,2->11; 

-- REGR_SXX() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_SXX(floor(cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.prc+cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.cn asc rows between floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.pn) preceding and current row );
-- mvd 4,5->3; 

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
-- mvd 2,1->3; 2,1->4; 6,2,7->5; 2,1->8; 7->9; 1,7->10; 

-- REGR_SXX() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_SXX(floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.pn asc rows between floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.prc) preceding and unbounded following );
-- mvd 1->2; 

-- REGR_SXX() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_SXX(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc) as int),NULL) OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(ROW_NUMBER() OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.vn)) OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.vn desc rows between floor(cf_olap_windowerr_sale.qty) preceding and unbounded following ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc);
-- mvd 3,4->2; 4->5; 4->6; 4->7; 

-- REGR_SXX() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(REGR_SXX(floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.qty) as int),cast (floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc,cf_olap_windowerr_sale.pn desc rows between current row and current row ),
win2 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc);
-- mvd 3->2; 1,5,6->4; 

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
-- mvd 2->4; 6,7,2->5; 6,7,2->8; 6,7,2->9; 6,7,2->10; 6,7,2->11; 

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
-- mvd 4,5->3; 5->6; 5->7; 5->8; 4->9; 11,5->10; 

-- REGR_SXX() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_SXX(floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.cn asc range floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.pn) preceding );
-- mvd 5->6; 

-- REGR_SXX() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(REGR_SXX(floor(cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn AND cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.cn asc range between unbounded preceding and floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.cn) preceding );
-- mvd 6,4->5; 

-- REGR_SXX() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(REGR_SXX(floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.cn asc range between floor(cf_olap_windowerr_sale.qty) preceding and floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc) preceding );
-- mvd 6->5; 

-- REGR_SXX() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_SXX(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(ROW_NUMBER() OVER(win2),0),'99999999.9999999'),
TO_CHAR(COALESCE(LAG(cast(floor(cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.prc) as int),NULL) OVER(win3),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.vn desc range between 4 preceding and current row ),
win2 as (partition by cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.vn asc),
win3 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc);
-- mvd 4,1,5->3; 5,7->6; 5,7->8; 1->9; 

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
-- mvd 5,1,2->4; 5,7,8->6; 5,7,8->9; 8->10; 5,7,8->11; 2->12; 

-- REGR_SXX() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(REGR_SXX(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc rows floor(cf_olap_windowerr_sale.cn) preceding );
-- mvd 4,5,6->3; 

-- REGR_SXX() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_SXX(floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.pn desc rows current row );
-- mvd 2,5,6->4; 2,5,6->7; 

-- REGR_SXX() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_SXX(floor(cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn desc rows between unbounded preceding and floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.cn) following );
-- mvd 4,5->3; 

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
-- mvd 4,5,2,1,6->3; 2,5,1,8,6->7; 4,5,2,1,6->9; 2,6->10; 4,2->11; 2,5,1,8,6->12; 

-- REGR_SXX() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_SXX(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc rows between current row and 0 following );
-- mvd 6,7->5; 

-- REGR_SXX() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_SXX(floor(cf_olap_windowerr_sale.qty*cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn desc rows between floor(cf_olap_windowerr_sale.prc) following and floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.pn) following );
-- mvd 3,6,2,4->5; 

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
-- mvd 1,4,5->3; 2,4,5->6; 8,2,4->7; 8,2,4->9; 

-- REGR_SXY() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_SXY(floor(cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.pn+cf_olap_windowerr_sale.vn)) OVER(order by cf_olap_windowerr_sale.cn asc range floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.pn) preceding ),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(PERCENT_RANK() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn asc range floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.pn) preceding ),
win2 as (order by cf_olap_windowerr_sale.pn desc);
-- mvd 5->4; 5->6; 5->7; 3->8; 

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
-- mvd 5->4; 5->6; 5->7; 5->8; 2->9; 5,2,11->10; 

-- REGR_SXY() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_SXY(floor(cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.vn desc range between floor(cf_olap_windowerr_sale.pn-cf_olap_windowerr_sale.qty) preceding and floor(cf_olap_windowerr_sale.prc) preceding );
-- mvd 5->6; 

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
-- mvd 6->5; 6,8,9->7; 6,8,9->10; 6->11; 6->12; 6->13; 

-- REGR_SXY() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_SXY(floor(cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.cn desc,cf_olap_windowerr_sale.cn desc rows unbounded preceding );
-- mvd 3->2; 

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
-- mvd 3,5->4; 3->6; 8->7; 3,5->9; 8->10; 3->11; 

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
-- mvd 4,1->5; 3->6; 4,1,3->7; 9,4,1,3->8; 4,3->10; 4,1->11; 

-- REGR_SXY() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_SXY(floor(cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.cn desc rows between unbounded preceding and current row );
-- mvd 3,1->4; 

-- REGR_SXY() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_SXY(floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.vn desc,cf_olap_windowerr_sale.pn desc rows between current row and floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.qty) following );
-- mvd 2,1,5->4; 

-- REGR_SXY() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_SXY(floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.cn asc range unbounded preceding );
-- mvd 7->6; 

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
-- mvd 1,5,6->4; 5,2->7; 5,2->8; 6->9; 6->10; 2->11; 

-- REGR_SXY() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_SXY(floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.qty) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(DENSE_RANK() OVER(win3),0),'99999999.9999999'),
TO_CHAR(COALESCE(RANK() OVER(partition by cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.pn asc),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.cn desc range between unbounded preceding and current row ),
win2 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc),
win3 as (partition by cf_olap_windowerr_sale.prc order by cf_olap_windowerr_sale.pn asc);
-- mvd 5,1->4; 1,7,2,8->6; 5,8->9; 5,8->10; 

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
-- mvd 3,1,5->4; 7,3,1->6; 2,3,1,9,5->8; 2,3,1,9,5->10; 5->11; 5->12; 

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
-- mvd 6,7->5; 6,7->8; 7->9; 7->10; 2->11; 

-- REGR_SXY() function with partition by and order by having range based framing clause --

SELECT cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_SXY(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.qty),floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.pn desc range between floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.qty) following and floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.prc) following );
-- mvd 2,1->5; 

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
-- mvd 6,7,8->5; 6,7,8->9; 8,1->10; 8,1->11; 7->12; 6,7,8->13; 

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
-- mvd 1,3,4->2; 6,4->5; 6,4->7; 6,9->8; 9->10; 6,9->11; 

-- REGR_SXY() function with partition by and order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_SXY(floor(cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.prc,
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.cn)) OVER(partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc rows between current row and current row ),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc rows between current row and current row );
-- mvd 4,1->3; 4,1->5; 4,1->6; 4,1->7; 

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
-- mvd 7->6; 7->8; 7->9; 7->10; 4->11; 

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
-- mvd 1->4; 6,1->5; 6,1->7; 6,1->8; 3->9; 

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
-- mvd 2->7; 2->8; 2,5,1->9; 2->10; 2,5,1->11; 

-- REGR_SYY() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(REGR_SYY(floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.cn desc range between current row and unbounded following );
-- mvd 4->3; 

-- REGR_SYY() function with ONLY order by having range based framing clause --

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_SYY(floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.vn asc range between floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.prc) following and floor(cf_olap_windowerr_sale.vn*cf_olap_windowerr_sale.pn) following );
-- mvd 3->5; 

-- REGR_SYY() function with ONLY order by having range based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_SYY(floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(MIN(floor(cf_olap_windowerr_sale.cn*cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.vn asc range between 1 following and floor(cf_olap_windowerr_sale.qty) following ),
win2 as (partition by cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn order by cf_olap_windowerr_sale.vn asc);
-- mvd 1->4; 1->5; 7,1,8->6; 

-- REGR_SYY() function with ONLY order by having rows based framing clause in combination with other functions --

SELECT cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty, TO_CHAR(COALESCE(REGR_SYY(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),
TO_CHAR(COALESCE(MAX(floor(cf_olap_windowerr_sale.cn)) OVER(order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.cn desc rows between unbounded preceding and floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.vn) preceding ),0),'99999999.9999999'),
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.prc/cf_olap_windowerr_sale.prc) as int),cast (floor(cf_olap_windowerr_sale.qty) as int),NULL) OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.vn asc,cf_olap_windowerr_sale.cn desc rows between unbounded preceding and floor(cf_olap_windowerr_sale.vn-cf_olap_windowerr_sale.vn) preceding ),
win2 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc);
-- mvd 3,1->6; 3,1->7; 1->8; 

-- REGR_SYY() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.cn, TO_CHAR(COALESCE(REGR_SYY(floor(cf_olap_windowerr_sale.cn),floor(cf_olap_windowerr_sale.cn-cf_olap_windowerr_sale.qty)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn ) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.cn asc,cf_olap_windowerr_sale.vn asc rows between floor(cf_olap_windowerr_sale.qty/cf_olap_windowerr_sale.prc) preceding and 8 following );
-- mvd 1,3->2; 

-- REGR_SYY() function with ONLY order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn, TO_CHAR(COALESCE(REGR_SYY(floor(cf_olap_windowerr_sale.qty*cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_customer,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.cn=cf_olap_windowerr_customer.cn AND cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc rows between floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.qty) preceding and unbounded following );
-- mvd 4->3; 

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
-- mvd 3->7; 3,2,9->8; 3,2,9->10; 12->11; 3->13; 12->14; 

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
-- mvd 2,6->5; 1->7; 1->8; 1,2->9; 1->10; 1,2->11; 

-- REGR_SYY() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(REGR_SYY(floor(cf_olap_windowerr_sale.vn+cf_olap_windowerr_sale.pn),floor(cf_olap_windowerr_sale.pn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,
TO_CHAR(COALESCE(COUNT(floor(cf_olap_windowerr_sale.pn)) OVER(partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn desc range between unbounded preceding and 3 following ),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_vendor WHERE cf_olap_windowerr_sale_ord.vn=cf_olap_windowerr_vendor.vn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.cn desc range between unbounded preceding and 3 following );
-- mvd 3,6,7->5; 3,6,7->8; 

-- REGR_SYY() function with partition by and order by having range based framing clause in combination with other functions--

SELECT cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_SYY(floor(cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.qty+cf_olap_windowerr_sale.vn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,
TO_CHAR(COALESCE(LEAD(cast(floor(cf_olap_windowerr_sale.vn) as int),cast (floor(cf_olap_windowerr_sale.vn) as int),NULL) OVER(win2),0),'99999999.9999999'),cf_olap_windowerr_sale.qty,
TO_CHAR(COALESCE(CUME_DIST() OVER(win2),0),'99999999.9999999')
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.vn asc range between floor(cf_olap_windowerr_sale.qty-cf_olap_windowerr_sale.vn) preceding and 1 following ),
win2 as (partition by cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc);
-- mvd 1,4,5,2->3; 4,1,5,7,2->6; 4,1,5,7,2->8; 

-- REGR_SYY() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.prc,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.prc, TO_CHAR(COALESCE(REGR_SYY(floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.pn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.pn asc rows between unbounded preceding and unbounded following );
-- mvd 7,8->6; 

-- REGR_SYY() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.dt, TO_CHAR(COALESCE(REGR_SYY(floor(cf_olap_windowerr_sale.vn),floor(cf_olap_windowerr_sale.vn/cf_olap_windowerr_sale.prc)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.dt,cf_olap_windowerr_sale.vn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.vn asc rows between 8 preceding and current row );
-- mvd 4,1,5,6->3; 

-- REGR_SYY() function with partition by and order by having rows based framing clause --

SELECT cf_olap_windowerr_sale.pn, TO_CHAR(COALESCE(REGR_SYY(floor(cf_olap_windowerr_sale.cn+cf_olap_windowerr_sale.prc),floor(cf_olap_windowerr_sale.cn)) OVER(win1),0),'99999999.9999999'),cf_olap_windowerr_sale.cn,cf_olap_windowerr_sale.qty
FROM (SELECT cf_olap_windowerr_sale_ord.* FROM cf_olap_windowerr_sale_ord,cf_olap_windowerr_product WHERE cf_olap_windowerr_sale_ord.pn=cf_olap_windowerr_product.pn) cf_olap_windowerr_sale
WINDOW win1 as (partition by cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn,cf_olap_windowerr_sale.qty,cf_olap_windowerr_sale.pn order by cf_olap_windowerr_sale.ord, cf_olap_windowerr_sale.cn asc rows between floor(cf_olap_windowerr_sale.prc-cf_olap_windowerr_sale.pn) preceding and unbounded following );
-- mvd 3,4,1->2; 


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

-- start_ignore
CREATE TABLE filter_test (i int, j int) DISTRIBUTED BY (i);
INSERT INTO filter_test VALUES (1, 1);
INSERT INTO filter_test VALUES (2, 1);
INSERT INTO filter_test VALUES (3, 1);
INSERT INTO filter_test VALUES (4, 2);
INSERT INTO filter_test VALUES (NULL, 2);
INSERT INTO filter_test VALUES (6, 2);
INSERT INTO filter_test VALUES (7, 3);
INSERT INTO filter_test VALUES (8, NULL);
INSERT INTO filter_test VALUES (9, 3);
INSERT INTO filter_test VALUES (10, NULL);
-- end_ignore

select j, i, ntile(j) over (partition by j order by i) FROM filter_test;

SELECT ntile(-1) over (order by i) FROM filter_test;

SELECT ntile(0) over (order by i) FROM filter_test;

SELECT ntile(0) over (order by i) FROM filter_test;


-- start_ignore
DROP TABLE filter_test;

--
-- STANDARD DATA FOR olap_* TESTS.
--
drop table cf_olap_windowerr_customer;
drop table cf_olap_windowerr_vendor;
drop table cf_olap_windowerr_product;
drop table cf_olap_windowerr_sale;
drop table cf_olap_windowerr_sale_ord;
drop table cf_olap_windowerr_util;
-- end_ignore
