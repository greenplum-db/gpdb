-- @product_version gpdb: [4.3.6.0-]

-- Ensure that external tables behave correcly when default storage
-- options are set.

create schema dsp_ext;
set search_path=dsp_ext;

set gp_default_storage_options='appendonly=false';
--Table with all data types
    CREATE TABLE all_types( bit1 bit(1), bit2 bit varying(50), boolean1 boolean, char1 char(50), charvar1 character varying(50), char2 character(50),
varchar1 varchar(50),date1 date,dp1 double precision,int1 integer,interval1 interval,numeric1 numeric,real1 real,smallint1 smallint,time1 time,bigint1 bigint,
bigserial1 bigserial,bytea1 bytea,cidr1 cidr,decimal1 decimal,inet1 inet,macaddr1 macaddr,money1 money,serial1 serial,text1 text,time2 time without time zone,time3 time with time zone,time4 timestamp without time zone,time5 timestamp with time zone) distributed randomly;

insert into all_types values ('1','0','t','c','varchar1','char1','varchar1','2001-11-11',234.23234,23,'24',234,23,4,'12:12:12',2,3,'d','0.0.0.0',1,'0.0.0.0','AA:AA:AA:AA:AA:AA','34.23',5,'text1','00:00:00','00:00:00+1359','2001-12-13 01:51:15','2001-12-13 01:51:15+1359');

insert into all_types values ('0','0','f','c','varchar2','char2','varchar2','2002-11-11',234.23234,23,'24',234,23,4,'12:12:12',2,3,'d','0.0.0.0',1,'0.0.0.0','BB:BB:BB:BB:BB:BB','34.23',5,'text2','00:00:00','00:00:00+1359','2002-12-13 01:51:15','2002-12-13 01:51:15+1359');

insert into all_types values ('1','0','t','c','varchar3','char3','varchar3','2003-11-11',234.23234,23,'24',234,23,4,'12:12:12',2,3,'d','0.0.0.0',1,'0.0.0.0','CC:CC:CC:CC:CC:CC','34.23',5,'text3','00:00:00','00:00:00+1359','2003-12-13 01:51:15','2003-12-13 01:51:15+1359');

insert into all_types values ('0','0','f','c','varchar4','char4','varchar4','2004-11-11',234.23234,23,'24',234,23,4,'12:12:12',2,3,'d','0.0.0.0',1,'0.0.0.0','DD:DD:DD:DD:DD:DD','34.23',5,'text4','00:00:00','00:00:00+1359','2004-12-13 01:51:15','2004-12-13 01:51:15+1359');

insert into all_types values ('1','0','t','c','varchar5','char5','varchar5','2005-11-11',234.23234,23,'24',234,23,4,'12:12:12',2,3,'d','0.0.0.0',1,'0.0.0.0','EE:EE:EE:EE:EE:EE','34.23',5,'text5','00:00:00','00:00:00+1359','2005-12-13 01:51:15','2005-12-13 01:51:15+1359');


CREATE WRITABLE EXTERNAL TABLE tbl_wet_all_types ( like all_types) LOCATION ('gpfdist://@hostname@:@gp_port@/output/wet_alltypes_dsp.tbl') FORMAT 'TEXT' (DELIMITER AS '|');

INSERT INTO tbl_wet_all_types SELECT * FROM all_types;

-- AO table
set gp_default_storage_options='appendonly=true';
create table ao_table ( a int, b text) distributed by (a);

insert into ao_table values (1,'test_1');
insert into ao_table values (2,'test_2');
insert into ao_table values (3,'test_3');

CREATE WRITABLE EXTERNAL TABLE tbl_wet_ao ( like ao_table) LOCATION ('gpfdist://@hostname@:@gp_port@/output/wet_ao_dsp.tbl') FORMAT 'TEXT' (DELIMITER AS '|');

\d+ tbl_wet_ao

INSERT INTO tbl_wet_ao SELECT * FROM ao_table;

-- CO table
set gp_default_storage_options='appendonly=true, orientation=column';
create table co_table ( a int, b text) distributed by (a);

insert into co_table values (1,'test_1');
insert into co_table values (2,'test_2');
insert into co_table values (3,'test_3');

CREATE WRITABLE EXTERNAL TABLE tbl_wet_co ( like co_table) LOCATION ('gpfdist://@hostname@:@gp_port@/output/wet_co_dsp.tbl') FORMAT 'TEXT' (DELIMITER AS '|');

INSERT INTO tbl_wet_co SELECT * FROM co_table;

reset gp_default_storage_options;
CREATE TABLE table_with_default_constraint (
    col_with_default_numeric numeric DEFAULT 0,
    col_with_default_text character varying(30) DEFAULT 'test1',
    col_with_constraint numeric UNIQUE
) DISTRIBUTED BY (col_with_constraint);

INSERT INTO table_with_default_constraint  (col_with_default_text,col_with_constraint) VALUES('0_zero',0);
INSERT INTO table_with_default_constraint  VALUES(11,'1_zero',1);
INSERT INTO table_with_default_constraint  (col_with_default_numeric,col_with_constraint) VALUES (33,3);
INSERT INTO table_with_default_constraint  (col_with_default_text,col_with_constraint) VALUES('2_zero',2);
INSERT INTO table_with_default_constraint  VALUES(12,'3_zero',5);
INSERT INTO table_with_default_constraint  (col_with_default_numeric,col_with_constraint) VALUES (35,4);

set gp_default_storage_options='appendonly=true, orientation=column';
CREATE WRITABLE EXTERNAL TABLE tbl_wet_non_supported_sql ( like table_with_default_constraint) LOCATION ('gpfdist://@hostname@:@gp_port@/output/wet_non_supported_sql_dsp.tbl') FORMAT 'TEXT'  (DELIMITER '|' ) ;

INSERT INTO tbl_wet_non_supported_sql SELECT * FROM table_with_default_constraint;

--Testing update - should not work for wet

UPDATE tbl_wet_non_supported_sql SET col_with_constraint =99 WHERE col_with_constraint =33;

--Testing delete - should not work for wet

DELETE FROM tbl_wet_non_supported_sql WHERE col_with_constraint =33;

DELETE FROM tbl_wet_non_supported_sql;

--Testing truncate - should not work for wet

TRUNCATE  tbl_wet_non_supported_sql;

--Testing index - should not work for wet

CREATE INDEX test_index ON tbl_wet_non_supported_sql(col_with_constraint);

CREATE INDEX test_index ON table_with_default_constraint (col_with_constraint);

CREATE WRITABLE EXTERNAL TABLE tbl_wet_non_supported_sql_idx ( like table_with_default_constraint) LOCATION ('gpfdist://@hostname@:@gp_port@/output/wet_non_supported_sql_idx_dsp.tbl') FORMAT 'TEXT'  (DELIMITER '|' ) ;

CREATE INDEX test_index ON tbl_wet_non_supported_sql_idx (col_with_constraint);

-- Testing ctas

    CREATE TABLE table_constraint  (
    did integer,
    name varchar(40)
    CONSTRAINT con1 CHECK (did > 99 AND name <> '')
    )DISTRIBUTED RANDOMLY;

INSERT INTO table_constraint  VALUES (100,'name_1');
INSERT INTO table_constraint  VALUES (200,'name_2');
INSERT INTO table_constraint  VALUES (300,'name_3');

CREATE WRITABLE EXTERNAL TABLE tbl_wet ( like table_constraint) LOCATION ('gpfdist://@hostname@:@gp_port@/output/wet_ctas_dsp.tbl' ) FORMAT 'TEXT'  (DELIMITER '|' ) ;

CREATE TABLE tbl_wet_ctas AS SELECT * from tbl_wet_non_supported_sql;

--negative testing

-- selecting from WET should error

    CREATE TABLE table_distributed_randomly (
    text_col text,
    bigint_col bigint,
    char_vary_col character varying(30),
    numeric_col numeric
    ) DISTRIBUTED RANDOMLY;

INSERT INTO table_distributed_randomly VALUES ('0_zero', 0, '0_zero', 0);
INSERT INTO table_distributed_randomly VALUES ('1_zero', 1, '1_zero', 1);
INSERT INTO table_distributed_randomly VALUES ('2_zero', 2, '2_zero', 2);

CREATE WRITABLE EXTERNAL TABLE tbl_wet_select ( like table_distributed_randomly) LOCATION ('gpfdist://@hostname@:@gp_port@/output/wet_select_dsp.tbl' ) FORMAT 'TEXT'  (DELIMITER '|' ) ;

INSERT INTO tbl_wet_select SELECT * FROM table_distributed_randomly;

select * from tbl_wet_select;

-- Insert into WET doesnt match col's from the target table it should fail 

CREATE WRITABLE EXTERNAL TABLE tbl_wet_insert (text_col text, bigint_col bigint, char_vary_col character varying(30)) LOCATION ('gpfdist://@hostname@:@gp_port@/output/wet_insert_dsp.tbl' ) FORMAT 'TEXT'  (DELIMITER '|' ) ;

INSERT INTO tbl_wet_insert SELECT * FROM table_distributed_randomly;


-- create a normal heap table
CREATE TABLE REGION_1   ( R_REGIONKEY  INTEGER NOT NULL,
                            R_NAME       CHAR(25) NOT NULL,
                            R_COMMENT    VARCHAR(152)) ; 


-- create a RET to load data into the normal heap table
CREATE external web TABLE e_REGION  ( R_REGIONKEY  INTEGER ,
                            R_NAME       CHAR(25) ,
                            R_COMMENT    VARCHAR(152))
                        execute 'echo "0|AFRICA|lar deposits. blithely final packages cajole. regular waters are final requests. regular accounts are according to
                                       1|AMERICA|hs use ironic, even requests. s
                                       2|ASIA|ges. thinly even pinto beans ca
                                       3|EUROPE|ly final courts cajole furiously final excuse
                                       4|MIDDLE EAST|uickly special accounts cajole carefully blithely close requests. carefully final asymptotes haggle furiousl"'
                        on 1 format 'text' (delimiter '|');

-- load data into the heap table selecting from RET
insert into region_1 select * from e_region;


-- create WET with similiar schema def as the original heap table 
CREATE WRITABLE EXTERNAL TABLE wet_region ( like region_1) LOCATION ('gpfdist://@hostname@:@gp_port@/output/wet_region_dsp.tbl') FORMAT 'TEXT' (DELIMITER AS '|');

-- insert data into the WET selecting from original table
INSERT INTO wet_region SELECT * FROM region_1;

-- create a RET reading data from the file created by WET
CREATE EXTERNAL TABLE ret_region ( like region_1) LOCATION ('gpfdist://@hostname@:@gp_port@/output/wet_region_dsp.tbl') FORMAT 'TEXT' (DELIMITER AS '|');

-- create second table with same schema def
CREATE TABLE region_2 (like region_1);

-- insert into the second table reading from the RET
INSERT INTO region_2 SELECT * FROM ret_region;

-- compare contents of original table vs the table loaded from ret which in turn took the data which was created by wet

select * from region_1 order by r_regionkey;

select * from region_2 order by r_regionkey;

    CREATE TABLE table_execute  (
    did integer,
    name varchar(40)
    CONSTRAINT con1 CHECK (did > 99 AND name <> '')
    )DISTRIBUTED RANDOMLY;

INSERT INTO table_execute  VALUES (100,'name_1');
INSERT INTO table_execute  VALUES (200,'name_2');
INSERT INTO table_execute  VALUES (300,'name_3');

CREATE WRITABLE EXTERNAL WEB TABLE tbl_wet_execute ( like table_execute) EXECUTE ' cat > wet_execute_dsp.tbl' FORMAT 'TEXT'  (DELIMITER '|' ) ;

INSERT INTO  tbl_wet_execute SELECT * from table_execute ;

--Table with all data types

    CREATE TABLE all_types_csv ( bit1 bit(1), bit2 bit varying(50), boolean1 boolean, char1 char(1), charvar1 character varying(50), char2 character(50),
varchar1 varchar(50),date1 date,dp1 double precision,int1 integer,interval1 interval,numeric1 numeric,real1 real,smallint1 smallint,time1 time,bigint1 bigint,
bigserial1 bigserial,bytea1 bytea,cidr1 cidr,decimal1 decimal,inet1 inet,macaddr1 macaddr,money1 money,serial1 serial,text1 text,time2 time without time zone,time3 time with time zone,time4 timestamp without time zone,time5 timestamp with time zone) distributed randomly;

insert into all_types_csv values ('1','0','t','a','varchar1','char1','varchar1','2001-11-11',234.23234,23,'24',234,23,4,'12:12:12',2,3,'d','10.1.2.10',1,'10.1.2.10','AA:AA:AA:AA:AA:AA','34.23',5,'text1','00:00:00','00:00:00+1359','2001-12-13 01:51:15','2001-12-13 01:51:15+1359');

insert into all_types_csv values ('0','0','f','b','varchar2','char2','varchar2','2002-11-11',234.23234,23,'24',234,23,4,'12:12:12',2,3,'d','10.1.2.10',1,'10.1.2.10','BB:BB:BB:BB:BB:BB','34.23',5,'text2','00:00:00','00:00:00+1359','2002-12-13 01:51:15','2002-12-13 01:51:15+1359');

insert into all_types_csv values ('1','0','t','c','varchar3','char3','varchar3','2003-11-11',234.23234,23,'24',234,23,4,'12:12:12',2,3,'d','10.1.2.10',1,'10.1.2.10','CC:CC:CC:CC:CC:CC','34.23',5,'text3','00:00:00','00:00:00+1359','2003-12-13 01:51:15','2003-12-13 01:51:15+1359');

insert into all_types_csv values ('0','0','f','d','varchar4','char4','varchar4','2004-11-11',234.23234,23,'24',234,23,4,'12:12:12',2,3,'d','10.1.2.10',1,'10.1.2.10','DD:DD:DD:DD:DD:DD','34.23',5,'text4','00:00:00','00:00:00+1359','2004-12-13 01:51:15','2004-12-13 01:51:15+1359');

insert into all_types_csv values ('1','0','t','e','varchar5','char5','varchar5','2005-11-11',234.23234,23,'24',234,23,4,'12:12:12',2,3,'d','10.1.2.10',1,'10.1.2.10','EE:EE:EE:EE:EE:EE','34.23',5,'text5','00:00:00','00:00:00+1359','2005-12-13 01:51:15','2005-12-13 01:51:15+1359');


-- with AS for DELIMITER , NULL, ESCAPE

CREATE WRITABLE EXTERNAL TABLE tbl_wet_csv1 (like all_types_csv)LOCATION ('gpfdist://@hostname@:@gp_port@/output/wet_csv_dsp.tbl') FORMAT 'CSV' (DELIMITER AS '|' NULL AS 'null' ESCAPE AS ' '   ) ;

-- without AS for DELIMITER , NULL, ESCAPE

CREATE WRITABLE EXTERNAL TABLE tbl_wet_csv2 (like all_types_csv)LOCATION ('gpfdist://@hostname@:@gp_port@/output/wet_csv_dsp.tbl') FORMAT 'CSV' (DELIMITER AS ',' NULL 'null' ESCAPE ' ');

-- with header

CREATE WRITABLE EXTERNAL TABLE tbl_wet_csv3 (like all_types_csv)LOCATION ('gpfdist://@hostname@:@gp_port@/output/wet_csv_dsp.tbl') FORMAT 'CSV' (DELIMITER AS '|' NULL AS 'null' ESCAPE AS ' '  HEADER ) ;

-- with double quotes

CREATE WRITABLE EXTERNAL TABLE tbl_wet_csv4 (like all_types_csv)LOCATION ('gpfdist://@hostname@:@gp_port@/output/wet_csv_dsp.tbl') FORMAT 'CSV' (DELIMITER AS '|' NULL AS 'null' ESCAPE AS ' '  QUOTE AS '"') ;

-- with single quotes

CREATE WRITABLE EXTERNAL TABLE tbl_wet_csv5 (like all_types_csv)LOCATION ('gpfdist://@hostname@:@gp_port@/output/wet_csv_dsp.tbl') FORMAT 'CSV' (DELIMITER AS '|' NULL
 AS 'null' ESCAPE AS ' '  QUOTE AS '''') ;

-- with force quote

CREATE WRITABLE EXTERNAL TABLE tbl_wet_csv6 (like all_types_csv)LOCATION ('gpfdist://@hostname@:@gp_port@/output/wet_csv_dsp.tbl') FORMAT 'CSV' (DELIMITER AS '|' NULL AS 'null' ESCAPE AS ' '  QUOTE AS '"' FORCE QUOTE char1) ;


--Table with all data types

    CREATE TABLE all_types_text ( bit1 bit(1), bit2 bit varying(50), boolean1 boolean, char1 char(1), charvar1 character varying(50), char2 character(50),
varchar1 varchar(50),date1 date,dp1 double precision,int1 integer,interval1 interval,numeric1 numeric,real1 real,smallint1 smallint,time1 time,bigint1 bigint,
bigserial1 bigserial,bytea1 bytea,cidr1 cidr,decimal1 decimal,inet1 inet,macaddr1 macaddr,money1 money,serial1 serial,text1 text,time2 time without time zone,time3 time with time zone,time4 timestamp without time zone,time5 timestamp with time zone) distributed randomly;

insert into all_types_text values ('1','0','t','a','varchar1','char1','varchar1','2001-11-11',234.23234,23,'24',234,23,4,'12:12:12',2,3,'d','10.1.2.10',1,'10.1.2.10','AA:AA:AA:AA:AA:AA','34.23',5,'text1','00:00:00','00:00:00+1359','2001-12-13 01:51:15','2001-12-13 01:51:15+1359');

insert into all_types_text values ('0','0','f','b','varchar2','char2','varchar2','2002-11-11',234.23234,23,'24',234,23,4,'12:12:12',2,3,'d','10.1.2.10',1,'10.1.2.10','BB:BB:BB:BB:BB:BB','34.23',5,'text2','00:00:00','00:00:00+1359','2002-12-13 01:51:15','2002-12-13 01:51:15+1359');

insert into all_types_text values ('1','0','t','c','varchar3','char3','varchar3','2003-11-11',234.23234,23,'24',234,23,4,'12:12:12',2,3,'d','10.1.2.10',1,'10.1.2.10','CC:CC:CC:CC:CC:CC','34.23',5,'text3','00:00:00','00:00:00+1359','2003-12-13 01:51:15','2003-12-13 01:51:15+1359');

insert into all_types_text values ('0','0','f','d','varchar4','char4','varchar4','2004-11-11',234.23234,23,'24',234,23,4,'12:12:12',2,3,'d','10.1.2.10',1,'10.1.2.10','DD:DD:DD:DD:DD:DD','34.23',5,'text4','00:00:00','00:00:00+1359','2004-12-13 01:51:15','2004-12-13 01:51:15+1359');

insert into all_types_text values ('1','0','t','e','varchar5','char5','varchar5','2005-11-11',234.23234,23,'24',234,23,4,'12:12:12',2,3,'d','10.1.2.10',1,'10.1.2.10','EE:EE:EE:EE:EE:EE','34.23',5,'text5','00:00:00','00:00:00+1359','2005-12-13 01:51:15','2005-12-13 01:51:15+1359');


-- with AS for DELIMITER , NULL, ESCAPE

CREATE WRITABLE EXTERNAL TABLE tbl_wet_text1 (like all_types_text)LOCATION ('gpfdist://@hostname@:@gp_port@/output/wet_text_dsp.tbl') FORMAT 'TEXT' (DELIMITER AS '|' NULL AS 'null' ESCAPE AS ' '   ) ;

-- without AS for DELIMITER , NULL, ESCAPE

CREATE WRITABLE EXTERNAL TABLE tbl_wet_text2 (like all_types_text)LOCATION ('gpfdist://@hostname@:@gp_port@/output/wet_text_dsp.tbl') FORMAT 'TEXT' (DELIMITER AS ',' NULL 'null' ESCAPE ' ');

-- with ESCAPE OFF

CREATE WRITABLE EXTERNAL TABLE tbl_wet_text3 (like all_types_text)LOCATION ('gpfdist://@hostname@:@gp_port@/output/wet_text_dsp.tbl') FORMAT 'TEXT' (DELIMITER AS '|'
NULL AS 'null' ESCAPE 'OFF') ;

-- with header

CREATE WRITABLE EXTERNAL TABLE tbl_wet_text4 (like all_types_text)LOCATION ('gpfdist://@hostname@:@gp_port@/output/wet_text_dsp.tbl') FORMAT 'TEXT' (DELIMITER AS '|' NULL AS 'null' ESCAPE AS ' '  HEADER ) ;

-- with double quotes

CREATE WRITABLE EXTERNAL TABLE tbl_wet_text5 (like all_types_text)LOCATION ('gpfdist://@hostname@:@gp_port@/output/wet_text_dsp.tbl') FORMAT 'TEXT' (DELIMITER AS '|' NULL AS 'null' ESCAPE AS ' '  QUOTE AS '"') ;

-- with single quotes

CREATE WRITABLE EXTERNAL TABLE tbl_wet_text6 (like all_types_text)LOCATION ('gpfdist://@hostname@:@gp_port@/output/wet_text_dsp.tbl') FORMAT 'TEXT' (DELIMITER AS '|' NULL AS 'null' ESCAPE AS ' '  QUOTE AS '''') ;

-- with force quote

CREATE WRITABLE EXTERNAL TABLE tbl_wet_text7 (like all_types_text)LOCATION ('gpfdist://@hostname@:@gp_port@/output/wet_text_dsp.tbl') FORMAT 'TEXT' (DELIMITER AS '|' NULL AS 'null' ESCAPE AS ' ' FORCE QUOTE char1) ;


create table test_tbl_for_view1 (a int, b text);

insert into test_tbl_for_view1 values ( generate_series(1,5), 'test_1');

create view test_view1 as select * from test_tbl_for_view1;

CREATE WRITABLE EXTERNAL TABLE tbl_wet_view1 (like  test_tbl_for_view1) LOCATION ('gpfdist://@hostname@:@gp_port@/output/wet_view.txt') FORMAT 'CSV' (DELIMITER AS '|' NULL AS 'null' ESCAPE AS ' '   ) ;

insert into tbl_wet_view1 select * from test_view1;


-- view with joins

create table test_tbl_for_view3 (a int, b text);

insert into test_tbl_for_view3 values ( generate_series(1,5), 'test_1');

create table test_tbl_for_view2 (c int, d text);

insert into test_tbl_for_view2 values ( generate_series(1,5), 'test_2');

create view test_view2 as select  t3.a,t3.b,t2.d from test_tbl_for_view3 t3, test_tbl_for_view2 t2 where t3.a=t2.c ;

CREATE WRITABLE EXTERNAL TABLE tbl_wet_view2 (a int, b text, c text) LOCATION ('gpfdist://@hostname@:@gp_port@/output/wet_view.txt') FORMAT 'CSV' (DELIMITER AS '|' NULL AS 'null' ESCAPE AS ' '   ) ;

insert into tbl_wet_view2 select * from test_view2;

CREATE TABLE  test_order_by (a int, b text);

INSERT INTO test_order_by VALUES ( generate_series(1,5), 'test_1');

CREATE WRITABLE EXTERNAL TABLE tbl_wet_order_by(like  test_order_by) LOCATION ('gpfdist://@hostname@:@gp_port@/output/wet_tbl_order_by.txt') FORMAT 'CSV' (DELIMITER AS '|' NULL AS 'null' ESCAPE AS ' '   ) ;

INSERT INTO  tbl_wet_order_by SELECT * FROM  test_order_by ORDER BY a;

CREATE TABLE test_tran (a int, b text);

INSERT INTO test_tran VALUES ( generate_series(1,5), 'test_1');

CREATE WRITABLE EXTERNAL TABLE tbl_wet_tran(like  test_tran) LOCATION ('gpfdist://@hostname@:@gp_port@/output/wet_tbl_tran.txt') FORMAT 'CSV' (DELIMITER AS '|' NULL AS 'null' ESCAPE AS ' '   ) ;

BEGIN;
INSERT INTO tbl_wet_tran SELECT * FROM test_tran ORDER BY a;
ROLLBACK;

create table test_dp1 ( a int, b text) distributed by (a);

CREATE WRITABLE EXTERNAL TABLE tbl_wet_syntax1 ( like test_dp1) LOCATION ('gpfdist://@hostname@:@gp_port@/output/wet_syntax1_dsp.tbl') FORMAT 'TEXT' (DELIMITER '|' ) DISTRIBUTED BY (a);

insert into tbl_wet_syntax1 values ( generate_series(1,10), 'test_dp_1');

CREATE WRITABLE EXTERNAL TABLE tbl_wet_syntax2 ( like test_dp1) LOCATION ('gpfdist://@hostname@:@gp_port@/output/wet_syntax2_dsp.tbl') FORMAT 'TEXT' (DELIMITER '|' ) DISTRIBUTED BY (b);

insert into tbl_wet_syntax2 values ( generate_series(1,10), 'test_dp_1');


CREATE WRITABLE EXTERNAL TABLE tbl_wet_syntax3 ( like test_dp1) LOCATION ('gpfdist://@hostname@:@gp_port@/output/wet_syntax3_dsp.tbl') FORMAT 'TEXT' (DELIMITER '|' ) DISTRIBUTED randomly;

insert into tbl_wet_syntax3 values ( generate_series(1,10), 'test_dp_1');


create table test_dp2 ( a int, b text) distributed by (b);

CREATE WRITABLE EXTERNAL TABLE tbl_wet_syntax4 ( like test_dp2) LOCATION ('gpfdist://@hostname@:@gp_port@/output/wet_syntax4_dsp.tbl') FORMAT 'TEXT' (DELIMITER '|' ) DISTRIBUTED BY (a);

insert into tbl_wet_syntax4 values ( generate_series(1,10), 'test_dp_1');

CREATE WRITABLE EXTERNAL TABLE tbl_wet_syntax5 ( like test_dp2) LOCATION ('gpfdist://@hostname@:@gp_port@/output/wet_syntax5_dsp.tbl') FORMAT 'TEXT' (DELIMITER '|' ) DISTRIBUTED BY (b);

insert into tbl_wet_syntax5 values ( generate_series(1,10), 'test_dp_1');


CREATE WRITABLE EXTERNAL TABLE tbl_wet_syntax6 ( like test_dp2) LOCATION ('gpfdist://@hostname@:@gp_port@/output/wet_syntax6_dsp.tbl') FORMAT 'TEXT' (DELIMITER '|' ) DISTRIBUTED randomly;

insert into tbl_wet_syntax6 values ( generate_series(1,10), 'test_dp_1');


create table test_dp3 ( a int, b text) distributed randomly;

CREATE WRITABLE EXTERNAL TABLE tbl_wet_syntax7 ( like test_dp3) LOCATION ('gpfdist://@hostname@:@gp_port@/output/wet_syntax7_dsp.tbl') FORMAT 'TEXT' (DELIMITER '|' ) DISTRIBUTED BY (a);

insert into tbl_wet_syntax7 values ( generate_series(1,10), 'test_dp_1');

CREATE WRITABLE EXTERNAL TABLE tbl_wet_syntax8 ( like test_dp3) LOCATION ('gpfdist://@hostname@:@gp_port@/output/wet_syntax8_dsp.tbl') FORMAT 'TEXT' (DELIMITER '|' ) DISTRIBUTED BY (b);

insert into tbl_wet_syntax8 values ( generate_series(1,10), 'test_dp_1');


CREATE WRITABLE EXTERNAL TABLE tbl_wet_syntax9 ( like test_dp3) LOCATION ('gpfdist://@hostname@:@gp_port@/output/wet_syntax9_dsp.tbl') FORMAT 'TEXT' (DELIMITER '|' ) DISTRIBUTED randomly;

insert into tbl_wet_syntax9 values ( generate_series(1,10), 'test_dp_1');


CREATE WRITABLE EXTERNAL TABLE tbl_wet_syntax10 ( a int, b text) LOCATION ('gpfdist://@hostname@:@gp_port@/output/wet_syntax10_dsp.tbl') FORMAT 'TEXT' DISTRIBUTED RANDOMLY;
 insert into tbl_wet_syntax10 values ( generate_series(1,10), 'test_dp_1');

CREATE WRITABLE EXTERNAL TABLE tbl_wet_syntax11 ( a int, b text) LOCATION ('gpfdist://@hostname@:@gp_port@/output/wet_syntax11_dsp.tbl') FORMAT 'TEXT' (DELIMITER '|' ) DISTRIBUTED BY (a);
 insert into tbl_wet_syntax11 values ( generate_series(1,10), 'test_dp_1');

CREATE WRITABLE EXTERNAL TABLE tbl_wet_syntax12 ( a int, b text) LOCATION ('gpfdist://@hostname@:@gp_port@/output/wet_syntax12_dsp.tbl') FORMAT 'TEXT' (DELIMITER '|' ) DISTRIBUTED BY (b);
 insert into tbl_wet_syntax12 values ( generate_series(1,10), 'test_dp_1');
