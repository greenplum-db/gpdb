\echo --start_ignore
drop external table rescan_lineitem;
drop external table rescan_orders;
drop external table rescan_lineitem_writeHDFS;
drop external table rescan_lineitem_readHDFS;
drop external table rescan_orders_writeHDFS;
drop external table rescan_orders_readHDFS;
\echo --end_ignore



create readable external table rescan_lineitem(
L_ORDERKEY    INTEGER ,
L_PARTKEY     INTEGER ,
L_SUPPKEY INTEGER ,
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
L_COMMENT      VARCHAR(44) 
) location ('gphdfs://%HADOOP_HOST%/plaintext/lineitem.txt')format 'text' (delimiter as '|');

create writable external table rescan_lineitem_writeHDFS (like rescan_lineitem) location ('gphdfs://%HADOOP_HOST%/extwrite/lineitem') format 'custom' (formatter='gphdfs_export');
insert into rescan_lineitem_writeHDFS select * from rescan_lineitem;

create readable external table rescan_lineitem_readHDFS (like rescan_lineitem) location ('gphdfs://%HADOOP_HOST%/extwrite/lineitem') format 'custom' (formatter='gphdfs_import');


create readable external table rescan_orders(
O_ORDERKEY       INTEGER ,
O_CUSTKEY        INTEGER ,
O_ORDERSTATUS CHAR(1) ,
O_TOTALPRICE     DECIMAL(15,2),
O_ORDERDATE      DATE ,
O_ORDERPRIORITY  CHAR(15) ,
O_CLERK          CHAR(15) ,
O_SHIPPRIORITY   INTEGER ,
O_COMMENT        VARCHAR(79) 
) location ('gphdfs://%HADOOP_HOST%/plaintext/orders.txt')format 'text' (delimiter as '|');

create writable external table rescan_orders_writeHDFS (like rescan_orders ) location ('gphdfs://%HADOOP_HOST%/extwrite/orders') format 'custom' (formatter='gphdfs_export');
insert into rescan_orders_writeHDFS select * from rescan_orders;

create readable external table rescan_orders_readHDFS (like rescan_orders ) location ('gphdfs://%HADOOP_HOST%/extwrite/orders') format 'custom' (formatter='gphdfs_import');

select * from rescan_lineitem order by l_partkey;
select * from rescan_orders order by o_custkey;
select * from rescan_lineitem_readHDFS order by l_partkey;
select * from rescan_orders_readHDFS order by o_custkey;

select count(rescan_lineitem.*) from rescan_lineitem,rescan_orders where l_extendedprice < o_totalprice;
select count(rescan_lineitem_readHDFS.*) from rescan_lineitem_readHDFS,rescan_orders_readHDFS where l_extendedprice < o_totalprice;
