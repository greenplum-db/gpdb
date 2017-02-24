--start_ignore
drop database if exists mpph_co;
create database mpph_co;
\c mpph_co
drop table if exists customer;
drop table if exists lineitem;
drop table if exists nation;
drop table if exists orders;
drop table if exists part;
drop table if exists partsupp;
drop table if exists region;
drop table if exists supplier;
--end_ignore

CREATE TABLE customer (
    c_custkey integer NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    c_name character varying(25) NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    c_address character varying(40) NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    c_nationkey integer NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    c_phone character(15) NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    c_acctbal numeric(15,2) NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    c_mktsegment character(10) NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    c_comment character varying(117) NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0)
)
WITH (appendonly=true, orientation=column) DISTRIBUTED BY (c_custkey);

CREATE TABLE lineitem (
    l_orderkey bigint NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    l_partkey integer NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    l_suppkey integer NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    l_linenumber integer NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    l_quantity numeric(15,2) NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    l_extendedprice numeric(15,2) NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    l_discount numeric(15,2) NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    l_tax numeric(15,2) NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    l_returnflag character(1) NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    l_linestatus character(1) NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    l_shipdate date NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    l_commitdate date NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    l_receiptdate date NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    l_shipinstruct character(25) NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    l_shipmode character(10) NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    l_comment character varying(44) NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0)
)
WITH (appendonly=true, orientation=column) DISTRIBUTED BY (l_orderkey);

CREATE TABLE nation (
    n_nationkey integer ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    n_name character(25) ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    n_regionkey integer ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    n_comment character varying(152) ENCODING (compresstype=none,blocksize=32768,compresslevel=0)
)
WITH (appendonly=true, orientation=column) DISTRIBUTED BY (n_nationkey);

CREATE TABLE orders (
    o_orderkey bigint NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    o_custkey integer NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    o_orderstatus character(1) NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    o_totalprice numeric(15,2) NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    o_orderdate date NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    o_orderpriority character(15) NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    o_clerk character(15) NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    o_shippriority integer NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    o_comment character varying(79) NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0)
)
WITH (appendonly=true, orientation=column) DISTRIBUTED BY (o_orderkey);

CREATE TABLE part (
    p_partkey integer NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    p_name character varying(55) NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    p_mfgr character(25) NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    p_brand character(10) NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    p_type character varying(25) NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    p_size integer NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    p_container character(10) NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    p_retailprice numeric(15,2) NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    p_comment character varying(23) NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0)
)
WITH (appendonly=true, orientation=column) DISTRIBUTED BY (p_partkey);


CREATE TABLE partsupp (
    ps_partkey integer NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    ps_suppkey integer NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    ps_availqty integer NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    ps_supplycost numeric(15,2) NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    ps_comment character varying(199) NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0)
)
WITH (appendonly=true, orientation=column) DISTRIBUTED BY (ps_partkey);


CREATE TABLE region (
    r_regionkey integer NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    r_name character(25) NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    r_comment character varying(152) ENCODING (compresstype=none,blocksize=32768,compresslevel=0)
)
WITH (appendonly=true, orientation=column) DISTRIBUTED BY (r_regionkey);

CREATE TABLE supplier (
    s_suppkey integer NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    s_name character(25) NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    s_address character varying(40) NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    s_nationkey integer NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    s_phone character(15) NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    s_acctbal numeric(15,2) NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0),
    s_comment character varying(101) NOT NULL ENCODING (compresstype=none,blocksize=32768,compresslevel=0)
)
WITH (appendonly=true, orientation=column) DISTRIBUTED BY (s_suppkey);



\copy customer (C_CUSTKEY,C_NAME,C_ADDRESS,C_NATIONKEY,C_PHONE,C_ACCTBAL,C_MKTSEGMENT,C_COMMENT) from 'data/customer.csv' with delimiter '|';

\copy lineitem ( L_ORDERKEY, L_PARTKEY, L_SUPPKEY,L_LINENUMBER,L_QUANTITY, L_EXTENDEDPRICE,L_DISCOUNT,L_TAX,L_RETURNFLAG,L_LINESTATUS,L_SHIPDATE,L_COMMITDATE,L_RECEIPTDATE,L_SHIPINSTRUCT,L_SHIPMODE,L_COMMENT) from 'data/lineitem.csv' with delimiter '|'; 

\copy nation (N_NATIONKEY ,N_NAME, N_REGIONKEY,N_COMMENT) from 'data/nation.csv' with delimiter '|';

\copy orders ( O_ORDERKEY,O_CUSTKEY,O_ORDERSTATUS,O_TOTALPRICE,O_ORDERDATE,O_ORDERPRIORITY,O_CLERK,O_SHIPPRIORITY,O_COMMENT) from 'data/order.csv' with delimiter '|'; 

\copy part (P_PARTKEY,P_NAME,P_MFGR,P_BRAND,P_TYPE,P_SIZE,P_CONTAINER,P_RETAILPRICE,P_COMMENT) from 'data/part.csv' with delimiter '|'; 

\copy partsupp (PS_PARTKEY,PS_SUPPKEY,PS_AVAILQTY,PS_SUPPLYCOST,PS_COMMENT ) from 'data/partsupp.csv' with delimiter '|';

\copy region ( R_REGIONKEY,R_NAME,R_COMMENT) from 'data/region.csv' with delimiter '|';

\copy supplier (S_SUPPKEY,S_NAME,S_ADDRESS,S_NATIONKEY,S_PHONE,S_ACCTBAL,S_COMMENT) from 'data/supplier.csv' with delimiter '|';

