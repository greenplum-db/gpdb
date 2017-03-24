-- 
-- @created 2009-01-27 14:00:00
-- @modified 2013-06-24 17:00:00
-- @tags ddl schema_topology
-- @description CO Tables

create database co_db;
\c co_db

set time zone PST8PDT;

-- create all types CO table and insert values
CREATE TABLE all_types( bit1 bit(1), bit2 bit varying(50), boolean1 boolean, char1 char(50), charvar1 character varying(50), char2 character(50),
varchar1 varchar(50),date1 date,dp1 double precision,int1 integer,interval1 interval,numeric1 numeric,real1 real,smallint1 smallint,time1 time,bigint1 bigint,
bigserial1 bigserial,bytea1 bytea,cidr1 cidr,decimal1 decimal,inet1 inet,macaddr1 macaddr,money1 money,serial1 serial,text1 text,time2 time without time zone,
time3 time with time zone,time4 timestamp without time zone,time5 timestamp with time zone) WITH (orientation='column', appendonly=true) distributed randomly;

insert into all_types values ('1','0','t','c','varchar1','char1','varchar1','2001-01-11',134.23234,1,'24',134,13,1,'12:12:12',1,1,'d','0.0.0.0',1,'0.0.0.0','AA:AA:AA:AA:AA:AA','14.23',1,'text1','00:00:00','00:00:00+1359','2001-12-10 01:51:15','2001-10-11 01:51:15+1359');

insert into all_types values ('1','0','t','c','varchar6','char6','varchar6','2006-02-12',234.23234,2,'24',234,23,2,'12:12:12',2,2,'d','0.0.0.0',2,'0.0.0.0','AA:AA:AA:AA:AA:AA','24.23',2,'text6','00:00:00','00:00:00+1359','2006-11-11 01:51:15','2001-11-12 01:51:15+1359');

insert into all_types values ('0','0','f','c','varchar2','char2','varchar2','2002-03-13',334.23234,3,'24',334,33,3,'12:12:12',3,3,'d','0.0.0.0',3,'0.0.0.0','BB:BB:BB:BB:BB:BB','34.23',3,'text2','00:00:00','00:00:00+1359','2002-10-12 01:51:15','2002-12-13 01:51:15+1359');

insert into all_types values ('1','0','t','c','varchar3','char3','varchar3','2003-04-14',434.23234,4,'24',434,43,4,'12:12:12',4,4,'d','0.0.0.0',4,'0.0.0.0','CC:CC:CC:CC:CC:CC','44.23',4,'text3','00:00:00','00:00:00+1359','2003-09-13 01:51:15','2003-09-14 01:51:15+1359');

insert into all_types values ('0','0','f','c','varchar4','char4','varchar4','2004-05-15',534.23234,5,'24',534,53,5,'12:12:12',5,5,'d','0.0.0.0',5,'0.0.0.0','DD:DD:DD:DD:DD:DD','54.23',5,'text4','00:00:00','00:00:00+1359','2004-08-14 01:51:15','2004-08-15 01:51:15+1359');

insert into all_types values ('1','0','t','c','varchar5','char5','varchar5','2005-06-16',634.23234,6,'24',634,63,6,'12:12:12',6,6,'d','0.0.0.0',6,'0.0.0.0','EE:EE:EE:EE:EE:EE','64.23',generate_series(6,100),repeat('text',100),'00:00:00','00:00:00+1359','2005-07-15 01:51:15','2005-07-16 01:51:15+1359');

-- Testing Insert into select with CO tables

CREATE TABLE all_types_insert_into_select( bit1 bit(1), bit2 bit varying(50), boolean1 boolean, char1 char(50), charvar1 character varying(50), char2 character(50),
varchar1 varchar(50),date1 date,dp1 double precision,int1 integer,interval1 interval,numeric1 numeric,real1 real,smallint1 smallint,time1 time,bigint1 bigint,
bigserial1 bigserial,bytea1 bytea,cidr1 cidr,decimal1 decimal,inet1 inet,macaddr1 macaddr,money1 money,serial1 serial,text1 text,time2 time without time zone,
time3 time with time zone,time4 timestamp without time zone,time5 timestamp with time zone) WITH (orientation='column', appendonly=true) distributed randomly; 

insert into all_types_insert_into_select select * from all_types;

-- Testing Copy with CO tables

CREATE TABLE all_types_copy( bit1 bit(1), bit2 bit varying(50), boolean1 boolean, char1 char(50), charvar1 character varying(50), char2 character(50),
varchar1 varchar(50),date1 date,dp1 double precision,int1 integer,interval1 interval,numeric1 numeric,real1 real,smallint1 smallint,time1 time,bigint1 bigint,
bigserial1 bigserial,bytea1 bytea,cidr1 cidr,decimal1 decimal,inet1 inet,macaddr1 macaddr,money1 money,serial1 serial,text1 text,time2 time without time zone,
time3 time with time zone,time4 timestamp without time zone,time5 timestamp with time zone) WITH (orientation='column', appendonly=true) distributed randomly;

COPY (select * from all_types) TO '/data/gpdb_p1/anu/dev/cdbfast/main/schema_topology/CO_all_types_file_copy' WITH DELIMITER AS ',' NULL AS 'null string' ESCAPE AS E'\n' CSV QUOTE AS '"' FORCE QUOTE varchar1;

COPY all_types_copy FROM '/data/gpdb_p1/anu/dev/cdbfast/main/schema_topology/CO_all_types_file_copy' WITH DELIMITER AS ',' NULL AS 'null string' ESCAPE AS 'OFF' LOG ERRORS INTO err_all_types KEEP SEGMENT REJECT LIMIT 10 PERCENT;

-- Testing CTAS on CO Table

CREATE TABLE all_types_ctas AS SELECT * from all_types;

-- Testing temp table
create temp table temp_table1 ( a int, b text) with (appendonly=true,orientation=column) distributed by (a);
insert into temp_table1 values (1,'test_1');
insert into temp_table1 values (2,'test_2');
insert into temp_table1 values (2,'test_3');
insert into temp_table1 select i,i||'_'||repeat('text',100) from generate_series(3,100)i;

-- Testing select into from CO table creates heap table

select count(*) into all_types_select_into from all_types;

-- This is a basic sanity test of column-oriented storage.

-- start_ignore
DROP TABLE IF EXISTS cos_table1;
-- end_ignore

-- Create a column-oriented table.
CREATE TABLE cos_table1 (
    id VARCHAR, lname CHAR(20), fname CHAR(10), tincan FLOAT
    ) 
 WITH (orientation='column', appendonly=true)
 DISTRIBUTED BY (id)
 ;

-- Insert some data into the table.
INSERT INTO cos_table1 (id, lname, fname, tincan) 
 VALUES ('1122', 'Root', 'Tibor', 1.0);
INSERT INTO cos_table1 (id, lname, fname, tincan) 
 VALUES ('1123', 'Strata', 'Lysa', 2.0);

-- Retrieve some data from the table.
SELECT * FROM cos_table1 ORDER BY id;
SELECT SUM(tincan) FROM cos_table1;

-- We will start with an AO table as our "baseline".
CREATE TABLE table2 (id VARCHAR, drop_this_column_later FLOAT)
 WITH (appendonly=true) 
 DISTRIBUTED BY (id)
 ;

-- We can drop a column.
ALTER TABLE table2 DROP COLUMN drop_this_column_later;

-- We can add a column (if it has a default value).
ALTER TABLE table2 ADD COLUMN tincan FLOAT DEFAULT 1.0;

-- MPP-7043 ( CO: Adding a column and then altering the type of the column in CO tables gives SIGSEGV)
CREATE TABLE cos_table1_co (   id VARCHAR, lname CHAR(20), fname CHAR(10), tincan FLOAT,age numeric ) WITH (orientation='column', appendonly=true)DISTRIBUTED BY (id) ;

INSERT INTO cos_table1_co (id, lname, fname, tincan, age) VALUES ('1122', 'Root', 'Tibor', 1.0,15);

INSERT INTO cos_table1_co (id, lname, fname, tincan, age) VALUES ('1123', 'Strata', 'Lysa', 2.0,18);

SELECT * FROM cos_table1_co ORDER BY id;

Alter table cos_table1_co alter age Type int;

Alter table cos_table1_co alter lname Type varchar;

Alter table cos_table1_co alter tincan Type real;

Alter table cos_table1_co alter tincan Type numeric;

Alter table cos_table1_co alter tincan Type int;

Alter table cos_table1_co alter tincan Type double;
