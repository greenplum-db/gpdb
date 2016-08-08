-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- SYNC1 CREATE TABLESPACE a
--
CREATE TABLESPACE sync1_ts_a1 filespace filerep_fs_a;
CREATE TABLESPACE sync1_ts_a2 filespace filerep_fs_a;
CREATE TABLESPACE sync1_ts_a3 filespace filerep_fs_a;
CREATE TABLESPACE sync1_ts_a4 filespace filerep_fs_a;
CREATE TABLESPACE sync1_ts_a5 filespace filerep_fs_a;
CREATE TABLESPACE sync1_ts_a6 filespace filerep_fs_a;
CREATE TABLESPACE sync1_ts_a7 filespace filerep_fs_a;
CREATE TABLESPACE sync1_ts_a8 filespace filerep_fs_a;
--
-- SYNC1 CREATE TABLESPACE b  
--
CREATE TABLESPACE sync1_ts_b1 filespace filerep_fs_b;
CREATE TABLESPACE sync1_ts_b2 filespace filerep_fs_b;
CREATE TABLESPACE sync1_ts_b3 filespace filerep_fs_b;
CREATE TABLESPACE sync1_ts_b4 filespace filerep_fs_b;
CREATE TABLESPACE sync1_ts_b5 filespace filerep_fs_b;
CREATE TABLESPACE sync1_ts_b6 filespace filerep_fs_b;
CREATE TABLESPACE sync1_ts_b7 filespace filerep_fs_b;
CREATE TABLESPACE sync1_ts_b8 filespace filerep_fs_b;
--
-- SYNC1 CREATE TABLESPACE c
--
CREATE TABLESPACE sync1_ts_c1 filespace filerep_fs_c;
CREATE TABLESPACE sync1_ts_c2 filespace filerep_fs_c;
CREATE TABLESPACE sync1_ts_c3 filespace filerep_fs_c;
CREATE TABLESPACE sync1_ts_c4 filespace filerep_fs_c;
CREATE TABLESPACE sync1_ts_c5 filespace filerep_fs_c;
CREATE TABLESPACE sync1_ts_c6 filespace filerep_fs_c;
CREATE TABLESPACE sync1_ts_c7 filespace filerep_fs_c;
CREATE TABLESPACE sync1_ts_c8 filespace filerep_fs_c;

--
--
-- SYNC1
--
--
--
-- HEAP TABLE 
--

--
--
-- CREATE HEAP TABLE IN TABLESPACE - JUST IN TIME
--
--
CREATE TABLE sync1_heap_table_ts_1 ( phase text,a int,col001 char DEFAULT 'z',col002 numeric,col003 boolean DEFAULT false,col004 bit(3) DEFAULT '111',col005 text DEFAULT 'pookie', col006 integer[] DEFAULT '{5, 4, 3, 2, 1}',
col007 character varying(512) DEFAULT 'Now is the time', col008 character varying DEFAULT 'Now is the time', col009 character varying(512)[], col010 numeric(8),col011 int,col012 double precision, col013 bigint, col014 char(8),
col015 bytea,col016 timestamp with time zone,col017 interval, col018 cidr, col019 inet, col020 macaddr,col022 money, col024 timetz,col025 circle, col026 box, col027 name,col028 path, col029 int2,
col031 bit varying(256),col032 date, col034 lseg,col035 point,col036 polygon,col037 real,col039 time, col040 timestamp ) tablespace sync1_ts_a1 ;


INSERT INTO sync1_heap_table_ts_1 VALUES ('sync1_heap1',generate_series(1,10),'a',11,true,'111', repeat('text_',10), '{1,2,3,4,5}', 'Hello .. how are you 1', 'Hello .. how are you 1',    '{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd',  
'2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA',   '34.23',   '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_heap_table_ts_1;
--
--
-- ALTER HEAP TABLE TO DIFFERENT TABLESPACE 
--
--
ALTER TABLE sync1_heap_table_ts_1 set TABLESPACE sync1_ts_b1;

INSERT INTO sync1_heap_table_ts_1 VALUES ('sync1_heap1',generate_series(1,10),'a',11,true,'111', repeat('text_',10), '{1,2,3,4,5}', 'Hello .. how are you 1', 'Hello .. how are you 1',    '{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd',
'2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA',   '34.23',   '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',
  '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_heap_table_ts_1;
--
--
-- ALTER HEAP TABLE TO TABLESPACE
--
--
CREATE TABLE sync1_heap_table_ts_11 ( phase text,a int,col001 char DEFAULT 'z',col002 numeric,col003 boolean DEFAULT false,col004 bit(3) DEFAULT '111',col005 text DEFAULT 'pookie', col006 integer[] DEFAULT '{5, 4, 3, 2, 1}',
col007 character varying(512) DEFAULT 'Now is the time', col008 character varying DEFAULT 'Now is the time', col009 character varying(512)[], col010 numeric(8),col011 int,col012 double precision, col013 bigint, col014 char(8),
col015 bytea,col016 timestamp with time zone,col017 interval, col018 cidr, col019 inet, col020 macaddr,col022 money, col024 timetz,col025 circle, col026 box, col027 name,col028 path, col029 int2,
col031 bit varying(256),col032 date, col034 lseg,col035 point,col036 polygon,col037 real,col039 time, col040 timestamp ) ;


INSERT INTO sync1_heap_table_ts_11 VALUES ('sync1_heap1',generate_series(1,10),'a',11,true,'111', repeat('text_',10), '{1,2,3,4,5}', 'Hello .. how are you 1', 'Hello .. how are you 1',    '{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd',  
'2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA',   '34.23',   '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_heap_table_ts_11;

ALTER TABLE sync1_heap_table_ts_11 set TABLESPACE sync1_ts_c1;

--
-- AO TABLE 
--

--
--
-- CREATE AO TABLE IN TABLESPACE - JUST IN TIME
--
--
CREATE TABLE sync1_ao_table_ts_1 ( phase text,a int,col001 char DEFAULT 'z',col002 numeric,col003 boolean DEFAULT false,col004 bit(3) DEFAULT '111',col005 text DEFAULT 'pookie', col006 integer[] DEFAULT '{5, 4, 3, 2, 1}',
col007 character varying(512) DEFAULT 'Now is the time', col008 character varying DEFAULT 'Now is the time', col009 character varying(512)[], col010 numeric(8),col011 int,col012 double precision, col013 bigint, col014 char(8),
col015 bytea,col016 timestamp with time zone,col017 interval, col018 cidr, col019 inet, col020 macaddr,col022 money, col024 timetz,col025 circle, col026 box, col027 name,col028 path, col029 int2,
col031 bit varying(256),col032 date, col034 lseg,col035 point,col036 polygon,col037 real,col039 time, col040 timestamp ) with (appendonly=true) tablespace sync1_ts_a1 ;


INSERT INTO sync1_ao_table_ts_1 VALUES ('sync1_ao1',generate_series(1,10),'a',11,true,'111', repeat('text_',10), '{1,2,3,4,5}', 'Hello .. how are you 1', 'Hello .. how are you 1',    '{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd',
'2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA',   '34.23',   '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',
 '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_ao_table_ts_1;
--
--
-- ALTER AO TABLE TO DIFFERENT TABLESPACE
--
--
ALTER TABLE sync1_ao_table_ts_1 set TABLESPACE sync1_ts_b1;

INSERT INTO sync1_ao_table_ts_1 VALUES ('sync1_ao1',generate_series(1,10),'a',11,true,'111', repeat('text_',10), '{1,2,3,4,5}', 'Hello .. how are you 1', 'Hello .. how are you 1',    '{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd',
'2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA',   '34.23',   '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',
  '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_ao_table_ts_1;
--
--
-- ALTER AO TABLE TO TABLESPACE
--
--
CREATE TABLE sync1_ao_table_ts_11 ( phase text,a int,col001 char DEFAULT 'z',col002 numeric,col003 boolean DEFAULT false,col004 bit(3) DEFAULT '111',col005 text DEFAULT 'pookie', col006 integer[] DEFAULT '{5, 4, 3, 2, 1}',
col007 character varying(512) DEFAULT 'Now is the time', col008 character varying DEFAULT 'Now is the time', col009 character varying(512)[], col010 numeric(8),col011 int,col012 double precision, col013 bigint, col014 char(8),
col015 bytea,col016 timestamp with time zone,col017 interval, col018 cidr, col019 inet, col020 macaddr,col022 money, col024 timetz,col025 circle, col026 box, col027 name,col028 path, col029 int2,
col031 bit varying(256),col032 date, col034 lseg,col035 point,col036 polygon,col037 real,col039 time, col040 timestamp )  with (appendonly=true) ;


INSERT INTO sync1_ao_table_ts_11 VALUES ('sync1_ao1',generate_series(1,10),'a',11,true,'111', repeat('text_',10), '{1,2,3,4,5}', 'Hello .. how are you 1', 'Hello .. how are you 1',    '{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd',
'2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA',   '34.23',   '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',
 '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_ao_table_ts_11;

ALTER TABLE sync1_ao_table_ts_11 set TABLESPACE sync1_ts_c1;

--
-- CO TABLE 
--

--
--
-- CREATE CO TABLE IN TABLESPACE - JUST IN TIME
--
--
CREATE TABLE sync1_co_table_ts_1 ( phase text,a int,col001 char DEFAULT 'z',col002 numeric,col003 boolean DEFAULT false,col004 bit(3) DEFAULT '111',col005 text DEFAULT 'pookie', col006 integer[] DEFAULT '{5, 4, 3, 2, 1}',
col007 character varying(512) DEFAULT 'Now is the time', col008 character varying DEFAULT 'Now is the time', col009 character varying(512)[], col010 numeric(8),col011 int,col012 double precision, col013 bigint, col014 char(8),
col015 bytea,col016 timestamp with time zone,col017 interval, col018 cidr, col019 inet, col020 macaddr,col022 money, col024 timetz,col025 circle, col026 box, col027 name,col028 path, col029 int2,
col031 bit varying(256),col032 date, col034 lseg,col035 point,col036 polygon,col037 real,col039 time, col040 timestamp )  with ( appendonly='true', orientation='column') tablespace sync1_ts_a1 ;


INSERT INTO sync1_co_table_ts_1 VALUES ('sync1_co1',generate_series(1,10),'a',11,true,'111', repeat('text_',10), '{1,2,3,4,5}', 'Hello .. how are you 1', 'Hello .. how are you 1',    '{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd',
'2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA',   '34.23',   '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',
 '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_co_table_ts_1;
--
--
-- ALTER CO TABLE TO DIFFERENT TABLESPACE
--
--
ALTER TABLE sync1_co_table_ts_1 set TABLESPACE sync1_ts_b1;

INSERT INTO sync1_co_table_ts_1 VALUES ('sync1_co1',generate_series(1,10),'a',11,true,'111', repeat('text_',10), '{1,2,3,4,5}', 'Hello .. how are you 1', 'Hello .. how are you 1',    '{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd',
'2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA',   '34.23',   '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',
  '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_co_table_ts_1;
--
--
-- ALTER CO TABLE TO TABLESPACE
--
--
CREATE TABLE sync1_co_table_ts_11 ( phase text,a int,col001 char DEFAULT 'z',col002 numeric,col003 boolean DEFAULT false,col004 bit(3) DEFAULT '111',col005 text DEFAULT 'pookie', col006 integer[] DEFAULT '{5, 4, 3, 2, 1}',
col007 character varying(512) DEFAULT 'Now is the time', col008 character varying DEFAULT 'Now is the time', col009 character varying(512)[], col010 numeric(8),col011 int,col012 double precision, col013 bigint, col014 char(8),
col015 bytea,col016 timestamp with time zone,col017 interval, col018 cidr, col019 inet, col020 macaddr,col022 money, col024 timetz,col025 circle, col026 box, col027 name,col028 path, col029 int2,
col031 bit varying(256),col032 date, col034 lseg,col035 point,col036 polygon,col037 real,col039 time, col040 timestamp ) with ( appendonly='true', orientation='column')  ;


INSERT INTO sync1_co_table_ts_11 VALUES ('sync1_co1',generate_series(1,10),'a',11,true,'111', repeat('text_',10), '{1,2,3,4,5}', 'Hello .. how are you 1', 'Hello .. how are you 1',    '{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd',
'2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA',   '34.23',   '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',
 '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_co_table_ts_11;

ALTER TABLE sync1_co_table_ts_11 set TABLESPACE sync1_ts_c1;

