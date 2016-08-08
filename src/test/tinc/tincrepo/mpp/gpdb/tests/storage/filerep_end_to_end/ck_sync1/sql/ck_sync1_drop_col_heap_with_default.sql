-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- CK_SYNC1 TABLE ck_sync1_drop_column_heap_default1
--

CREATE TABLE ck_sync1_drop_column_heap_default1 ( a int,col001 char DEFAULT 'z',col002 numeric,col003 boolean DEFAULT false,col004 bit(3) DEFAULT '111',col005 text DEFAULT 'pookie', col006 integer[] DEFAULT '{5, 4, 3, 2, 1}', col007 character varying(512) DEFAULT 'Now is the time', col008 character varying DEFAULT 'Now is the time', col009 character varying(512)[], col010 numeric(8),col011 int,col012 double precision, col013 bigint, col014 char(8), col015 bytea,col016 timestamp with time zone,col017 interval, col018 cidr, col019 inet, col020 macaddr,col021 serial ,col022 money,col023 bigserial, col024 timetz,col025 circle, col026 box, col027 name,col028 path, col029 int2, 
col031 bit varying(256),col032 date, col034 lseg,col035 point,col036 polygon,col037 real,col039 time, col040 timestamp );

INSERT INTO ck_sync1_drop_column_heap_default1 VALUES (generate_series(1,10) ,'a',11,true,'111', '1_one', '{1,2,3,4,5}', 'Hello .. how are you', 'Hello .. how are you',    '{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd','2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');
SELECT count(*) FROM ck_sync1_drop_column_heap_default1;

--
-- CK_SYNC1 TABLE ck_sync1_drop_column_heap_default2
--

CREATE TABLE ck_sync1_drop_column_heap_default2 ( a int,col001 char DEFAULT 'z',col002 numeric,col003 boolean DEFAULT false,col004 bit(3) DEFAULT '111',col005 text DEFAULT 'pookie', col006 integer[] DEFAULT '{5, 4, 3, 2, 1}', col007 character varying(512) DEFAULT 'Now is the time', col008 character varying DEFAULT 'Now is the time', col009 character varying(512)[], col010 numeric(8),col011 int,col012 double precision, col013 bigint, col014 char(8), col015 bytea,col016 timestamp with time zone,col017 interval, col018 cidr, col019 inet, col020 macaddr,col021 serial ,col022 money,col023 bigserial, col024 timetz,col025 circle, col026 box, col027 name,col028 path, col029 int2, 
col031 bit varying(256),col032 date, col034 lseg,col035 point,col036 polygon,col037 real,col039 time, col040 timestamp );

INSERT INTO ck_sync1_drop_column_heap_default2 VALUES (generate_series(1,10) ,'a',11,true,'111', '1_one', '{1,2,3,4,5}', 'Hello .. how are you', 'Hello .. how are you',    '{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd','2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_drop_column_heap_default2;

--
-- CK_SYNC1 TABLE ck_sync1_drop_column_heap_default3
--

CREATE TABLE ck_sync1_drop_column_heap_default3 ( a int,col001 char DEFAULT 'z',col002 numeric,col003 boolean DEFAULT false,col004 bit(3) DEFAULT '111',col005 text DEFAULT 'pookie', col006 integer[] DEFAULT '{5, 4, 3, 2, 1}', col007 character varying(512) DEFAULT 'Now is the time', col008 character varying DEFAULT 'Now is the time', col009 character varying(512)[], col010 numeric(8),col011 int,col012 double precision, col013 bigint, col014 char(8), col015 bytea,col016 timestamp with time zone,col017 interval, col018 cidr, col019 inet, col020 macaddr,col021 serial ,col022 money,col023 bigserial, col024 timetz,col025 circle, col026 box, col027 name,col028 path, col029 int2, 
col031 bit varying(256),col032 date, col034 lseg,col035 point,col036 polygon,col037 real,col039 time, col040 timestamp );

INSERT INTO ck_sync1_drop_column_heap_default3 VALUES (generate_series(1,10) ,'a',11,true,'111', '1_one', '{1,2,3,4,5}', 'Hello .. how are you', 'Hello .. how are you',    '{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd','2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_drop_column_heap_default3;

--
-- CK_SYNC1 TABLE ck_sync1_drop_column_heap_default4
--

CREATE TABLE ck_sync1_drop_column_heap_default4 ( a int,col001 char DEFAULT 'z',col002 numeric,col003 boolean DEFAULT false,col004 bit(3) DEFAULT '111',col005 text DEFAULT 'pookie', col006 integer[] DEFAULT '{5, 4, 3, 2, 1}', col007 character varying(512) DEFAULT 'Now is the time', col008 character varying DEFAULT 'Now is the time', col009 character varying(512)[], col010 numeric(8),col011 int,col012 double precision, col013 bigint, col014 char(8), col015 bytea,col016 timestamp with time zone,col017 interval, col018 cidr, col019 inet, col020 macaddr,col021 serial ,col022 money,col023 bigserial, col024 timetz,col025 circle, col026 box, col027 name,col028 path, col029 int2, 
col031 bit varying(256),col032 date, col034 lseg,col035 point,col036 polygon,col037 real,col039 time, col040 timestamp );

INSERT INTO ck_sync1_drop_column_heap_default4 VALUES (generate_series(1,10) ,'a',11,true,'111', '1_one', '{1,2,3,4,5}', 'Hello .. how are you', 'Hello .. how are you',    '{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd','2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',  '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_drop_column_heap_default4;

--
-- CK_SYNC1 TABLE ck_sync1_drop_column_heap_default5
--

CREATE TABLE ck_sync1_drop_column_heap_default5 ( a int,col001 char DEFAULT 'z',col002 numeric,col003 boolean DEFAULT false,col004 bit(3) DEFAULT '111',col005 text DEFAULT 'pookie', col006 integer[] DEFAULT '{5, 4, 3, 2, 1}', col007 character varying(512) DEFAULT 'Now is the time', col008 character varying DEFAULT 'Now is the time', col009 character varying(512)[], col010 numeric(8),col011 int,col012 double precision, col013 bigint, col014 char(8), col015 bytea,col016 timestamp with time zone,col017 interval, col018 cidr, col019 inet, col020 macaddr,col021 serial ,col022 money,col023 bigserial, col024 timetz,col025 circle, col026 box, col027 name,col028 path, col029 int2, 
col031 bit varying(256),col032 date, col034 lseg,col035 point,col036 polygon,col037 real,col039 time, col040 timestamp );

INSERT INTO ck_sync1_drop_column_heap_default5 VALUES (generate_series(1,10) ,'a',11,true,'111', '1_one', '{1,2,3,4,5}', 'Hello .. how are you', 'Hello .. how are you',    '{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd','2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_drop_column_heap_default5;

--
-- CK_SYNC1 TABLE ck_sync1_drop_column_heap_default6
--

CREATE TABLE ck_sync1_drop_column_heap_default6 ( a int,col001 char DEFAULT 'z',col002 numeric,col003 boolean DEFAULT false,col004 bit(3) DEFAULT '111',col005 text DEFAULT 'pookie', col006 integer[] DEFAULT '{5, 4, 3, 2, 1}', col007 character varying(512) DEFAULT 'Now is the time', col008 character varying DEFAULT 'Now is the time', col009 character varying(512)[], col010 numeric(8),col011 int,col012 double precision, col013 bigint, col014 char(8), col015 bytea,col016 timestamp with time zone,col017 interval, col018 cidr, col019 inet, col020 macaddr,col021 serial ,col022 money,col023 bigserial, col024 timetz,col025 circle, col026 box, col027 name,col028 path, col029 int2, 
col031 bit varying(256),col032 date, col034 lseg,col035 point,col036 polygon,col037 real,col039 time, col040 timestamp );

INSERT INTO ck_sync1_drop_column_heap_default6 VALUES (generate_series(1,10) ,'a',11,true,'111', '1_one', '{1,2,3,4,5}', 'Hello .. how are you', 'Hello .. how are you',    '{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd','2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_drop_column_heap_default6;

--
-- CK_SYNC1 TABLE ck_sync1_drop_column_heap_default7
--

CREATE TABLE ck_sync1_drop_column_heap_default7 ( a int,col001 char DEFAULT 'z',col002 numeric,col003 boolean DEFAULT false,col004 bit(3) DEFAULT '111',col005 text DEFAULT 'pookie', col006 integer[] DEFAULT '{5, 4, 3, 2, 1}', col007 character varying(512) DEFAULT 'Now is the time', col008 character varying DEFAULT 'Now is the time', col009 character varying(512)[], col010 numeric(8),col011 int,col012 double precision, col013 bigint, col014 char(8), col015 bytea,col016 timestamp with time zone,col017 interval, col018 cidr, col019 inet, col020 macaddr,col021 serial ,col022 money,col023 bigserial, col024 timetz,col025 circle, col026 box, col027 name,col028 path, col029 int2, 
col031 bit varying(256),col032 date, col034 lseg,col035 point,col036 polygon,col037 real,col039 time, col040 timestamp );

INSERT INTO ck_sync1_drop_column_heap_default7 VALUES (generate_series(1,10) ,'a',11,true,'111', '1_one', '{1,2,3,4,5}', 'Hello .. how are you', 'Hello .. how are you',    '{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd','2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_drop_column_heap_default7;



--
-- CK_SYNC1 TABLE - used for insert select
--

CREATE TABLE ck_sync1_heap_drop ( a int,col001 char DEFAULT 'z',col002 numeric,col003 boolean DEFAULT false,col004 bit(3) DEFAULT '111',col005 text DEFAULT 'pookie', col006 integer[] DEFAULT '{5, 4, 3, 2, 1}', col007 character varying(512) DEFAULT 'Now is the time', col008 character varying DEFAULT 'Now is the time', col009 character varying(512)[], col010 numeric(8),col011 int,col012 double precision, col013 bigint, col014 char(8), col015 bytea,col016 timestamp with time zone,col017 interval, col018 cidr, col019 inet, col020 macaddr,col021 serial ,col022 money,col023 bigserial, col024 timetz,col025 circle, col026 box, col027 name,col028 path, col029 int2, 
col031 bit varying(256),col032 date, col034 lseg,col035 point,col036 polygon,col037 real,col039 time, col040 timestamp );

INSERT INTO ck_sync1_heap_drop VALUES (generate_series(1,10) ,'a',11,true,'111', '1_one', '{1,2,3,4,5}', 'Hello .. how are you', 'Hello .. how are you',    '{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd','2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_heap_drop;
--
--
--
-- DROP ALL COLUMN WITH DEFAULT TO  - > CK_SYNC1 TABLE ck_sync1_drop_column_heap_default1
--
--
--
ALTER TABLE ck_sync1_drop_column_heap_default1 DROP COLUMN col001;

INSERT INTO ck_sync1_drop_column_heap_default1 VALUES (generate_series(1,10) ,11,true,'111', '1_one', '{1,2,3,4,5}', 'Hello .. how are you', 'Hello .. how are you',    '{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd','2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;

 INSERT INTO ck_sync1_drop_column_heap_default1 SELECT a,col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col021, col022, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from ck_sync1_heap_drop;

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;
--
--
ALTER TABLE ck_sync1_drop_column_heap_default1 DROP COLUMN col002;

INSERT INTO ck_sync1_drop_column_heap_default1 VALUES (generate_series(1,10) ,true,'111', '1_one', '{1,2,3,4,5}', 'Hello .. how are you', 'Hello .. how are you',    '{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd','2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;

 INSERT INTO ck_sync1_drop_column_heap_default1 SELECT a,col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col021, col022, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from ck_sync1_heap_drop;

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;
--
--
ALTER TABLE ck_sync1_drop_column_heap_default1 DROP COLUMN col003;

INSERT INTO ck_sync1_drop_column_heap_default1 VALUES (generate_series(1,10) ,'111', '1_one', '{1,2,3,4,5}', 'Hello .. how are you', 'Hello .. how are you',    '{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd','2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;

 INSERT INTO ck_sync1_drop_column_heap_default1 SELECT a,col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col021, col022, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from ck_sync1_heap_drop;

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;
--
--
ALTER TABLE ck_sync1_drop_column_heap_default1 DROP COLUMN col004;

INSERT INTO ck_sync1_drop_column_heap_default1 VALUES (generate_series(1,10) ,'1_one',  '{1,2,3,4,5}', 'Hello .. how are you', 'Hello .. how are you',    '{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd','2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;

 INSERT INTO ck_sync1_drop_column_heap_default1 SELECT a,col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col021, col022, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from ck_sync1_heap_drop;

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;
--
--
ALTER TABLE ck_sync1_drop_column_heap_default1 DROP COLUMN col005;

INSERT INTO ck_sync1_drop_column_heap_default1 VALUES (generate_series(1,10) , '{1,2,3,4,5}', 'Hello .. how are you', 'Hello .. how are you',    '{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd','2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;

 INSERT INTO ck_sync1_drop_column_heap_default1 SELECT a,col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col021, col022, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from ck_sync1_heap_drop;

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;
--
--
ALTER TABLE ck_sync1_drop_column_heap_default1 DROP COLUMN col006;

INSERT INTO ck_sync1_drop_column_heap_default1 VALUES (generate_series(1,10) , 'Hello .. how are you',  'Hello .. how are you',    '{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd','2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;

 INSERT INTO ck_sync1_drop_column_heap_default1 SELECT a, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col021, col022, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from ck_sync1_heap_drop;

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;
--
--
ALTER TABLE ck_sync1_drop_column_heap_default1 DROP COLUMN col007;

INSERT INTO ck_sync1_drop_column_heap_default1 VALUES (generate_series(1,10) ,  'Hello .. how are you',   '{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd','2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;

 INSERT INTO ck_sync1_drop_column_heap_default1 SELECT a, col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col021, col022, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from ck_sync1_heap_drop;

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;
--
--
ALTER TABLE ck_sync1_drop_column_heap_default1 DROP COLUMN col008;

INSERT INTO ck_sync1_drop_column_heap_default1 VALUES (generate_series(1,10) ,'{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd','2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;

 INSERT INTO ck_sync1_drop_column_heap_default1 SELECT a, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col021, col022, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from ck_sync1_heap_drop;

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;
--
--
ALTER TABLE ck_sync1_drop_column_heap_default1 DROP COLUMN col009;

INSERT INTO ck_sync1_drop_column_heap_default1 VALUES (generate_series(1,10) ,12345678, 1, 111.1111,  11,  '1_one_11',   'd','2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;

 INSERT INTO ck_sync1_drop_column_heap_default1 SELECT a,col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col021, col022, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from ck_sync1_heap_drop;

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;
--
--
ALTER TABLE ck_sync1_drop_column_heap_default1 DROP COLUMN col010;

INSERT INTO ck_sync1_drop_column_heap_default1 VALUES (generate_series(1,10) ,1, 111.1111,  11,  '1_one_11',   'd','2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;

 INSERT INTO ck_sync1_drop_column_heap_default1 SELECT a,col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col021, col022, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from ck_sync1_heap_drop;

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;
--
--
ALTER TABLE ck_sync1_drop_column_heap_default1 DROP COLUMN col011;

INSERT INTO ck_sync1_drop_column_heap_default1 VALUES (generate_series(1,10) , 111.1111,  11,  '1_one_11',   'd','2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;

 INSERT INTO ck_sync1_drop_column_heap_default1 SELECT a,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col021, col022, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from ck_sync1_heap_drop;

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;
--
--
ALTER TABLE ck_sync1_drop_column_heap_default1 DROP COLUMN col012;

INSERT INTO ck_sync1_drop_column_heap_default1 VALUES (generate_series(1,10) , 11, '1_one_11',   'd','2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;

 INSERT INTO ck_sync1_drop_column_heap_default1 SELECT a,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col021, col022, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from ck_sync1_heap_drop;

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;
--
--
ALTER TABLE ck_sync1_drop_column_heap_default1 DROP COLUMN col013;

INSERT INTO ck_sync1_drop_column_heap_default1 VALUES (generate_series(1,10) ,'1_one_11',  'd','2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;

 INSERT INTO ck_sync1_drop_column_heap_default1 SELECT a,col014, col015 , col016,  col017 ,col018 ,col019, col020, col021, col022, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from ck_sync1_heap_drop;

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;
--
--
ALTER TABLE ck_sync1_drop_column_heap_default1 DROP COLUMN col014;

INSERT INTO ck_sync1_drop_column_heap_default1 VALUES (generate_series(1,10) ,'d','2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;

 INSERT INTO ck_sync1_drop_column_heap_default1 SELECT a, col015 , col016,  col017 ,col018 ,col019, col020, col021, col022, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from ck_sync1_heap_drop;

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;
--
--
ALTER TABLE ck_sync1_drop_column_heap_default1 DROP COLUMN col015;

INSERT INTO ck_sync1_drop_column_heap_default1 VALUES (generate_series(1,10) ,'2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;

 INSERT INTO ck_sync1_drop_column_heap_default1 SELECT a,col016,  col017 ,col018 ,col019, col020, col021, col022, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from ck_sync1_heap_drop;

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;
--
--
ALTER TABLE ck_sync1_drop_column_heap_default1 DROP COLUMN col016;

INSERT INTO ck_sync1_drop_column_heap_default1 VALUES (generate_series(1,10) ,  '11', '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;

 INSERT INTO ck_sync1_drop_column_heap_default1 SELECT a,  col017 ,col018 ,col019, col020, col021, col022, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from ck_sync1_heap_drop;

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;
--
--
ALTER TABLE ck_sync1_drop_column_heap_default1 DROP COLUMN col017;

INSERT INTO ck_sync1_drop_column_heap_default1 VALUES (generate_series(1,10) ,'0.0.0.0','0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;

 INSERT INTO ck_sync1_drop_column_heap_default1 SELECT a,col018 ,col019, col020, col021, col022, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from ck_sync1_heap_drop;

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;
--
--
ALTER TABLE ck_sync1_drop_column_heap_default1 DROP COLUMN col018;

INSERT INTO ck_sync1_drop_column_heap_default1 VALUES (generate_series(1,10) ,'0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;

 INSERT INTO ck_sync1_drop_column_heap_default1 SELECT a,col019, col020, col021, col022, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from ck_sync1_heap_drop;

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;
--
--
ALTER TABLE ck_sync1_drop_column_heap_default1 DROP COLUMN col019;

INSERT INTO ck_sync1_drop_column_heap_default1 VALUES (generate_series(1,10) ,'AA:AA:AA:AA:AA:AA',  generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;

 INSERT INTO ck_sync1_drop_column_heap_default1 SELECT a,col020, col021, col022, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from ck_sync1_heap_drop;

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;
--
--
ALTER TABLE ck_sync1_drop_column_heap_default1 DROP COLUMN col020;

INSERT INTO ck_sync1_drop_column_heap_default1 VALUES (generate_series(1,10) ,  generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;

 INSERT INTO ck_sync1_drop_column_heap_default1 SELECT a, col021, col022, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from ck_sync1_heap_drop;

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;
--
--
ALTER TABLE ck_sync1_drop_column_heap_default1 DROP COLUMN col021;

INSERT INTO ck_sync1_drop_column_heap_default1 VALUES (generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;

 INSERT INTO ck_sync1_drop_column_heap_default1 SELECT a, col022, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from ck_sync1_heap_drop;

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;
--
--
ALTER TABLE ck_sync1_drop_column_heap_default1 DROP COLUMN col022;

INSERT INTO ck_sync1_drop_column_heap_default1 VALUES (generate_series(1,10) , generate_series(1,10),'00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;

 INSERT INTO ck_sync1_drop_column_heap_default1 SELECT a, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from ck_sync1_heap_drop;

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;
--
--
ALTER TABLE ck_sync1_drop_column_heap_default1 DROP COLUMN col023;

INSERT INTO ck_sync1_drop_column_heap_default1 VALUES (generate_series(1,10) , '00:00:00+1359', '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;

 INSERT INTO ck_sync1_drop_column_heap_default1 SELECT a,col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from ck_sync1_heap_drop;

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;
--
--
ALTER TABLE ck_sync1_drop_column_heap_default1 DROP COLUMN col024;

INSERT INTO ck_sync1_drop_column_heap_default1 VALUES (generate_series(1,10) ,   '((2,2),1)','((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;

 INSERT INTO ck_sync1_drop_column_heap_default1 SELECT a,col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from ck_sync1_heap_drop;

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;
--
--
ALTER TABLE ck_sync1_drop_column_heap_default1 DROP COLUMN col025;

INSERT INTO ck_sync1_drop_column_heap_default1 VALUES (generate_series(1,10) , '((1,2),(2,1))',  'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;

 INSERT INTO ck_sync1_drop_column_heap_default1 SELECT a,col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from ck_sync1_heap_drop;

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;
--
--
ALTER TABLE ck_sync1_drop_column_heap_default1 DROP COLUMN col026;

INSERT INTO ck_sync1_drop_column_heap_default1 VALUES (generate_series(1,10) ,'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;

 INSERT INTO ck_sync1_drop_column_heap_default1 SELECT a,col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from ck_sync1_heap_drop;

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;
--
--
ALTER TABLE ck_sync1_drop_column_heap_default1 DROP COLUMN col027;

INSERT INTO ck_sync1_drop_column_heap_default1 VALUES (generate_series(1,10) ,'((1,2),(2,1))',11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;

 INSERT INTO ck_sync1_drop_column_heap_default1 SELECT a,col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from ck_sync1_heap_drop;

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;
--
--
ALTER TABLE ck_sync1_drop_column_heap_default1 DROP COLUMN col028;

INSERT INTO ck_sync1_drop_column_heap_default1 VALUES (generate_series(1,10) ,  11, '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;

 INSERT INTO ck_sync1_drop_column_heap_default1 SELECT a,col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from ck_sync1_heap_drop;

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;
--
--
ALTER TABLE ck_sync1_drop_column_heap_default1 DROP COLUMN col029;

INSERT INTO ck_sync1_drop_column_heap_default1 VALUES (generate_series(1,10) , '010101',  '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;

 INSERT INTO ck_sync1_drop_column_heap_default1 SELECT a, col031, col032 , col034 , col035, col036 , col037, col039 , col040 from ck_sync1_heap_drop;

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;
--
--
ALTER TABLE ck_sync1_drop_column_heap_default1 DROP COLUMN col031;

INSERT INTO ck_sync1_drop_column_heap_default1 VALUES (generate_series(1,10) , '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;

 INSERT INTO ck_sync1_drop_column_heap_default1 SELECT a, col032 , col034 , col035, col036 , col037, col039 , col040 from ck_sync1_heap_drop;

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;
--
--
ALTER TABLE ck_sync1_drop_column_heap_default1 DROP COLUMN col032;

INSERT INTO ck_sync1_drop_column_heap_default1 VALUES (generate_series(1,10) ,  '((1,1),(2,2))','(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;

 INSERT INTO ck_sync1_drop_column_heap_default1 SELECT a,col034 , col035, col036 , col037, col039 , col040 from ck_sync1_heap_drop;

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;

--
--
ALTER TABLE ck_sync1_drop_column_heap_default1 DROP COLUMN col034;

INSERT INTO ck_sync1_drop_column_heap_default1 VALUES (generate_series(1,10) , '(1,1)','((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;

 INSERT INTO ck_sync1_drop_column_heap_default1 SELECT a,col035, col036 , col037, col039 , col040 from ck_sync1_heap_drop;

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;
--
--
ALTER TABLE ck_sync1_drop_column_heap_default1 DROP COLUMN col035;

INSERT INTO ck_sync1_drop_column_heap_default1 VALUES (generate_series(1,10) ,'((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))', 111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;

 INSERT INTO ck_sync1_drop_column_heap_default1 SELECT a, col036 , col037, col039 , col040 from ck_sync1_heap_drop;

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;
--
--
ALTER TABLE ck_sync1_drop_column_heap_default1 DROP COLUMN col036;

INSERT INTO ck_sync1_drop_column_heap_default1 VALUES (generate_series(1,10) , 111111,'23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;

 INSERT INTO ck_sync1_drop_column_heap_default1 SELECT a, col037, col039 , col040 from ck_sync1_heap_drop;

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;
--
--
ALTER TABLE ck_sync1_drop_column_heap_default1 DROP COLUMN col037;

INSERT INTO ck_sync1_drop_column_heap_default1 VALUES (generate_series(1,10) , '23:00:00',  '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;

 INSERT INTO ck_sync1_drop_column_heap_default1 SELECT a,col039 , col040 from ck_sync1_heap_drop;

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;
--
--
ALTER TABLE ck_sync1_drop_column_heap_default1 DROP COLUMN col039;

INSERT INTO ck_sync1_drop_column_heap_default1 VALUES (generate_series(1,10) , '2001-12-13 01:51:15');

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;

 INSERT INTO ck_sync1_drop_column_heap_default1 SELECT a,col040 from ck_sync1_heap_drop;

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;
--
--
ALTER TABLE ck_sync1_drop_column_heap_default1 DROP COLUMN col040;

INSERT INTO ck_sync1_drop_column_heap_default1 VALUES (generate_series(1,10) );

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;

 INSERT INTO ck_sync1_drop_column_heap_default1 SELECT a from ck_sync1_heap_drop;

SELECT count(*) FROM ck_sync1_drop_column_heap_default1;

--
--
--
-- DROP ALL COLUMN WITH DEFAULT TO  - > SYNC1 TABLE sync1_drop_column_heap_default2
--
--
--
ALTER TABLE sync1_drop_column_heap_default2 DROP COLUMN col001;

INSERT INTO sync1_drop_column_heap_default2 VALUES (generate_series(1,10) ,11,true,'111', '1_one', '{1,2,3,4,5}', 'Hello .. how are you', 'Hello .. how are you',    '{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd','2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_drop_column_heap_default2;

 INSERT INTO sync1_drop_column_heap_default2 SELECT a,col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col021, col022, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from sync1_heap_drop;

SELECT count(*) FROM sync1_drop_column_heap_default2;
--
--
ALTER TABLE sync1_drop_column_heap_default2 DROP COLUMN col002;

INSERT INTO sync1_drop_column_heap_default2 VALUES (generate_series(1,10) ,true,'111', '1_one', '{1,2,3,4,5}', 'Hello .. how are you', 'Hello .. how are you',    '{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd','2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_drop_column_heap_default2;

 INSERT INTO sync1_drop_column_heap_default2 SELECT a,col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col021, col022, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from sync1_heap_drop;

SELECT count(*) FROM sync1_drop_column_heap_default2;
--
--
ALTER TABLE sync1_drop_column_heap_default2 DROP COLUMN col003;

INSERT INTO sync1_drop_column_heap_default2 VALUES (generate_series(1,10) ,'111', '1_one', '{1,2,3,4,5}', 'Hello .. how are you', 'Hello .. how are you',    '{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd','2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_drop_column_heap_default2;

 INSERT INTO sync1_drop_column_heap_default2 SELECT a,col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col021, col022, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from sync1_heap_drop;

SELECT count(*) FROM sync1_drop_column_heap_default2;
--
--
ALTER TABLE sync1_drop_column_heap_default2 DROP COLUMN col004;

INSERT INTO sync1_drop_column_heap_default2 VALUES (generate_series(1,10) ,'1_one',  '{1,2,3,4,5}', 'Hello .. how are you', 'Hello .. how are you',    '{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd','2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_drop_column_heap_default2;

 INSERT INTO sync1_drop_column_heap_default2 SELECT a,col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col021, col022, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from sync1_heap_drop;

SELECT count(*) FROM sync1_drop_column_heap_default2;
--
--
ALTER TABLE sync1_drop_column_heap_default2 DROP COLUMN col005;

INSERT INTO sync1_drop_column_heap_default2 VALUES (generate_series(1,10) , '{1,2,3,4,5}', 'Hello .. how are you', 'Hello .. how are you',    '{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd','2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_drop_column_heap_default2;

 INSERT INTO sync1_drop_column_heap_default2 SELECT a,col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col021, col022, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from sync1_heap_drop;

SELECT count(*) FROM sync1_drop_column_heap_default2;
--
--
ALTER TABLE sync1_drop_column_heap_default2 DROP COLUMN col006;

INSERT INTO sync1_drop_column_heap_default2 VALUES (generate_series(1,10) , 'Hello .. how are you',  'Hello .. how are you',    '{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd','2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_drop_column_heap_default2;

 INSERT INTO sync1_drop_column_heap_default2 SELECT a, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col021, col022, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from sync1_heap_drop;

SELECT count(*) FROM sync1_drop_column_heap_default2;
--
--
ALTER TABLE sync1_drop_column_heap_default2 DROP COLUMN col007;

INSERT INTO sync1_drop_column_heap_default2 VALUES (generate_series(1,10) ,  'Hello .. how are you',   '{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd','2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_drop_column_heap_default2;

 INSERT INTO sync1_drop_column_heap_default2 SELECT a, col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col021, col022, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from sync1_heap_drop;

SELECT count(*) FROM sync1_drop_column_heap_default2;
--
--
ALTER TABLE sync1_drop_column_heap_default2 DROP COLUMN col008;

INSERT INTO sync1_drop_column_heap_default2 VALUES (generate_series(1,10) ,'{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd','2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_drop_column_heap_default2;

 INSERT INTO sync1_drop_column_heap_default2 SELECT a, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col021, col022, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from sync1_heap_drop;

SELECT count(*) FROM sync1_drop_column_heap_default2;
--
--
ALTER TABLE sync1_drop_column_heap_default2 DROP COLUMN col009;

INSERT INTO sync1_drop_column_heap_default2 VALUES (generate_series(1,10) ,12345678, 1, 111.1111,  11,  '1_one_11',   'd','2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_drop_column_heap_default2;

 INSERT INTO sync1_drop_column_heap_default2 SELECT a,col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col021, col022, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from sync1_heap_drop;

SELECT count(*) FROM sync1_drop_column_heap_default2;
--
--
ALTER TABLE sync1_drop_column_heap_default2 DROP COLUMN col010;

INSERT INTO sync1_drop_column_heap_default2 VALUES (generate_series(1,10) ,1, 111.1111,  11,  '1_one_11',   'd','2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_drop_column_heap_default2;

 INSERT INTO sync1_drop_column_heap_default2 SELECT a,col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col021, col022, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from sync1_heap_drop;

SELECT count(*) FROM sync1_drop_column_heap_default2;
--
--
ALTER TABLE sync1_drop_column_heap_default2 DROP COLUMN col011;

INSERT INTO sync1_drop_column_heap_default2 VALUES (generate_series(1,10) , 111.1111,  11,  '1_one_11',   'd','2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_drop_column_heap_default2;

 INSERT INTO sync1_drop_column_heap_default2 SELECT a,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col021, col022, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from sync1_heap_drop;

SELECT count(*) FROM sync1_drop_column_heap_default2;
--
--
ALTER TABLE sync1_drop_column_heap_default2 DROP COLUMN col012;

INSERT INTO sync1_drop_column_heap_default2 VALUES (generate_series(1,10) , 11, '1_one_11',   'd','2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_drop_column_heap_default2;

 INSERT INTO sync1_drop_column_heap_default2 SELECT a,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col021, col022, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from sync1_heap_drop;

SELECT count(*) FROM sync1_drop_column_heap_default2;
--
--
ALTER TABLE sync1_drop_column_heap_default2 DROP COLUMN col013;

INSERT INTO sync1_drop_column_heap_default2 VALUES (generate_series(1,10) ,'1_one_11',  'd','2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_drop_column_heap_default2;

 INSERT INTO sync1_drop_column_heap_default2 SELECT a,col014, col015 , col016,  col017 ,col018 ,col019, col020, col021, col022, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from sync1_heap_drop;

SELECT count(*) FROM sync1_drop_column_heap_default2;
--
--
ALTER TABLE sync1_drop_column_heap_default2 DROP COLUMN col014;

INSERT INTO sync1_drop_column_heap_default2 VALUES (generate_series(1,10) ,'d','2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_drop_column_heap_default2;

 INSERT INTO sync1_drop_column_heap_default2 SELECT a, col015 , col016,  col017 ,col018 ,col019, col020, col021, col022, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from sync1_heap_drop;

SELECT count(*) FROM sync1_drop_column_heap_default2;
--
--
ALTER TABLE sync1_drop_column_heap_default2 DROP COLUMN col015;

INSERT INTO sync1_drop_column_heap_default2 VALUES (generate_series(1,10) ,'2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_drop_column_heap_default2;

 INSERT INTO sync1_drop_column_heap_default2 SELECT a,col016,  col017 ,col018 ,col019, col020, col021, col022, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from sync1_heap_drop;

SELECT count(*) FROM sync1_drop_column_heap_default2;
--
--
ALTER TABLE sync1_drop_column_heap_default2 DROP COLUMN col016;

INSERT INTO sync1_drop_column_heap_default2 VALUES (generate_series(1,10) ,  '11', '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_drop_column_heap_default2;

 INSERT INTO sync1_drop_column_heap_default2 SELECT a,  col017 ,col018 ,col019, col020, col021, col022, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from sync1_heap_drop;

SELECT count(*) FROM sync1_drop_column_heap_default2;
--
--
ALTER TABLE sync1_drop_column_heap_default2 DROP COLUMN col017;

INSERT INTO sync1_drop_column_heap_default2 VALUES (generate_series(1,10) ,'0.0.0.0','0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_drop_column_heap_default2;

 INSERT INTO sync1_drop_column_heap_default2 SELECT a,col018 ,col019, col020, col021, col022, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from sync1_heap_drop;

SELECT count(*) FROM sync1_drop_column_heap_default2;
--
--
ALTER TABLE sync1_drop_column_heap_default2 DROP COLUMN col018;

INSERT INTO sync1_drop_column_heap_default2 VALUES (generate_series(1,10) ,'0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_drop_column_heap_default2;

 INSERT INTO sync1_drop_column_heap_default2 SELECT a,col019, col020, col021, col022, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from sync1_heap_drop;

SELECT count(*) FROM sync1_drop_column_heap_default2;
--
--
ALTER TABLE sync1_drop_column_heap_default2 DROP COLUMN col019;

INSERT INTO sync1_drop_column_heap_default2 VALUES (generate_series(1,10) ,'AA:AA:AA:AA:AA:AA',  generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_drop_column_heap_default2;

 INSERT INTO sync1_drop_column_heap_default2 SELECT a,col020, col021, col022, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from sync1_heap_drop;

SELECT count(*) FROM sync1_drop_column_heap_default2;
--
--
ALTER TABLE sync1_drop_column_heap_default2 DROP COLUMN col020;

INSERT INTO sync1_drop_column_heap_default2 VALUES (generate_series(1,10) ,  generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_drop_column_heap_default2;

 INSERT INTO sync1_drop_column_heap_default2 SELECT a, col021, col022, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from sync1_heap_drop;

SELECT count(*) FROM sync1_drop_column_heap_default2;
--
--
ALTER TABLE sync1_drop_column_heap_default2 DROP COLUMN col021;

INSERT INTO sync1_drop_column_heap_default2 VALUES (generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_drop_column_heap_default2;

 INSERT INTO sync1_drop_column_heap_default2 SELECT a, col022, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from sync1_heap_drop;

SELECT count(*) FROM sync1_drop_column_heap_default2;
--
--
ALTER TABLE sync1_drop_column_heap_default2 DROP COLUMN col022;

INSERT INTO sync1_drop_column_heap_default2 VALUES (generate_series(1,10) , generate_series(1,10),'00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_drop_column_heap_default2;

 INSERT INTO sync1_drop_column_heap_default2 SELECT a, col023, col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from sync1_heap_drop;

SELECT count(*) FROM sync1_drop_column_heap_default2;
--
--
ALTER TABLE sync1_drop_column_heap_default2 DROP COLUMN col023;

INSERT INTO sync1_drop_column_heap_default2 VALUES (generate_series(1,10) , '00:00:00+1359', '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_drop_column_heap_default2;

 INSERT INTO sync1_drop_column_heap_default2 SELECT a,col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from sync1_heap_drop;

SELECT count(*) FROM sync1_drop_column_heap_default2;
--
--
ALTER TABLE sync1_drop_column_heap_default2 DROP COLUMN col024;

INSERT INTO sync1_drop_column_heap_default2 VALUES (generate_series(1,10) ,   '((2,2),1)','((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_drop_column_heap_default2;

 INSERT INTO sync1_drop_column_heap_default2 SELECT a,col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from sync1_heap_drop;

SELECT count(*) FROM sync1_drop_column_heap_default2;
--
--
ALTER TABLE sync1_drop_column_heap_default2 DROP COLUMN col025;

INSERT INTO sync1_drop_column_heap_default2 VALUES (generate_series(1,10) , '((1,2),(2,1))',  'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_drop_column_heap_default2;

 INSERT INTO sync1_drop_column_heap_default2 SELECT a,col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from sync1_heap_drop;

SELECT count(*) FROM sync1_drop_column_heap_default2;
--
--
ALTER TABLE sync1_drop_column_heap_default2 DROP COLUMN col026;

INSERT INTO sync1_drop_column_heap_default2 VALUES (generate_series(1,10) ,'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_drop_column_heap_default2;

 INSERT INTO sync1_drop_column_heap_default2 SELECT a,col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from sync1_heap_drop;

SELECT count(*) FROM sync1_drop_column_heap_default2;
--
--
ALTER TABLE sync1_drop_column_heap_default2 DROP COLUMN col027;

INSERT INTO sync1_drop_column_heap_default2 VALUES (generate_series(1,10) ,'((1,2),(2,1))',11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_drop_column_heap_default2;

 INSERT INTO sync1_drop_column_heap_default2 SELECT a,col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from sync1_heap_drop;

SELECT count(*) FROM sync1_drop_column_heap_default2;
--
--
ALTER TABLE sync1_drop_column_heap_default2 DROP COLUMN col028;

INSERT INTO sync1_drop_column_heap_default2 VALUES (generate_series(1,10) ,  11, '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_drop_column_heap_default2;

 INSERT INTO sync1_drop_column_heap_default2 SELECT a,col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from sync1_heap_drop;

SELECT count(*) FROM sync1_drop_column_heap_default2;
--
--
ALTER TABLE sync1_drop_column_heap_default2 DROP COLUMN col029;

INSERT INTO sync1_drop_column_heap_default2 VALUES (generate_series(1,10) , '010101',  '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_drop_column_heap_default2;

 INSERT INTO sync1_drop_column_heap_default2 SELECT a, col031, col032 , col034 , col035, col036 , col037, col039 , col040 from sync1_heap_drop;

SELECT count(*) FROM sync1_drop_column_heap_default2;
--
--
ALTER TABLE sync1_drop_column_heap_default2 DROP COLUMN col031;

INSERT INTO sync1_drop_column_heap_default2 VALUES (generate_series(1,10) , '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_drop_column_heap_default2;

 INSERT INTO sync1_drop_column_heap_default2 SELECT a, col032 , col034 , col035, col036 , col037, col039 , col040 from sync1_heap_drop;

SELECT count(*) FROM sync1_drop_column_heap_default2;
--
--
ALTER TABLE sync1_drop_column_heap_default2 DROP COLUMN col032;

INSERT INTO sync1_drop_column_heap_default2 VALUES (generate_series(1,10) ,  '((1,1),(2,2))','(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_drop_column_heap_default2;

 INSERT INTO sync1_drop_column_heap_default2 SELECT a,col034 , col035, col036 , col037, col039 , col040 from sync1_heap_drop;

SELECT count(*) FROM sync1_drop_column_heap_default2;

--
--
ALTER TABLE sync1_drop_column_heap_default2 DROP COLUMN col034;

INSERT INTO sync1_drop_column_heap_default2 VALUES (generate_series(1,10) , '(1,1)','((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_drop_column_heap_default2;

 INSERT INTO sync1_drop_column_heap_default2 SELECT a,col035, col036 , col037, col039 , col040 from sync1_heap_drop;

SELECT count(*) FROM sync1_drop_column_heap_default2;
--
--
ALTER TABLE sync1_drop_column_heap_default2 DROP COLUMN col035;

INSERT INTO sync1_drop_column_heap_default2 VALUES (generate_series(1,10) ,'((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))', 111111, '23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_drop_column_heap_default2;

 INSERT INTO sync1_drop_column_heap_default2 SELECT a, col036 , col037, col039 , col040 from sync1_heap_drop;

SELECT count(*) FROM sync1_drop_column_heap_default2;
--
--
ALTER TABLE sync1_drop_column_heap_default2 DROP COLUMN col036;

INSERT INTO sync1_drop_column_heap_default2 VALUES (generate_series(1,10) , 111111,'23:00:00',   '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_drop_column_heap_default2;

 INSERT INTO sync1_drop_column_heap_default2 SELECT a, col037, col039 , col040 from sync1_heap_drop;

SELECT count(*) FROM sync1_drop_column_heap_default2;
--
--
ALTER TABLE sync1_drop_column_heap_default2 DROP COLUMN col037;

INSERT INTO sync1_drop_column_heap_default2 VALUES (generate_series(1,10) , '23:00:00',  '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_drop_column_heap_default2;

 INSERT INTO sync1_drop_column_heap_default2 SELECT a,col039 , col040 from sync1_heap_drop;

SELECT count(*) FROM sync1_drop_column_heap_default2;
--
--
ALTER TABLE sync1_drop_column_heap_default2 DROP COLUMN col039;

INSERT INTO sync1_drop_column_heap_default2 VALUES (generate_series(1,10) , '2001-12-13 01:51:15');

SELECT count(*) FROM sync1_drop_column_heap_default2;

 INSERT INTO sync1_drop_column_heap_default2 SELECT a,col040 from sync1_heap_drop;

SELECT count(*) FROM sync1_drop_column_heap_default2;
--
--
ALTER TABLE sync1_drop_column_heap_default2 DROP COLUMN col040;

INSERT INTO sync1_drop_column_heap_default2 VALUES (generate_series(1,10) );

SELECT count(*) FROM sync1_drop_column_heap_default2;

 INSERT INTO sync1_drop_column_heap_default2 SELECT a from sync1_heap_drop;

SELECT count(*) FROM sync1_drop_column_heap_default2;


