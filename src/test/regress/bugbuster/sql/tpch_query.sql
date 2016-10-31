\c tpch_heap
select 'tpch00-1',count(*), sum(n_nationkey), min(n_nationkey), max(n_nationkey) from nation;
select 'tpch00-2',count(*), sum(r_regionkey), min(r_regionkey), max(r_regionkey) from region;
select 'tpch00-3',count(*), sum(p_partkey), min(p_partkey), max(p_partkey) from part;
select 'tpch00-4',count(*), sum(s_suppkey), min(s_suppkey), max(s_suppkey) from supplier;
select 'tpch00-5',count(*), sum(ps_partkey + ps_suppkey), min(ps_partkey + ps_suppkey), max(ps_partkey + ps_suppkey) from partsupp;
select 'tpch00-6',count(*), sum(c_custkey), min(c_custkey), max(c_custkey) from customer;
select 'tpch00-7',count(*), sum(o_orderkey), min(o_orderkey), max(o_orderkey) from orders;
select 'tpch00-8',count(*), sum(l_linenumber), min(l_linenumber), max(l_linenumber) from lineitem;
select  'tpch1',
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
select  'tpch2',
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
	p_partkey
LIMIT 100;
select  'tpch3',
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
select  'tpch4',
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
select  'tpch5',
	n_name,
	sum(l_extendedprice * (1 - l_discount)) as revenue
from
	customer,
	orders,
	lineitem,
	supplier,
	nation,
	region
where
	c_custkey = o_custkey
	and l_orderkey = o_orderkey
	and l_suppkey = s_suppkey
	and c_nationkey = s_nationkey
	and s_nationkey = n_nationkey
	and n_regionkey = r_regionkey
	and r_name = 'AMERICA'
	and o_orderdate >= date '1997-01-01'
	and o_orderdate < date '1997-01-01' + interval '1 year'
group by
	n_name
order by
	revenue desc;
select  'tpch6',
	sum(l_extendedprice * l_discount) as revenue
from
	lineitem
where
	l_shipdate >= date '1996-01-01'
	and l_shipdate < date '1996-01-01' + interval '1 year'
	and l_discount between 0.04 - 0.01 and 0.04 + 0.01
	and l_quantity < 24;
select  'tpch7',
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
	supp_nation,
	cust_nation,
	l_year
order by
	supp_nation,
	cust_nation,
	l_year;
select  'tpch8',
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
select  'tpch9',
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
select  'tpch10',
	c_custkey,
	c_name,
	sum(l_extendedprice * (1 - l_discount)) as revenue,
	c_acctbal,
	n_name,
	c_address,
	c_phone,
	c_comment
from
	customer,
	orders,
	lineitem,
	nation
where
	c_custkey = o_custkey
	and l_orderkey = o_orderkey
	and o_orderdate >= date '1994-10-01'
	and o_orderdate < date '1994-10-01' + interval '3 month'
	and l_returnflag = 'R'
	and c_nationkey = n_nationkey
group by
	c_custkey,
	c_name,
	c_acctbal,
	c_phone,
	n_name,
	c_address,
	c_comment
order by
	revenue desc
LIMIT 20;
select  'tpch11',
	ps_partkey,
	sum(ps_supplycost * ps_availqty) as value
from
	partsupp,
	supplier,
	nation
where
	ps_suppkey = s_suppkey
	and s_nationkey = n_nationkey
	and n_name = 'ROMANIA'
group by
	ps_partkey having
		sum(ps_supplycost * ps_availqty) > (
			select
				sum(ps_supplycost * ps_availqty) * 0.0001000000
			from
				partsupp,
				supplier,
				nation
			where
				ps_suppkey = s_suppkey
				and s_nationkey = n_nationkey
				and n_name = 'ROMANIA'
		)
order by
	value desc;
select  'tpch12',
	l_shipmode,
	sum(case
		when o_orderpriority = '1-URGENT'
			or o_orderpriority = '2-HIGH'
			then 1
		else 0
	end) as high_line_count,
	sum(case
		when o_orderpriority <> '1-URGENT'
			and o_orderpriority <> '2-HIGH'
			then 1
		else 0
	end) as low_line_count
from
	orders,
	lineitem
where
	o_orderkey = l_orderkey
	and l_shipmode in ('RAIL', 'MAIL')
	and l_commitdate < l_receiptdate
	and l_shipdate < l_commitdate
	and l_receiptdate >= date '1993-01-01'
	and l_receiptdate < date '1993-01-01' + interval '1 year'
group by
	l_shipmode
order by
	l_shipmode;
select  'tpch13',
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
select  'tpch14',
	100.00 * sum(case
		when p_type like 'PROMO%'
			then l_extendedprice * (1 - l_discount)
		else 0
	end) / sum(l_extendedprice * (1 - l_discount)) as promo_revenue
from
	lineitem,
	part
where
	l_partkey = p_partkey
	and l_shipdate >= date '1993-01-01'
	and l_shipdate < date '1993-01-01' + interval '1 month';
create view revenue0 (supplier_no, total_revenue) as
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



select  'tpch15',
	s_suppkey,
	s_name,
	s_address,
	s_phone,
	total_revenue
from
	supplier,
	revenue0
where
	s_suppkey = supplier_no
	and total_revenue = (
		select
			max(total_revenue)
		from
			revenue0
	)
order by
	s_suppkey;

drop view revenue0;
select 'tpch16',
	p_brand, 
	p_type, 
	p_size, 
	count(distinct ps_suppkey) as supplier_cnt 
from 
	part, 
	partsupp left join supplier on (ps_suppkey=s_suppkey and s_comment like '%Customer%Complaints%' ) 
where 
	p_partkey = ps_partkey 
	and p_brand <> 'Brand#35' 
	and p_type not like 'MEDIUM ANODIZED%' 
	and p_size in (39, 31, 24, 22, 46, 20, 42, 15)
	and s_suppkey is null 
group by 
	p_brand, 
	p_type, 
	p_size 
order by 
	supplier_cnt desc, 
	p_brand, 
	p_type, 
	p_size;
select
	p_brand,
	p_type,
	p_size,
	count(distinct ps_suppkey) as supplier_cnt
from
	partsupp,
	part
where
	p_partkey = ps_partkey
	and p_brand <> 'Brand#35'
	and p_type not like 'MEDIUM ANODIZED%'
	and p_size in (39, 31, 24, 22, 46, 20, 42, 15)
	and ps_suppkey not in (
		select
			s_suppkey
		from
			supplier
		where
			s_comment like '%Customer%Complaints%'
	)
group by
	p_brand,
	p_type,
	p_size
order by
	supplier_cnt desc,
	p_brand,
	p_type,
	p_size;select 'tpch17',
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
              and p_container = 'JUMBO PACK'
          group by
              p_partkey
      ) x
where
      x_partkey = l_partkey
and l_quantity < x_avg_20;
select
	sum(l_extendedprice) / 7.0 as avg_yearly
from
	lineitem,
	part
where
	p_partkey = l_partkey
	and p_brand = 'Brand#32'
	and p_container = 'JUMBO PACK'
	and l_quantity < (
		select
			0.2 * avg(l_quantity)
		from
			lineitem
		where
			l_partkey = p_partkey
	);
select  'tpch18',
	c_name,
	c_custkey,
	o_orderkey,
	o_orderdate,
	o_totalprice,
	sum(l_quantity)
from
	customer,
	orders,
	lineitem
where
	o_orderkey in (
		select
			l_orderkey
		from
			lineitem
		group by
			l_orderkey having
				sum(l_quantity) > 300
	)
	and c_custkey = o_custkey
	and o_orderkey = l_orderkey
group by
	c_name,
	c_custkey,
	o_orderkey,
	o_orderdate,
	o_totalprice
order by
	o_totalprice desc,
	o_orderdate
LIMIT 100;
select  'tpch19',
	sum(l_extendedprice* (1 - l_discount)) as revenue
from
	lineitem,
	part
where
	(
		p_partkey = l_partkey
		and p_brand = 'Brand#23'
		and p_container in ('SM CASE', 'SM BOX', 'SM PACK', 'SM PKG')
		and l_quantity >= 1 and l_quantity <= 1 + 10
		and p_size between 1 and 15
		and l_shipmode in ('AIR', 'AIR REG')
		and l_shipinstruct = 'DELIVER IN PERSON'
	)
	or
	(
		p_partkey = l_partkey
		and p_brand = 'Brand#53'
		and p_container in ('MED BAG', 'MED BOX', 'MED PKG', 'MED PACK')
		and l_quantity >= 10 and l_quantity <= 10 + 10
		and p_size between 1 and 15
		and l_shipmode in ('AIR', 'AIR REG')
		and l_shipinstruct = 'DELIVER IN PERSON'
	)
	or
	(
		p_partkey = l_partkey
		and p_brand = 'Brand#21'
		and p_container in ('LG CASE', 'LG BOX', 'LG PACK', 'LG PKG')
		and l_quantity >= 20 and l_quantity <= 20 + 10
		and p_size between 1 and 15
		and l_shipmode in ('AIR', 'AIR REG')
		and l_shipinstruct = 'DELIVER IN PERSON'
	);select  'tpch20',
	s_name,
	s_address 
from 
	supplier, 
	nation 
where 
	s_suppkey in( 
		select 
			ps_suppkey 
		from 
			partsupp, 
			( 
				select 
					sum(l_quantity) as qty_sum, l_partkey, l_suppkey 
				from 
					lineitem 
				where 
					l_shipdate >= date '1996-01-01' 
					and l_shipdate < date '1996-01-01' + interval '1 year' 
				group by l_partkey, l_suppkey ) g 
		where 
			g.l_partkey = ps_partkey 
			and g.l_suppkey = ps_suppkey 
			and ps_availqty > 0.5 * g.qty_sum 
			and ps_partkey in ( select p_partkey from part where p_name like 'medium%' ) 
		) 
	and s_nationkey = n_nationkey 
	and n_name = 'UNITED STATES'
order by 
	s_name;

select
	s_name,
	s_address
from
	supplier,
	nation
where
	s_suppkey in (
		select
			ps_suppkey
		from
			partsupp
		where
			ps_partkey in (
				select
					p_partkey
				from
					part
				where
					p_name like 'medium%'
			)
			and ps_availqty > (
				select
					0.5 * sum(l_quantity)
				from
					lineitem
				where
					l_partkey = ps_partkey
					and l_suppkey = ps_suppkey
					and l_shipdate >= date '1996-01-01'
					and l_shipdate < date '1996-01-01' + interval '1 year'
			)
	)
	and s_nationkey = n_nationkey
	and n_name = 'UNITED STATES'
order by
	s_name;select  'tpch21',
	s_name, 
	count(distinct(l1.l_orderkey::text||l1.l_linenumber::text)) as numwait 
from 
	supplier, 
	orders, 
	nation, 
	lineitem l1 
		left join lineitem l2 
			on (l2.l_orderkey = l1.l_orderkey and l2.l_suppkey <> l1.l_suppkey) 
		left join (
			select 
				l3.l_orderkey,
				l3.l_suppkey 
			from 
				lineitem l3 
			where 
				l3.l_receiptdate > l3.l_commitdate) l4 
			on (l4.l_orderkey = l1.l_orderkey and l4.l_suppkey <> l1.l_suppkey) 
where 
	s_suppkey = l1.l_suppkey 
	and o_orderkey = l1.l_orderkey 
	and o_orderstatus = 'F' 
	and l1.l_receiptdate > l1.l_commitdate 
	and l2.l_orderkey is not null 
	and l4.l_orderkey is null 
	and s_nationkey = n_nationkey 
	and n_name = 'MOZAMBIQUE' 
group by 
	s_name 
order by 
	numwait desc, 
	s_name
LIMIT 100;
select  'tpch22',
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
	cntrycode;
select
	p_brand,
	p_type,
	p_size,
	count(distinct ps_suppkey) as supplier_cnt
from
	partsupp,
	part
where
	p_partkey = ps_partkey
	and p_brand <> 'Brand#35'
	and p_type not like 'MEDIUM ANODIZED%'
	and p_size in (39, 31, 24, 22, 46, 20, 42, 15)
	and ps_suppkey not in (
		select
			s_suppkey
		from
			supplier
		where
			s_comment like '%Customer%Complaints%'
	)
group by
	p_brand,
	p_type,
	p_size
order by

	supplier_cnt desc,
	p_brand,
	p_type,
	p_size;

select
	p_brand, 
	p_type, 
	p_size, 
	count(distinct ps_suppkey) as supplier_cnt 
from 
	part, 
	partsupp left join supplier on (ps_suppkey=s_suppkey and s_comment like '%Customer%Complaints%' ) 
where 
	p_partkey = ps_partkey 
	and p_brand <> 'Brand#35' 
	and p_type not like 'MEDIUM ANODIZED%' 
	and p_size in (39, 31, 24, 22, 46, 20, 42, 15)
	and s_suppkey is null 
group by 
	p_brand, 
	p_type, 
	p_size 
order by 
	supplier_cnt desc, 
	p_brand, 
	p_type, 
	p_size;

\c tpch_ao
select 'tpch00-1',count(*), sum(n_nationkey), min(n_nationkey), max(n_nationkey) from nation;
select 'tpch00-2',count(*), sum(r_regionkey), min(r_regionkey), max(r_regionkey) from region;
select 'tpch00-3',count(*), sum(p_partkey), min(p_partkey), max(p_partkey) from part;
select 'tpch00-4',count(*), sum(s_suppkey), min(s_suppkey), max(s_suppkey) from supplier;
select 'tpch00-5',count(*), sum(ps_partkey + ps_suppkey), min(ps_partkey + ps_suppkey), max(ps_partkey + ps_suppkey) from partsupp;
select 'tpch00-6',count(*), sum(c_custkey), min(c_custkey), max(c_custkey) from customer;
select 'tpch00-7',count(*), sum(o_orderkey), min(o_orderkey), max(o_orderkey) from orders;
select 'tpch00-8',count(*), sum(l_linenumber), min(l_linenumber), max(l_linenumber) from lineitem;
select  'tpch1',
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
select  'tpch2',
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
	p_partkey
LIMIT 100;
select  'tpch3',
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
select  'tpch4',
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
select  'tpch5',
	n_name,
	sum(l_extendedprice * (1 - l_discount)) as revenue
from
	customer,
	orders,
	lineitem,
	supplier,
	nation,
	region
where
	c_custkey = o_custkey
	and l_orderkey = o_orderkey
	and l_suppkey = s_suppkey
	and c_nationkey = s_nationkey
	and s_nationkey = n_nationkey
	and n_regionkey = r_regionkey
	and r_name = 'AMERICA'
	and o_orderdate >= date '1997-01-01'
	and o_orderdate < date '1997-01-01' + interval '1 year'
group by
	n_name
order by
	revenue desc;
select  'tpch6',
	sum(l_extendedprice * l_discount) as revenue
from
	lineitem
where
	l_shipdate >= date '1996-01-01'
	and l_shipdate < date '1996-01-01' + interval '1 year'
	and l_discount between 0.04 - 0.01 and 0.04 + 0.01
	and l_quantity < 24;
select  'tpch7',
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
	supp_nation,
	cust_nation,
	l_year
order by
	supp_nation,
	cust_nation,
	l_year;
select  'tpch8',
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
select  'tpch9',
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
select  'tpch10',
	c_custkey,
	c_name,
	sum(l_extendedprice * (1 - l_discount)) as revenue,
	c_acctbal,
	n_name,
	c_address,
	c_phone,
	c_comment
from
	customer,
	orders,
	lineitem,
	nation
where
	c_custkey = o_custkey
	and l_orderkey = o_orderkey
	and o_orderdate >= date '1994-10-01'
	and o_orderdate < date '1994-10-01' + interval '3 month'
	and l_returnflag = 'R'
	and c_nationkey = n_nationkey
group by
	c_custkey,
	c_name,
	c_acctbal,
	c_phone,
	n_name,
	c_address,
	c_comment
order by
	revenue desc
LIMIT 20;
select  'tpch11',
	ps_partkey,
	sum(ps_supplycost * ps_availqty) as value
from
	partsupp,
	supplier,
	nation
where
	ps_suppkey = s_suppkey
	and s_nationkey = n_nationkey
	and n_name = 'ROMANIA'
group by
	ps_partkey having
		sum(ps_supplycost * ps_availqty) > (
			select
				sum(ps_supplycost * ps_availqty) * 0.0001000000
			from
				partsupp,
				supplier,
				nation
			where
				ps_suppkey = s_suppkey
				and s_nationkey = n_nationkey
				and n_name = 'ROMANIA'
		)
order by
	value desc;
select  'tpch12',
	l_shipmode,
	sum(case
		when o_orderpriority = '1-URGENT'
			or o_orderpriority = '2-HIGH'
			then 1
		else 0
	end) as high_line_count,
	sum(case
		when o_orderpriority <> '1-URGENT'
			and o_orderpriority <> '2-HIGH'
			then 1
		else 0
	end) as low_line_count
from
	orders,
	lineitem
where
	o_orderkey = l_orderkey
	and l_shipmode in ('RAIL', 'MAIL')
	and l_commitdate < l_receiptdate
	and l_shipdate < l_commitdate
	and l_receiptdate >= date '1993-01-01'
	and l_receiptdate < date '1993-01-01' + interval '1 year'
group by
	l_shipmode
order by
	l_shipmode;
select  'tpch13',
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
select  'tpch14',
	100.00 * sum(case
		when p_type like 'PROMO%'
			then l_extendedprice * (1 - l_discount)
		else 0
	end) / sum(l_extendedprice * (1 - l_discount)) as promo_revenue
from
	lineitem,
	part
where
	l_partkey = p_partkey
	and l_shipdate >= date '1993-01-01'
	and l_shipdate < date '1993-01-01' + interval '1 month';
create view revenue0 (supplier_no, total_revenue) as
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



select  'tpch15',
	s_suppkey,
	s_name,
	s_address,
	s_phone,
	total_revenue
from
	supplier,
	revenue0
where
	s_suppkey = supplier_no
	and total_revenue = (
		select
			max(total_revenue)
		from
			revenue0
	)
order by
	s_suppkey;

drop view revenue0;
select 'tpch16',
	p_brand, 
	p_type, 
	p_size, 
	count(distinct ps_suppkey) as supplier_cnt 
from 
	part, 
	partsupp left join supplier on (ps_suppkey=s_suppkey and s_comment like '%Customer%Complaints%' ) 
where 
	p_partkey = ps_partkey 
	and p_brand <> 'Brand#35' 
	and p_type not like 'MEDIUM ANODIZED%' 
	and p_size in (39, 31, 24, 22, 46, 20, 42, 15)
	and s_suppkey is null 
group by 
	p_brand, 
	p_type, 
	p_size 
order by 
	supplier_cnt desc, 
	p_brand, 
	p_type, 
	p_size;
select
	p_brand,
	p_type,
	p_size,
	count(distinct ps_suppkey) as supplier_cnt
from
	partsupp,
	part
where
	p_partkey = ps_partkey
	and p_brand <> 'Brand#35'
	and p_type not like 'MEDIUM ANODIZED%'
	and p_size in (39, 31, 24, 22, 46, 20, 42, 15)
	and ps_suppkey not in (
		select
			s_suppkey
		from
			supplier
		where
			s_comment like '%Customer%Complaints%'
	)
group by
	p_brand,
	p_type,
	p_size
order by
	supplier_cnt desc,
	p_brand,
	p_type,
	p_size;select 'tpch17',
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
              and p_container = 'JUMBO PACK'
          group by
              p_partkey
      ) x
where
      x_partkey = l_partkey
and l_quantity < x_avg_20;
select
	sum(l_extendedprice) / 7.0 as avg_yearly
from
	lineitem,
	part
where
	p_partkey = l_partkey
	and p_brand = 'Brand#32'
	and p_container = 'JUMBO PACK'
	and l_quantity < (
		select
			0.2 * avg(l_quantity)
		from
			lineitem
		where
			l_partkey = p_partkey
	);
select  'tpch18',
	c_name,
	c_custkey,
	o_orderkey,
	o_orderdate,
	o_totalprice,
	sum(l_quantity)
from
	customer,
	orders,
	lineitem
where
	o_orderkey in (
		select
			l_orderkey
		from
			lineitem
		group by
			l_orderkey having
				sum(l_quantity) > 300
	)
	and c_custkey = o_custkey
	and o_orderkey = l_orderkey
group by
	c_name,
	c_custkey,
	o_orderkey,
	o_orderdate,
	o_totalprice
order by
	o_totalprice desc,
	o_orderdate
LIMIT 100;
select  'tpch19',
	sum(l_extendedprice* (1 - l_discount)) as revenue
from
	lineitem,
	part
where
	(
		p_partkey = l_partkey
		and p_brand = 'Brand#23'
		and p_container in ('SM CASE', 'SM BOX', 'SM PACK', 'SM PKG')
		and l_quantity >= 1 and l_quantity <= 1 + 10
		and p_size between 1 and 15
		and l_shipmode in ('AIR', 'AIR REG')
		and l_shipinstruct = 'DELIVER IN PERSON'
	)
	or
	(
		p_partkey = l_partkey
		and p_brand = 'Brand#53'
		and p_container in ('MED BAG', 'MED BOX', 'MED PKG', 'MED PACK')
		and l_quantity >= 10 and l_quantity <= 10 + 10
		and p_size between 1 and 15
		and l_shipmode in ('AIR', 'AIR REG')
		and l_shipinstruct = 'DELIVER IN PERSON'
	)
	or
	(
		p_partkey = l_partkey
		and p_brand = 'Brand#21'
		and p_container in ('LG CASE', 'LG BOX', 'LG PACK', 'LG PKG')
		and l_quantity >= 20 and l_quantity <= 20 + 10
		and p_size between 1 and 15
		and l_shipmode in ('AIR', 'AIR REG')
		and l_shipinstruct = 'DELIVER IN PERSON'
	);select  'tpch20',
	s_name,
	s_address 
from 
	supplier, 
	nation 
where 
	s_suppkey in( 
		select 
			ps_suppkey 
		from 
			partsupp, 
			( 
				select 
					sum(l_quantity) as qty_sum, l_partkey, l_suppkey 
				from 
					lineitem 
				where 
					l_shipdate >= date '1996-01-01' 
					and l_shipdate < date '1996-01-01' + interval '1 year' 
				group by l_partkey, l_suppkey ) g 
		where 
			g.l_partkey = ps_partkey 
			and g.l_suppkey = ps_suppkey 
			and ps_availqty > 0.5 * g.qty_sum 
			and ps_partkey in ( select p_partkey from part where p_name like 'medium%' ) 
		) 
	and s_nationkey = n_nationkey 
	and n_name = 'UNITED STATES'
order by 
	s_name;

select
	s_name,
	s_address
from
	supplier,
	nation
where
	s_suppkey in (
		select
			ps_suppkey
		from
			partsupp
		where
			ps_partkey in (
				select
					p_partkey
				from
					part
				where
					p_name like 'medium%'
			)
			and ps_availqty > (
				select
					0.5 * sum(l_quantity)
				from
					lineitem
				where
					l_partkey = ps_partkey
					and l_suppkey = ps_suppkey
					and l_shipdate >= date '1996-01-01'
					and l_shipdate < date '1996-01-01' + interval '1 year'
			)
	)
	and s_nationkey = n_nationkey
	and n_name = 'UNITED STATES'
order by
	s_name;select  'tpch21',
	s_name, 
	count(distinct(l1.l_orderkey::text||l1.l_linenumber::text)) as numwait 
from 
	supplier, 
	orders, 
	nation, 
	lineitem l1 
		left join lineitem l2 
			on (l2.l_orderkey = l1.l_orderkey and l2.l_suppkey <> l1.l_suppkey) 
		left join (
			select 
				l3.l_orderkey,
				l3.l_suppkey 
			from 
				lineitem l3 
			where 
				l3.l_receiptdate > l3.l_commitdate) l4 
			on (l4.l_orderkey = l1.l_orderkey and l4.l_suppkey <> l1.l_suppkey) 
where 
	s_suppkey = l1.l_suppkey 
	and o_orderkey = l1.l_orderkey 
	and o_orderstatus = 'F' 
	and l1.l_receiptdate > l1.l_commitdate 
	and l2.l_orderkey is not null 
	and l4.l_orderkey is null 
	and s_nationkey = n_nationkey 
	and n_name = 'MOZAMBIQUE' 
group by 
	s_name 
order by 
	numwait desc, 
	s_name
LIMIT 100;
select  'tpch22',
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
	cntrycode;
select
	p_brand,
	p_type,
	p_size,
	count(distinct ps_suppkey) as supplier_cnt
from
	partsupp,
	part
where
	p_partkey = ps_partkey
	and p_brand <> 'Brand#35'
	and p_type not like 'MEDIUM ANODIZED%'
	and p_size in (39, 31, 24, 22, 46, 20, 42, 15)
	and ps_suppkey not in (
		select
			s_suppkey
		from
			supplier
		where
			s_comment like '%Customer%Complaints%'
	)
group by
	p_brand,
	p_type,
	p_size
order by
	supplier_cnt desc,
	p_brand,
	p_type,
	p_size;

select
	p_brand, 
	p_type, 
	p_size, 
	count(distinct ps_suppkey) as supplier_cnt 
from 
	part, 
	partsupp left join supplier on (ps_suppkey=s_suppkey and s_comment like '%Customer%Complaints%' ) 
where 
	p_partkey = ps_partkey 
	and p_brand <> 'Brand#35' 
	and p_type not like 'MEDIUM ANODIZED%' 
	and p_size in (39, 31, 24, 22, 46, 20, 42, 15)
	and s_suppkey is null 
group by 
	p_brand, 
	p_type, 
	p_size 
order by 
	supplier_cnt desc, 
	p_brand, 
	p_type, 
	p_size;

\c tpch_co
select 'tpch00-1',count(*), sum(n_nationkey), min(n_nationkey), max(n_nationkey) from nation;
select 'tpch00-2',count(*), sum(r_regionkey), min(r_regionkey), max(r_regionkey) from region;
select 'tpch00-3',count(*), sum(p_partkey), min(p_partkey), max(p_partkey) from part;
select 'tpch00-4',count(*), sum(s_suppkey), min(s_suppkey), max(s_suppkey) from supplier;
select 'tpch00-5',count(*), sum(ps_partkey + ps_suppkey), min(ps_partkey + ps_suppkey), max(ps_partkey + ps_suppkey) from partsupp;
select 'tpch00-6',count(*), sum(c_custkey), min(c_custkey), max(c_custkey) from customer;
select 'tpch00-7',count(*), sum(o_orderkey), min(o_orderkey), max(o_orderkey) from orders;
select 'tpch00-8',count(*), sum(l_linenumber), min(l_linenumber), max(l_linenumber) from lineitem;
select  'tpch1',
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
select  'tpch2',
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
	p_partkey
LIMIT 100;
select  'tpch3',
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
select  'tpch4',
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
select  'tpch5',
	n_name,
	sum(l_extendedprice * (1 - l_discount)) as revenue
from
	customer,
	orders,
	lineitem,
	supplier,
	nation,
	region
where
	c_custkey = o_custkey
	and l_orderkey = o_orderkey
	and l_suppkey = s_suppkey
	and c_nationkey = s_nationkey
	and s_nationkey = n_nationkey
	and n_regionkey = r_regionkey
	and r_name = 'AMERICA'
	and o_orderdate >= date '1997-01-01'
	and o_orderdate < date '1997-01-01' + interval '1 year'
group by
	n_name
order by
	revenue desc;
select  'tpch6',
	sum(l_extendedprice * l_discount) as revenue
from
	lineitem
where
	l_shipdate >= date '1996-01-01'
	and l_shipdate < date '1996-01-01' + interval '1 year'
	and l_discount between 0.04 - 0.01 and 0.04 + 0.01
	and l_quantity < 24;
select  'tpch7',
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
	supp_nation,
	cust_nation,
	l_year
order by
	supp_nation,
	cust_nation,
	l_year;
select  'tpch8',
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
select  'tpch9',
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
select  'tpch10',
	c_custkey,
	c_name,
	sum(l_extendedprice * (1 - l_discount)) as revenue,
	c_acctbal,
	n_name,
	c_address,
	c_phone,
	c_comment
from
	customer,
	orders,
	lineitem,
	nation
where
	c_custkey = o_custkey
	and l_orderkey = o_orderkey
	and o_orderdate >= date '1994-10-01'
	and o_orderdate < date '1994-10-01' + interval '3 month'
	and l_returnflag = 'R'
	and c_nationkey = n_nationkey
group by
	c_custkey,
	c_name,
	c_acctbal,
	c_phone,
	n_name,
	c_address,
	c_comment
order by
	revenue desc
LIMIT 20;
select  'tpch11',
	ps_partkey,
	sum(ps_supplycost * ps_availqty) as value
from
	partsupp,
	supplier,
	nation
where
	ps_suppkey = s_suppkey
	and s_nationkey = n_nationkey
	and n_name = 'ROMANIA'
group by
	ps_partkey having
		sum(ps_supplycost * ps_availqty) > (
			select
				sum(ps_supplycost * ps_availqty) * 0.0001000000
			from
				partsupp,
				supplier,
				nation
			where
				ps_suppkey = s_suppkey
				and s_nationkey = n_nationkey
				and n_name = 'ROMANIA'
		)
order by
	value desc;
select  'tpch12',
	l_shipmode,
	sum(case
		when o_orderpriority = '1-URGENT'
			or o_orderpriority = '2-HIGH'
			then 1
		else 0
	end) as high_line_count,
	sum(case
		when o_orderpriority <> '1-URGENT'
			and o_orderpriority <> '2-HIGH'
			then 1
		else 0
	end) as low_line_count
from
	orders,
	lineitem
where
	o_orderkey = l_orderkey
	and l_shipmode in ('RAIL', 'MAIL')
	and l_commitdate < l_receiptdate
	and l_shipdate < l_commitdate
	and l_receiptdate >= date '1993-01-01'
	and l_receiptdate < date '1993-01-01' + interval '1 year'
group by
	l_shipmode
order by
	l_shipmode;
select  'tpch13',
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
select  'tpch14',
	100.00 * sum(case
		when p_type like 'PROMO%'
			then l_extendedprice * (1 - l_discount)
		else 0
	end) / sum(l_extendedprice * (1 - l_discount)) as promo_revenue
from
	lineitem,
	part
where
	l_partkey = p_partkey
	and l_shipdate >= date '1993-01-01'
	and l_shipdate < date '1993-01-01' + interval '1 month';
create view revenue0 (supplier_no, total_revenue) as
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



select  'tpch15',
	s_suppkey,
	s_name,
	s_address,
	s_phone,
	total_revenue
from
	supplier,
	revenue0
where
	s_suppkey = supplier_no
	and total_revenue = (
		select
			max(total_revenue)
		from
			revenue0
	)
order by
	s_suppkey;

drop view revenue0;
select 'tpch16',
	p_brand, 
	p_type, 
	p_size, 
	count(distinct ps_suppkey) as supplier_cnt 
from 
	part, 
	partsupp left join supplier on (ps_suppkey=s_suppkey and s_comment like '%Customer%Complaints%' ) 
where 
	p_partkey = ps_partkey 
	and p_brand <> 'Brand#35' 
	and p_type not like 'MEDIUM ANODIZED%' 
	and p_size in (39, 31, 24, 22, 46, 20, 42, 15)
	and s_suppkey is null 
group by 
	p_brand, 
	p_type, 
	p_size 
order by 
	supplier_cnt desc, 
	p_brand, 
	p_type, 
	p_size;
select
	p_brand,
	p_type,
	p_size,
	count(distinct ps_suppkey) as supplier_cnt
from
	partsupp,
	part
where
	p_partkey = ps_partkey
	and p_brand <> 'Brand#35'
	and p_type not like 'MEDIUM ANODIZED%'
	and p_size in (39, 31, 24, 22, 46, 20, 42, 15)
	and ps_suppkey not in (
		select
			s_suppkey
		from
			supplier
		where
			s_comment like '%Customer%Complaints%'
	)
group by
	p_brand,
	p_type,
	p_size
order by
	supplier_cnt desc,
	p_brand,
	p_type,
	p_size;select 'tpch17',
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
              and p_container = 'JUMBO PACK'
          group by
              p_partkey
      ) x
where
      x_partkey = l_partkey
and l_quantity < x_avg_20;
select
	sum(l_extendedprice) / 7.0 as avg_yearly
from
	lineitem,
	part
where
	p_partkey = l_partkey
	and p_brand = 'Brand#32'
	and p_container = 'JUMBO PACK'
	and l_quantity < (
		select
			0.2 * avg(l_quantity)
		from
			lineitem
		where
			l_partkey = p_partkey
	);
select  'tpch18',
	c_name,
	c_custkey,
	o_orderkey,
	o_orderdate,
	o_totalprice,
	sum(l_quantity)
from
	customer,
	orders,
	lineitem
where
	o_orderkey in (
		select
			l_orderkey
		from
			lineitem
		group by
			l_orderkey having
				sum(l_quantity) > 300
	)
	and c_custkey = o_custkey
	and o_orderkey = l_orderkey
group by
	c_name,
	c_custkey,
	o_orderkey,
	o_orderdate,
	o_totalprice
order by
	o_totalprice desc,
	o_orderdate
LIMIT 100;
select  'tpch19',
	sum(l_extendedprice* (1 - l_discount)) as revenue
from
	lineitem,
	part
where
	(
		p_partkey = l_partkey
		and p_brand = 'Brand#23'
		and p_container in ('SM CASE', 'SM BOX', 'SM PACK', 'SM PKG')
		and l_quantity >= 1 and l_quantity <= 1 + 10
		and p_size between 1 and 15
		and l_shipmode in ('AIR', 'AIR REG')
		and l_shipinstruct = 'DELIVER IN PERSON'
	)
	or
	(
		p_partkey = l_partkey
		and p_brand = 'Brand#53'
		and p_container in ('MED BAG', 'MED BOX', 'MED PKG', 'MED PACK')
		and l_quantity >= 10 and l_quantity <= 10 + 10
		and p_size between 1 and 15
		and l_shipmode in ('AIR', 'AIR REG')
		and l_shipinstruct = 'DELIVER IN PERSON'
	)
	or
	(
		p_partkey = l_partkey
		and p_brand = 'Brand#21'
		and p_container in ('LG CASE', 'LG BOX', 'LG PACK', 'LG PKG')
		and l_quantity >= 20 and l_quantity <= 20 + 10
		and p_size between 1 and 15
		and l_shipmode in ('AIR', 'AIR REG')
		and l_shipinstruct = 'DELIVER IN PERSON'
	);select  'tpch20',
	s_name,
	s_address 
from 
	supplier, 
	nation 
where 
	s_suppkey in( 
		select 
			ps_suppkey 
		from 
			partsupp, 
			( 
				select 
					sum(l_quantity) as qty_sum, l_partkey, l_suppkey 
				from 
					lineitem 
				where 
					l_shipdate >= date '1996-01-01' 
					and l_shipdate < date '1996-01-01' + interval '1 year' 
				group by l_partkey, l_suppkey ) g 
		where 
			g.l_partkey = ps_partkey 
			and g.l_suppkey = ps_suppkey 
			and ps_availqty > 0.5 * g.qty_sum 
			and ps_partkey in ( select p_partkey from part where p_name like 'medium%' ) 
		) 
	and s_nationkey = n_nationkey 
	and n_name = 'UNITED STATES'
order by 
	s_name;

select
	s_name,
	s_address
from
	supplier,
	nation
where
	s_suppkey in (
		select
			ps_suppkey
		from
			partsupp
		where
			ps_partkey in (
				select
					p_partkey
				from
					part
				where
					p_name like 'medium%'
			)
			and ps_availqty > (
				select
					0.5 * sum(l_quantity)
				from
					lineitem
				where
					l_partkey = ps_partkey
					and l_suppkey = ps_suppkey
					and l_shipdate >= date '1996-01-01'
					and l_shipdate < date '1996-01-01' + interval '1 year'
			)
	)
	and s_nationkey = n_nationkey
	and n_name = 'UNITED STATES'
order by
	s_name;select  'tpch21',
	s_name, 
	count(distinct(l1.l_orderkey::text||l1.l_linenumber::text)) as numwait 
from 
	supplier, 
	orders, 
	nation, 
	lineitem l1 
		left join lineitem l2 
			on (l2.l_orderkey = l1.l_orderkey and l2.l_suppkey <> l1.l_suppkey) 
		left join (
			select 
				l3.l_orderkey,
				l3.l_suppkey 
			from 
				lineitem l3 
			where 
				l3.l_receiptdate > l3.l_commitdate) l4 
			on (l4.l_orderkey = l1.l_orderkey and l4.l_suppkey <> l1.l_suppkey) 
where 
	s_suppkey = l1.l_suppkey 
	and o_orderkey = l1.l_orderkey 
	and o_orderstatus = 'F' 
	and l1.l_receiptdate > l1.l_commitdate 
	and l2.l_orderkey is not null 
	and l4.l_orderkey is null 
	and s_nationkey = n_nationkey 
	and n_name = 'MOZAMBIQUE' 
group by 
	s_name 
order by 
	numwait desc, 
	s_name
LIMIT 100;
select  'tpch22',
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
	cntrycode;
select
	p_brand,
	p_type,
	p_size,
	count(distinct ps_suppkey) as supplier_cnt
from
	partsupp,
	part
where
	p_partkey = ps_partkey
	and p_brand <> 'Brand#35'
	and p_type not like 'MEDIUM ANODIZED%'
	and p_size in (39, 31, 24, 22, 46, 20, 42, 15)
	and ps_suppkey not in (
		select
			s_suppkey
		from
			supplier
		where
			s_comment like '%Customer%Complaints%'
	)
group by
	p_brand,
	p_type,
	p_size
order by
	supplier_cnt desc,
	p_brand,
	p_type,
	p_size;

select
	p_brand, 
	p_type, 
	p_size, 
	count(distinct ps_suppkey) as supplier_cnt 
from 
	part, 
	partsupp left join supplier on (ps_suppkey=s_suppkey and s_comment like '%Customer%Complaints%' ) 
where 
	p_partkey = ps_partkey 
	and p_brand <> 'Brand#35' 
	and p_type not like 'MEDIUM ANODIZED%' 
	and p_size in (39, 31, 24, 22, 46, 20, 42, 15)
	and s_suppkey is null 
group by 
	p_brand, 
	p_type, 
	p_size 
order by 
	supplier_cnt desc, 
	p_brand, 
	p_type, 
	p_size;

