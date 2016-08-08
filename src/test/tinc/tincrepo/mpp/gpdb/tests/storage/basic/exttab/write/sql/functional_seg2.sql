--
-- This test case will be skipped if # of primary is < 2
--
CREATE EXTERNAL TABLE E_LINEITEM ( L_ORDERKEY    INT8 ,
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
                              LOCATION ('file://@hostname@/@tincrepohome@/mpp/lib/datagen/datasets/lineitem.csv')
                              FORMAT 'CSV' (DELIMITER '|');

CREATE WRITABLE EXTERNAL TABLE tbl_wet_lineitem1 ( like e_lineitem) LOCATION ('gpfdist://@hostname@:@gp_port@/output/new_lineitem.tbl') FORMAT 'TEXT' (DELIMITER AS '|');

INSERT INTO tbl_wet_lineitem1 SELECT * FROM e_lineitem limit 1000;
DROP TABLE IF EXISTS lineitem;
DROP EXTERNAL TABLE IF EXISTS e_lineitem;

CREATE TABLE LINEITEM ( L_ORDERKEY    INT8 NOT NULL,
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
                              L_COMMENT      VARCHAR(44) NOT NULL) WITH (orientation='column', appendonly=true);


CREATE EXTERNAL TABLE E_LINEITEM ( L_ORDERKEY    INT8 ,
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
                              LOCATION ('file://@hostname@/@tincrepohome@/mpp/lib/datagen/datasets/lineitem.csv')
                              FORMAT 'CSV' (DELIMITER '|');

INSERT INTO lineitem SELECT * FROM e_lineitem limit 1000; 

CREATE WRITABLE EXTERNAL TABLE tbl_wet_lineitem2 ( like lineitem) location ('gpfdist://@hostname@:@gp_port@/output/new_lineitem.tbl') FORMAT 'TEXT' (DELIMITER AS '|');

INSERT INTO tbl_wet_lineitem2 SELECT * FROM lineitem limit 1000;

-- The use of more than one gpfdist. Each gpfdist points to a different file to write into

create table test_a ( a int, b text) distributed by (a);

insert into test_a values (generate_series(1,5),'test_1');

CREATE WRITABLE EXTERNAL TABLE tbl_wet_2gpfdist ( a int, b text) LOCATION ('gpfdist://@hostname@:@gp_port@/output/wet_2gpfdist1.tbl', 'gpfdist://@hostname@:@gp_port@/output/wet_2gpfdist2.tbl') FORMAT 'TEXT' (DELIMITER AS '|' NULL AS 'null'  ) ;

insert into tbl_wet_2gpfdist select * from test_a;    


