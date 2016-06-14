--Generate 

drop table if exists customer;
drop table if exists lineitem;
drop table if exists nation;
drop table if exists orders;
drop table if exists part;
drop table if exists partsupp;
drop table if exists region;
drop table if exists supplier;

CREATE TABLE customer (
    c_custkey integer NOT NULL,
    c_name character varying(25) NOT NULL,
    c_address character varying(40) NOT NULL,
    c_nationkey integer NOT NULL,
    c_phone character(15) NOT NULL,
    c_acctbal numeric(15,2) NOT NULL,
    c_mktsegment character(10) NOT NULL,
    c_comment character varying(117) NOT NULL
)
WITH (appendonly=false) DISTRIBUTED BY (c_custkey);

CREATE TABLE lineitem (
    l_orderkey bigint NOT NULL,
    l_partkey integer NOT NULL,
    l_suppkey integer NOT NULL,
    l_linenumber integer NOT NULL,
    l_quantity numeric(15,2) NOT NULL,
    l_extendedprice numeric(15,2) NOT NULL,
    l_discount numeric(15,2) NOT NULL,
    l_tax numeric(15,2) NOT NULL,
    l_returnflag character(1) NOT NULL,
    l_linestatus character(1) NOT NULL,
    l_shipdate date NOT NULL,
    l_commitdate date NOT NULL,
    l_receiptdate date NOT NULL,
    l_shipinstruct character(25) NOT NULL,
    l_shipmode character(10) NOT NULL,
    l_comment character varying(44) NOT NULL
)
WITH (appendonly=false) DISTRIBUTED BY (l_orderkey);

CREATE TABLE nation (
    n_nationkey integer,
    n_name character(25),
    n_regionkey integer,
    n_comment character varying(152)
)
WITH (appendonly=false) DISTRIBUTED BY (n_nationkey);

CREATE TABLE orders (
    o_orderkey bigint NOT NULL,
    o_custkey integer NOT NULL,
    o_orderstatus character(1) NOT NULL,
    o_totalprice numeric(15,2) NOT NULL,
    o_orderdate date NOT NULL,
    o_orderpriority character(15) NOT NULL,
    o_clerk character(15) NOT NULL,
    o_shippriority integer NOT NULL,
    o_comment character varying(79) NOT NULL
)
WITH (appendonly=false) DISTRIBUTED BY (o_orderkey);

CREATE TABLE part (
    p_partkey integer NOT NULL,
    p_name character varying(55) NOT NULL,
    p_mfgr character(25) NOT NULL,
    p_brand character(10) NOT NULL,
    p_type character varying(25) NOT NULL,
    p_size integer NOT NULL,
    p_container character(10) NOT NULL,
    p_retailprice numeric(15,2) NOT NULL,
    p_comment character varying(23) NOT NULL
)
WITH (appendonly=false) DISTRIBUTED BY (p_partkey);


CREATE TABLE partsupp (
    ps_partkey integer NOT NULL,
    ps_suppkey integer NOT NULL,
    ps_availqty integer NOT NULL,
    ps_supplycost numeric(15,2) NOT NULL,
    ps_comment character varying(199) NOT NULL
)
WITH (appendonly=false) DISTRIBUTED BY (ps_partkey);


CREATE TABLE region (
    r_regionkey integer NOT NULL,
    r_name character(25) NOT NULL,
    r_comment character varying(152)
)
WITH (appendonly=false) DISTRIBUTED BY (r_regionkey);

CREATE TABLE supplier (
    s_suppkey integer NOT NULL,
    s_name character(25) NOT NULL,
    s_address character varying(40) NOT NULL,
    s_nationkey integer NOT NULL,
    s_phone character(15) NOT NULL,
    s_acctbal numeric(15,2) NOT NULL,
    s_comment character varying(101) NOT NULL
)
WITH (appendonly=false) DISTRIBUTED BY (s_suppkey);



\copy customer (C_CUSTKEY,C_NAME,C_ADDRESS,C_NATIONKEY,C_PHONE,C_ACCTBAL,C_MKTSEGMENT,C_COMMENT) from '../data/customer.csv' with delimiter '|';

\copy lineitem ( L_ORDERKEY, L_PARTKEY, L_SUPPKEY,L_LINENUMBER,L_QUANTITY, L_EXTENDEDPRICE,L_DISCOUNT,L_TAX,L_RETURNFLAG,L_LINESTATUS,L_SHIPDATE,L_COMMITDATE,L_RECEIPTDATE,L_SHIPINSTRUCT,L_SHIPMODE,L_COMMENT) from '../data/lineitem.csv' with delimiter '|'; 

\copy nation (N_NATIONKEY ,N_NAME, N_REGIONKEY,N_COMMENT) from '../data/nation.csv' with delimiter '|';

\copy orders ( O_ORDERKEY,O_CUSTKEY,O_ORDERSTATUS,O_TOTALPRICE,O_ORDERDATE,O_ORDERPRIORITY,O_CLERK,O_SHIPPRIORITY,O_COMMENT) from '../data/order.csv' with delimiter '|'; 

\copy part (P_PARTKEY,P_NAME,P_MFGR,P_BRAND,P_TYPE,P_SIZE,P_CONTAINER,P_RETAILPRICE,P_COMMENT)from '../data/part.csv' with delimiter '|'; 

\copy partsupp (PS_PARTKEY,PS_SUPPKEY,PS_AVAILQTY,PS_SUPPLYCOST,PS_COMMENT ) from '../data/partsupp.csv' with delimiter '|';

\copy region ( R_REGIONKEY,R_NAME,R_COMMENT) from '../data/region.csv' with delimiter '|';

\copy supplier (S_SUPPKEY,S_NAME,S_ADDRESS,S_NATIONKEY,S_PHONE,S_ACCTBAL,S_COMMENT) from '../data/supplier.csv' with delimiter '|';

