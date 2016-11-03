\c tpch_heap

-- Create table region
CREATE TABLE aopart_REGION (
    R_REGIONKEY INTEGER,
    R_NAME CHAR(25),
    R_COMMENT VARCHAR(152)
    )
partition by range (r_regionkey) (partition p1 start('0') end('5'));
insert into aopart_region select * from region;
select count(*) from aopart_region;
ANALYZE aopart_REGION;

-- Create table nation
CREATE TABLE aopart_NATION (
    N_NATIONKEY INTEGER,
    N_NAME CHAR(25),
    N_REGIONKEY INTEGER,
    N_COMMENT VARCHAR(152)
    )
partition by range (n_nationkey)
subpartition by range (n_regionkey) subpartition template (start('0') end('1') inclusive,start('1') exclusive)
(partition p1 start('0') end('10') WITH (appendonly=true,checksum=true,compresslevel=9), partition p2 start('10') end('25') WITH (checksum=false,appendonly=true,compresslevel=7));

insert into aopart_nation select * from nation;
select count(*) from aopart_nation;
ANALYZE aopart_NATION;

-- Create table customer
CREATE TABLE aopart_CUSTOMER (
    C_CUSTKEY INTEGER,
    C_NAME VARCHAR(25),
    C_ADDRESS VARCHAR(40),
    C_NATIONKEY INTEGER,
    C_PHONE CHAR(15),
    C_ACCTBAL decimal,
    C_MKTSEGMENT CHAR(10),
    C_COMMENT VARCHAR(117)
    )
    WITH (appendonly=true,checksum=false,compresslevel=1)
    partition by list (c_mktsegment)
    (partition p1 values('BUILDING','FURNITURE'), partition p2 values('MACHINERY'), partition p3 values('AUTOMOBILE'), partition p4 values('HOUSEHOLD'), partition p5 values(null));

insert into aopart_customer select * from customer;
select count(*) from aopart_customer;
ANALYZE aopart_CUSTOMER;

-- Create table part
CREATE TABLE aopart_part (
    P_PARTKEY INTEGER,
    P_NAME VARCHAR(55),
    P_MFGR CHAR(25),
    P_BRAND CHAR(10),
    P_TYPE VARCHAR(25),
    P_SIZE integer,
    P_CONTAINER CHAR(10),
    P_RETAILPRICE decimal,
    P_COMMENT VARCHAR(23)
    )
    WITH (blocksize=16384,appendonly=true,checksum=false,compresstype=zlib,compresslevel=1)
    partition by list (p_brand)
    (partition p1 values('Brand#45','Brand#31','Brand#25') WITH (appendonly=true,checksum=true,compresslevel=5,compresstype=zlib), partition p2 values('Brand#34','Brand#22','Brand#21','Brand#55','Brand#32','Brand#13','Brand#35','Brand#51','Brand#24','Brand#43','Brand#54','Brand#33','Brand#23','Brand#14','Brand#53','Brand#15','Brand#52','Brand#44','Brand#41','Brand#42','Brand#11','Brand#12'));

insert into aopart_part select * from part;
select count(*) from aopart_part;
ANALYZE aopart_part;

-- Create table supplier
CREATE TABLE aopart_supplier (
    S_SUPPKEY INTEGER,
    S_NAME CHAR(25),
    S_ADDRESS VARCHAR(40),
    S_NATIONKEY INTEGER,
    S_PHONE CHAR(15),
    S_ACCTBAL decimal,
    S_COMMENT VARCHAR(101)
    )
    WITH (blocksize=32768,appendonly=true,checksum=true,compresslevel=2)
    partition by range (s_nationkey)
(partition p1 start('0') end('25') every(12));
insert into aopart_supplier select * from supplier;
select count(*) from aopart_supplier;
ANALYZE aopart_supplier;

-- Create table partsupp
CREATE TABLE aopart_PARTSUPP (
    PS_PARTKEY INTEGER,
    PS_SUPPKEY INTEGER,
    PS_AVAILQTY integer,
    PS_SUPPLYCOST decimal,
    PS_COMMENT VARCHAR(199)
    )
    WITH (checksum=false,appendonly=true,blocksize=49152,compresslevel=8)
    partition by range (ps_partkey)
(partition p1 start('1') end('9214980'), partition p2 start('9214980') end('43244457') exclusive , partition p3 end('60489818'), partition p4 start('60489818') end('63663358') inclusive WITH (appendonly=true,checksum=false,compresstype=zlib,compresslevel=1), partition p5 end('100000001') WITH (checksum=true,appendonly=true,compresstype=zlib,compresslevel=1));
insert into aopart_partsupp select * from partsupp;
select count(*) from aopart_partsupp;
ANALYZE aopart_PARTSUPP;

-- Create table orders
CREATE TABLE aopart_ORDERS (
    O_ORDERKEY INT8,
    O_CUSTKEY INTEGER,
    O_ORDERSTATUS CHAR(1),
    O_TOTALPRICE decimal,
    O_ORDERDATE date,
    O_ORDERPRIORITY CHAR(15),
    O_CLERK CHAR(15),
    O_SHIPPRIORITY integer,
    O_COMMENT VARCHAR(79)
    )
    partition by range (o_orderkey) subpartition by range (o_orderdate), subpartition by list (o_orderstatus) subpartition template (values('F','O','P'))
    (partition p1 start('1') end('6000001') every(2000000)
    (subpartition sp1 start('1992-01-01') ,subpartition sp2 start('1996-08-03') end('1998-08-03')));
insert into aopart_orders select * from orders;
select count(*) from aopart_orders;
ANALYZE aopart_ORDERS;

-- Create table lineitem
CREATE TABLE aopart_LINEITEM (
    L_ORDERKEY INT8,
    L_PARTKEY INTEGER,
    L_SUPPKEY INTEGER,
    L_LINENUMBER integer,
    L_QUANTITY decimal,
    L_EXTENDEDPRICE decimal,
    L_DISCOUNT decimal,
    L_TAX decimal,
    L_RETURNFLAG CHAR(1),
    L_LINESTATUS CHAR(1),
    L_SHIPDATE date,
    L_COMMITDATE date,
    L_RECEIPTDATE date,
    L_SHIPINSTRUCT CHAR(25),
    L_SHIPMODE CHAR(10),
    L_COMMENT VARCHAR(44)
    )
    WITH (appendonly=true,checksum=false,compresslevel=3)
    partition by list (l_tax)
    subpartition by range (l_suppkey) subpartition template (start('1') end('5000001') every(1666666))
    ,subpartition by range (l_commitdate) subpartition template (start('1992-01-31') end('1998-11-01') every(interval '15 months'))
    ,subpartition by list (l_discount) subpartition template (
    values('0','0.1'),
    values('0.06','0.01','0.02','0.07','0.08') WITH (appendonly=true,checksum=false,compresstype=zlib,compresslevel=1),
    values('0.09','0.05','0.04','0.03'))
    (partition p1 values('0','0.08') WITH (compresslevel=1,compresstype=zlib,appendonly=true,checksum=true), partition p2 values('0.07','0.01','0.06','0.05','0.02','0.04','0.03'));
insert into aopart_lineitem select * from lineitem;
select count(*) from aopart_lineitem;

create view revenue (supplier_no, total_revenue) as
    select l_suppkey,
           sum(l_extendedprice * (1 - l_discount))
    from aopart_lineitem
    where l_shipdate >= date '1-jan-1996'
          and l_shipdate < date '1-jan-1996' + interval '3 month'
    group by l_suppkey;
ANALYZE aopart_LINEITEM;

select 'TPCH QUERY 01', l_returnflag, l_linestatus, sum(l_quantity) as sum_qty, sum(l_extendedprice) as sum_base_price,
    sum(l_extendedprice*(1-l_discount)) as sum_disc_price, sum(l_extendedprice*(1-l_discount)*(1+l_tax)) as sum_charge,
    avg(l_quantity) as avg_qty, avg(l_extendedprice) as avg_price, avg(l_discount) as avg_disc, count(*) as count_order
    from aopart_lineitem where l_shipdate <= date '1998-12-01' - interval '90 day' group by l_returnflag, l_linestatus
    order by l_returnflag, l_linestatus;

select 'TPCH QUERY 02', s.s_acctbal,s.s_name,n.n_name,p.p_partkey,p.p_mfgr,s.s_address,s.s_phone,s.s_comment
    from aopart_supplier s, aopart_partsupp ps, aopart_nation n, aopart_region r, aopart_part p,
        (select p_partkey, min(ps_supplycost) as min_ps_cost
            from aopart_part, aopart_partsupp , aopart_supplier,aopart_nation, aopart_region
            where p_partkey=ps_partkey and s_suppkey = ps_suppkey and s_nationkey = n_nationkey and
                  n_regionkey = r_regionkey and r_name = 'EUROPE'
            group by p_partkey) g
    where p.p_partkey = ps.ps_partkey and g.p_partkey = p.p_partkey and g. min_ps_cost = ps.ps_supplycost and
          s.s_suppkey = ps.ps_suppkey and p.p_size = 15 and p.p_type like '%BRASS' and s.s_nationkey = n.n_nationkey and
          n.n_regionkey = r.r_regionkey and r.r_name = 'EUROPE' order by s. s_acctbal desc,n.n_name,s.s_name,p.p_partkey limit 100;

select 'TPCH QUERY 03', l_orderkey,sum(l_extendedprice*(1-l_discount)) as revenue,o_orderdate, o_shippriority
    from aopart_customer,aopart_orders,aopart_lineitem
    where c_mktsegment = 'BUILDING' and c_custkey = o_custkey and l_orderkey = o_orderkey and
          o_orderdate < date '15-mar-1995' and l_shipdate > date '15-mar-1995'
    group by l_orderkey,o_orderdate,o_shippriority order by revenue desc,o_orderdate limit 10;

select 'TPCH QUERY 04',o_orderpriority,count (distinct o_orderkey) as order_count
    from aopart_orders left join aopart_lineitem on l_orderkey = o_orderkey
    where o_orderdate >= date '1-jul-1993' and o_orderdate < date '1-jul-1993' + interval '3 month' and
          l_commitdate < l_receiptdate and l_orderkey is not null
    group by o_orderpriority order by o_orderpriority;

select 'TPCH QUERY 05',n_name, sum(l_extendedprice * (1 - l_discount)) as revenue
    from aopart_customer, aopart_orders, aopart_lineitem, aopart_supplier, aopart_nation, aopart_region
    where c_custkey = o_custkey and l_orderkey = o_orderkey and l_suppkey = s_suppkey and c_nationkey = s_nationkey and
          s_nationkey = n_nationkey and n_regionkey = r_regionkey and r_name = 'ASIA' and o_orderdate >= date '1-jan-1994' and
          o_orderdate < date '1-jan-1994' + interval '1 year'
    group by n_name order by revenue desc;

select 'TPCH QUERY 06',sum(l_extendedprice*l_discount) as revenue
    from aopart_lineitem where l_shipdate >= date '1-jan-1994' and l_shipdate < date '1-jan-1994' + interval '1 year' and
         l_discount between 0.06 - 0.01 and 0.06 + 0.01 and l_quantity < 24;

select 'TPCH QUERY 07',supp_nation,cust_nation,l_year, sum(volume) as revenue
    from (
        select n1.n_name as supp_nation,n2.n_name as cust_nation, extract(year from l_shipdate) as l_year,
               l_extendedprice * (1 - l_discount) as volume
        from aopart_supplier,aopart_lineitem,aopart_orders,aopart_customer,aopart_nation n1,aopart_nation n2
        where s_suppkey = l_suppkey and o_orderkey = l_orderkey and c_custkey = o_custkey and s_nationkey = n1.n_nationkey and
              c_nationkey = n2.n_nationkey and ( (n1.n_name = 'FRANCE' and n2.n_name = 'GERMANY') or (n1.n_name = 'GERMANY' and
              n2.n_name = 'FRANCE')) and l_shipdate between date '1995-01-01' and date '1996-12-31') as shipping
    group by supp_nation,cust_nation,l_year order by supp_nation,cust_nation,l_year;

select 'TPCH QUERY 08',o_year, sum(case when nation = 'BRAZIL' then volume else 0 end) / sum(volume) as mkt_share
    from (
        select extract(year from o_orderdate) as o_year, l_extendedprice * (1-l_discount) as volume, n2.n_name as nation
            from aopart_part,aopart_supplier,aopart_lineitem,aopart_orders,aopart_customer,aopart_nation n1,aopart_nation n2,aopart_region
            where p_partkey = l_partkey and s_suppkey = l_suppkey and l_orderkey = o_orderkey and o_custkey = c_custkey and
                  c_nationkey = n1.n_nationkey and n1.n_regionkey = r_regionkey and r_name = 'AMERICA' and s_nationkey = n2.n_nationkey
                  and o_orderdate between date '1995-01-01' and date '1996-12-31' and p_type = 'ECONOMY ANODIZED STEEL') as all_nations
    group by o_year order by o_year;

select 'TPCH QUERY 09',nation,o_year,sum(amount) as sum_profit
    from (
        select n_name as nation,extract(year from o_orderdate) as o_year,
               l_extendedprice * (1 - l_discount) - ps_supplycost * l_quantity as amount
            from aopart_part,aopart_supplier,aopart_lineitem,aopart_partsupp,aopart_orders,aopart_nation
            where s_suppkey = l_suppkey and ps_suppkey = l_suppkey and ps_partkey = l_partkey and p_partkey = l_partkey and
            o_orderkey = l_orderkey and s_nationkey = n_nationkey and p_name like '%green%') as profit
    group by nation,o_year order by nation,o_year desc;

select 'TPCH QUERY 10', c_custkey,c_name,sum(l_extendedprice * (1 - l_discount)) as revenue, c_acctbal,n_name,c_address,c_phone,c_comment
    from aopart_customer,aopart_orders,aopart_lineitem,aopart_nation
    where c_custkey = o_custkey and l_orderkey = o_orderkey and o_orderdate >= date '1-oct-1993' and o_orderdate < date '1-oct-1993' +
          interval '3 month' and l_returnflag = 'R' and c_nationkey = n_nationkey
    group by c_custkey,c_name,c_acctbal,c_phone,n_name,c_address,c_comment order by revenue desc limit 20;

select 'TPCH QUERY 11', ps_partkey,sum(ps_supplycost * ps_availqty) as value
    from aopart_partsupp,aopart_supplier,aopart_nation
    where ps_suppkey = s_suppkey and s_nationkey = n_nationkey and n_name = 'GERMANY'
    group by ps_partkey having sum(ps_supplycost * ps_availqty) > (
        select sum(ps_supplycost * ps_availqty) * .0001
            from aopart_partsupp,aopart_supplier,aopart_nation
            where ps_suppkey = s_suppkey and s_nationkey = n_nationkey and n_name = 'GERMANY')
    order by value desc;

select 'TPCH QUERY 12', l_shipmode, sum(case when o_orderpriority ='1-URGENT' or o_orderpriority ='2-HIGH' then 1 else 0 end) as
       high_line_count, sum(case when o_orderpriority <> '1-URGENT' and o_orderpriority <> '2-HIGH' then 1 else 0 end) as low_line_count
    from aopart_orders, aopart_lineitem
    where o_orderkey = l_orderkey and l_shipmode in ('MAIL', 'SHIP') and l_commitdate < l_receiptdate and l_shipdate < l_commitdate and
          l_receiptdate >= date '1-jan-1994' and l_receiptdate < date '1-jan-1994' + interval '1 year'
    group by l_shipmode order by l_shipmode;

select 'TPCH QUERY 13', c_count, count(*) as custdist
    from (
        select c_custkey, count(o_orderkey)
            from aopart_customer left outer join aopart_orders on c_custkey = o_custkey and o_comment not like '%special%requests%'
            group by c_custkey ) as c_orders (c_custkey, c_count)
    group by c_count order by custdist desc,c_count desc;

select 'TPCH QUERY 14', 100.00 * sum(case when p_type like 'PROMO%' then l_extendedprice*(1-l_discount) else 0 end) /
       sum(l_extendedprice * (1 - l_discount)) as promo_revenue 
    from aopart_lineitem,aopart_part
    where l_partkey = p_partkey and l_shipdate >= date '1-sep-1995' and l_shipdate < date '1-sep-1995' + interval '1 month';

select 'TPCH QUERY 15',s_suppkey, s_name, s_address, s_phone, total_revenue
    from aopart_supplier,revenue
    where s_suppkey = supplier_no and total_revenue = ( select max(total_revenue) from revenue ) order by s_suppkey;

select 'TPCH QUERY 16a', p_brand, p_type, p_size, count(distinct ps_suppkey) as supplier_cnt
    from aopart_part, aopart_partsupp left join aopart_supplier on (ps_suppkey=s_suppkey and s_comment like '%Customer%Complaints%' )
    where p_partkey = ps_partkey and p_brand <> 'Brand#45' and p_type not like 'MEDIUM POLISHED%' and
          p_size in (49, 14, 23, 45, 19, 3, 36, 9) and s_suppkey is null
    group by p_brand, p_type, p_size order by supplier_cnt desc, p_brand, p_type, p_size;

select 'TPCH QUERY 17a', sum(l_extendedprice) / 7.0 as avg_yearly
    from aopart_lineitem l, aopart_part, ( select l_partkey, avg(l_quantity) as avg_qty from aopart_lineitem group by l_partkey ) g
    where p_partkey = l.l_partkey and p_partkey = g.l_partkey and l.l_quantity < 0.2* g.avg_qty and p_brand = 'Brand#23' and
          p_container = 'JUMBO PACK';

select c_name, c_custkey, o_orderkey,'TPCH QUERY 18', o_orderdate, o_totalprice, sum(l_quantity)
    from aopart_customer, aopart_orders, aopart_lineitem
    where o_orderkey in ( select l_orderkey from aopart_lineitem group by l_orderkey having sum(l_quantity) > 300 ) and
          c_custkey = o_custkey and o_orderkey = l_orderkey
    group by c_name,c_custkey,o_orderkey,o_orderdate,o_totalprice order by o_totalprice desc,o_orderdate;

select 'TPCH QUERY 19',sum(l_extendedprice* (1 - l_discount)) as revenue
    from aopart_lineitem, aopart_part
    where p_partkey = l_partkey and l_shipmode in ('AIR', 'AIR REG') and l_shipinstruct = 'DELIVER IN PERSON' and
          ( ( p_brand = 'Brand#12' and p_container in ('SM CASE', 'SM BOX', 'SM PACK', 'SM PKG') and l_quantity >= 1 and
          l_quantity <= 1 + 10 and p_size between 1 and 15 ) or ( p_brand = 'Brand#23' and p_container in ('MED BAG', 'MED BOX',
          'MED PKG', 'MED PACK') and l_quantity >= 10 and l_quantity <= 10 + 10 and p_size between 1 and 15 ) or ( p_brand = 'Brand#34'
          and p_container in ('LG CASE', 'LG BOX', 'LG PACK', 'LG PKG') and l_quantity >= 20 and l_quantity <= 20 + 10 and
          p_size between 1 and 15 ) );

select 'TPCH QUERY 20',s_name,s_address
    from aopart_supplier, aopart_nation
    where s_suppkey in (
        select ps_suppkey from aopart_partsupp, (
            select sum(l_quantity) as qty_sum, l_partkey, l_suppkey
                from aopart_lineitem
                where l_shipdate >= date '1-jan-1994' and l_shipdate < date '1-jan-1994' + interval '1 year'
                group by l_partkey, l_suppkey ) g
        where g.l_partkey = ps_partkey and g.l_suppkey = ps_suppkey and ps_availqty > 0.5*g.qty_sum and ps_partkey in (select p_partkey
              from aopart_part where p_name like 'forest%' )) and s_nationkey = n_nationkey and n_name = 'CANADA' order by s_name;

select 'TPCH QUERY 21',s_name, count( distinct (l1.l_orderkey::text || l1.l_linenumber::text)) as numwait
    from aopart_supplier, aopart_orders, aopart_nation, aopart_lineitem l1 left join aopart_lineitem l2 on (l2.l_orderkey = l1.l_orderkey
         and l2.l_suppkey <> l1.l_suppkey) left join (
        select l3.l_orderkey,l3.l_suppkey
            from aopart_lineitem l3
            where l3.l_receiptdate > l3.l_commitdate) l4
    on (l4.l_orderkey = l1.l_orderkey and l4.l_suppkey <> l1.l_suppkey)
    where s_suppkey = l1.l_suppkey and o_orderkey = l1.l_orderkey and o_orderstatus = 'F' and l1.l_receiptdate > l1.l_commitdate and
          l2.l_orderkey is not null and l4.l_orderkey is null and s_nationkey = n_nationkey and n_name = 'SAUDI ARABIA'
    group by s_name order by numwait desc, s_name limit 100;

select 'TPCH QUERY 22',cntrycode, count(*) as numcust, sum(c_acctbal) as totacctbal
    from (
        select substring(c_phone from 1 for 2) as cntrycode, c_acctbal
            from aopart_customer left join aopart_orders on c_custkey = o_custkey
            where substring(c_phone from 1 for 2) in ('13', '31', '23', '29', '30', '18', '17') and c_acctbal > (
                  select avg(c_acctbal) from aopart_customer where c_acctbal > 0.00 and substring(c_phone from 1 for 2) in ('13', '31',
                  '23', '29', '30', '18', '17') ) and o_custkey is null ) as custsale
    group by cntrycode order by cntrycode;
checkpoint;
