-- 
-- @created 2009-01-27 14:00:00
-- @modified 2013-06-24 17:00:00
-- @tags ddl schema_topology
-- @description Visibility of data and objects--Update, Delete, Insert of data into a table

--Vaccuumed vs. Unvaccumed Data
--DROP TABLE
--TRUNCATE TABLE

--VACUUMED DATA

create database vacuum_data_db;
\c vacuum_data_db;

CREATE TABLE all_types(bit1 bit(1),bit2 bit varying(50), boolean1 boolean, char1 char(50), charvar1 character varying(50), char2 character(50),varchar1 varchar(50),date1 date,dp1 double precision,int1 integer, interval1 interval, numeric1 numeric, real1 real,smallint1 smallint,time1 time,bigint1 bigint,bigserial1 bigserial,bytea1 bytea,cidr1 cidr,decimal1 decimal,inet1 inet,macaddr1 macaddr,money1 money,serial1 serial,text1 text,time2 time without time zone,time3 time with time zone,time4 timestamp without time zone,time5 timestamp with time zone) distributed randomly; 

insert into all_types values ('1','0','t','c','varchar1','char1','varchar1','2001-11-11',234.23234,23,'24',234,23,4,'12:12:12',2,3,'d','0.0.0.0',1,'0.0.0.0','AA:AA:AA:AA:AA:AA','34.23',5,'text1','00:00:00','00:00:00+1359','2001-12-13 01:51:15','2001-12-13 01:51:15+1359');

insert into all_types values ('0','0','f','c','varchar2','char2','varchar2','2002-11-11',234.23234,23,'24',234,23,4,'12:12:12',2,3,'d','0.0.0.0',1,'0.0.0.0','BB:BB:BB:BB:BB:BB','34.23',5,'text2','00:00:00','00:00:00+1359','2002-12-13 01:51:15','2002-12-13 01:51:15+1359');

insert into all_types values ('1','0','t','c','varchar3','char3','varchar3','2003-11-11',234.23234,23,'24',234,23,4,'12:12:12',2,3,'d','0.0.0.0',1,'0.0.0.0','CC:CC:CC:CC:CC:CC','34.23',5,'text3','00:00:00','00:00:00+1359','2003-12-13 01:51:15','2003-12-13 01:51:15+1359');

insert into all_types values ('0','0','f','c','varchar4','char4','varchar4','2004-11-11',234.23234,23,'24',234,23,4,'12:12:12',2,3,'d','0.0.0.0',1,'0.0.0.0','DD:DD:DD:DD:DD:DD','34.23',5,'text4','00:00:00','00:00:00+1359','2004-12-13 01:51:15','2004-12-13 01:51:15+1359');

insert into all_types values ('1','0','t','c','varchar5','char5','varchar5','2005-11-11',234.23234,23,'24',234,23,4,'12:12:12',2,3,'d','0.0.0.0',1,'0.0.0.0','EE:EE:EE:EE:EE:EE','34.23',5,'text5','00:00:00','00:00:00+1359','2005-12-13 01:51:15','2005-12-13 01:51:15+1359');

update all_types set cidr1='1.1.1.1' where cidr1='0.0.0.0';

update all_types set bytea1='x' where bytea1='d';

update all_types set charvar1='hello' where charvar1='varchar1';

delete from all_types where  charvar1='varchar2';

delete from all_types where  date1 = '2003-11-11';

delete from all_types;

truncate all_types;

drop table all_types cascade;

CREATE EXTERNAL TABLE ext_table1 (
    a_party_id bigint,
    start_date date,
    start_time_hour smallint,
    start_time_min smallint,
    start_time_sec smallint,
    duration integer,
    record_dt timestamp without time zone,
    framed_ip_address character varying(15)
) LOCATION (
    'gpfdist://97.253.36.12:9010/R_LAA_DATA_2008-10-27.dat'
) FORMAT 'text' (delimiter ',' null '' )
ENCODING 'UTF8';
DROP EXTERNAL TABLE ext_table1;

vacuum ;

--UNVACUUMED DATA
create database unvacuum_data_db;
\c unvacuum_data_db

CREATE TABLE all_types(bit1 bit(1),bit2 bit varying(50), boolean1 boolean, char1 char(50), charvar1 character varying(50), char2 character(50),varchar1 varchar(50),date1 date,dp1 double precision,int1 integer, interval1 interval, numeric1 numeric, real1 real,smallint1 smallint,time1 time,bigint1 bigint,bigserial1 bigserial,bytea1 bytea,cidr1 cidr,decimal1 decimal,inet1 inet,macaddr1 macaddr,money1 money,serial1 serial,text1 text,time2 time without time zone,time3 time with time zone,time4 timestamp without time zone,time5 timestamp with time zone) distributed randomly;

insert into all_types values ('1','0','t','c','varchar1','char1','varchar1','2001-11-11',234.23234,23,'24',234,23,4,'12:12:12',2,3,'d','0.0.0.0',1,'0.0.0.0','AA:AA:AA:AA:AA:AA','34.23',5,'text1','00:00:00','00:00:00+1359','2001-12-13 01:51:15','2001-12-13 01:51:15+1359');

insert into all_types values ('0','0','f','c','varchar2','char2','varchar2','2002-11-11',234.23234,23,'24',234,23,4,'12:12:12',2,3,'d','0.0.0.0',1,'0.0.0.0','BB:BB:BB:BB:BB:BB','34.23',5,'text2','00:00:00','00:00:00+1359','2002-12-13 01:51:15','2002-12-13 01:51:15+1359');

insert into all_types values ('1','0','t','c','varchar3','char3','varchar3','2003-11-11',234.23234,23,'24',234,23,4,'12:12:12',2,3,'d','0.0.0.0',1,'0.0.0.0','CC:CC:CC:CC:CC:CC','34.23',5,'text3','00:00:00','00:00:00+1359','2003-12-13 01:51:15','2003-12-13 01:51:15+1359');

insert into all_types values ('0','0','f','c','varchar4','char4','varchar4','2004-11-11',234.23234,23,'24',234,23,4,'12:12:12',2,3,'d','0.0.0.0',1,'0.0.0.0','DD:DD:DD:DD:DD:DD','34.23',5,'text4','00:00:00','00:00:00+1359','2004-12-13 01:51:15','2004-12-13 01:51:15+1359');

insert into all_types values ('1','0','t','c','varchar5','char5','varchar5','2005-11-11',234.23234,23,'24',234,23,4,'12:12:12',2,3,'d','0.0.0.0',1,'0.0.0.0','EE:EE:EE:EE:EE:EE','34.23',5,'text5','00:00:00','00:00:00+1359','2005-12-13 01:51:15','2005-12-13 01:51:15+1359');

update all_types set cidr1='1.1.1.1' where cidr1='0.0.0.0';

update all_types set bytea1='x' where bytea1='d';

update all_types set charvar1='hello' where charvar1='varchar1';

delete from all_types where  charvar1='varchar2';

delete from all_types where  date1 = '2003-11-11';

delete from all_types;

truncate all_types;

drop table all_types cascade;

CREATE EXTERNAL TABLE ext_table1 (
    a_party_id bigint,
    start_date date,
    start_time_hour smallint,
    start_time_min smallint,
    start_time_sec smallint,
    duration integer,
    record_dt timestamp without time zone,
    framed_ip_address character varying(15)
) LOCATION (
    'gpfdist://97.253.36.12:9010/R_LAA_DATA_2008-10-27.dat'
) FORMAT 'text' (delimiter ',' null '' )
ENCODING 'UTF8';
DROP EXTERNAL TABLE ext_table1;

