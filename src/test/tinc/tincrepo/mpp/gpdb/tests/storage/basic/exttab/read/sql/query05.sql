-- start_ignore
drop external table if exists ext_lineitem;
-- end_ignore
CREATE EXTERNAL web TABLE EXT_LINEITEM ( L_ORDERKEY    INTEGER ,
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
execute '(cat @gpfdist_datadir@/lineitem.tbl)' on segment 0
FORMAT 'text' (delimiter '|');


select count(*) from ext_lineitem;
select count(*) from (select * from ext_lineitem limit 10) as foo;
select * from ext_lineitem limit 10;

-- start_ignore
drop external table if exists ext_lineitem;
-- end_ignore
