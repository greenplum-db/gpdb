-- 
-- @created 2009-01-27 14:00:00
-- @modified 2013-06-24 17:00:00
-- @tags ddl schema_topology

set time zone PST8PDT;

-- Create heap table

   CREATE TABLE sch_fsts_table (
          text_col text,
          bigint_col bigint,
          char_vary_col character varying(30),
          numeric_col numeric
          ) tablespace ts_sch1 DISTRIBUTED RANDOMLY;
          
          
-- Insert few records into the table
   INSERT into sch_fsts_table values ('text_1' , 55667788,'hello .. hi there',1);
   INSERT into sch_fsts_table values ('text_2' , 55667788,'hello .. hi there',2);
   INSERT into sch_fsts_table values ('text_3' , 55667788,'hello .. hi there',3);

-- SELECT from the Table
   SELECT * from sch_fsts_table;

-- ALTER the table to new table space ts_b1
   ALTER table sch_fsts_table set tablespace ts_sch3;

-- ALTER the table to add a column with default
   ALTER table sch_fsts_table add column new_col int default 10;

-- SELECT from the Table
   SELECT * from sch_fsts_table;
          
   

-- Create AO table
   CREATE TABLE sch_fsts_table_ao ( a int,col001 char DEFAULT 'z',col002 numeric,col003 boolean DEFAULT false,col004 bit(3) DEFAULT
   '111',col005 text DEFAULT 'pookie',col006 integer[] DEFAULT '{5, 4, 3, 2, 1}', col007 character varying(512) DEFAULT 'Now is the time',
   col008 character varying DEFAULT 'Now is the time', col009 character varying(512)[], col010 numeric(8),col011 int,col012 double precision,
   col013 bigint, col014 char(8), col015 bytea,col016 timestamp with time zone,col017 interval, col018 cidr, col019 inet, col020 macaddr,
   col022 money, col024 timetz,col025 circle, col026 box, col027 name,col028 path, col029 int2, col031 bit varying(256),col032 date,
   col034 lseg,col035 point,col036 polygon,col037 real,col039 time, col040 timestamp ) with (appendonly=true) tablespace ts_sch3;

-- create an index for the creation of block dir
   CREATE index sch_fsts_table_ao_index on sch_fsts_table_ao(a);
   
-- Insert few records into the table
   INSERT INTO sch_fsts_table_ao VALUES (1,'a',11,true,'111', '1_one', '{1,2,3,4,5}', 'Hello .. how are you 1', 'Hello .. how are you 1',    '{one,two,three,four,five}',12345678, 1, 111.1111,  11,  '1_one_11',   'd',  '2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA',   '34.23',   '00:00:00+1359',  '((2,2),1)',  '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00','2001-12-13 01:51:15');
   INSERT INTO sch_fsts_table_ao VALUES (    2,   'b',   22,  false, '001',   '2_one',  '{6,7,8,9,10}',  'Hey.. whtz up 2', 'Hey .. whtz up 2',    '{one,two,three,four,five}',76767669, 1, 222.2222, 11,   '2_two_22',   'c',   '2002-12-13 01:51:15+1359',   '22',    '0.0.0.0',  '0.0.0.0',  'BB:BB:BB:BB:BB:BB', '200.00',    '00:00:05-07', '((3,3),2)',  '((3,2),(2,3))',   'hello',  '((3,2),(2,3))', 22,    '01010100101',  '2002-12-13', '((2,2),(3,3))', '(2,2)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))', 11111,  '22:00:00', '2002-12-13 01:51:15');

-- SELECT from the Table
   SELECT * from sch_fsts_table_ao;

-- ALTER the table set distributed randomly
   ALTER table sch_fsts_table_ao set with ( reorganize='true') distributed randomly;

-- Vacuum analyze the table
   vacuum analyze sch_fsts_table_ao;

-- ALTER the table to new table space 
   ALTER table sch_fsts_table_ao set tablespace ts_sch6;

-- Insert few records into the table
   INSERT INTO sch_fsts_table_ao VALUES (1,'a',11,true,'111', '1_one', '{1,2,3,4,5}', 'Hello .. how are you 1', 'Hello .. how are you 1',    '{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd',  '2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA',   '34.23',   '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');
   INSERT INTO sch_fsts_table_ao VALUES (    2,   'b',   22,  false, '001',   '2_one',  '{6,7,8,9,10}',  'Hey.. whtz up 2', 'Hey .. whtz up 2',    '{one,two,three,four,five}', 76767669, 1, 222.2222, 11,   '2_two_22',   'c',   '2002-12-13 01:51:15+1359',   '22',    '0.0.0.0',  '0.0.0.0',  'BB:BB:BB:BB:BB:BB', '200.00',    '00:00:05-07', '((3,3),2)',  '((3,2),(2,3))',   'hello',  '((3,2),(2,3))', 22,    '01010100101',  '2002-12-13', '((2,2),(3,3))', '(2,2)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))', 11111,  '22:00:00', '2002-12-13 01:51:15');


-- ALTER the table set distributed by 
   ALTER table sch_fsts_table_ao set with ( reorganize='true') distributed by (col010);

-- SELECT from the Table
   SELECT * from sch_fsts_table_ao;

-- Vacuum analyze the table
   vacuum analyze sch_fsts_table_ao;
   

   
-- Create CO table
   CREATE TABLE sch_fsts_table_co ( a int,col001 char DEFAULT 'z',col002 numeric,col003 boolean DEFAULT false,col004 bit(3) DEFAULT
   '111',col005 text DEFAULT 'pookie',col006 integer[] DEFAULT '{5, 4, 3, 2, 1}', col007 character varying(512) DEFAULT 'Now is the time',
   col008 character varying DEFAULT 'Now is the time', col009 character varying(512)[], col010 numeric(8),col011 int,col012 double precision,
   col013 bigint, col014 char(8), col015 bytea,col016 timestamp with time zone,col017 interval, col018 cidr, col019 inet, col020 macaddr,
   col022 money, col024 timetz,col025 circle, col026 box, col027 name,col028 path, col029 int2, col031 bit varying(256),col032 date,
   col034 lseg,col035 point,col036 polygon,col037 real,col039 time, col040 timestamp ) with (orientation='column',appendonly=true) tablespace ts_sch4;

-- create an index for the creation of block dir
   CREATE index sch_fsts_table_co_index on sch_fsts_table_co(a);

-- Insert few records into the table
   INSERT INTO sch_fsts_table_co VALUES (1,'a',11,true,'111', '1_one', '{1,2,3,4,5}', 'Hello .. how are you 1', 'Hello .. how are you 1',    '{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd',  '2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA',   '34.23',   '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))', 'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');
   INSERT INTO sch_fsts_table_co VALUES (    2,   'b',   22,  false, '001',   '2_one',  '{6,7,8,9,10}',  'Hey.. whtz up 2', 'Hey .. whtz up 2',    '{one,two,three,four,five}', 76767669, 1, 222.2222, 11,   '2_two_22',   'c',   '2002-12-13 01:51:15+1359',   '22',    '0.0.0.0',  '0.0.0.0',  'BB:BB:BB:BB:BB:BB', '200.00',    '00:00:05-07', '((3,3),2)',  '((3,2),(2,3))',   'hello',  '((3,2),(2,3))', 22,    '01010100101',  '2002-12-13', '((2,2),(3,3))', '(2,2)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))', 11111,  '22:00:00', '2002-12-13 01:51:15');

-- SELECT from the Table
   SELECT * from sch_fsts_table_co;

-- ALTER the table set distributed randomly 
   ALTER table sch_fsts_table_co set with ( reorganize='true') distributed randomly;

-- Vacuum analyze the table
   vacuum analyze sch_fsts_table_co;

-- ALTER the table to new table space 
   ALTER table sch_fsts_table_co set tablespace ts_sch2;

-- Insert few records into the table
   INSERT INTO sch_fsts_table_co VALUES (1,'a',11,true,'111', '1_one', '{1,2,3,4,5}', 'Hello .. how are you 1', 'Hello .. how are you 1',    '{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd',  '2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA',   '34.23',   '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))',    111111, '23:00:00',   '2001-12-13 01:51:15');
   INSERT INTO sch_fsts_table_co VALUES (    2,   'b',   22,  false, '001',   '2_one',  '{6,7,8,9,10}',  'Hey.. whtz up 2', 'Hey .. whtz up 2',    '{one,two,three,four,five}', 76767669, 1, 222.2222, 11,   '2_two_22',   'c',   '2002-12-13 01:51:15+1359',   '22',    '0.0.0.0',  '0.0.0.0',  'BB:BB:BB:BB:BB:BB', '200.00',    '00:00:05-07', '((3,3),2)',  '((3,2),(2,3))',   'hello',  '((3,2),(2,3))', 22,    '01010100101',  '2002-12-13', '((2,2),(3,3))', '(2,2)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))', 11111,  '22:00:00', '2002-12-13 01:51:15');

-- ALTER the table set distributed by 
   ALTER table sch_fsts_table_co set with ( reorganize='true') distributed by (col010);

-- SELECT from the Table
   SELECT * from sch_fsts_table_co;

-- Vacuum analyze the table
   vacuum analyze sch_fsts_table_co;

   
-- Create hybrid partitioned table
   CREATE table sch_fsts_part_ao_co (a char, b int, d char,e text) WITH (orientation='column',appendonly=true)tablespace ts_sch2
       partition by range (b)
       subpartition by list (d)
       subpartition template (
       subpartition sp1 values ('a'),
       subpartition sp2 values ('b'))
    (
     start (1) end (10) every (5)
    );

-- create an index for the creation of block dir
   CREATE index sch_fsts_part_ao_co_index on sch_fsts_part_ao_co(b);

-- Insert few records into the table
   INSERT into sch_fsts_part_ao_co values ('a',1,'b','test_1');
   INSERT into sch_fsts_part_ao_co values ('a',2,'b','test_2');
   INSERT into sch_fsts_part_ao_co values ('b',3,'b','test_3');
   INSERT into sch_fsts_part_ao_co values ('a',4,'a','test_4');
   INSERT into sch_fsts_part_ao_co values ('a',5,'a','test_5');

-- SELECT from the Table
   SELECT * from sch_fsts_part_ao_co;

-- Add partition
   ALTER table sch_fsts_part_ao_co add partition p1 end (11);

-- Vacuum analyze the table
   vacuum analyze sch_fsts_part_ao_co;

-- ALTER the table to new table space
   ALTER table sch_fsts_part_ao_co  set tablespace ts_sch5;

-- Insert few records into the table
   INSERT into sch_fsts_part_ao_co values ('a',1,'b','test_1');
   INSERT into sch_fsts_part_ao_co values ('a',2,'b','test_2');
   INSERT into sch_fsts_part_ao_co values ('b',3,'b','test_3');
   INSERT into sch_fsts_part_ao_co values ('a',4,'a','test_4');
   INSERT into sch_fsts_part_ao_co values ('a',5,'a','test_5');

-- ALTER the table set distributed by 
   ALTER table sch_fsts_part_ao_co set with ( reorganize='true') distributed by (b);

-- SELECT from the Table
   SELECT * from sch_fsts_part_ao_co;

-- Vacuum analyze the table
   vacuum analyze sch_fsts_part_ao_co;   
