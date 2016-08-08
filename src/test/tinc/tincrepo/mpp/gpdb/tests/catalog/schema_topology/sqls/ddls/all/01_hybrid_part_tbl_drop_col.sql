-- 
-- @created 2009-01-27 14:00:00
-- @modified 2013-06-24 17:00:00
-- @tags ddl schema_topology
-- @description Hybrid partitions

CREATE TABLE REGION_hybrid_part (
                    R_REGIONKEY INTEGER,
                    R_NAME CHAR(25),
                    R_COMMENT VARCHAR(152)
                    )
partition by range (r_regionkey)
subpartition by list (r_name) subpartition template (
        values('EUROPE') WITH (blocksize=1622016,appendonly=true,checksum=true,compresslevel=5),
        values('AFRICA','MIDDLE EAST'),
        values('ASIA','AMERICA')
)
(
partition p1 start('0') end('5') every(1)

);

CREATE TABLE NATION_hybrid_part (
            N_NATIONKEY INTEGER,
            N_NAME CHAR(25),
            N_REGIONKEY INTEGER,
            N_COMMENT VARCHAR(152)
            )
partition by list (n_regionkey) 
subpartition by range (n_nationkey) subpartition template (start('0') end('25') every (12)

)
(
partition p1 values('4') WITH (blocksize=1622016,appendonly=true,checksum=true,compresslevel=5), partition p2 values('3'), partition p3 values('1') WITH (blocksize=1622016,appendonly=true,checksum=true,            compresslevel=5), partition p4 values('0','2') WITH (blocksize=1622016,appendonly=true,checksum=true,    compresslevel=5)
);

CREATE TABLE CUSTOMER_hybrid_part (
                C_CUSTKEY INTEGER,
                C_NAME VARCHAR(25),
                C_ADDRESS VARCHAR(40),
               C_NATIONKEY INTEGER,
                C_PHONE CHAR(15),
                C_ACCTBAL decimal,
                C_MKTSEGMENT CHAR(10),
                C_COMMENT VARCHAR(117)
                )
partition by list (c_nationkey) 
subpartition by list (c_mktsegment) 
,subpartition by range (c_acctbal) subpartition template (start('-999.99') end('3973.01'),start('3973.01') ,start('6434.01') 
end('10000.99')
)
(
partition p1 values('22','11','0','7','23','2')(subpartition sp1 values('FURNITURE','HOUSEHOLD'),         subpartition sp2 values('MACHINERY'),
subpartition sp3 values('BUILDING','AUTOMOBILE')), 
partition p2 values('6','1','3','9','17')(subpartition sp1 values('BUILDING','AUTOMOBILE','HOUSEHOLD')    WITH (blocksize=32768,appendonly=
true,checksum=false,compresslevel=9),subpartition sp2 values('MACHINERY'),
subpartition sp3 values('FURNITURE') WITH (compresslevel=9,appendonly=true,checksum=true,blocksize=1376256)), partition p3 values('21',
'18','16','13','12','20','15','14','8','4','24','19','10','5')
(subpartition sp1 values('BUILDING','HOUSEHOLD','AUTOMOBILE'),subpartition sp2 values('MACHINERY') WITH   (appendonly=true,checksum=true,
blocksize=1130496,compresslevel=6),subpartition sp3 values('FURNITURE') 
WITH (blocksize=655360,appendonly=true,checksum=false,compresslevel=5))
);

CREATE TABLE part_hybrid_part(
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
partition by range (p_size) 
(
partition p1 start('1')  WITH (appendonly=true,checksum=true,blocksize=819200,compresslevel=8), partition p2 start('21') end('28'), partition p3 start('28') WITH (appendonly=true,checksum=true,blocksize=819200,
compresslevel=8), partition p4 start('32') end('33'), partition p5 start('33') WITH (appendonly=true,     checksum=true,blocksize=819200,compresslevel=8) 
);

CREATE TABLE supplier_hybrid_part(
                S_SUPPKEY INTEGER,
                S_NAME CHAR(25),
                S_ADDRESS VARCHAR(40),
                S_NATIONKEY INTEGER,
                S_PHONE CHAR(15),
                S_ACCTBAL decimal,
                S_COMMENT VARCHAR(101)
                )
partition by range (s_suppkey) 
subpartition by list (s_nationkey) subpartition template (
	values('22','21','17'),
	values('6','11','1','7','16','2') WITH (checksum=false,appendonly=true,blocksize=1171456,         compresslevel=3),
	values('18','20'),
	values('9','23','13') WITH (checksum=true,appendonly=true,blocksize=1335296,compresslevel=7),
	values('0','3','12','15','14','8','4','24','19','10','5')
)
(
partition p1 start('1') end('10001') every(10000)

);

CREATE TABLE PARTSUPP_hybrid_part (
                PS_PARTKEY INTEGER,
                PS_SUPPKEY INTEGER,
                PS_AVAILQTY integer,
                PS_SUPPLYCOST decimal,
                PS_COMMENT VARCHAR(199)
                )
partition by range (ps_supplycost) 
subpartition by range (ps_suppkey)  
,subpartition by range (ps_partkey) subpartition template (start('1') end('200001') every(66666)

)
(
partition p1 start('1') end('20') inclusive(subpartition sp1 start('1') end('3030')  WITH (checksum=false,   appendonly=true,blocksize=1171456,compresslevel=3),subpartition sp2 end('6096') inclusive,subpartition sp3   start('6096') exclusive end('7201'),subpartition sp4 end('10001')  WITH (checksum=false,appendonly=true,     blocksize=1171456,compresslevel=3)), partition p2 end('1001') inclusive(subpartition sp1 start('1') ,        subpartition sp2 start('4139') ,subpartition sp3 start('4685')  WITH (checksum=false,appendonly=true,        blocksize=1171456,compresslevel=3) ,subpartition sp4 start('5675') )
);

CREATE TABLE ORDERS_hybrid_part (
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
partition by list (o_shippriority)
subpartition by list (o_orderpriority) subpartition template (
        values('5-LOW'),
        values('4-NOT SPECIFIED') WITH (blocksize=1630208,appendonly=true),
        values('2-HIGH','1-URGENT'),
        values('3-MEDIUM'),
        values(null)
)
,subpartition by list (o_orderstatus) subpartition template (
        values('O','P'),
        values('F') WITH (appendonly=true,checksum=true,blocksize=499712,compresslevel=8)
)
(
partition p1 values('0') , partition p2 values(null) WITH (compresslevel=1,appendonly=true,checksum=true,    blocksize=2080768)
);

CREATE TABLE LINEITEM_hybrid_part (
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
partition by list (l_linenumber) 
subpartition by list (l_discount) 
,subpartition by range (l_receiptdate) subpartition template (start('1992-01-03') end('1999-01-01')          every(interval '21 months') WITH (compresslevel=1,appendonly=true,checksum=true,blocksize=2080768) 
)
(
partition p1 values('7','2','1','6','3','4','5') (subpartition sp1 values('0.09','0.07','0.01','0','0.08',   '0.06','0.1','0.05','0.02','0.04','0.03'))
);

insert into nation_hybrid_part ( N_NATIONKEY   ,N_NAME  ,N_REGIONKEY   ,N_COMMENT   ) select N_NATIONKEY   ,N_NAME ,N_REGIONKEY   ,N_COMMENT  from nation;

insert into region_hybrid_part ( R_REGIONKEY ,R_NAME ,R_COMMENT ) select  R_REGIONKEY ,R_NAME  ,R_COMMENT    from region;

insert into part_hybrid_part  ( P_PARTKEY ,P_NAME ,P_MFGR ,P_BRAND , P_TYPE ,P_SIZE , P_CONTAINER,        P_RETAILPRICE  ,P_COMMENT ) select   P_PARTKEY ,P_NAME ,P_MFGR ,P_BRAND , P_TYPE ,P_SIZE , P_CONTAINER   ,P_RETAILPRICE  ,P_COMMENT  from part;

insert into supplier_hybrid_part (S_SUPPKEY ,S_NAME ,S_ADDRESS ,S_NATIONKEY ,S_PHONE ,S_ACCTBAL,S_COMMENT) select S_SUPPKEY ,S_NAME ,S_ADDRESS ,S_NATIONKEY ,S_PHONE ,S_ACCTBAL ,S_COMMENT from supplier;

insert into partsupp_hybrid_part ( PS_PARTKEY ,PS_SUPPKEY,PS_AVAILQTY ,PS_SUPPLYCOST ,PS_COMMENT   ) select  PS_PARTKEY ,PS_SUPPKEY ,PS_AVAILQTY ,PS_SUPPLYCOST ,PS_COMMENT from partsupp;

insert into customer_hybrid_part ( C_CUSTKEY  ,C_NAME  ,C_ADDRESS ,C_NATIONKEY  ,C_PHONE ,C_ACCTBAL  ,C_MKTSEGMENT  ,C_COMMENT) select C_CUSTKEY,C_NAME ,C_ADDRESS,C_NATIONKEY ,C_PHONE  ,C_ACCTBAL  ,C_MKTSEGMENT  ,C_COMMENT from customer;

insert into orders_hybrid_part ( O_ORDERKEY,O_CUSTKEY,O_ORDERSTATUS,O_TOTALPRICE ,O_ORDERDATE,O_ORDERPRIORITY ,O_CLERK ,O_SHIPPRIORITY ,O_COMMENT ) select  O_ORDERKEY ,O_CUSTKEY ,O_ORDERSTATUS ,O_TOTALPRICE,O_ORDERDATE ,O_ORDERPRIORITY   ,O_CLERK ,O_SHIPPRIORITY,O_COMMENT from orders;

insert into lineitem_hybrid_part ( L_ORDERKEY ,L_PARTKEY ,L_SUPPKEY,L_LINENUMBER ,L_QUANTITY,L_EXTENDEDPRICE   ,L_DISCOUNT ,L_TAX ,L_RETURNFLAG ,L_LINESTATUS ,L_SHIPDATE ,L_COMMITDATE  ,L_RECEIPTDATE ,L_SHIPINSTRUCT  ,L_SHIPMODE ,L_COMMENT ) select  L_ORDERKEY ,L_PARTKEY ,L_SUPPKEY ,L_LINENUMBER ,L_QUANTITY ,L_EXTENDEDPRICE   ,L_DISCOUNT,L_TAX ,L_RETURNFLAG ,L_LINESTATUS ,L_SHIPDATE ,L_COMMITDATE  ,L_RECEIPTDATE ,L_SHIPINSTRUCT     ,L_SHIPMODE  ,L_COMMENT from lineitem;

ALTER TABLE NATION_hybrid_part DROP COLUMN N_NAME;
insert into nation_hybrid_part ( N_NATIONKEY   ,N_REGIONKEY   ,N_COMMENT   ) select N_NATIONKEY   ,N_REGIONKEY   ,N_COMMENT  from nation ;
ALTER TABLE NATION_hybrid_part DROP COLUMN N_COMMENT;
insert into nation_hybrid_part (  N_NATIONKEY ,N_REGIONKEY ) select N_NATIONKEY,N_REGIONKEY from nation;
\d+ nation_hybrid_part

ALTER TABLE REGION_hybrid_part DROP COLUMN R_COMMENT;
insert into region_hybrid_part ( R_REGIONKEY ,R_NAME   ) select  R_REGIONKEY,R_NAME  from region;
\d+ region_hybrid_part 

ALTER TABLE PART_hybrid_part DROP COLUMN P_NAME;
insert into part_hybrid_part  ( P_PARTKEY ,P_MFGR ,P_BRAND, P_TYPE ,P_SIZE , P_CONTAINER ,P_RETAILPRICE  ,P_COMMENT ) select P_PARTKEY ,P_MFGR ,P_BRAND , P_TYPE ,P_SIZE , P_CONTAINER ,P_RETAILPRICE  ,P_COMMENT    from part LIMIT 10000;
ALTER TABLE PART_hybrid_part DROP COLUMN P_MFGR;
insert into part_hybrid_part  ( P_PARTKEY ,P_BRAND, P_TYPE ,P_SIZE , P_CONTAINER ,P_RETAILPRICE  ,
P_COMMENT ) select P_PARTKEY ,P_BRAND , P_TYPE ,P_SIZE , P_CONTAINER ,P_RETAILPRICE  ,P_COMMENT   
 from part LIMIT 10000;
ALTER TABLE PART_hybrid_part DROP COLUMN P_BRAND;
insert into part_hybrid_part  ( P_PARTKEY  ,P_TYPE ,P_SIZE , P_CONTAINER ,P_RETAILPRICE  ,P_COMMENT )     select P_PARTKEY , P_TYPE ,P_SIZE  , P_CONTAINER   , P_RETAILPRICE  , P_COMMENT  from part LIMIT 10000;
ALTER TABLE PART_hybrid_part DROP COLUMN P_TYPE;
insert into part_hybrid_part ( P_PARTKEY ,P_SIZE, P_CONTAINER ,P_RETAILPRICE ,P_COMMENT ) select P_PARTKEY ,P_SIZE  , P_CONTAINER   , P_RETAILPRICE  , P_COMMENT  from part LIMIT 10000;
ALTER TABLE PART_hybrid_part DROP COLUMN P_PARTKEY;
insert into part_hybrid_part ( P_SIZE  , P_CONTAINER ,P_RETAILPRICE ,P_COMMENT) select P_SIZE, P_CONTAINER, P_RETAILPRICE  , P_COMMENT from part LIMIT 10000;
ALTER TABLE PART_hybrid_part DROP COLUMN P_CONTAINER;
insert into part_hybrid_part  ( P_SIZE   ,P_RETAILPRICE  ,P_COMMENT ) select   P_SIZE  , P_RETAILPRICE  , P_COMMENT  from part LIMIT 10000;
ALTER TABLE PART_hybrid_part DROP COLUMN P_RETAILPRICE;
insert into part_hybrid_part  ( P_SIZE  ,P_COMMENT ) select   P_SIZE  , P_COMMENT from part LIMIT 10000;
ALTER TABLE PART_hybrid_part DROP COLUMN P_COMMENT;
insert into part_hybrid_part  (P_SIZE) select   P_SIZE  from part LIMIT 10000;
\d+ part_hybrid_part
ALTER TABLE SUPPLIER_hybrid_part DROP COLUMN S_NAME;
insert into supplier_hybrid_part ( S_SUPPKEY ,S_ADDRESS ,S_NATIONKEY,S_PHONE  ,S_ACCTBAL   ,S_COMMENT )   select  S_SUPPKEY ,  S_ADDRESS , S_NATIONKEY , S_PHONE , S_ACCTBAL , S_COMMENT  from supplier LIMIT 10000;
ALTER TABLE SUPPLIER_hybrid_part DROP COLUMN S_ADDRESS;
insert into supplier_hybrid_part ( S_SUPPKEY   ,S_NATIONKEY    ,S_PHONE  ,S_ACCTBAL   ,S_COMMENT  ) select  S_SUPPKEY   ,   S_NATIONKEY    , S_PHONE , S_ACCTBAL   , S_COMMENT    from supplier LIMIT 10000;
ALTER TABLE SUPPLIER_hybrid_part DROP COLUMN S_PHONE;
insert into supplier_hybrid_part (S_NATIONKEY, S_SUPPKEY   ,S_ACCTBAL   ,S_COMMENT  ) select  S_NATIONKEY,S_SUPPKEY,  S_ACCTBAL   , S_COMMENT    from supplier LIMIT 10000;
ALTER TABLE SUPPLIER_hybrid_part DROP COLUMN S_ACCTBAL;
insert into supplier_hybrid_part ( S_NATIONKEY,S_SUPPKEY ,S_COMMENT ) select S_NATIONKEY, S_SUPPKEY,      S_COMMENT    from supplier LIMIT 10000;
ALTER TABLE SUPPLIER_hybrid_part DROP COLUMN S_COMMENT;
insert into supplier_hybrid_part ( S_NATIONKEY,S_SUPPKEY ) select S_NATIONKEY, S_SUPPKEY from supplier    LIMIT 10000;
\d+ supplier_hybrid_part


ALTER TABLE PARTSUPP_hybrid_part DROP COLUMN PS_AVAILQTY;
insert into partsupp_hybrid_part ( PS_PARTKEY ,PS_SUPPKEY,PS_SUPPLYCOST ,PS_COMMENT ) select
PS_PARTKEY ,PS_SUPPKEY ,PS_SUPPLYCOST ,PS_COMMENT from partsupp LIMIT 10000;
ALTER TABLE PARTSUPP_hybrid_part DROP COLUMN PS_COMMENT;
insert into partsupp_hybrid_part ( PS_PARTKEY ,PS_SUPPKEY,PS_SUPPLYCOST ) select
PS_PARTKEY ,PS_SUPPKEY ,PS_SUPPLYCOST from partsupp LIMIT 10000;
\d+ partsupp_hybrid_part
ALTER TABLE CUSTOMER_hybrid_part DROP COLUMN C_NAME;
insert into customer_hybrid_part ( C_CUSTKEY ,C_ADDRESS ,C_NATIONKEY ,C_PHONE ,C_ACCTBAL ,C_MKTSEGMENT ,  C_COMMENT ) select C_CUSTKEY  , C_ADDRESS , C_NATIONKEY,C_PHONE, C_ACCTBAL ,C_MKTSEGMENT  ,C_COMMENT
from customer LIMIT 10000;
ALTER TABLE CUSTOMER_hybrid_part DROP COLUMN C_ADDRESS;
insert into customer_hybrid_part ( C_CUSTKEY ,C_NATIONKEY,C_PHONE ,C_ACCTBAL ,C_MKTSEGMENT ,C_COMMENT      ) select C_CUSTKEY  ,C_NATIONKEY ,C_PHONE ,C_ACCTBAL , C_MKTSEGMENT, C_COMMENT from customer LIMIT 10000;
ALTER TABLE CUSTOMER_hybrid_part DROP COLUMN C_CUSTKEY;
insert into customer_hybrid_part ( C_NATIONKEY  ,C_PHONE  ,C_ACCTBAL ,C_MKTSEGMENT,C_COMMENT     ) select C_NATIONKEY ,C_PHONE   ,  C_ACCTBAL , C_MKTSEGMENT  , C_COMMENT from customer LIMIT 10000;
ALTER TABLE CUSTOMER_hybrid_part DROP COLUMN C_PHONE;
insert into customer_hybrid_part ( C_NATIONKEY    ,C_ACCTBAL  ,C_MKTSEGMENT ,C_COMMENT) select C_NATIONKEY  ,  C_ACCTBAL  , C_MKTSEGMENT  , C_COMMENT  from customer LIMIT 10000;
ALTER TABLE CUSTOMER_hybrid_part DROP COLUMN C_COMMENT;
insert into customer_hybrid_part ( C_NATIONKEY,C_ACCTBAL,C_MKTSEGMENT) select C_NATIONKEY, C_ACCTBAL,     C_MKTSEGMENT  from customer LIMIT 10000;
\d+ customer_hybrid_part
ALTER TABLE ORDERS_hybrid_part DROP COLUMN O_ORDERKEY ;
insert into orders_hybrid_part (  O_CUSTKEY ,O_ORDERSTATUS,O_TOTALPRICE ,O_ORDERDATE ,O_ORDERPRIORITY ,O_CLERK,O_SHIPPRIORITY,O_COMMENT) select O_CUSTKEY , O_ORDERSTATUS, O_TOTALPRICE,O_ORDERDATE ,O_ORDERPRIORITY,O_CLERK ,O_SHIPPRIORITY,O_COMMENT from orders LIMIT 10000;
ALTER TABLE ORDERS_hybrid_part DROP COLUMN O_CUSTKEY;
insert into orders_hybrid_part ( O_ORDERSTATUS ,O_TOTALPRICE ,O_ORDERDATE,O_ORDERPRIORITY ,O_CLERK        ,O_SHIPPRIORITY ,O_COMMENT ) select  O_ORDERSTATUS, O_TOTALPRICE, O_ORDERDATE ,O_ORDERPRIORITY ,O_CLERK    ,O_SHIPPRIORITY  ,O_COMMENT  from orders LIMIT 10000;
ALTER TABLE ORDERS_hybrid_part DROP COLUMN O_TOTALPRICE;
insert into orders_hybrid_part ( O_ORDERSTATUS   ,O_ORDERDATE ,O_ORDERPRIORITY   ,O_CLERK  ,O_SHIPPRIORITY    ,O_COMMENT ) select  O_ORDERSTATUS  , O_ORDERDATE , O_ORDERPRIORITY   , O_CLERK, O_SHIPPRIORITY ,O_COMMENT        from orders LIMIT 10000;
ALTER TABLE ORDERS_hybrid_part DROP COLUMN O_ORDERDATE;
insert into orders_hybrid_part ( O_ORDERSTATUS     ,O_ORDERPRIORITY ,O_CLERK   ,O_SHIPPRIORITY    ,O_COMMENT  ) select  O_ORDERSTATUS,O_ORDERPRIORITY, O_CLERK , O_SHIPPRIORITY, O_COMMENT  from orders LIMIT 10000;
ALTER TABLE ORDERS_hybrid_part DROP COLUMN O_CLERK;
insert into orders_hybrid_part ( O_ORDERSTATUS  ,O_ORDERPRIORITY ,O_SHIPPRIORITY ,O_COMMENT) select           O_ORDERSTATUS     ,O_ORDERPRIORITY   , O_SHIPPRIORITY    , O_COMMENT        from orders;
ALTER TABLE ORDERS_hybrid_part DROP COLUMN O_COMMENT ;
insert into orders_hybrid_part ( O_ORDERSTATUS ,O_ORDERPRIORITY ,O_SHIPPRIORITY) select  O_ORDERSTATUS       ,O_ORDERPRIORITY   , O_SHIPPRIORITY   from orders LIMIT 10000;
\d+ ORDERS_hybrid_part
ALTER TABLE LINEITEM_hybrid_part DROP COLUMN L_ORDERKEY;
insert into lineitem_hybrid_part ( L_PARTKEY , L_SUPPKEY  ,L_LINENUMBER ,L_QUANTITY     , L_EXTENDEDPRICE   ,L_DISCOUNT ,L_TAX  ,L_RETURNFLAG , L_LINESTATUS ,L_SHIPDATE , L_COMMITDATE  , L_RECEIPTDATE ,L_SHIPINSTRUCT  ,L_SHIPMODE ,L_COMMENT  ) select L_PARTKEY  ,L_SUPPKEY ,L_LINENUMBER ,L_QUANTITY,L_EXTENDEDPRICE , L_DISCOUNT  , L_TAX , L_RETURNFLAG , L_LINESTATUS ,L_SHIPDATE ,L_COMMITDATE,L_RECEIPTDATE ,L_SHIPINSTRUCT  ,L_SHIPMODE ,L_COMMENT from lineitem LIMIT 10000;
ALTER TABLE LINEITEM_hybrid_part DROP COLUMN L_PARTKEY;
insert into lineitem_hybrid_part (  L_SUPPKEY ,L_LINENUMBER ,L_QUANTITY , L_EXTENDEDPRICE ,L_DISCOUNT ,L_TAX ,L_RETURNFLAG , L_LINESTATUS ,L_SHIPDATE , L_COMMITDATE, L_RECEIPTDATE , L_SHIPINSTRUCT,L_SHIPMODE ,L_COMMENT) select L_SUPPKEY ,L_LINENUMBER   ,L_QUANTITY ,L_EXTENDEDPRICE , L_DISCOUNT , L_TAX , L_RETURNFLAG   ,       L_LINESTATUS   ,L_SHIPDATE , L_COMMITDATE  ,L_RECEIPTDATE ,L_SHIPINSTRUCT  ,L_SHIPMODE ,L_COMMENT   from    lineitem LIMIT 10000;
ALTER TABLE LINEITEM_hybrid_part DROP COLUMN L_SUPPKEY;
insert into lineitem_hybrid_part ( L_LINENUMBER ,L_QUANTITY, L_EXTENDEDPRICE ,L_DISCOUNT ,L_TAX,L_RETURNFLAG , L_LINESTATUS ,L_SHIPDATE , L_COMMITDATE  , L_RECEIPTDATE , L_SHIPINSTRUCT  , L_SHIPMODE     , L_COMMENT      ) select L_LINENUMBER ,L_QUANTITY ,L_EXTENDEDPRICE , L_DISCOUNT , L_TAX  , L_RETURNFLAG   , L_LINESTATUS   ,L_SHIPDATE    , L_COMMITDATE  , L_RECEIPTDATE , L_SHIPINSTRUCT  ,L_SHIPMODE ,L_COMMENT  from lineitem LIMIT 10000;
ALTER TABLE LINEITEM_hybrid_part DROP COLUMN L_QUANTITY;
insert into lineitem_hybrid_part ( L_LINENUMBER   , L_EXTENDEDPRICE   ,L_DISCOUNT     ,L_TAX  ,L_RETURNFLAG   , L_LINESTATUS   ,L_SHIPDATE    , L_COMMITDATE  , L_RECEIPTDATE , L_SHIPINSTRUCT  , L_SHIPMODE , L_COMMENT      ) select L_LINENUMBER   ,L_EXTENDEDPRICE   , L_DISCOUNT     , L_TAX    , L_RETURNFLAG   , L_LINESTATUS   ,L_SHIPDATE    , L_COMMITDATE  , L_RECEIPTDATE , L_SHIPINSTRUCT  ,L_SHIPMODE  , L_COMMENT      from lineitem LIMIT 10000;
ALTER TABLE LINEITEM_hybrid_part DROP COLUMN L_EXTENDEDPRICE;
insert into lineitem_hybrid_part ( L_LINENUMBER   , L_DISCOUNT     ,L_TAX          ,L_RETURNFLAG   , L_LINESTATUS   ,L_SHIPDATE    , L_COMMITDATE  , L_RECEIPTDATE , L_SHIPINSTRUCT  , L_SHIPMODE     , L_COMMENT      ) select L_LINENUMBER   ,L_DISCOUNT     , L_TAX          , L_RETURNFLAG   , L_LINESTATUS   ,L_SHIPDATE    , L_COMMITDATE  , L_RECEIPTDATE , L_SHIPINSTRUCT  ,L_SHIPMODE  , L_COMMENT from lineitem LIMIT 10000;
ALTER TABLE LINEITEM_hybrid_part DROP COLUMN L_TAX;
insert into lineitem_hybrid_part ( L_LINENUMBER   , L_DISCOUNT     ,L_RETURNFLAG   , L_LINESTATUS ,L_SHIPDATE    , L_COMMITDATE  , L_RECEIPTDATE , L_SHIPINSTRUCT  , L_SHIPMODE     , L_COMMENT      ) select L_LINENUMBER   ,L_DISCOUNT     , L_RETURNFLAG   , L_LINESTATUS   ,L_SHIPDATE    , L_COMMITDATE  , L_RECEIPTDATE ,        L_SHIPINSTRUCT  ,L_SHIPMODE     , L_COMMENT       from lineitem LIMIT 10000;
ALTER TABLE LINEITEM_hybrid_part DROP COLUMN L_RETURNFLAG;
insert into lineitem_hybrid_part ( L_LINENUMBER   , L_DISCOUNT     , L_LINESTATUS   ,L_SHIPDATE, L_COMMITDATE  , L_RECEIPTDATE , L_SHIPINSTRUCT  , L_SHIPMODE     , L_COMMENT      ) select L_LINENUMBER   , L_DISCOUNT     ,  L_LINESTATUS   ,L_SHIPDATE    , L_COMMITDATE  , L_RECEIPTDATE , L_SHIPINSTRUCT  ,L_SHIPMODE , L_COMMENT       from lineitem LIMIT 10000;
ALTER TABLE LINEITEM_hybrid_part DROP COLUMN L_LINESTATUS;
insert into lineitem_hybrid_part ( L_LINENUMBER   , L_DISCOUNT , L_SHIPDATE , L_COMMITDATE  , L_RECEIPTDATE , L_SHIPINSTRUCT  , L_SHIPMODE , L_COMMENT ) select L_LINENUMBER   , L_DISCOUNT ,  L_SHIPDATE , L_COMMITDATE  , L_RECEIPTDATE , L_SHIPINSTRUCT  ,L_SHIPMODE , L_COMMENT from lineitem LIMIT 10000;
ALTER TABLE LINEITEM_hybrid_part DROP COLUMN L_SHIPDATE;
insert into lineitem_hybrid_part ( L_LINENUMBER   , L_DISCOUNT ,L_COMMITDATE , L_RECEIPTDATE , L_SHIPINSTRUCT  , L_SHIPMODE     , L_COMMENT  ) select L_LINENUMBER   , L_DISCOUNT  ,  L_COMMITDATE  , L_RECEIPTDATE ,     L_SHIPINSTRUCT  ,L_SHIPMODE , L_COMMENT  from lineitem LIMIT 10000;
ALTER TABLE LINEITEM_hybrid_part DROP COLUMN L_COMMITDATE;
insert into lineitem_hybrid_part ( L_LINENUMBER   , L_DISCOUNT ,L_RECEIPTDATE , L_SHIPINSTRUCT, L_SHIPMODE,   L_COMMENT) select L_LINENUMBER , L_DISCOUNT ,L_RECEIPTDATE , L_SHIPINSTRUCT , L_SHIPMODE, L_COMMENT from    lineitem LIMIT 10000;
ALTER TABLE LINEITEM_hybrid_part DROP COLUMN L_SHIPINSTRUCT;
insert into lineitem_hybrid_part ( L_LINENUMBER   , L_DISCOUNT ,L_RECEIPTDATE , L_SHIPMODE , L_COMMENT)       select L_LINENUMBER   , L_DISCOUNT ,L_RECEIPTDATE , L_SHIPMODE , L_COMMENT from lineitem LIMIT 10000;
ALTER TABLE LINEITEM_hybrid_part DROP COLUMN L_SHIPMODE;
insert into lineitem_hybrid_part ( L_LINENUMBER, L_DISCOUNT,L_RECEIPTDATE, L_COMMENT ) select L_LINENUMBER,   L_DISCOUNT ,L_RECEIPTDATE ,L_COMMENT  from lineitem LIMIT 10000;
ALTER TABLE LINEITEM_hybrid_part DROP COLUMN L_COMMENT;
insert into lineitem_hybrid_part ( L_LINENUMBER   , L_DISCOUNT ,L_RECEIPTDATE  ) select L_LINENUMBER   ,     L_DISCOUNT ,L_RECEIPTDATE from lineitem LIMIT 10000;
\d+ lineitem_hybrid_part
ALTER TABLE NATION_HYBRID_PART ADD COLUMN  N_NAME       CHAR(25) DEFAULT 'ARGENTINA';
ALTER TABLE NATION_HYBRID_PART ADD COLUMN  N_COMMENT    VARCHAR(152) DEFAULT 'regular';
\d+ NATION_HYBRID_PART
ALTER TABLE REGION_HYBRID_PART ADD COLUMN  R_COMMENT    VARCHAR(152)  DEFAULT 'regular';
\d+ REGION_HYBRID_PART
ALTER TABLE PART_HYBRID_PART ADD COLUMN   P_PARTKEY  INTEGER  DEFAULT 100;
ALTER TABLE PART_HYBRID_PART ADD COLUMN    P_NAME  VARCHAR(55)  DEFAULT 'goldenlace';
ALTER TABLE PART_HYBRID_PART ADD COLUMN   P_MFGR  CHAR(25) DEFAULT 'Manufacturer#1';
ALTER TABLE PART_HYBRID_PART ADD COLUMN    P_BRAND CHAR(10)  DEFAULT 'Brand#13';
ALTER TABLE PART_HYBRID_PART ADD COLUMN   P_TYPE  VARCHAR(25) DEFAULT 'PROMO BURNISHED COPPER';
ALTER TABLE PART_HYBRID_PART ADD COLUMN   P_CONTAINER   CHAR(10) DEFAULT 'JUMBO PKG';
ALTER TABLE PART_HYBRID_PART ADD COLUMN   P_RETAILPRICE DECIMAL(15,2) DEFAULT 100.00;
ALTER TABLE PART_HYBRID_PART ADD COLUMN    P_COMMENT VARCHAR(23) DEFAULT 'default';
\d+ PART_HYBRID_PART
ALTER TABLE SUPPLIER_HYBRID_PART ADD COLUMN   S_NAME  CHAR(25) DEFAULT 'Supplier#000000001';
ALTER TABLE SUPPLIER_HYBRID_PART ADD COLUMN   S_ADDRESS VARCHAR(40) DEFAULT 'N kD4on9OM Ipw3,gf0JBoQDd7tgrzrddZ' ;
ALTER TABLE SUPPLIER_HYBRID_PART ADD COLUMN    S_PHONE  CHAR(15)  DEFAULT '27-918-335-1736';
ALTER TABLE SUPPLIER_HYBRID_PART ADD COLUMN   S_ACCTBAL  DECIMAL(15,2) DEFAULT 100.00;
ALTER TABLE SUPPLIER_HYBRID_PART ADD COLUMN   S_COMMENT  VARCHAR(101) DEFAULT 'default';
\d+ SUPPLIER_HYBRID_PART
ALTER TABLE PARTSUPP_HYBRID_PART ADD COLUMN   PS_AVAILQTY    INTEGER DEFAULT 100.00;
ALTER TABLE PARTSUPP_HYBRID_PART ADD COLUMN   PS_COMMENT     VARCHAR(199) DEFAULT 'default' ;
\d+ PARTSUPP_HYBRID_PART
ALTER TABLE CUSTOMER_HYBRID_PART ADD COLUMN   C_CUSTKEY     INTEGER  DEFAULT 100 ;
ALTER TABLE CUSTOMER_HYBRID_PART ADD COLUMN  C_NAME  VARCHAR(25) DEFAULT 'Customer#000000001' ;
ALTER TABLE CUSTOMER_HYBRID_PART ADD COLUMN   C_ADDRESS  VARCHAR(40) DEFAULT 'IVhzIApeRb ot,c,E';
ALTER TABLE CUSTOMER_HYBRID_PART ADD COLUMN  C_PHONE  CHAR(15) DEFAULT '5-989-741-2988';
ALTER TABLE CUSTOMER_HYBRID_PART ADD COLUMN   C_COMMENT VARCHAR(117) DEFAULT 'default';
\d+ CUSTOMER_HYBRID_PART
ALTER TABLE ORDERS_HYBRID_PART ADD COLUMN   O_ORDERKEY       INTEGER  DEFAULT 100;
ALTER TABLE ORDERS_HYBRID_PART ADD COLUMN   O_CUSTKEY        INTEGER  DEFAULT 100;
ALTER TABLE ORDERS_HYBRID_PART ADD COLUMN  O_TOTALPRICE     DECIMAL(15,2)  DEFAULT 56789.90;
ALTER TABLE ORDERS_HYBRID_PART ADD COLUMN  O_ORDERDATE      DATE  DEFAULT '1996-12-01';
ALTER TABLE ORDERS_HYBRID_PART ADD COLUMN  O_CLERK          CHAR(15)  DEFAULT 'Clerk#000000880' ;
ALTER TABLE ORDERS_HYBRID_PART ADD COLUMN  O_COMMENT        VARCHAR(79)  DEFAULT 'faxes';
\d+ ORDERS_HYBRID_PART
ALTER TABLE LINEITEM_HYBRID_PART ADD COLUMN    L_ORDERKEY    INTEGER DEFAULT 100;
ALTER TABLE LINEITEM_HYBRID_PART ADD COLUMN    L_PARTKEY     INTEGER DEFAULT 100;
ALTER TABLE LINEITEM_HYBRID_PART ADD COLUMN    L_SUPPKEY     INTEGER DEFAULT 100;
ALTER TABLE LINEITEM_HYBRID_PART ADD COLUMN    L_QUANTITY    DECIMAL(15,2) DEFAULT 100.00;
ALTER TABLE LINEITEM_HYBRID_PART ADD COLUMN    L_EXTENDEDPRICE  DECIMAL(15,2) DEFAULT 100.00;
ALTER TABLE LINEITEM_HYBRID_PART ADD COLUMN    L_TAX         DECIMAL(15,2) DEFAULT 100.00;
ALTER TABLE LINEITEM_HYBRID_PART ADD COLUMN    L_RETURNFLAG  CHAR(1) DEFAULT 'N';
ALTER TABLE LINEITEM_HYBRID_PART ADD COLUMN    L_LINESTATUS  CHAR(1) DEFAULT 'O';
ALTER TABLE LINEITEM_HYBRID_PART ADD COLUMN    L_SHIPDATE    DATE DEFAULT '1996-03-13';
ALTER TABLE LINEITEM_HYBRID_PART ADD COLUMN    L_COMMITDATE  DATE DEFAULT '1996-03-13';
ALTER TABLE LINEITEM_HYBRID_PART ADD COLUMN    L_SHIPINSTRUCT CHAR(25) DEFAULT 'DELIVER IN PERSON';
ALTER TABLE LINEITEM_HYBRID_PART ADD COLUMN    L_SHIPMODE     CHAR(10) DEFAULT 'TRUCK';
ALTER TABLE LINEITEM_HYBRID_PART ADD COLUMN    L_COMMENT      VARCHAR(44) DEFAULT 'regular';
\d+ LINEITEM_HYBRID_PART
insert into nation_hybrid_part ( N_NATIONKEY ,N_NAME ,N_REGIONKEY ,N_COMMENT ) select N_NATIONKEY ,N_NAME ,N_REGIONKEY   ,N_COMMENT  from nation;

insert into region_hybrid_part(R_REGIONKEY,R_NAME,R_COMMENT) select R_REGIONKEY,R_NAME,R_COMMENT from region;

insert into part_hybrid_part( P_PARTKEY,P_NAME ,P_MFGR,P_BRAND, P_TYPE,P_SIZE, P_CONTAINER ,P_RETAILPRICE  ,P_COMMENT) select P_PARTKEY,P_NAME,P_MFGR,P_BRAND, P_TYPE,P_SIZE,P_CONTAINER,P_RETAILPRICE,P_COMMENT    from part;

insert into supplier_hybrid_part ( S_SUPPKEY ,S_NAME ,S_ADDRESS,S_NATIONKEY ,S_PHONE ,S_ACCTBAL ,S_COMMENT ) select     S_SUPPKEY,S_NAME ,S_ADDRESS ,S_NATIONKEY  ,S_PHONE ,S_ACCTBAL ,S_COMMENT from supplier;

insert into partsupp_hybrid_part ( PS_PARTKEY ,PS_SUPPKEY,PS_AVAILQTY ,PS_SUPPLYCOST ,PS_COMMENT ) select    PS_PARTKEY ,PS_SUPPKEY ,PS_AVAILQTY ,PS_SUPPLYCOST ,PS_COMMENT from partsupp;

insert into customer_hybrid_part ( C_CUSTKEY,C_NAME,C_ADDRESS ,C_NATIONKEY,C_PHONE ,C_ACCTBAL,C_MKTSEGMENT  ,C_COMMENT) select C_CUSTKEY ,C_NAME ,C_ADDRESS ,C_NATIONKEY,C_PHONE ,C_ACCTBAL ,C_MKTSEGMENT ,C_COMMENT from customer;

insert into orders_hybrid_part ( O_ORDERKEY ,O_CUSTKEY ,O_ORDERSTATUS ,O_TOTALPRICE,O_ORDERDATE ,             O_ORDERPRIORITY   ,O_CLERK ,O_SHIPPRIORITY ,O_COMMENT) select  O_ORDERKEY ,O_CUSTKEY ,O_ORDERSTATUS ,       O_TOTALPRICE  ,O_ORDERDATE  ,O_ORDERPRIORITY   ,O_CLERK  ,O_SHIPPRIORITY  ,O_COMMENT  from orders;

insert into lineitem_hybrid_part ( L_ORDERKEY ,L_PARTKEY ,L_SUPPKEY,L_LINENUMBER ,L_QUANTITY,L_EXTENDEDPRICE ,L_DISCOUNT ,L_TAX ,L_RETURNFLAG ,L_LINESTATUS ,L_SHIPDATE ,L_COMMITDATE,L_RECEIPTDATE ,L_SHIPINSTRUCT  ,    L_SHIPMODE ,L_COMMENT) select  L_ORDERKEY,L_PARTKEY,L_SUPPKEY,L_LINENUMBER,L_QUANTITY,L_EXTENDEDPRICE   ,     L_DISCOUNT,L_TAX ,L_RETURNFLAG,L_LINESTATUS,L_SHIPDATE ,L_COMMITDATE  ,L_RECEIPTDATE ,L_SHIPINSTRUCT  ,      L_SHIPMODE ,L_COMMENT from lineitem;
