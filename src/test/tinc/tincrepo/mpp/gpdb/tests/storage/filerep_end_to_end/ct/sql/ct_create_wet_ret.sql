-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- create a normal heap table
--
CREATE TABLE ct_REGION  ( R_REGIONKEY  INTEGER NOT NULL,
                            R_NAME       CHAR(25) NOT NULL,
                            R_COMMENT    VARCHAR(152)) ;

--
-- create a RET to load data into the normal heap table
--
CREATE external web TABLE ct_e_REGION  ( R_REGIONKEY  INTEGER ,
                            R_NAME       CHAR(25) ,
                            R_COMMENT    VARCHAR(152))
                        execute 'echo "0|AFRICA|lar deposits. blithely final packages cajole. regular waters are final requests. regular accounts are according to
                                       1|AMERICA|hs use ironic, even requests. s
                                       2|ASIA|ges. thinly even pinto beans ca
                                       3|EUROPE|ly final courts cajole furiously final excuse
                                       4|MIDDLE EAST|uickly special accounts cajole carefully blithely close requests. carefully final asymptotes haggle furiousl"'
                        on 1 format 'text' (delimiter '|');
--
-- load data into the heap table selecting from RET
--
insert into ct_region select * from ct_e_region;

--
-- SYNC1
--
--
-- create WET with similiar schema def as the original heap table
--
CREATE WRITABLE EXTERNAL TABLE sync1_wet_region4 ( like sync1_region) LOCATION ('gpfdist://10.0.0.6:8088/wet_region4.tbl') FORMAT 'TEXT' (DELIMITER AS '|');

--
-- insert data into the WET selecting from original table
--
INSERT INTO sync1_wet_region4 SELECT * FROM sync1_region;
--
-- create a RET reading data from the file created by WET
--
CREATE EXTERNAL TABLE sync1_ret_region4 ( like sync1_region) LOCATION ('gpfdist://10.0.0.6:8088/wet_region4.tbl') FORMAT 'TEXT' (DELIMITER AS '|');
--
-- create second table with same schema def
--
CREATE TABLE sync1_new_region4 (like sync1_region);
--
-- insert into the second table reading from the RET
--
INSERT INTO sync1_new_region4 SELECT * FROM sync1_ret_region4;
--
-- compare contents of original table vs the table loaded from ret which in turn took the data which was created by wet
--
select * from sync1_region order by r_regionkey;

select * from sync1_new_region4 order by r_regionkey;

--
-- CK_SYNC1
--
--
-- create WET with similiar schema def as the original heap table
--
CREATE WRITABLE EXTERNAL TABLE ck_sync1_wet_region3 ( like ck_sync1_region) LOCATION ('gpfdist://10.0.0.6:8088/wet_region3.tbl') FORMAT 'TEXT' (DELIMITER AS '|');

--
-- insert data into the WET selecting from original table
--
INSERT INTO ck_sync1_wet_region3 SELECT * FROM ck_sync1_region;
--
-- create a RET reading data from the file created by WET
--
CREATE EXTERNAL TABLE ck_sync1_ret_region3 ( like ck_sync1_region) LOCATION ('gpfdist://10.0.0.6:8088/wet_region3.tbl') FORMAT 'TEXT' (DELIMITER AS '|');
--
-- create second table with same schema def
--
CREATE TABLE ck_sync1_new_region3 (like ck_sync1_region);
--
-- insert into the second table reading from the RET
--
INSERT INTO ck_sync1_new_region3 SELECT * FROM ck_sync1_ret_region3;
--
-- compare contents of original table vs the table loaded from ret which in turn took the data which was created by wet
--
select * from ck_sync1_region order by r_regionkey;

select * from ck_sync1_new_region3 order by r_regionkey;

--
-- CT
--

--
-- create WET with similiar schema def as the original heap table
--
CREATE WRITABLE EXTERNAL TABLE ct_wet_region1 ( like ct_region) LOCATION ('gpfdist://10.0.0.6:8088/wet_region1.tbl') FORMAT 'TEXT' (DELIMITER AS '|');

--
-- insert data into the WET selecting from original table
--
INSERT INTO ct_wet_region1 SELECT * FROM ct_region;
--
-- create a RET reading data from the file created by WET
--
CREATE EXTERNAL TABLE ct_ret_region1 ( like ct_region) LOCATION ('gpfdist://10.0.0.6:8088/wet_region1.tbl') FORMAT 'TEXT' (DELIMITER AS '|');
--
-- create second table with same schema def
--
CREATE TABLE ct_new_region1 (like ct_region);
--
-- insert into the second table reading from the RET
--
INSERT INTO ct_new_region1 SELECT * FROM ct_ret_region1;
--
-- compare contents of original table vs the table loaded from ret which in turn took the data which was created by wet
--
select * from ct_region order by r_regionkey;

select * from ct_new_region1 order by r_regionkey;

--
--
--


