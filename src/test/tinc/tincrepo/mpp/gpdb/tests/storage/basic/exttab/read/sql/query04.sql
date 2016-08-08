--
-- table splitting
--

--
-- look at lineitem 3 ways, and check that always get the same answer
--
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
location ('file://@hostname@@gpfdist_datadir@/lineitem.tbl' )
FORMAT 'text' (delimiter '|');


-- start_ignore
drop external web table if exists ext_lineitem1;
-- end_ignore
CREATE EXTERNAL WEB TABLE EXT_LINEITEM1 ( L_ORDERKEY    INTEGER ,
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
execute E'(@gpwhich_curl@ http://@hostname@:@gp_port@/lineitem.tbl-dir) '
on SEGMENT 0
FORMAT 'text' (delimiter '|');

-- start_ignore
drop external web table if exists ext_lineitem2;
-- end_ignore
CREATE EXTERNAL WEB TABLE EXT_LINEITEM2 ( L_ORDERKEY    INTEGER ,
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
execute E'(@gpwhich_curl@ http://@hostname@:@gp_port@/lineitem.tbl-dir/x*) '
on SEGMENT 0
FORMAT 'text' (delimiter '|');


select count(*) from ext_lineitem;
select count(*) from ext_lineitem1;
select count(*) from ext_lineitem2;
select * from ext_lineitem except select * from ext_lineitem1;
select * from ext_lineitem except select * from ext_lineitem2;
