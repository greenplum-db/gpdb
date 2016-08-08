CREATE TABLE NATION  ( N_NATIONKEY  INTEGER NOT NULL,
                            N_NAME       CHAR(25) NOT NULL,
                            N_REGIONKEY  INTEGER NOT NULL,
                            N_COMMENT    VARCHAR(152));

CREATE TABLE REGION  ( R_REGIONKEY  INTEGER NOT NULL,
                            R_NAME       CHAR(25) NOT NULL,
                            R_COMMENT    VARCHAR(152));

CREATE TABLE PART  ( P_PARTKEY     INTEGER NOT NULL,
                          P_NAME        VARCHAR(55) NOT NULL,
                          P_MFGR        CHAR(25) NOT NULL,
                          P_BRAND       CHAR(10) NOT NULL,
                          P_TYPE        VARCHAR(25) NOT NULL,
                          P_SIZE        INTEGER NOT NULL,
                          P_CONTAINER   CHAR(10) NOT NULL,
                          P_RETAILPRICE DECIMAL(15,2) NOT NULL,
                          P_COMMENT     VARCHAR(23) NOT NULL );

CREATE TABLE SUPPLIER ( S_SUPPKEY     INTEGER NOT NULL,
                             S_NAME        CHAR(25) NOT NULL,
                             S_ADDRESS     VARCHAR(40) NOT NULL,
                             S_NATIONKEY   INTEGER NOT NULL,
                             S_PHONE       CHAR(15) NOT NULL,
                             S_ACCTBAL     DECIMAL(15,2) NOT NULL,
                             S_COMMENT     VARCHAR(101) NOT NULL);
CREATE TABLE PARTSUPP ( PS_PARTKEY     INTEGER NOT NULL,
                             PS_SUPPKEY     INTEGER NOT NULL,
                             PS_AVAILQTY    INTEGER NOT NULL,
                             PS_SUPPLYCOST  DECIMAL(15,2)  NOT NULL,
                             PS_COMMENT     VARCHAR(199) NOT NULL );

CREATE TABLE CUSTOMER ( C_CUSTKEY     INTEGER NOT NULL,
                             C_NAME        VARCHAR(25) NOT NULL,
                             C_ADDRESS     VARCHAR(40) NOT NULL,
                             C_NATIONKEY   INTEGER NOT NULL,
                             C_PHONE       CHAR(15) NOT NULL,
                             C_ACCTBAL     DECIMAL(15,2)   NOT NULL,
                             C_MKTSEGMENT  CHAR(10) NOT NULL,
                             C_COMMENT     VARCHAR(117) NOT NULL);

CREATE TABLE ORDERS  ( O_ORDERKEY       INTEGER NOT NULL,
                           O_CUSTKEY        INTEGER NOT NULL,
                           O_ORDERSTATUS    CHAR(1) NOT NULL,
                           O_TOTALPRICE     DECIMAL(15,2) NOT NULL,
                           O_ORDERDATE      DATE NOT NULL,
                           O_ORDERPRIORITY  CHAR(15) NOT NULL,
                           O_CLERK          CHAR(15) NOT NULL,
                           O_SHIPPRIORITY   INTEGER NOT NULL,
                           O_COMMENT        VARCHAR(79) NOT NULL);

CREATE TABLE LINEITEM ( L_ORDERKEY    INTEGER NOT NULL,
                             L_PARTKEY     INTEGER NOT NULL,
                             L_SUPPKEY     INTEGER NOT NULL,
                             L_LINENUMBER  INTEGER NOT NULL,
                             L_QUANTITY    DECIMAL(15,2) NOT NULL,
                             L_EXTENDEDPRICE  DECIMAL(15,2) NOT NULL,
                             L_DISCOUNT    DECIMAL(15,2) NOT NULL,
                             L_TAX         DECIMAL(15,2) NOT NULL,
                             L_RETURNFLAG  CHAR(1) NOT NULL,
                             L_LINESTATUS  CHAR(1) NOT NULL,
                             L_SHIPDATE    DATE NOT NULL,
                             L_COMMITDATE  DATE NOT NULL,
                             L_RECEIPTDATE DATE NOT NULL,
                             L_SHIPINSTRUCT CHAR(25) NOT NULL,
                             L_SHIPMODE     CHAR(10) NOT NULL,
                             L_COMMENT      VARCHAR(44) NOT NULL);

CREATE TABLE partlist
( pid INTEGER UNIQUE,
  plist TEXT NOT NULL,
  pval TEXT
);


COPY CUSTOMER FROM '/home/jsoedomo/perforce/cdbfast/main/partition/_data/customer.tbl' delimiter '|';
COPY NATION FROM '/home/jsoedomo/perforce/cdbfast/main/partition/_data/nation.tbl' delimiter '|';
COPY LINEITEM FROM '/home/jsoedomo/perforce/cdbfast/main/partition/_data/lineitem.tbl' delimiter '|';
COPY PART FROM '/home/jsoedomo/perforce/cdbfast/main/partition/_data/part.tbl' delimiter '|';
COPY PARTSUPP FROM '/home/jsoedomo/perforce/cdbfast/main/partition/_data/partsupp.tbl' delimiter '|';
COPY ORDERS FROM '/home/jsoedomo/perforce/cdbfast/main/partition/_data/orders.tbl' delimiter '|';
COPY REGION FROM '/home/jsoedomo/perforce/cdbfast/main/partition/_data/region.tbl' delimiter '|';
COPY SUPPLIER FROM '/home/jsoedomo/perforce/cdbfast/main/partition/_data/supplier.tbl' delimiter '|';

INSERT INTO partlist VALUES (1,'A','AAAAA');
INSERT INTO partlist VALUES (2,'A','AAAAA');
INSERT INTO partlist VALUES (3,'A','AAAAA');
INSERT INTO partlist VALUES (4,'A','AAAAA');
INSERT INTO partlist VALUES (5,'B','AAAAA');
INSERT INTO partlist VALUES (6,'B','AAAAA');
INSERT INTO partlist VALUES (7,'B','AAAAA');
INSERT INTO partlist VALUES (8,'B','AAAAA');
INSERT INTO partlist VALUES (9,'C','AAAAA');
INSERT INTO partlist VALUES (10,'C','AAAAA');
INSERT INTO partlist VALUES (11,'C','AAAAA');
INSERT INTO partlist VALUES (12,'C','AAAAA');
INSERT INTO partlist VALUES (13,'C','AAAAA');
INSERT INTO partlist VALUES (14,'C','AAAAA');
INSERT INTO partlist VALUES (15,'C','AAAAA');

select count(*) from customer;
select count(*) from nation;
select count(*) from lineitem;
select count(*) from part;
select count(*) from partsupp;
select count(*) from orders;
select count(*) from region;
select count(*) from supplier;
select count(*) from partlist;

-- Make sure that MASTER_DATA_DIRECTORY is set first, requirement for using gpcreatepart
\! gpcreatepart -a -t public.orders -f o_orderdate -p d -c y -s '1990-01-01' -e '2000-12-31' -n 10 -x gptest
\! gpcreatepart -a -t public.lineitem -f l_shipdate -p d -c y -s '1990-01-01' -e '2000-12-31' -n 10 -x gptest
\! gpcreatepart -a -t public.customer -f c_nationkey -p r -s 0 -e 25 -g 5 -x gptest
\! gpcreatepart -a -t public.partlist -f plist -p l -o A,B,C -x gptest

-- There is an existing INDEX for the table, what happen after upgrade for new enhanced partition?
CREATE INDEX lineitem_shipdate ON lineitem(l_shipdate);
CREATE INDEX lineitem_p1 ON lineitem_c_d_1_y_1_16536_year(l_shipdate);
CREATE INDEX lineitem_p2 ON lineitem_c_d_1_y_2_16536_year(l_shipdate);
CREATE INDEX lineitem_p3 ON lineitem_c_d_1_y_3_16536_year(l_shipdate);
CREATE INDEX lineitem_p4 ON lineitem_c_d_1_y_4_16536_year(l_shipdate);
CREATE INDEX lineitem_p5 ON lineitem_c_d_1_y_5_16536_year(l_shipdate);
CREATE INDEX lineitem_p6 ON lineitem_c_d_1_y_6_16536_year(l_shipdate);
CREATE INDEX lineitem_p7 ON lineitem_c_d_1_y_7_16536_year(l_shipdate);
CREATE INDEX lineitem_p8 ON lineitem_c_d_1_y_8_16536_year(l_shipdate);
CREATE INDEX lineitem_p9 ON lineitem_c_d_1_y_9_16536_year(l_shipdate);
CREATE INDEX lineitem_p10 ON lineitem_c_d_1_y_10_16536_year(l_shipdate);

ANALYZE;

\d

-- Run Upgrade for 3.2 with a script? or gpmigrator?
-- Should use the new enhance partition
-- \d
-- select count(*) from pg_partition;
select count(*) from customer;
select count(*) from nation;
select count(*) from lineitem;
select count(*) from part;
select count(*) from partsupp;
select count(*) from orders;
select count(*) from region;
select count(*) from supplier;
select count(*) from partlist;
