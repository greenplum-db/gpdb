-- MPP-7661
-- create, dump, drop, recreate table
-- Johnny Soedomo
CREATE TABLE MPP_7661_LINEITEM (
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
partition by range (l_commitdate) 
(
partition p1 start('1992-01-31') end('1998-11-01') every(interval '13 months')

);

select * from pg_partitions where tablename='mpp_7661_lineitem' order by partitionposition;
\! pg_dump --no-owner -d @DBNAME@ -t mpp_7661_lineitem -f @out_dir@/mpp7661-dump.out
drop table mpp_7661_lineitem;

-- pg_dump seems to backup all the trusted protocols in the schema as well even with -t option even if the table 
-- does not depend on those protocols. Hence this test keeps failing if there are trusted protocols in the schema
-- before doing the pg_dump above. Hence ignoring the following and making sure the required table gets restored after.

-- start_ignore
\i @out_dir@/mpp7661-dump.out
-- end_ignore

select * from pg_partitions where tablename='mpp_7661_lineitem' order by partitionposition;

drop table mpp_7661_lineitem;
