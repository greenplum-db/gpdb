\c mpph_heap
drop table if exists olap_customer;
drop table if exists olap_product;
drop table if exists olap_vendor;
drop table if exists olap_util;
drop table if exists  olap_sale;
drop table if exists olap_sale_ord;


create table olap_customer 
(
	cn int not null,
	cname text not null,
	cloc text,
	
	primary key (cn)
	
) distributed by (cn);

create table olap_vendor 
(
	vn int not null,
	vname text not null,
	vloc text,
	
	primary key (vn)
	
) distributed by (vn);

create table olap_product 
(
	pn int not null,
	pname text not null,
	pcolor text,
	
	primary key (pn)
	
) distributed by (pn);

create table olap_sale
(
	cn int not null,
	vn int not null,
	pn int not null,
	dt date not null,
	qty int not null,
	prc float not null,
	
	primary key (cn, vn, pn)
	
) distributed by (cn,vn,pn);

create table olap_sale_ord
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

create table olap_util
(
	xn int not null,
	
	primary key (xn)
	
) distributed by (xn);

-- Customers
insert into olap_customer values 
  ( 1, 'Macbeth', 'Inverness'),
  ( 2, 'Duncan', 'Forres'),
  ( 3, 'Lady Macbeth', 'Inverness'),
  ( 4, 'Witches, Inc', 'Lonely Heath');

-- Vendors
insert into olap_vendor values 
  ( 10, 'Witches, Inc', 'Lonely Heath'),
  ( 20, 'Lady Macbeth', 'Inverness'),
  ( 30, 'Duncan', 'Forres'),
  ( 40, 'Macbeth', 'Inverness'),
  ( 50, 'Macduff', 'Fife');

-- Products
insert into olap_product values 
  ( 100, 'Sword', 'Black'),
  ( 200, 'Dream', 'Black'),
  ( 300, 'Castle', 'Grey'),
  ( 400, 'Justice', 'Clear'),
  ( 500, 'Donuts', 'Plain'),
  ( 600, 'Donuts', 'Chocolate'),
  ( 700, 'Hamburger', 'Grey'),
  ( 800, 'Fries', 'Grey');


-- Sales (transactions)
insert into olap_sale values 
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
insert into olap_sale_ord values 
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

-- Util

insert into olap_util values 
  (1),
  (20),
  (300);

set enable_hashagg = off;
set enable_hashjoin = off;
set gp_enable_mk_sort = on;

select
	l_returnflag,
	l_linestatus,
	sum(l_quantity) as sum_qty,
	sum(l_extendedprice) as sum_base_price,
	sum(l_extendedprice * (1 - l_discount)) as sum_disc_price,
	sum(l_extendedprice * (1 - l_discount) * (1 + l_tax)) as sum_charge,
	avg(l_quantity) as avg_qty,
	avg(l_extendedprice) as avg_price,
	avg(l_discount) as avg_disc,
	count(*) as count_order
from
	lineitem
where
	l_shipdate <= date '1998-12-01' - interval '108 day'
group by
	l_returnflag,
	l_linestatus
order by
	l_returnflag,
	l_linestatus;

select
	s_acctbal,
	s_name,
	n_name,
	p_partkey,
	p_mfgr,
	s_address,
	s_phone,
	s_comment
from
	part,
	supplier,
	partsupp,
	nation,
	region
where
	p_partkey = ps_partkey
	and s_suppkey = ps_suppkey
	and p_size = 45
	and p_type like '%NICKEL'
	and s_nationkey = n_nationkey
	and n_regionkey = r_regionkey
	and r_name = 'EUROPE'
	and ps_supplycost = (
		select
			min(ps_supplycost)
		from
			partsupp,
			supplier,
			nation,
			region
		where
			p_partkey = ps_partkey
			and s_suppkey = ps_suppkey
			and s_nationkey = n_nationkey
			and n_regionkey = r_regionkey
			and r_name = 'EUROPE'
	)
order by
	s_acctbal desc,
	n_name,
	s_name,
	p_partkey;

select 
	s.s_acctbal,
	s.s_name,
	n.n_name,
	p.p_partkey,
	p.p_mfgr,
	s.s_address,
	s.s_phone,
	s.s_comment 
from 
	supplier s, 
	partsupp ps, 
	nation n, 
	region r, 
	part p, 
	(select p_partkey, min(ps_supplycost) as min_ps_cost 
		from 
			part, 
			partsupp , 	
			supplier,
			nation, 
			region 
		where 
			p_partkey=ps_partkey 
			and s_suppkey = ps_suppkey 
			and s_nationkey = n_nationkey 
			and n_regionkey = r_regionkey 
			and r_name = 'EUROPE' 
		group by p_partkey ) g 
where 
	p.p_partkey = ps.ps_partkey 
	and g.p_partkey = p.p_partkey 
	and g. min_ps_cost = ps.ps_supplycost 
	and s.s_suppkey = ps.ps_suppkey 
	and p.p_size = 45 
	and p.p_type like '%NICKEL' 
	and s.s_nationkey = n.n_nationkey 
	and n.n_regionkey = r.r_regionkey 
	and r.r_name = 'EUROPE' 
order by 
	s.s_acctbal desc,
	n.n_name,
	s.s_name,
	p.p_partkey
LIMIT 100;


select
	l_orderkey,
	sum(l_extendedprice * (1 - l_discount)) as revenue,
	o_orderdate,
	o_shippriority
from
	customer,
	orders,
	lineitem
where
	c_mktsegment = 'MACHINERY'
	and c_custkey = o_custkey
	and l_orderkey = o_orderkey
	and o_orderdate < date '1995-03-15'
	and l_shipdate > date '1995-03-15'
group by
	l_orderkey,
	o_orderdate,
	o_shippriority
order by
	revenue desc,
	o_orderdate
LIMIT 10;



select
        o_orderpriority,
        count(*) as order_count
from
        orders
where
        o_orderdate >= date '1994-05-01'
        and o_orderdate < date '1994-05-01' + interval '3 month'
        and exists (
                select
                        *
                from
                        lineitem
                where
                        l_orderkey = o_orderkey
                        and l_commitdate < l_receiptdate
        )
group by
        o_orderpriority
order by
        o_orderpriority;

select
	sum(l_extendedprice * l_discount) as revenue
from
	lineitem
where
	l_shipdate >= date '1996-01-01'
	and l_shipdate < date '1996-01-01' + interval '1 year'
	and l_discount between 0.04 - 0.01 and 0.04 + 0.01
	and l_quantity < 24;

select
	supp_nation,
	cust_nation,
	l_year,
	sum(volume) as revenue
from
	(
		select
			n1.n_name as supp_nation,
			n2.n_name as cust_nation,
			extract(year from l_shipdate) as l_year,
			l_extendedprice * (1 - l_discount) as volume
		from
			supplier,
			lineitem,
			orders,
			customer,
			nation n1,
			nation n2
		where
			s_suppkey = l_suppkey
			and o_orderkey = l_orderkey
			and c_custkey = o_custkey
			and s_nationkey = n1.n_nationkey
			and c_nationkey = n2.n_nationkey
			and (
				(n1.n_name = 'MOZAMBIQUE' and n2.n_name = 'GERMANY')
				or (n1.n_name = 'GERMANY' and n2.n_name = 'MOZAMBIQUE')
			)
			and l_shipdate between date '1995-01-01' and date '1996-12-31'
	) as shipping
group by
	cust_nation,
	l_year
order by
	supp_nation,
	cust_nation,
	l_year;

select
	o_year,
	sum(case
		when nation = 'PERU' then volume
		else 0
	end) / sum(volume) as mkt_share
from
	(
		select
			extract(year from o_orderdate) as o_year,
			l_extendedprice * (1 - l_discount) as volume,
			n2.n_name as nation
		from
			part,
			supplier,
			lineitem,
			orders,
			customer,
			nation n1,
			nation n2,
			region
		where
			p_partkey = l_partkey
			and s_suppkey = l_suppkey
			and l_orderkey = o_orderkey
			and o_custkey = c_custkey
			and c_nationkey = n1.n_nationkey
			and n1.n_regionkey = r_regionkey
			and r_name = 'AMERICA'
			and s_nationkey = n2.n_nationkey
			and o_orderdate between date '1995-01-01' and date '1996-12-31'
			and p_type = 'MEDIUM POLISHED TIN'
	) as all_nations
group by
	o_year
order by
	o_year;

select
	nation,
	o_year,
	sum(amount) as sum_profit
from
	(
		select
			n_name as nation,
			extract(year from o_orderdate) as o_year,
			l_extendedprice * (1 - l_discount) - ps_supplycost * l_quantity as amount
		from
			part,
			supplier,
			lineitem,
			partsupp,
			orders,
			nation
		where
			s_suppkey = l_suppkey
			and ps_suppkey = l_suppkey
			and ps_partkey = l_partkey
			and p_partkey = l_partkey
			and o_orderkey = l_orderkey
			and s_nationkey = n_nationkey
			and p_name like '%tan%'
	) as profit
group by
	nation,
	o_year
order by
	nation,
	o_year desc;

select
	c_count,
	count(*) as custdist
from
	(
		select
			c_custkey,
			count(o_orderkey)
		from
			customer left outer join orders on
				c_custkey = o_custkey
				and o_comment not like '%express%deposits%'
		group by
			c_custkey
	) as c_orders (c_custkey, c_count)
group by
	c_count
order by
	custdist desc,
	c_count desc;


create view gpsort_revenue0 (supplier_no, total_revenue) as
	select
		l_suppkey,
		sum(l_extendedprice * (1 - l_discount))
	from
		lineitem
	where
		l_shipdate >= date '1993-01-01'
		and l_shipdate < date '1993-01-01' + interval '3 month'
	group by
		l_suppkey;


select
	s_suppkey,
	s_name,
	s_address,
	s_phone,
	total_revenue
from
	supplier,
	gpsort_revenue0
where
	s_suppkey = supplier_no
	and total_revenue = (
		select
			max(total_revenue)
		from
			gpsort_revenue0
	)
order by
	s_suppkey;


drop view gpsort_revenue0;


select
	sum(l_extendedprice) / 7.0 as avg_yearly
from
	lineitem,
	part
where
	p_partkey = l_partkey
	and p_brand = 'Brand#32'
	and p_container = 'JUMBO PKG'
	and l_quantity < (
		select
			0.2 * avg(l_quantity)
		from
			lineitem
		where
			l_partkey = p_partkey
	);

select
      sum(l_extendedprice) / 7.0 as avg_yearly
from
      lineitem,
      (
          select
              p_partkey as x_partkey,
              0.2 * avg(l_quantity) as x_avg_20
          from
              part,
              lineitem
          where
              p_partkey = l_partkey
              and p_brand = 'Brand#32'
              and p_container = 'JUMBO PKG'
          group by
              p_partkey
      ) x
where
      x_partkey = l_partkey
and l_quantity < x_avg_20;

select
	sum(l_extendedprice* (1 - l_discount)) as revenue
from
	lineitem,
	part
where
	(
		p_partkey = l_partkey
		and p_brand = 'Brand#23'
		and p_container in ('SM CASE', 'SM BOX', 'SM PACK', 'SM PKG')
		and l_quantity >= 3 and l_quantity <= 3 + 10
		and p_size between 1 and 5
		and l_shipmode in ('AIR', 'AIR REG')
		and l_shipinstruct = 'DELIVER IN PERSON'
	)
	or
	(
		p_partkey = l_partkey
		and p_brand = 'Brand#53'
		and p_container in ('MED BAG', 'MED BOX', 'MED PKG', 'MED PACK')
		and l_quantity >= 11 and l_quantity <= 11 + 10
		and p_size between 1 and 10
		and l_shipmode in ('AIR', 'AIR REG')
		and l_shipinstruct = 'DELIVER IN PERSON'
	)
	or
	(
		p_partkey = l_partkey
		and p_brand = 'Brand#21'
		and p_container in ('LG CASE', 'LG BOX', 'LG PACK', 'LG PKG')
		and l_quantity >= 29 and l_quantity <= 29 + 10
		and p_size between 1 and 15
		and l_shipmode in ('AIR', 'AIR REG')
		and l_shipinstruct = 'DELIVER IN PERSON'
	);


select
	cntrycode,
	count(*) as numcust,
	sum(c_acctbal) as totacctbal
from
	(
		select
			substring(c_phone from 1 for 2) as cntrycode,
			c_acctbal
		from
			customer
		where
			substring(c_phone from 1 for 2) in
				('11', '28', '21', '26', '19', '13', '22')
			and c_acctbal > (
				select
					avg(c_acctbal)
				from
					customer
				where
					c_acctbal > 0.00
					and substring(c_phone from 1 for 2) in
						('11', '28', '21', '26', '19', '13', '22')
			)
			and not exists (
				select
					*
				from
					orders
				where
					o_custkey = c_custkey
			)
	) as custsale
group by
	cntrycode
order by
	cntrycode
LIMIT -1;

select 
	cntrycode, 
	count(*) as numcust, 
	sum(c_acctbal) as totacctbal 
from 
	( 
		select 
			substring(c_phone from 1 for 2) as cntrycode, 
			c_acctbal 
		from 
			customer left join orders 
				on c_custkey = o_custkey 
		where 
			substring(c_phone from 1 for 2) in 
				('11', '28', '21', '26', '19', '13', '22') 
			and c_acctbal > ( 
				select 
					avg(c_acctbal) 
				from 
					customer 
				where 
					c_acctbal > 0.00 
					and substring(c_phone from 1 for 2) in 
						('11', '28', '21', '26', '19', '13', '22') 
			) 
			and o_custkey is null 
	) as custsale 
group by 
	cntrycode 
order by 
	cntrycode;


select count (*) from (select l_orderkey, l_partkey, l_comment,
count(l_tax) from lineitem group by 1, 2, 3) tmpt;

create table gpsort_alltypes(col1 bigint, col2 bigserial, col3 bit, col4 bit varying(10), col5 bool, col6 char, col7 varchar(10), col8 cidr, col9 circle, col10 date, col11 numeric(5,2), col12 float8, col13 inet, col14 int4, col15 interval, col16 lseg, col17 macaddr, col18 money, col19 path, col20 point, col21 polygon, col22 float4, col23 serial4, col24 smallint, col25 text, col26 time, col27 timetz, col28 timestamp, col29 timestamptz) with oids;


insert into gpsort_alltypes values(1234567891011,13942492347494,'1','0001','yes', 'a', 'abcdefgh', '192.168.100.1', circle '((0,0),1)', '2007-01-01', 123.45, 12323423424.324, inet '192.168.1.1', 123123, interval '24 hours',lseg '((0,0),(1,1))', macaddr '12:34:56:78:90:ab', '$1000.00',  path '((0,0),(1,1),(2,2))', point '(0,0)',polygon '((0,0),(1,1))',213234.23,1,7123,'abcdsafasfasfasfasfdasfasfdasf2asdfhsadfsfs','04:00:00','04:00:00 EST','2007-01-01 04:00:00','2007-01-01 04:00:00 EST');
insert into gpsort_alltypes values(10987654321,212394723492342,'0','0010','y', 'b', 'xyz', '192.168.100.2', circle '((0,0),2)', '2007-02-01', 23.45, 1232324.324, inet '192.168.1.2', 123124, interval '12 hours',lseg '((0,0),(1,2))', macaddr '12:34:56:78:90:00', '$5000.00',  path '((0,0),(4,4),(3,3))', point '(0,1)',polygon '((-1,-2),(1,1))',213.23234,2,2343,'2342abcddfasfasf234234234','04:30:00','04:30:00 EST','2007-02-01 04:30:00','2007-02-01 04:30:00 EST');
insert into gpsort_alltypes values(99999999999999999,312394234,'1','0000','false', 'c', 'cde', '192.168.100.3', circle '((1,1),2)', '2007-02-02', 34.45, 122324.324, inet '192.168.1.5', 13124, interval '30 mins',lseg '((0,0),(5,6))', macaddr '12:34:56:78:90:cd', '$4321.00',  path '((0,0),(4,4),(3,3))', point '(1,1)',polygon '((1,0),(2,3))',2133459.23123,3,1323,'234234abcddasdflasjflasfjalf','14:30:00','14:30:00 PST','2007-02-01 14:30:00','2007-02-01 14:30:00 PST');
insert into gpsort_alltypes values(122223333333366,423402340240234,'1','0100','f', 'd', '1xyz', '192.168.100.10', circle '((2,1),2)', '2001-03-02', 34.45, 312324.324, inet '192.168.2.5', 1324, interval '10 secs',lseg '((1,1),(6,6))', macaddr '12:34:56:78:89:cd', '$1.50',  path '((0,0),(4,4),(3,3),(5,5))', point '(2,1)',polygon '((2,0),(2,1))',21312121.23,4,123,'abcd23423afasflasfasf','16:30:00','16:30:00 PST','2006-02-01 16:30:00','2006-02-01 16:30:00 PST');
insert gpsort_alltypes values(4666,434234242,'1','1001','f', 'e', 'xyz', '127.0.0.1', circle '((2,1),2)', '2008-03-02', 34.45, 12324.324, inet '127.0.0.1', 1344, interval '40 secs',lseg '((1,1),(6,6))', macaddr '12:34:56:78:89:cd', '$1.00',  path '((0,0),(4,4),(3,3),(5,5))', point '(3,3)',polygon '((2,-1),(2,1))',213123.23,4,12334,'abcd','23:30:00','16:30:00 EST','2006-02-01 16:30:00','2006-02-01 16:30:00 PST');



select col1 from gpsort_alltypes order by col1 asc;
select col1 from gpsort_alltypes order by col1 desc;
select col2 from gpsort_alltypes order by col2 asc;
select col2 from gpsort_alltypes order by col2 desc;
select col3 from gpsort_alltypes order by col3 asc;
select col3 from gpsort_alltypes order by col3 desc;
select col4 from gpsort_alltypes order by col4 asc;
select col4 from gpsort_alltypes order by col4 desc;
select col5 from gpsort_alltypes order by col5 asc;
select col5 from gpsort_alltypes order by col5 desc;
select col6 from gpsort_alltypes order by col6 asc;
select col6 from gpsort_alltypes order by col6 desc;
select col7 from gpsort_alltypes order by col7 asc;
select col7 from gpsort_alltypes order by col7 desc;
select col8 from gpsort_alltypes order by col8 asc;
select col8 from gpsort_alltypes order by col8 desc;
select col9 from gpsort_alltypes order by col9 asc;
select col9 from gpsort_alltypes order by col9 desc;
select col10 from gpsort_alltypes order by col10 asc;
select col10 from gpsort_alltypes order by col10 desc;
select col11 from gpsort_alltypes order by col11 asc;
select col11 from gpsort_alltypes order by col11 desc;
select col12 from gpsort_alltypes order by col12 asc;
select col12 from gpsort_alltypes order by col12 desc;
select col13 from gpsort_alltypes order by col13 asc;
select col13 from gpsort_alltypes order by col13 desc;
select col14 from gpsort_alltypes order by col14 asc;
select col14 from gpsort_alltypes order by col14 desc;
select col15 from gpsort_alltypes order by col15 asc;
select col15 from gpsort_alltypes order by col15 desc;
select col16 from gpsort_alltypes order by col16 asc;
select col16 from gpsort_alltypes order by col16 desc;
select col17 from gpsort_alltypes order by col17 asc;
select col17 from gpsort_alltypes order by col17 desc;
select col18 from gpsort_alltypes order by col18 asc;
select col18 from gpsort_alltypes order by col18 desc;
select col19 from gpsort_alltypes order by col19 asc;
select col19 from gpsort_alltypes order by col19 desc;
select col20 from gpsort_alltypes order by col20 asc;
select col20 from gpsort_alltypes order by col20 desc;
select col21 from gpsort_alltypes order by col21 asc;
select col21 from gpsort_alltypes order by col21 desc;
select col22 from gpsort_alltypes order by col22 asc;
select col22 from gpsort_alltypes order by col22 desc;
select col23 from gpsort_alltypes order by col23 asc;
select col23 from gpsort_alltypes order by col23 desc;
select col24 from gpsort_alltypes order by col24 asc;
select col24 from gpsort_alltypes order by col24 desc;
select col25 from gpsort_alltypes order by col25 asc;
select col25 from gpsort_alltypes order by col25 desc;
select col26 from gpsort_alltypes order by col26 asc;
select col26 from gpsort_alltypes order by col26 desc;
select col27 from gpsort_alltypes order by col27 asc;
select col27 from gpsort_alltypes order by col27 desc;
select col28 from gpsort_alltypes order by col28 asc;
select col28 from gpsort_alltypes order by col28 desc;
select col29 from gpsort_alltypes order by col29 asc;
select col29 from gpsort_alltypes order by col29 desc;


drop table gpsort_alltypes;


SELECT CASE WHEN olap_sale.prc < 10 THEN 1 ELSE 2 END as newalias1,olap_sale.pn + olap_sale.prc as newalias2,olap_sale.qty as newalias3,GROUPING(olap_sale.qty,olap_sale.prc,olap_sale.prc),GROUP_ID(), TO_CHAR(COALESCE(AVG(DISTINCT floor(olap_sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(olap_sale.pn-olap_sale.cn)),0),'99999999.9999999') 
FROM olap_sale,olap_vendor
WHERE olap_sale.vn=olap_vendor.vn
GROUP BY GROUPING SETS(ROLLUP((olap_sale.vn),(newalias1,olap_sale.dt,olap_sale.qty),(olap_sale.qty,newalias2,newalias1,olap_sale.prc,olap_sale.pn),(olap_sale.dt),(newalias1,olap_sale.cn),(olap_sale.prc,olap_sale.pn,olap_sale.prc,olap_sale.prc),(olap_sale.vn,newalias3,olap_sale.pn,olap_sale.cn,newalias1),(olap_sale.cn,olap_sale.vn,newalias2),(olap_sale.pn,newalias2,newalias2))),CUBE((olap_sale.pn,olap_sale.cn,newalias2),(olap_sale.cn,newalias3,olap_sale.prc,olap_sale.cn),(olap_sale.vn,newalias1,newalias2),(newalias2,olap_sale.cn,newalias3,newalias3)),olap_sale.prc,olap_sale.pn,olap_sale.qty ORDER BY 1 asc,newalias2 asc,newalias2 desc,4,5; 


SELECT CASE WHEN olap_sale.vn < 10 THEN 1 ELSE 2 END as newalias1,olap_sale.dt as newalias2,CASE WHEN olap_sale.dt < 10 THEN 1 ELSE 2 END as newalias3,CASE WHEN olap_sale.qty < 10 THEN 1 ELSE 2 END as newalias4,CASE WHEN olap_sale.pn < 10 THEN 1 ELSE 2 END as newalias5,CASE WHEN olap_sale.cn < 10 THEN 1 ELSE 2 END as newalias6,GROUPING(olap_sale.dt,olap_sale.dt,olap_sale.dt,olap_sale.dt,olap_sale.qty,olap_sale.qty),GROUP_ID(), TO_CHAR(COALESCE(COUNT(DISTINCT floor(olap_sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(olap_sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(olap_sale.cn/olap_sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(olap_sale.qty-olap_sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(olap_sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(olap_sale.pn+olap_sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(olap_sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(olap_sale.pn+olap_sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(olap_sale.prc-olap_sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(olap_sale.vn)),0),'99999999.9999999') 
FROM olap_sale,olap_customer,olap_product
WHERE olap_sale.cn=olap_customer.cn AND olap_sale.pn=olap_product.pn
GROUP BY ROLLUP((olap_sale.qty),(olap_sale.pn,olap_sale.cn,newalias2,olap_sale.dt),(newalias3,olap_sale.vn,olap_sale.qty,newalias2),(olap_sale.prc,newalias1,olap_sale.prc,olap_sale.qty),(olap_sale.vn),(newalias1,olap_sale.pn,newalias3,newalias1,olap_sale.pn)),olap_sale.vn,olap_sale.dt,olap_sale.qty,olap_sale.pn,olap_sale.cn




SELECT CASE WHEN olap_sale.pn < 10 THEN 1 ELSE 2 END as newalias1,GROUPING(olap_sale.pn), TO_CHAR(COALESCE(STDDEV(DISTINCT floor(olap_sale.qty+olap_sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(olap_sale.qty-olap_sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(olap_sale.qty*olap_sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(olap_sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(olap_sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV(DISTINCT floor(olap_sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(SUM(DISTINCT floor(olap_sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(olap_sale.cn-olap_sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(olap_sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(olap_sale.cn)),0),'99999999.9999999') 
FROM olap_sale
GROUP BY (),olap_sale.pn ORDER BY newalias1 desc; -- order 1

SELECT olap_sale.qty*2 as newalias1,olap_sale.cn + olap_sale.cn as newalias2,olap_sale.cn + olap_sale.cn as newalias3, TO_CHAR(COALESCE(MAX(DISTINCT floor(olap_sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV(DISTINCT floor(olap_sale.pn-olap_sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(olap_sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(olap_sale.pn+olap_sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(olap_sale.prc/olap_sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(olap_sale.vn*olap_sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(olap_sale.pn-olap_sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(olap_sale.prc*olap_sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(olap_sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(COUNT(DISTINCT floor(olap_sale.pn-olap_sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(olap_sale.qty-olap_sale.qty)),0),'99999999.9999999') 
FROM olap_sale,olap_product
WHERE olap_sale.pn=olap_product.pn
GROUP BY (),GROUPING SETS(()),ROLLUP((olap_sale.prc,olap_sale.cn,olap_sale.prc),(olap_sale.cn),(newalias2),(olap_sale.pn),(olap_sale.cn,newalias1),(newalias1,newalias1,olap_sale.prc),(olap_sale.cn,olap_sale.dt,olap_sale.prc),(newalias2,newalias2)),olap_sale.qty,olap_sale.cn ORDER BY newalias3 desc,newalias2 desc,7 asc; -- order 3,2,7

SELECT olap_sale.qty*2 as newalias1,olap_sale.pn*2 as newalias2,olap_sale.cn as newalias3,olap_sale.cn*2 as newalias4,GROUP_ID(), TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(olap_sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(AVG(DISTINCT floor(olap_sale.qty+olap_sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(olap_sale.vn+olap_sale.vn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(olap_sale.cn)),0),'99999999.9999999'),TO_CHAR(COALESCE(STDDEV(DISTINCT floor(olap_sale.pn)),0),'99999999.9999999'),TO_CHAR(COALESCE(MIN(DISTINCT floor(olap_sale.qty)),0),'99999999.9999999'),TO_CHAR(COALESCE(MAX(DISTINCT floor(olap_sale.cn+olap_sale.prc)),0),'99999999.9999999'),TO_CHAR(COALESCE(VARIANCE(DISTINCT floor(olap_sale.pn)),0),'99999999.9999999') 
FROM olap_sale
GROUP BY GROUPING SETS(CUBE((olap_sale.dt,olap_sale.prc,newalias2,olap_sale.dt),(olap_sale.pn,newalias4,olap_sale.vn),(olap_sale.cn,olap_sale.cn,olap_sale.qty,olap_sale.cn,olap_sale.vn),(olap_sale.pn)),(olap_sale.cn),(olap_sale.cn),()),olap_sale.qty,olap_sale.pn,olap_sale.cn ORDER BY newalias1 desc,11 desc,newalias2 asc,newalias4 asc; -- order 1,11,2,4







