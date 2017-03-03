\c gptest;

-- start_ignore
DROP TABLE  IF EXISTS ao_table;
DROP TABLE  IF EXISTS new_ao_table;
-- end_ignore
CREATE TABLE ao_table ( a int,col001 char DEFAULT 'z',col002 numeric,col003 boolean DEFAULT false,col004 bit(3) DEFAULT '111',col005 text DEFAULT 'pookie', col006 integer[] DEFAULT '{5, 4, 3, 2, 1}', col007 character varying(512) DEFAULT 'Now is the time', col008 character varying DEFAULT 'Now is the time', col009 character varying(512)[], col010 numeric(8),col011 int,col012 double precision, col013 bigint, col014 char(8), col015 bytea,col016 timestamp with time zone,col017 interval, col018 cidr, col019 inet, col020 macaddr,col022 money, col025 circle, col026 box, col027 name,col028 path, col029 int2, col031 bit varying(256),col032 date, col034 lseg,col035 point,col036 polygon,col037 real,col039 time, col040 timestamp )with (appendonly=true);


INSERT INTO ao_table VALUES (1,'a',11,true,'111', '1_one', '{1,2,3,4,5}', 'Hello .. how are you 1', 'Hello .. how are you 1',    '{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd',  '2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA',   '34.23',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

INSERT INTO ao_table VALUES (    2,   'b',   22,  false, '001',   '2_one',  '{6,7,8,9,10}',  'Hey.. whtz up 2', 'Hey .. whtz up 2',    '{one,two,three,four,five}', 76767669, 1, 222.2222, 11,   '2_two_22',   'c',   '2002-12-13 01:51:15+1359',   '22',    '0.0.0.0',  '0.0.0.0',  'BB:BB:BB:BB:BB:BB', '200.00',  '((3,3),2)',  '((3,2),(2,3))',   'hello',  '((3,2),(2,3))', 22,    '01010100101',  '2002-12-13', '((2,2),(3,3))', '(2,2)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))', 11111,  '22:00:00', '2002-12-13 01:51:15');


CREATE TABLE new_ao_table ( a int,col001 char DEFAULT 'z',col002 numeric,col003 boolean DEFAULT false,col004 bit(3) DEFAULT '111',col005 text DEFAULT 'pookie', col006 integer[] DEFAULT '{5, 4, 3, 2, 1}', col007 character varying(512) DEFAULT 'Now is the time', col008 character varying DEFAULT 'Now is the time', col009 character varying(512)[], col010 numeric(8),col011 int,col012 double precision, col013 bigint, col014 char(8), col015 bytea,col016 timestamp with time zone,col017 interval, col018 cidr, col019 inet, col020 macaddr,col022 money, col025 circle, col026 box, col027 name,col028 path, col029 int2, col031 bit varying(256),col032 date, col034 lseg,col035 point,col036 polygon,col037 real,col039 time, col040 timestamp )with (appendonly=true);


INSERT INTO new_ao_table VALUES (generate_series(500,505),'a',generate_series(500,505),true,'111', '1_one', '{1,2,3,4,5}', 'Hello .. how are you 1', 'Hello .. how are you 1',    '{one,two,three,four,five}',  generate_series(500,505), generate_series(500,505), 111.1111,  generate_series(500,505),  '1_one_11',   'd',  '2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA',   '34.23',   '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');

