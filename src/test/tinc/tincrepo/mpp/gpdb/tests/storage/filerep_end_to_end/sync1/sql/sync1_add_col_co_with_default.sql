-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore

--
-- SYNC1 TABLE sync1_add_column_co_default1
--
CREATE TABLE sync1_add_column_co_default1 (col000 int DEFAULT 1) with ( appendonly='true', orientation='column')  distributed randomly;
INSERT INTO sync1_add_column_co_default1 VALUES (generate_series(1,10));
SELECT COUNT(*) from sync1_add_column_co_default1 ;


--
-- SYNC1 TABLE sync1_add_column_co_default2
--
CREATE TABLE sync1_add_column_co_default2 (col000 int DEFAULT 1) with ( appendonly='true', orientation='column')  distributed randomly;
INSERT INTO sync1_add_column_co_default2 VALUES (generate_series(1,10));
SELECT COUNT(*) from sync1_add_column_co_default2 ;

--
-- SYNC1 TABLE sync1_add_column_co_default3
--
CREATE TABLE sync1_add_column_co_default3 (col000 int DEFAULT 1) with ( appendonly='true', orientation='column')  distributed randomly;
INSERT INTO sync1_add_column_co_default3 VALUES (generate_series(1,10));
SELECT COUNT(*) from sync1_add_column_co_default3 ;

--
-- SYNC1 TABLE sync1_add_column_co_default4
--
CREATE TABLE sync1_add_column_co_default4 (col000 int DEFAULT 1) with ( appendonly='true', orientation='column')  distributed randomly;
INSERT INTO sync1_add_column_co_default4 VALUES (generate_series(1,10));
SELECT COUNT(*) from sync1_add_column_co_default4 ;

--
-- SYNC1 TABLE sync1_add_column_co_default5
--
CREATE TABLE sync1_add_column_co_default5 (col000 int DEFAULT 1) with ( appendonly='true', orientation='column')  distributed randomly;
INSERT INTO sync1_add_column_co_default5 VALUES (generate_series(1,10));
SELECT COUNT(*) from sync1_add_column_co_default5 ;

--
-- SYNC1 TABLE sync1_add_column_co_default6
--
CREATE TABLE sync1_add_column_co_default6 (col000 int DEFAULT 1) with ( appendonly='true', orientation='column')  distributed randomly;
INSERT INTO sync1_add_column_co_default6 VALUES (generate_series(1,10));
SELECT COUNT(*) from sync1_add_column_co_default6 ;

--
-- SYNC1 TABLE sync1_add_column_co_default7
--
CREATE TABLE sync1_add_column_co_default7 (col000 int DEFAULT 1) with ( appendonly='true', orientation='column')  distributed randomly;
INSERT INTO sync1_add_column_co_default7 VALUES (generate_series(1,10));
SELECT COUNT(*) from sync1_add_column_co_default7 ;

--
-- SYNC1 TABLE sync1_add_column_co_default8
--
CREATE TABLE sync1_add_column_co_default8 (col000 int DEFAULT 1) with ( appendonly='true', orientation='column')  distributed randomly;
INSERT INTO sync1_add_column_co_default8 VALUES (generate_series(1,10));
SELECT COUNT(*) from sync1_add_column_co_default8 ;

--
-- SYNC1 TABLE - used for insert select
--

CREATE TABLE sync1_co ( a int,col001 char DEFAULT 'z',col002 numeric,col003 boolean DEFAULT false,col004 bit(3) DEFAULT '111',col005 text DEFAULT 'pookie', col006 integer[] DEFAULT '{5, 4, 3, 2, 1}', col007 character varying(512) DEFAULT 'Now is the time', col008 character varying DEFAULT 'Now is the time', col009 character varying(512)[], col010 numeric(8),col011 int,col012 double precision, col013 bigint, col014 char(8), col015 bytea,col016 timestamp with time zone,col017 interval, col018 cidr, col019 inet, col020 macaddr,col021 serial ,col022 money,col023 bigserial, col024 timetz,col025 circle, col026 box, col027 name,col028 path, col029 int2, 
col031 bit varying(256),col032 date, col034 lseg,col035 point,col036 polygon,col037 real,col039 time, col040 timestamp ) with ( appendonly='true', orientation='column') ;


INSERT INTO sync1_co VALUES (generate_series(1,10) ,'a',11,true,'111', '1_one', '{1,2,3,4,5}', 'Hello .. how are you', 'Hello .. how are you',    '{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd',
'2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))
',    111111, '23:00:00',   '2001-12-13 01:51:15');



--
-- ADD ALL COLUMN WITH DEFAULT TO  - > SYNC1 TABLE sync1_add_column_co_default1
--

ALTER TABLE sync1_add_column_co_default1 ADD COLUMN col001 char DEFAULT 'z'   ;
INSERT INTO sync1_add_column_co_default1 VALUES (generate_series(1,10),'a');
SELECT COUNT(*) from sync1_add_column_co_default1 ;
INSERT INTO sync1_add_column_co_default1 SELECT a, col001 from sync1_co;
SELECT COUNT(*) from sync1_add_column_co_default1 ;
--
--

ALTER TABLE sync1_add_column_co_default1 ADD COLUMN col002 numeric  DEFAULT  100;
INSERT INTO sync1_add_column_co_default1 VALUES (generate_series(1,10),'b', generate_series(1,10));
SELECT COUNT(*) from sync1_add_column_co_default1 ;
INSERT INTO sync1_add_column_co_default1 SELECT a, col001, col002 from sync1_co;
SELECT COUNT(*) from sync1_add_column_co_default1 ;
--
--
ALTER TABLE sync1_add_column_co_default1 ADD COLUMN col003 boolean DEFAULT false   ;
INSERT INTO sync1_add_column_co_default1 VALUES (generate_series(1,10),'c', generate_series(1,10),true);
SELECT COUNT(*) from sync1_add_column_co_default1 ;
INSERT INTO sync1_add_column_co_default1 SELECT a, col001, col002, col003 from sync1_co;
SELECT COUNT(*) from sync1_add_column_co_default1 ;
--
--

ALTER TABLE sync1_add_column_co_default1 ADD COLUMN col004 bit(3) DEFAULT '111'   ;
INSERT INTO sync1_add_column_co_default1 VALUES (generate_series(1,10),'d', generate_series(1,10),true,'110');
SELECT COUNT(*) from sync1_add_column_co_default1 ;
INSERT INTO sync1_add_column_co_default1 SELECT a, col001, col002, col003, col004 from sync1_co;
SELECT COUNT(*) from sync1_add_column_co_default1 ;
--
--
ALTER TABLE sync1_add_column_co_default1 ADD COLUMN col005 text DEFAULT 'pookie'   ;
INSERT INTO sync1_add_column_co_default1 VALUES (generate_series(1,10),'e', generate_series(1,10),true,'100',repeat('text_',100));
SELECT COUNT(*) from sync1_add_column_co_default1 ;
INSERT INTO sync1_add_column_co_default1 SELECT a, col001, col002, col003, col004, col005 from sync1_co;
SELECT COUNT(*) from sync1_add_column_co_default1 ;
--
--
ALTER TABLE sync1_add_column_co_default1 ADD COLUMN col006 integer[] DEFAULT '{5,4, 3, 2, 1}'   ;
INSERT INTO sync1_add_column_co_default1 VALUES (generate_series(1,10),'f', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}');
SELECT COUNT(*) from sync1_add_column_co_default1 ;
INSERT INTO sync1_add_column_co_default1 SELECT a, col001, col002, col003, col004, col005, col006 from sync1_co;
SELECT COUNT(*) from sync1_add_column_co_default1 ;
--
--

ALTER TABLE sync1_add_column_co_default1 ADD COLUMN col007 character varying(512) DEFAULT 'Now is the time'  ;
INSERT INTO sync1_add_column_co_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello');
SELECT COUNT(*) from sync1_add_column_co_default1 ;
INSERT INTO sync1_add_column_co_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 from sync1_co;
SELECT COUNT(*) from sync1_add_column_co_default1 ;
--
--

ALTER TABLE sync1_add_column_co_default1 ADD COLUMN col008 character varying DEFAULT 'Now is the time'   ;
INSERT INTO sync1_add_column_co_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo');
SELECT COUNT(*) from sync1_add_column_co_default1 ;
INSERT INTO sync1_add_column_co_default1 SELECT a, col001, col002 , col003, col004, col005, col006, col007  , col008 from sync1_co;
SELECT COUNT(*) from sync1_add_column_co_default1 ;
--
--

ALTER TABLE sync1_add_column_co_default1 ADD COLUMN col009 character varying(512)[]  DEFAULT '{one,two,three,four,five}';
INSERT INTO sync1_add_column_co_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' );
SELECT COUNT(*) from sync1_add_column_co_default1 ;
INSERT INTO sync1_add_column_co_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007, col008, col009 from sync1_co;
SELECT COUNT(*) from sync1_add_column_co_default1 ;
--
--
ALTER TABLE sync1_add_column_co_default1 ADD COLUMN col010 numeric(8)  DEFAULT 1111111;
INSERT INTO sync1_add_column_co_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127);
SELECT COUNT(*) from sync1_add_column_co_default1 ;
INSERT INTO sync1_add_column_co_default1 SELECT a, col001, col002 , col003, col004, col005, col006, col007 , col008, col009, col010 from sync1_co;
SELECT COUNT(*) from sync1_add_column_co_default1 ;
--
--
ALTER TABLE sync1_add_column_co_default1 ADD COLUMN col011 int  DEFAULT  12;
INSERT INTO sync1_add_column_co_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13);
SELECT COUNT(*) from sync1_add_column_co_default1 ;
INSERT INTO sync1_add_column_co_default1 SELECT a, col001, col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011 from sync1_co;
SELECT COUNT(*) from sync1_add_column_co_default1 ;
--
--
ALTER TABLE sync1_add_column_co_default1 ADD COLUMN col012 double precision  DEFAULT  78.90;
INSERT INTO sync1_add_column_co_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80);
SELECT COUNT(*) from sync1_add_column_co_default1 ;
INSERT INTO sync1_add_column_co_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 from sync1_co;
SELECT COUNT(*) from sync1_add_column_co_default1 ;
--
--
ALTER TABLE sync1_add_column_co_default1 ADD COLUMN col013 bigint  DEFAULT 797090 ;
INSERT INTO sync1_add_column_co_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654);
SELECT COUNT(*) from sync1_add_column_co_default1 ;
INSERT INTO sync1_add_column_co_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013 from sync1_co;
SELECT COUNT(*) from sync1_add_column_co_default1 ;
--
--
ALTER TABLE sync1_add_column_co_default1 ADD COLUMN col014 char(8)  DEFAULT 'i_am_eig';
INSERT INTO sync1_add_column_co_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana');
SELECT COUNT(*) from sync1_add_column_co_default1 ;
INSERT INTO sync1_add_column_co_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014 from sync1_co;
SELECT COUNT(*) from sync1_add_column_co_default1 ;
--
--
ALTER TABLE sync1_add_column_co_default1 ADD COLUMN col015 bytea  DEFAULT 'd';
INSERT INTO sync1_add_column_co_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a');
SELECT COUNT(*) from sync1_add_column_co_default1 ;
INSERT INTO sync1_add_column_co_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014,col015 from sync1_co;
SELECT COUNT(*) from sync1_add_column_co_default1 ;
--
--
ALTER TABLE sync1_add_column_co_default1 ADD COLUMN col016 timestamp with time zone  DEFAULT '2000-12-13 01:51:15';
INSERT INTO sync1_add_column_co_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50');
SELECT COUNT(*) from sync1_add_column_co_default1 ;
INSERT INTO sync1_add_column_co_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016 from sync1_co;
SELECT COUNT(*) from sync1_add_column_co_default1 ;
--
--
ALTER TABLE sync1_add_column_co_default1 ADD COLUMN col017 interval  DEFAULT '10';
INSERT INTO sync1_add_column_co_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5');
SELECT COUNT(*) from sync1_add_column_co_default1 ;
INSERT INTO sync1_add_column_co_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 from sync1_co;
SELECT COUNT(*) from sync1_add_column_co_default1 ;
--
--
ALTER TABLE sync1_add_column_co_default1 ADD COLUMN col018 cidr  DEFAULT '0.0.0.0';
INSERT INTO sync1_add_column_co_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0');
SELECT COUNT(*) from sync1_add_column_co_default1 ;
INSERT INTO sync1_add_column_co_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 from sync1_co;
SELECT COUNT(*) from sync1_add_column_co_default1 ;
--
--
ALTER TABLE sync1_add_column_co_default1 ADD COLUMN col019 inet  DEFAULT '0.0.0.0';
INSERT INTO sync1_add_column_co_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0');
SELECT COUNT(*) from sync1_add_column_co_default1 ;
INSERT INTO sync1_add_column_co_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019 from sync1_co;
SELECT COUNT(*) from sync1_add_column_co_default1 ;
--
--
ALTER TABLE sync1_add_column_co_default1 ADD COLUMN col020 macaddr  DEFAULT 'AA:AA:AA:AA:AA:AA';
INSERT INTO sync1_add_column_co_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB');
SELECT COUNT(*) from sync1_add_column_co_default1 ;
INSERT INTO sync1_add_column_co_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020 from sync1_co;
SELECT COUNT(*) from sync1_add_column_co_default1 ;

--
--
ALTER TABLE sync1_add_column_co_default1 ADD COLUMN col022 money  DEFAULT '100.00';
INSERT INTO sync1_add_column_co_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00');
SELECT COUNT(*) from sync1_add_column_co_default1 ;
INSERT INTO sync1_add_column_co_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022 from sync1_co;
SELECT COUNT(*) from sync1_add_column_co_default1 ;
--
--

ALTER TABLE sync1_add_column_co_default1 ADD COLUMN col024 timetz  DEFAULT '00:00:01';
INSERT INTO sync1_add_column_co_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01');
SELECT COUNT(*) from sync1_add_column_co_default1 ;
INSERT INTO sync1_add_column_co_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024 from sync1_co;
SELECT COUNT(*) from sync1_add_column_co_default1 ;
--
--
ALTER TABLE sync1_add_column_co_default1 ADD COLUMN col025 circle  DEFAULT '((2,2),1)' ;
INSERT INTO sync1_add_column_co_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)');
SELECT COUNT(*) from sync1_add_column_co_default1 ;
INSERT INTO sync1_add_column_co_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 from sync1_co;
SELECT COUNT(*) from sync1_add_column_co_default1 ;
--
--
ALTER TABLE sync1_add_column_co_default1 ADD COLUMN col026 box  DEFAULT '((1,2),(2,1))';
INSERT INTO sync1_add_column_co_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))');
SELECT COUNT(*) from sync1_add_column_co_default1 ;
 INSERT INTO sync1_add_column_co_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026 from sync1_co;
SELECT COUNT(*) from sync1_add_column_co_default1 ;
--
--
ALTER TABLE sync1_add_column_co_default1 ADD COLUMN col027 name  DEFAULT 'ann' ;
INSERT INTO sync1_add_column_co_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom');
SELECT COUNT(*) from sync1_add_column_co_default1 ;
 INSERT INTO sync1_add_column_co_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 from sync1_co;
SELECT COUNT(*) from sync1_add_column_co_default1 ;
--
--
ALTER TABLE sync1_add_column_co_default1 ADD COLUMN col028 path  DEFAULT '((1,2),(2,1))';
INSERT INTO sync1_add_column_co_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))');
SELECT COUNT(*) from sync1_add_column_co_default1 ;
 INSERT INTO sync1_add_column_co_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028 from sync1_co;
SELECT COUNT(*) from sync1_add_column_co_default1 ;
--
--
ALTER TABLE sync1_add_column_co_default1 ADD COLUMN col029 int2  DEFAULT 22;
INSERT INTO sync1_add_column_co_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11);
SELECT COUNT(*) from sync1_add_column_co_default1 ;
 INSERT INTO sync1_add_column_co_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 from sync1_co;
SELECT COUNT(*) from sync1_add_column_co_default1 ;
--
--
ALTER TABLE sync1_add_column_co_default1 ADD COLUMN col031 bit varying(256)  DEFAULT '1000001';
INSERT INTO sync1_add_column_co_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11, '1001101');
SELECT COUNT(*) from sync1_add_column_co_default1 ;
 INSERT INTO sync1_add_column_co_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 , col031 from sync1_co;
SELECT COUNT(*) from sync1_add_column_co_default1 ;
--
--
ALTER TABLE sync1_add_column_co_default1 ADD COLUMN col032 date  DEFAULT '2005-05-05';
INSERT INTO sync1_add_column_co_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11, '1001101',  '2008-08-08');
SELECT COUNT(*) from sync1_add_column_co_default1 ;
 INSERT INTO sync1_add_column_co_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 , col031, col032 from sync1_co;
SELECT COUNT(*) from sync1_add_column_co_default1 ;
--
--
ALTER TABLE sync1_add_column_co_default1 ADD COLUMN col034 lseg  DEFAULT '((1,2),(2,1))';
INSERT INTO sync1_add_column_co_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11, '1001101',  '2008-08-08' , '((2,3),(3,2))');
SELECT COUNT(*) from sync1_add_column_co_default1 ;
 INSERT INTO sync1_add_column_co_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 from sync1_co;
SELECT COUNT(*) from sync1_add_column_co_default1 ;
--
--
ALTER TABLE sync1_add_column_co_default1 ADD COLUMN col035 point  DEFAULT '(5,5)';
INSERT INTO sync1_add_column_co_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11, '1001101',  '2008-08-08' , '((2,3),(3,2))', '(4,4)');
SELECT COUNT(*) from sync1_add_column_co_default1 ;
 INSERT INTO sync1_add_column_co_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035 from sync1_co;
SELECT COUNT(*) from sync1_add_column_co_default1 ;
--
--

ALTER TABLE sync1_add_column_co_default1 ADD COLUMN col036 polygon  DEFAULT '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))' ;
INSERT INTO sync1_add_column_co_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11, '1001101',  '2008-08-08' , '((2,3),(3,2))', '(4,4)' , '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))' );
SELECT COUNT(*) from sync1_add_column_co_default1 ;
 INSERT INTO sync1_add_column_co_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 from sync1_co;
SELECT COUNT(*) from sync1_add_column_co_default1 ;
--
--
ALTER TABLE sync1_add_column_co_default1 ADD COLUMN col037 real  DEFAULT 23425;
INSERT INTO sync1_add_column_co_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11, '1001101',  '2008-08-08' , '((2,3),(3,2))', '(4,4)' , '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))' , 33435);
SELECT COUNT(*) from sync1_add_column_co_default1 ;
 INSERT INTO sync1_add_column_co_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037 from sync1_co;
SELECT COUNT(*) from sync1_add_column_co_default1 ;
--
--

ALTER TABLE sync1_add_column_co_default1 ADD COLUMN col039 time  DEFAULT '01:00:10';
INSERT INTO sync1_add_column_co_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11, '1001101',  '2008-08-08' , '((2,3),(3,2))', '(4,4)' , '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))' , 33435, '01:05:10');
SELECT COUNT(*) from sync1_add_column_co_default1 ;
 INSERT INTO sync1_add_column_co_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 from sync1_co;
SELECT COUNT(*) from sync1_add_column_co_default1 ;
--
--
 
ALTER TABLE sync1_add_column_co_default1 ADD COLUMN col040 timestamp  DEFAULT '2005-05-05 01:51:15';
INSERT INTO sync1_add_column_co_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11, '1001101',  '2008-08-08' , '((2,3),(3,2))', '(4,4)' , '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))' , 33435, '01:05:10' ,  '2009-09-09 01:55:19');
SELECT COUNT(*) from sync1_add_column_co_default1 ;
 INSERT INTO sync1_add_column_co_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from sync1_co;
SELECT COUNT(*) from sync1_add_column_co_default1 ;


--
--
 
ALTER TABLE sync1_add_column_co_default1 ADD COLUMN col021 serial ;
INSERT INTO sync1_add_column_co_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11, '1001101',  '2008-08-08' , '((2,3),(3,2))', '(4,4)' , '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))' , 33435, '01:05:10' ,  '2009-09-09 01:55:19',generate_series(1,10));
SELECT COUNT(*) from sync1_add_column_co_default1 ;
 INSERT INTO sync1_add_column_co_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from sync1_co;
SELECT COUNT(*) from sync1_add_column_co_default1 ;


--
--
 
ALTER TABLE sync1_add_column_co_default1 ADD COLUMN col023 bigserial;
INSERT INTO sync1_add_column_co_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11, '1001101',  '2008-08-08' , '((2,3),(3,2))', '(4,4)' , '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))' , 33435, '01:05:10' ,  '2009-09-09 01:55:19',generate_series(1,10),generate_series(1,10));
SELECT COUNT(*) from sync1_add_column_co_default1 ;
 INSERT INTO sync1_add_column_co_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from sync1_co;
SELECT COUNT(*) from sync1_add_column_co_default1 ;
