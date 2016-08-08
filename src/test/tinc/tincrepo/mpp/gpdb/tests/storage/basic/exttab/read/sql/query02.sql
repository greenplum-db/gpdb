--
-- gpfdist
--

-- start_ignore
drop external table if exists ext_nation;
-- end_ignore
CREATE EXTERNAL TABLE EXT_NATION  ( N_NATIONKEY  INTEGER ,
                            N_NAME       CHAR(25) ,
                            N_REGIONKEY  INTEGER ,
                            N_COMMENT    VARCHAR(152))
location ('gpfdist://@hostname@:@gp_port@/nation.tbl' )
FORMAT 'text' (delimiter '|');


-- start_ignore
drop external table if exists ext_region;
-- end_ignore
CREATE EXTERNAL TABLE EXT_REGION  ( R_REGIONKEY  INTEGER ,
                            R_NAME       CHAR(25) ,
                            R_COMMENT    VARCHAR(152))
location ('gpfdist://@hostname@:@gp_port@/region.tbl' )
FORMAT 'text' (delimiter '|');

-- start_ignore
drop external table if exists ext_part;
-- end_ignore
CREATE EXTERNAL TABLE EXT_PART  ( P_PARTKEY     INTEGER ,
                          P_NAME        VARCHAR(55) ,
                          P_MFGR        CHAR(25) ,
                          P_BRAND       CHAR(10) ,
                          P_TYPE        VARCHAR(25) ,
                          P_SIZE        INTEGER ,
                          P_CONTAINER   CHAR(10) ,
                          P_RETAILPRICE DECIMAL(15,2) ,
                          P_COMMENT     VARCHAR(23)  )
location ('gpfdist://@hostname@:@gp_port@/part.tbl' )
FORMAT 'text' (delimiter '|');

-- start_ignore
drop external table if exists ext_supplier;
-- end_ignore
CREATE EXTERNAL TABLE EXT_SUPPLIER ( S_SUPPKEY     INTEGER ,
                             S_NAME        CHAR(25) ,
                             S_ADDRESS     VARCHAR(40) ,
                             S_NATIONKEY   INTEGER ,
                             S_PHONE       CHAR(15) ,
                             S_ACCTBAL     DECIMAL(15,2) ,
                             S_COMMENT     VARCHAR(101) )
location ('gpfdist://@hostname@:@gp_port@/supplier.tbl' )
FORMAT 'text' (delimiter '|');


-- start_ignore
drop external table if exists ext_partsupp;
-- end_ignore
CREATE EXTERNAL TABLE EXT_PARTSUPP ( PS_PARTKEY     INTEGER ,
                             PS_SUPPKEY     INTEGER ,
                             PS_AVAILQTY    INTEGER ,
                             PS_SUPPLYCOST  DECIMAL(15,2)  ,
                             PS_COMMENT     VARCHAR(199)  )
location ('gpfdist://@hostname@:@gp_port@/partsupp.tbl' )
FORMAT 'text' (delimiter '|');

-- start_ignore
drop external table if exists ext_customer;
-- end_ignore
CREATE EXTERNAL TABLE EXT_CUSTOMER ( C_CUSTKEY     INTEGER ,
                             C_NAME        VARCHAR(25) ,
                             C_ADDRESS     VARCHAR(40) ,
                             C_NATIONKEY   INTEGER ,
                             C_PHONE       CHAR(15) ,
                             C_ACCTBAL     DECIMAL(15,2)   ,
                             C_MKTSEGMENT  CHAR(10) ,
                             C_COMMENT     VARCHAR(117) )
location ('gpfdist://@hostname@:@gp_port@/customer.tbl' )
FORMAT 'text' (delimiter '|');

-- start_ignore
drop external table if exists ext_orders;
-- end_ignore
CREATE EXTERNAL TABLE EXT_ORDERS  ( O_ORDERKEY       INTEGER ,
                           O_CUSTKEY        INTEGER ,
                           O_ORDERSTATUS    CHAR(1) ,
                           O_TOTALPRICE     DECIMAL(15,2) ,
                           O_ORDERDATE      DATE ,
                           O_ORDERPRIORITY  CHAR(15) ,  
                           O_CLERK          CHAR(15) , 
                           O_SHIPPRIORITY   INTEGER ,
                           O_COMMENT        VARCHAR(79) )
location ('gpfdist://@hostname@:@gp_port@/orders.tbl' )
FORMAT 'text' (delimiter '|');

-- start_ignore
drop external table if exists ext_lineitem;
-- end_ignore
CREATE EXTERNAL TABLE EXT_LINEITEM ( L_ORDERKEY    INTEGER ,
                             L_PARTKEY     INTEGER ,
                             L_SUPPKEY     INTEGER ,
                             L_LINENUMBER  INTEGER ,
                             L_QUANTITY    DECIMAL(15,2) ,
                             L_EXTENDEDPRICE  DECIMAL(15,2) ,
                             L_DISCOUNT    DECIMAL(15,2) ,
                             L_TAX         DECIMAL(15,2) ,
                             L_RETURNFLAG  CHAR(1) ,
                             L_LINESTATUS  CHAR(1) ,
                             L_SHIPDATE    DATE ,
                             L_COMMITDATE  DATE ,
                             L_RECEIPTDATE DATE ,
                             L_SHIPINSTRUCT CHAR(25) ,
                             L_SHIPMODE     CHAR(10) ,
                             L_COMMENT      VARCHAR(44) )
location ('gpfdist://@hostname@:@gp_port@/lineitem.tbl' )
FORMAT 'text' (delimiter '|');

SELECT * FROM EXT_NATION;

SELECT * FROM EXT_REGION;

SELECT count(*) FROM EXT_PART;

SELECT * FROM EXT_SUPPLIER;

SELECT count(*) FROM EXT_PARTSUPP;

SELECT count(*) FROM EXT_CUSTOMER;

SELECT count(*) FROM EXT_ORDERS;

SELECT count(*) FROM EXT_LINEITEM;

SELECT 
l_orderkey, sum(l_extendedprice*(1-l_discount)) as revenue,o_orderdate, o_shippriority
FROM
ext_customer, ext_orders, ext_lineitem 
WHERE 
c_mktsegment = 'BUILDING' and 
c_custkey = o_custkey and 
l_orderkey = o_orderkey and 
o_orderdate < date '15-mar-1995' and l_shipdate > date '15-mar-1995' 
GROUP BY l_orderkey, o_orderdate, o_shippriority
ORDER BY revenue desc, o_orderdate limit 10;


select n_name, 
sum(l_extendedprice * (1 - l_discount)) as revenue 
from 
ext_customer,   ext_orders,  ext_lineitem,  ext_supplier,  ext_nation,  ext_region 
where 
c_custkey = o_custkey and l_orderkey = o_orderkey and 
l_suppkey = s_suppkey and c_nationkey = s_nationkey and 
s_nationkey = n_nationkey and n_regionkey = r_regionkey and 
r_name = 'ASIA' and o_orderdate >= date '1-jan-1994' and 
o_orderdate < date '1-jan-1994' + interval '1 year' 
group by n_name order by revenue desc;

select supp_nation,cust_nation,l_year, 
sum(volume) as revenue 
from ( 
    select n1.n_name as supp_nation,n2.n_name as cust_nation, 
    extract(year from l_shipdate) as l_year, 
    l_extendedprice * (1 - l_discount) as volume 
    from 
    ext_supplier,  ext_lineitem, ext_orders, ext_customer, ext_nation n1, ext_nation n2 
    where 
    s_suppkey = l_suppkey and o_orderkey = l_orderkey and 
    c_custkey = o_custkey and s_nationkey = n1.n_nationkey and 
    c_nationkey = n2.n_nationkey and 
    ( (n1.n_name = 'FRANCE' and n2.n_name = 'GERMANY') or 
    (n1.n_name = 'GERMANY' and n2.n_name = 'FRANCE')) and 
    l_shipdate between date '1995-01-01' and 
    date '1996-12-31') as shipping 
    group by supp_nation,cust_nation,l_year 
    order by supp_nation,cust_nation,l_year;

select nation,o_year,sum(amount) as sum_profit 
from (
    select n_name as nation, extract(year from o_orderdate) as o_year, 
    l_extendedprice * (1 - l_discount) - ps_supplycost * l_quantity as amount
    from 
    ext_part,  ext_supplier, ext_lineitem, ext_partsupp, ext_orders, ext_nation
    where 
    s_suppkey = l_suppkey and ps_suppkey = l_suppkey and 
    ps_partkey = l_partkey and p_partkey = l_partkey and 
    o_orderkey = l_orderkey and s_nationkey = n_nationkey and 
    p_name like '%green%') as profit 
    group by nation,o_year 
    order by nation,o_year desc;

DROP EXTERNAL TABLE EXT_NATION;
DROP EXTERNAL TABLE EXT_REGION;
DROP EXTERNAL TABLE EXT_PART;
DROP EXTERNAL TABLE EXT_SUPPLIER;
DROP EXTERNAL TABLE EXT_PARTSUPP;
DROP EXTERNAL TABLE EXT_CUSTOMER;
DROP EXTERNAL TABLE EXT_ORDERS;
DROP EXTERNAL TABLE EXT_LINEITEM;
