-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore

--
-- CT TABLE ct_add_column_heap_wo_default1
--
CREATE TABLE ct_add_column_heap_wo_default1 (col000 int  ) distributed randomly;
INSERT INTO ct_add_column_heap_wo_default1 VALUES (generate_series(1,10));
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;


--
-- CT TABLE ct_add_column_heap_wo_default2
--
CREATE TABLE ct_add_column_heap_wo_default2 (col000 int  ) distributed randomly;
INSERT INTO ct_add_column_heap_wo_default2 VALUES (generate_series(1,10));
SELECT COUNT(*) from ct_add_column_heap_wo_default2 ;

--
-- CT TABLE ct_add_column_heap_wo_default3
--
CREATE TABLE ct_add_column_heap_wo_default3 (col000 int  ) distributed randomly;
INSERT INTO ct_add_column_heap_wo_default3 VALUES (generate_series(1,10));
SELECT COUNT(*) from ct_add_column_heap_wo_default3 ;

--
-- CT TABLE ct_add_column_heap_wo_default4
--
CREATE TABLE ct_add_column_heap_wo_default4 (col000 int  ) distributed randomly;
INSERT INTO ct_add_column_heap_wo_default4 VALUES (generate_series(1,10));
SELECT COUNT(*) from ct_add_column_heap_wo_default4 ;

--
-- CT TABLE ct_add_column_heap_wo_default5
--
CREATE TABLE ct_add_column_heap_wo_default5 (col000 int  ) distributed randomly;
INSERT INTO ct_add_column_heap_wo_default5 VALUES (generate_series(1,10));
SELECT COUNT(*) from ct_add_column_heap_wo_default5 ;


--
-- CT TABLE - used for insert select
--

CREATE TABLE ct_heap1 ( a int,col001 char ,col002 numeric,col003 boolean ,col004 bit(3) ,col005 text , col006 integer[] ,col007 character varying(512) , col008 character varying , col009 character varying(512)[], col010 numeric(8),col011 int,col012 double precision, col013 bigint, col014 char(8), col015 bytea,col016 timestamp with time zone,col017 interval, col018 cidr, col019 inet, col020 macaddr,col021 serial ,col022 money,col023 bigserial, col024 timetz,col025 circle, col026 box, col027 name,col028 path, col029 int2, 
col031 bit varying(256),col032 date, col034 lseg,col035 point,col036 polygon,col037 real,col039 time, col040 timestamp );


INSERT INTO ct_heap1 VALUES (generate_series(1,10) ,'a',11,true,'111', '1_one', '{1,2,3,4,5}', 'Hello .. how are you', 'Hello .. how are you',    '{one,two,three,four,five}',  12345678, 1, 111.1111,  11,  '1_one_11',   'd',
'2001-12-13 01:51:15+1359',  '11',   '0.0.0.0', '0.0.0.0', 'AA:AA:AA:AA:AA:AA', generate_series(1,10),  '34.23',  generate_series(1,10), '00:00:00+1359',  '((2,2),1)',   '((1,2),(2,1))',   'hello', '((1,2),(2,1))', 11,   '010101',   '2001-12-13', '((1,1),(2,2))', '(1,1)', '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))
',    111111, '23:00:00',   '2001-12-13 01:51:15');



--
-- ADD ALL COLUMN WITH DEFAULT TO  - > CT TABLE ct_add_column_heap_wo_default1
--

ALTER TABLE ct_add_column_heap_wo_default1 ADD COLUMN col001 char   ;
INSERT INTO ct_add_column_heap_wo_default1 VALUES (generate_series(1,10),'a');
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
INSERT INTO ct_add_column_heap_wo_default1 SELECT a, col001 from ct_heap1;
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
--
--

ALTER TABLE ct_add_column_heap_wo_default1 ADD COLUMN col002 numeric  ;
INSERT INTO ct_add_column_heap_wo_default1 VALUES (generate_series(1,10),'b', generate_series(1,10));
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
INSERT INTO ct_add_column_heap_wo_default1 SELECT a, col001, col002 from ct_heap1;
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
--
--
ALTER TABLE ct_add_column_heap_wo_default1 ADD COLUMN col003 boolean    ;
INSERT INTO ct_add_column_heap_wo_default1 VALUES (generate_series(1,10),'c', generate_series(1,10),true);
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
INSERT INTO ct_add_column_heap_wo_default1 SELECT a, col001, col002, col003 from ct_heap1;
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
--
--

ALTER TABLE ct_add_column_heap_wo_default1 ADD COLUMN col004 bit(3)   ;
INSERT INTO ct_add_column_heap_wo_default1 VALUES (generate_series(1,10),'d', generate_series(1,10),true,'110');
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
INSERT INTO ct_add_column_heap_wo_default1 SELECT a, col001, col002, col003, col004 from ct_heap1;
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
--
--
ALTER TABLE ct_add_column_heap_wo_default1 ADD COLUMN col005 text    ;
INSERT INTO ct_add_column_heap_wo_default1 VALUES (generate_series(1,10),'e', generate_series(1,10),true,'100',repeat('text_',100));
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
INSERT INTO ct_add_column_heap_wo_default1 SELECT a, col001, col002, col003, col004, col005 from ct_heap1;
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
--
--
ALTER TABLE ct_add_column_heap_wo_default1 ADD COLUMN col006 integer[]  ;
INSERT INTO ct_add_column_heap_wo_default1 VALUES (generate_series(1,10),'f', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}');
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
INSERT INTO ct_add_column_heap_wo_default1 SELECT a, col001, col002, col003, col004, col005, col006 from ct_heap1;
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
--
--

ALTER TABLE ct_add_column_heap_wo_default1 ADD COLUMN col007 character varying(512)  ;
INSERT INTO ct_add_column_heap_wo_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello');
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
INSERT INTO ct_add_column_heap_wo_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 from ct_heap1;
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
--
--

ALTER TABLE ct_add_column_heap_wo_default1 ADD COLUMN col008 character varying   ;
INSERT INTO ct_add_column_heap_wo_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo');
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
INSERT INTO ct_add_column_heap_wo_default1 SELECT a, col001, col002 , col003, col004, col005, col006, col007  , col008 from ct_heap1;
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
--
--

ALTER TABLE ct_add_column_heap_wo_default1 ADD COLUMN col009 character varying(512)[]  ;
INSERT INTO ct_add_column_heap_wo_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' );
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
INSERT INTO ct_add_column_heap_wo_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007, col008, col009 from ct_heap1;
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
--
--
ALTER TABLE ct_add_column_heap_wo_default1 ADD COLUMN col010 numeric(8)  ;
INSERT INTO ct_add_column_heap_wo_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127);
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
INSERT INTO ct_add_column_heap_wo_default1 SELECT a, col001, col002 , col003, col004, col005, col006, col007 , col008, col009, col010 from ct_heap1;
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
--
--
ALTER TABLE ct_add_column_heap_wo_default1 ADD COLUMN col011 int  ;
INSERT INTO ct_add_column_heap_wo_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13);
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
INSERT INTO ct_add_column_heap_wo_default1 SELECT a, col001, col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011 from ct_heap1;
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
--
--
ALTER TABLE ct_add_column_heap_wo_default1 ADD COLUMN col012 double precision  ;
INSERT INTO ct_add_column_heap_wo_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80);
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
INSERT INTO ct_add_column_heap_wo_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 from ct_heap1;
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
--
--
ALTER TABLE ct_add_column_heap_wo_default1 ADD COLUMN col013 bigint   ;
INSERT INTO ct_add_column_heap_wo_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654);
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
INSERT INTO ct_add_column_heap_wo_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013 from ct_heap1;
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
--
--
ALTER TABLE ct_add_column_heap_wo_default1 ADD COLUMN col014 char(8)  ;
INSERT INTO ct_add_column_heap_wo_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana');
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
INSERT INTO ct_add_column_heap_wo_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014 from ct_heap1;
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
--
--
ALTER TABLE ct_add_column_heap_wo_default1 ADD COLUMN col015 bytea  ;
INSERT INTO ct_add_column_heap_wo_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a');
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
INSERT INTO ct_add_column_heap_wo_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014,col015 from ct_heap1;
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
--
--
ALTER TABLE ct_add_column_heap_wo_default1 ADD COLUMN col016 timestamp with time zone  ;
INSERT INTO ct_add_column_heap_wo_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50');
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
INSERT INTO ct_add_column_heap_wo_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016 from ct_heap1;
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
--
--
ALTER TABLE ct_add_column_heap_wo_default1 ADD COLUMN col017 interval  ;
INSERT INTO ct_add_column_heap_wo_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5');
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
INSERT INTO ct_add_column_heap_wo_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 from ct_heap1;
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
--
--
ALTER TABLE ct_add_column_heap_wo_default1 ADD COLUMN col018 cidr  ;
INSERT INTO ct_add_column_heap_wo_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0');
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
INSERT INTO ct_add_column_heap_wo_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 from ct_heap1;
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
--
--
ALTER TABLE ct_add_column_heap_wo_default1 ADD COLUMN col019 inet  ;
INSERT INTO ct_add_column_heap_wo_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0');
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
INSERT INTO ct_add_column_heap_wo_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019 from ct_heap1;
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
--
--
ALTER TABLE ct_add_column_heap_wo_default1 ADD COLUMN col020 macaddr  ;
INSERT INTO ct_add_column_heap_wo_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB');
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
INSERT INTO ct_add_column_heap_wo_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020 from ct_heap1;
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;

--
--
ALTER TABLE ct_add_column_heap_wo_default1 ADD COLUMN col022 money  ;
INSERT INTO ct_add_column_heap_wo_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00');
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
INSERT INTO ct_add_column_heap_wo_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022 from ct_heap1;
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
--
--

ALTER TABLE ct_add_column_heap_wo_default1 ADD COLUMN col024 timetz  ;
INSERT INTO ct_add_column_heap_wo_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01');
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
INSERT INTO ct_add_column_heap_wo_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024 from ct_heap1;
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
--
--
ALTER TABLE ct_add_column_heap_wo_default1 ADD COLUMN col025 circle   ;
INSERT INTO ct_add_column_heap_wo_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)');
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
INSERT INTO ct_add_column_heap_wo_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 from ct_heap1;
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
--
--
ALTER TABLE ct_add_column_heap_wo_default1 ADD COLUMN col026 box  ;
INSERT INTO ct_add_column_heap_wo_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))');
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
 INSERT INTO ct_add_column_heap_wo_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026 from ct_heap1;
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
--
--
ALTER TABLE ct_add_column_heap_wo_default1 ADD COLUMN col027 name   ;
INSERT INTO ct_add_column_heap_wo_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom');
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
 INSERT INTO ct_add_column_heap_wo_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 from ct_heap1;
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
--
--
ALTER TABLE ct_add_column_heap_wo_default1 ADD COLUMN col028 path  ;
INSERT INTO ct_add_column_heap_wo_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))');
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
 INSERT INTO ct_add_column_heap_wo_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028 from ct_heap1;
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
--
--
ALTER TABLE ct_add_column_heap_wo_default1 ADD COLUMN col029 int2  ;
INSERT INTO ct_add_column_heap_wo_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11);
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
 INSERT INTO ct_add_column_heap_wo_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 from ct_heap1;
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
--
--
ALTER TABLE ct_add_column_heap_wo_default1 ADD COLUMN col031 bit varying(256)  ;
INSERT INTO ct_add_column_heap_wo_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11, '1001101');
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
 INSERT INTO ct_add_column_heap_wo_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 , col031 from ct_heap1;
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
--
--
ALTER TABLE ct_add_column_heap_wo_default1 ADD COLUMN col032 date  ;
INSERT INTO ct_add_column_heap_wo_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11, '1001101',  '2008-08-08');
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
 INSERT INTO ct_add_column_heap_wo_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 , col031, col032 from ct_heap1;
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
--
--
ALTER TABLE ct_add_column_heap_wo_default1 ADD COLUMN col034 lseg  ;
INSERT INTO ct_add_column_heap_wo_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11, '1001101',  '2008-08-08' , '((2,3),(3,2))');
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
 INSERT INTO ct_add_column_heap_wo_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 from ct_heap1;
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
--
--
ALTER TABLE ct_add_column_heap_wo_default1 ADD COLUMN col035 point  ;
INSERT INTO ct_add_column_heap_wo_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11, '1001101',  '2008-08-08' , '((2,3),(3,2))', '(4,4)');
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
 INSERT INTO ct_add_column_heap_wo_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035 from ct_heap1;
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
--
--

ALTER TABLE ct_add_column_heap_wo_default1 ADD COLUMN col036 polygon  ;
INSERT INTO ct_add_column_heap_wo_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11, '1001101',  '2008-08-08' , '((2,3),(3,2))', '(4,4)' , '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))' );
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
 INSERT INTO ct_add_column_heap_wo_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 from ct_heap1;
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
--
--
ALTER TABLE ct_add_column_heap_wo_default1 ADD COLUMN col037 real   ;
INSERT INTO ct_add_column_heap_wo_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11, '1001101',  '2008-08-08' , '((2,3),(3,2))', '(4,4)' , '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))' , 33435);
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
 INSERT INTO ct_add_column_heap_wo_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037 from ct_heap1;
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
--
--

ALTER TABLE ct_add_column_heap_wo_default1 ADD COLUMN col039 time  ;
INSERT INTO ct_add_column_heap_wo_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11, '1001101',  '2008-08-08' , '((2,3),(3,2))', '(4,4)' , '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))' , 33435, '01:05:10');
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
 INSERT INTO ct_add_column_heap_wo_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 from ct_heap1;
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
--
--
 
ALTER TABLE ct_add_column_heap_wo_default1 ADD COLUMN col040 timestamp   ;
INSERT INTO ct_add_column_heap_wo_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11, '1001101',  '2008-08-08' , '((2,3),(3,2))', '(4,4)' , '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))' , 33435, '01:05:10' ,  '2009-09-09 01:55:19');
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
 INSERT INTO ct_add_column_heap_wo_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from ct_heap1;
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;


--
--
 
ALTER TABLE ct_add_column_heap_wo_default1 ADD COLUMN col021 serial ;
INSERT INTO ct_add_column_heap_wo_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11, '1001101',  '2008-08-08' , '((2,3),(3,2))', '(4,4)' , '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))' , 33435, '01:05:10' ,  '2009-09-09 01:55:19',generate_series(1,10));
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
 INSERT INTO ct_add_column_heap_wo_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from ct_heap1;
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;


--
--
 
ALTER TABLE ct_add_column_heap_wo_default1 ADD COLUMN col023 bigserial;
INSERT INTO ct_add_column_heap_wo_default1 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11, '1001101',  '2008-08-08' , '((2,3),(3,2))', '(4,4)' , '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))' , 33435, '01:05:10' ,  '2009-09-09 01:55:19',generate_series(1,10),generate_series(1,10));
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;
 INSERT INTO ct_add_column_heap_wo_default1 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from ct_heap1;
SELECT COUNT(*) from ct_add_column_heap_wo_default1 ;


--
--
--
--
-- ADD ALL COLUMN WITH DEFAULT TO  - > CK_SYNC1 TABLE ck_sync1_add_column_heap_wo_default3
--
--
--
--
--
ALTER TABLE ck_sync1_add_column_heap_wo_default3 ADD COLUMN col001 char   ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 VALUES (generate_series(1,10),'a');
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 SELECT a, col001 from ck_sync1_heap1;
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
--
--

ALTER TABLE ck_sync1_add_column_heap_wo_default3 ADD COLUMN col002 numeric  ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 VALUES (generate_series(1,10),'b', generate_series(1,10));
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 SELECT a, col001, col002 from ck_sync1_heap1;
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
--
--
ALTER TABLE ck_sync1_add_column_heap_wo_default3 ADD COLUMN col003 boolean    ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 VALUES (generate_series(1,10),'c', generate_series(1,10),true);
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 SELECT a, col001, col002, col003 from ck_sync1_heap1;
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
--
--

ALTER TABLE ck_sync1_add_column_heap_wo_default3 ADD COLUMN col004 bit(3)   ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 VALUES (generate_series(1,10),'d', generate_series(1,10),true,'110');
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 SELECT a, col001, col002, col003, col004 from ck_sync1_heap1;
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
--
--
ALTER TABLE ck_sync1_add_column_heap_wo_default3 ADD COLUMN col005 text    ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 VALUES (generate_series(1,10),'e', generate_series(1,10),true,'100',repeat('text_',100));
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 SELECT a, col001, col002, col003, col004, col005 from ck_sync1_heap1;
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
--
--
ALTER TABLE ck_sync1_add_column_heap_wo_default3 ADD COLUMN col006 integer[]  ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 VALUES (generate_series(1,10),'f', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}');
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 SELECT a, col001, col002, col003, col004, col005, col006 from ck_sync1_heap1;
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
--
--

ALTER TABLE ck_sync1_add_column_heap_wo_default3 ADD COLUMN col007 character varying(512)  ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello');
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 from ck_sync1_heap1;
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
--
--

ALTER TABLE ck_sync1_add_column_heap_wo_default3 ADD COLUMN col008 character varying   ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo');
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 SELECT a, col001, col002 , col003, col004, col005, col006, col007  , col008 from ck_sync1_heap1;
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
--
--

ALTER TABLE ck_sync1_add_column_heap_wo_default3 ADD COLUMN col009 character varying(512)[]  ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' );
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 SELECT a, col001 , col002 , col003, col004, col005, col006, col007, col008, col009 from ck_sync1_heap1;
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
--
--
ALTER TABLE ck_sync1_add_column_heap_wo_default3 ADD COLUMN col010 numeric(8)  ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127);
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 SELECT a, col001, col002 , col003, col004, col005, col006, col007 , col008, col009, col010 from ck_sync1_heap1;
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
--
--
ALTER TABLE ck_sync1_add_column_heap_wo_default3 ADD COLUMN col011 int  ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13);
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 SELECT a, col001, col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011 from ck_sync1_heap1;
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
--
--
ALTER TABLE ck_sync1_add_column_heap_wo_default3 ADD COLUMN col012 double precision  ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80);
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 from ck_sync1_heap1;
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
--
--
ALTER TABLE ck_sync1_add_column_heap_wo_default3 ADD COLUMN col013 bigint   ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654);
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013 from ck_sync1_heap1;
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
--
--
ALTER TABLE ck_sync1_add_column_heap_wo_default3 ADD COLUMN col014 char(8)  ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana');
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014 from ck_sync1_heap1;
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
--
--
ALTER TABLE ck_sync1_add_column_heap_wo_default3 ADD COLUMN col015 bytea  ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a');
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014,col015 from ck_sync1_heap1;
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
--
--
ALTER TABLE ck_sync1_add_column_heap_wo_default3 ADD COLUMN col016 timestamp with time zone  ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50');
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016 from ck_sync1_heap1;
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
--
--
ALTER TABLE ck_sync1_add_column_heap_wo_default3 ADD COLUMN col017 interval  ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5');
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 from ck_sync1_heap1;
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
--
--
ALTER TABLE ck_sync1_add_column_heap_wo_default3 ADD COLUMN col018 cidr  ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0');
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 from ck_sync1_heap1;
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
--
--
ALTER TABLE ck_sync1_add_column_heap_wo_default3 ADD COLUMN col019 inet  ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0');
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019 from ck_sync1_heap1;
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
--
--
ALTER TABLE ck_sync1_add_column_heap_wo_default3 ADD COLUMN col020 macaddr  ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB');
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020 from ck_sync1_heap1;
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;

--
--
ALTER TABLE ck_sync1_add_column_heap_wo_default3 ADD COLUMN col022 money  ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00');
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022 from ck_sync1_heap1;
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
--
--

ALTER TABLE ck_sync1_add_column_heap_wo_default3 ADD COLUMN col024 timetz  ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01');
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024 from ck_sync1_heap1;
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
--
--
ALTER TABLE ck_sync1_add_column_heap_wo_default3 ADD COLUMN col025 circle   ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)');
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 from ck_sync1_heap1;
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
--
--
ALTER TABLE ck_sync1_add_column_heap_wo_default3 ADD COLUMN col026 box  ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))');
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
 INSERT INTO ck_sync1_add_column_heap_wo_default3 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026 from ck_sync1_heap1;
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
--
--
ALTER TABLE ck_sync1_add_column_heap_wo_default3 ADD COLUMN col027 name   ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom');
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
 INSERT INTO ck_sync1_add_column_heap_wo_default3 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 from ck_sync1_heap1;
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
--
--
ALTER TABLE ck_sync1_add_column_heap_wo_default3 ADD COLUMN col028 path  ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))');
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
 INSERT INTO ck_sync1_add_column_heap_wo_default3 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028 from ck_sync1_heap1;
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
--
--
ALTER TABLE ck_sync1_add_column_heap_wo_default3 ADD COLUMN col029 int2  ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11);
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
 INSERT INTO ck_sync1_add_column_heap_wo_default3 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 from ck_sync1_heap1;
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
--
--
ALTER TABLE ck_sync1_add_column_heap_wo_default3 ADD COLUMN col031 bit varying(256)  ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11, '1001101');
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
 INSERT INTO ck_sync1_add_column_heap_wo_default3 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 , col031 from ck_sync1_heap1;
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
--
--
ALTER TABLE ck_sync1_add_column_heap_wo_default3 ADD COLUMN col032 date  ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11, '1001101',  '2008-08-08');
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
 INSERT INTO ck_sync1_add_column_heap_wo_default3 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 , col031, col032 from ck_sync1_heap1;
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
--
--
ALTER TABLE ck_sync1_add_column_heap_wo_default3 ADD COLUMN col034 lseg  ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11, '1001101',  '2008-08-08' , '((2,3),(3,2))');
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
 INSERT INTO ck_sync1_add_column_heap_wo_default3 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 from ck_sync1_heap1;
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
--
--
ALTER TABLE ck_sync1_add_column_heap_wo_default3 ADD COLUMN col035 point  ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11, '1001101',  '2008-08-08' , '((2,3),(3,2))', '(4,4)');
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
 INSERT INTO ck_sync1_add_column_heap_wo_default3 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035 from ck_sync1_heap1;
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
--
--

ALTER TABLE ck_sync1_add_column_heap_wo_default3 ADD COLUMN col036 polygon  ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11, '1001101',  '2008-08-08' , '((2,3),(3,2))', '(4,4)' , '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))' );
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
 INSERT INTO ck_sync1_add_column_heap_wo_default3 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 from ck_sync1_heap1;
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
--
--
ALTER TABLE ck_sync1_add_column_heap_wo_default3 ADD COLUMN col037 real   ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11, '1001101',  '2008-08-08' , '((2,3),(3,2))', '(4,4)' , '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))' , 33435);
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
 INSERT INTO ck_sync1_add_column_heap_wo_default3 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037 from ck_sync1_heap1;
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
--
--

ALTER TABLE ck_sync1_add_column_heap_wo_default3 ADD COLUMN col039 time  ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11, '1001101',  '2008-08-08' , '((2,3),(3,2))', '(4,4)' , '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))' , 33435, '01:05:10');
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
 INSERT INTO ck_sync1_add_column_heap_wo_default3 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 from ck_sync1_heap1;
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
--
--
 
ALTER TABLE ck_sync1_add_column_heap_wo_default3 ADD COLUMN col040 timestamp   ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11, '1001101',  '2008-08-08' , '((2,3),(3,2))', '(4,4)' , '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))' , 33435, '01:05:10' ,  '2009-09-09 01:55:19');
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
 INSERT INTO ck_sync1_add_column_heap_wo_default3 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from ck_sync1_heap1;
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;


--
--
 
ALTER TABLE ck_sync1_add_column_heap_wo_default3 ADD COLUMN col021 serial ;
INSERT INTO ck_sync1_add_column_heap_wo_default3 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11, '1001101',  '2008-08-08' , '((2,3),(3,2))', '(4,4)' , '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))' , 33435, '01:05:10' ,  '2009-09-09 01:55:19',generate_series(1,10));
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
 INSERT INTO ck_sync1_add_column_heap_wo_default3 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from ck_sync1_heap1;
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;


--
--
 
ALTER TABLE ck_sync1_add_column_heap_wo_default3 ADD COLUMN col023 bigserial;
INSERT INTO ck_sync1_add_column_heap_wo_default3 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11, '1001101',  '2008-08-08' , '((2,3),(3,2))', '(4,4)' , '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))' , 33435, '01:05:10' ,  '2009-09-09 01:55:19',generate_series(1,10),generate_series(1,10));
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;
 INSERT INTO ck_sync1_add_column_heap_wo_default3 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from ck_sync1_heap1;
SELECT COUNT(*) from ck_sync1_add_column_heap_wo_default3 ;

--
--
--
--
-- ADD ALL COLUMN WITH DEFAULT TO  - > SYNC1 TABLE sync1_add_column_heap_wo_default4
--
--
--
--
--
ALTER TABLE sync1_add_column_heap_wo_default4 ADD COLUMN col001 char   ;
INSERT INTO sync1_add_column_heap_wo_default4 VALUES (generate_series(1,10),'a');
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
INSERT INTO sync1_add_column_heap_wo_default4 SELECT a, col001 from sync1_heap1;
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
--
--

ALTER TABLE sync1_add_column_heap_wo_default4 ADD COLUMN col002 numeric  ;
INSERT INTO sync1_add_column_heap_wo_default4 VALUES (generate_series(1,10),'b', generate_series(1,10));
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
INSERT INTO sync1_add_column_heap_wo_default4 SELECT a, col001, col002 from sync1_heap1;
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
--
--
ALTER TABLE sync1_add_column_heap_wo_default4 ADD COLUMN col003 boolean    ;
INSERT INTO sync1_add_column_heap_wo_default4 VALUES (generate_series(1,10),'c', generate_series(1,10),true);
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
INSERT INTO sync1_add_column_heap_wo_default4 SELECT a, col001, col002, col003 from sync1_heap1;
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
--
--

ALTER TABLE sync1_add_column_heap_wo_default4 ADD COLUMN col004 bit(3)   ;
INSERT INTO sync1_add_column_heap_wo_default4 VALUES (generate_series(1,10),'d', generate_series(1,10),true,'110');
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
INSERT INTO sync1_add_column_heap_wo_default4 SELECT a, col001, col002, col003, col004 from sync1_heap1;
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
--
--
ALTER TABLE sync1_add_column_heap_wo_default4 ADD COLUMN col005 text    ;
INSERT INTO sync1_add_column_heap_wo_default4 VALUES (generate_series(1,10),'e', generate_series(1,10),true,'100',repeat('text_',100));
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
INSERT INTO sync1_add_column_heap_wo_default4 SELECT a, col001, col002, col003, col004, col005 from sync1_heap1;
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
--
--
ALTER TABLE sync1_add_column_heap_wo_default4 ADD COLUMN col006 integer[]  ;
INSERT INTO sync1_add_column_heap_wo_default4 VALUES (generate_series(1,10),'f', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}');
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
INSERT INTO sync1_add_column_heap_wo_default4 SELECT a, col001, col002, col003, col004, col005, col006 from sync1_heap1;
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
--
--

ALTER TABLE sync1_add_column_heap_wo_default4 ADD COLUMN col007 character varying(512)  ;
INSERT INTO sync1_add_column_heap_wo_default4 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello');
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
INSERT INTO sync1_add_column_heap_wo_default4 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 from sync1_heap1;
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
--
--

ALTER TABLE sync1_add_column_heap_wo_default4 ADD COLUMN col008 character varying   ;
INSERT INTO sync1_add_column_heap_wo_default4 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo');
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
INSERT INTO sync1_add_column_heap_wo_default4 SELECT a, col001, col002 , col003, col004, col005, col006, col007  , col008 from sync1_heap1;
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
--
--

ALTER TABLE sync1_add_column_heap_wo_default4 ADD COLUMN col009 character varying(512)[]  ;
INSERT INTO sync1_add_column_heap_wo_default4 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' );
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
INSERT INTO sync1_add_column_heap_wo_default4 SELECT a, col001 , col002 , col003, col004, col005, col006, col007, col008, col009 from sync1_heap1;
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
--
--
ALTER TABLE sync1_add_column_heap_wo_default4 ADD COLUMN col010 numeric(8)  ;
INSERT INTO sync1_add_column_heap_wo_default4 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127);
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
INSERT INTO sync1_add_column_heap_wo_default4 SELECT a, col001, col002 , col003, col004, col005, col006, col007 , col008, col009, col010 from sync1_heap1;
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
--
--
ALTER TABLE sync1_add_column_heap_wo_default4 ADD COLUMN col011 int  ;
INSERT INTO sync1_add_column_heap_wo_default4 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13);
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
INSERT INTO sync1_add_column_heap_wo_default4 SELECT a, col001, col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011 from sync1_heap1;
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
--
--
ALTER TABLE sync1_add_column_heap_wo_default4 ADD COLUMN col012 double precision  ;
INSERT INTO sync1_add_column_heap_wo_default4 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80);
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
INSERT INTO sync1_add_column_heap_wo_default4 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 from sync1_heap1;
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
--
--
ALTER TABLE sync1_add_column_heap_wo_default4 ADD COLUMN col013 bigint   ;
INSERT INTO sync1_add_column_heap_wo_default4 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654);
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
INSERT INTO sync1_add_column_heap_wo_default4 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013 from sync1_heap1;
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
--
--
ALTER TABLE sync1_add_column_heap_wo_default4 ADD COLUMN col014 char(8)  ;
INSERT INTO sync1_add_column_heap_wo_default4 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana');
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
INSERT INTO sync1_add_column_heap_wo_default4 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014 from sync1_heap1;
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
--
--
ALTER TABLE sync1_add_column_heap_wo_default4 ADD COLUMN col015 bytea  ;
INSERT INTO sync1_add_column_heap_wo_default4 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a');
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
INSERT INTO sync1_add_column_heap_wo_default4 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014,col015 from sync1_heap1;
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
--
--
ALTER TABLE sync1_add_column_heap_wo_default4 ADD COLUMN col016 timestamp with time zone  ;
INSERT INTO sync1_add_column_heap_wo_default4 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50');
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
INSERT INTO sync1_add_column_heap_wo_default4 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016 from sync1_heap1;
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
--
--
ALTER TABLE sync1_add_column_heap_wo_default4 ADD COLUMN col017 interval  ;
INSERT INTO sync1_add_column_heap_wo_default4 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5');
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
INSERT INTO sync1_add_column_heap_wo_default4 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 from sync1_heap1;
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
--
--
ALTER TABLE sync1_add_column_heap_wo_default4 ADD COLUMN col018 cidr  ;
INSERT INTO sync1_add_column_heap_wo_default4 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0');
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
INSERT INTO sync1_add_column_heap_wo_default4 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 from sync1_heap1;
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
--
--
ALTER TABLE sync1_add_column_heap_wo_default4 ADD COLUMN col019 inet  ;
INSERT INTO sync1_add_column_heap_wo_default4 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0');
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
INSERT INTO sync1_add_column_heap_wo_default4 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019 from sync1_heap1;
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
--
--
ALTER TABLE sync1_add_column_heap_wo_default4 ADD COLUMN col020 macaddr  ;
INSERT INTO sync1_add_column_heap_wo_default4 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB');
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
INSERT INTO sync1_add_column_heap_wo_default4 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020 from sync1_heap1;
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;

--
--
ALTER TABLE sync1_add_column_heap_wo_default4 ADD COLUMN col022 money  ;
INSERT INTO sync1_add_column_heap_wo_default4 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00');
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
INSERT INTO sync1_add_column_heap_wo_default4 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022 from sync1_heap1;
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
--
--

ALTER TABLE sync1_add_column_heap_wo_default4 ADD COLUMN col024 timetz  ;
INSERT INTO sync1_add_column_heap_wo_default4 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01');
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
INSERT INTO sync1_add_column_heap_wo_default4 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024 from sync1_heap1;
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
--
--
ALTER TABLE sync1_add_column_heap_wo_default4 ADD COLUMN col025 circle   ;
INSERT INTO sync1_add_column_heap_wo_default4 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)');
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
INSERT INTO sync1_add_column_heap_wo_default4 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 from sync1_heap1;
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
--
--
ALTER TABLE sync1_add_column_heap_wo_default4 ADD COLUMN col026 box  ;
INSERT INTO sync1_add_column_heap_wo_default4 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))');
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
 INSERT INTO sync1_add_column_heap_wo_default4 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026 from sync1_heap1;
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
--
--
ALTER TABLE sync1_add_column_heap_wo_default4 ADD COLUMN col027 name   ;
INSERT INTO sync1_add_column_heap_wo_default4 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom');
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
 INSERT INTO sync1_add_column_heap_wo_default4 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 from sync1_heap1;
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
--
--
ALTER TABLE sync1_add_column_heap_wo_default4 ADD COLUMN col028 path  ;
INSERT INTO sync1_add_column_heap_wo_default4 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))');
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
 INSERT INTO sync1_add_column_heap_wo_default4 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028 from sync1_heap1;
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
--
--
ALTER TABLE sync1_add_column_heap_wo_default4 ADD COLUMN col029 int2  ;
INSERT INTO sync1_add_column_heap_wo_default4 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11);
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
 INSERT INTO sync1_add_column_heap_wo_default4 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 from sync1_heap1;
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
--
--
ALTER TABLE sync1_add_column_heap_wo_default4 ADD COLUMN col031 bit varying(256)  ;
INSERT INTO sync1_add_column_heap_wo_default4 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11, '1001101');
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
 INSERT INTO sync1_add_column_heap_wo_default4 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 , col031 from sync1_heap1;
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
--
--
ALTER TABLE sync1_add_column_heap_wo_default4 ADD COLUMN col032 date  ;
INSERT INTO sync1_add_column_heap_wo_default4 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11, '1001101',  '2008-08-08');
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
 INSERT INTO sync1_add_column_heap_wo_default4 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 , col031, col032 from sync1_heap1;
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
--
--
ALTER TABLE sync1_add_column_heap_wo_default4 ADD COLUMN col034 lseg  ;
INSERT INTO sync1_add_column_heap_wo_default4 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11, '1001101',  '2008-08-08' , '((2,3),(3,2))');
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
 INSERT INTO sync1_add_column_heap_wo_default4 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 from sync1_heap1;
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
--
--
ALTER TABLE sync1_add_column_heap_wo_default4 ADD COLUMN col035 point  ;
INSERT INTO sync1_add_column_heap_wo_default4 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11, '1001101',  '2008-08-08' , '((2,3),(3,2))', '(4,4)');
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
 INSERT INTO sync1_add_column_heap_wo_default4 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035 from sync1_heap1;
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
--
--

ALTER TABLE sync1_add_column_heap_wo_default4 ADD COLUMN col036 polygon  ;
INSERT INTO sync1_add_column_heap_wo_default4 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11, '1001101',  '2008-08-08' , '((2,3),(3,2))', '(4,4)' , '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))' );
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
 INSERT INTO sync1_add_column_heap_wo_default4 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 from sync1_heap1;
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
--
--
ALTER TABLE sync1_add_column_heap_wo_default4 ADD COLUMN col037 real   ;
INSERT INTO sync1_add_column_heap_wo_default4 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11, '1001101',  '2008-08-08' , '((2,3),(3,2))', '(4,4)' , '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))' , 33435);
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
 INSERT INTO sync1_add_column_heap_wo_default4 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037 from sync1_heap1;
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
--
--

ALTER TABLE sync1_add_column_heap_wo_default4 ADD COLUMN col039 time  ;
INSERT INTO sync1_add_column_heap_wo_default4 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11, '1001101',  '2008-08-08' , '((2,3),(3,2))', '(4,4)' , '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))' , 33435, '01:05:10');
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
 INSERT INTO sync1_add_column_heap_wo_default4 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 from sync1_heap1;
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
--
--
 
ALTER TABLE sync1_add_column_heap_wo_default4 ADD COLUMN col040 timestamp   ;
INSERT INTO sync1_add_column_heap_wo_default4 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11, '1001101',  '2008-08-08' , '((2,3),(3,2))', '(4,4)' , '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))' , 33435, '01:05:10' ,  '2009-09-09 01:55:19');
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
 INSERT INTO sync1_add_column_heap_wo_default4 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from sync1_heap1;
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;


--
--
 
ALTER TABLE sync1_add_column_heap_wo_default4 ADD COLUMN col021 serial ;
INSERT INTO sync1_add_column_heap_wo_default4 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11, '1001101',  '2008-08-08' , '((2,3),(3,2))', '(4,4)' , '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))' , 33435, '01:05:10' ,  '2009-09-09 01:55:19',generate_series(1,10));
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
 INSERT INTO sync1_add_column_heap_wo_default4 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from sync1_heap1;
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;


--
--
 
ALTER TABLE sync1_add_column_heap_wo_default4 ADD COLUMN col023 bigserial;
INSERT INTO sync1_add_column_heap_wo_default4 VALUES (generate_series(1,10),'g', generate_series(1,10),true,'100',repeat('text_',100), '{1,2, 3, 4, 5}', 'hello', 'hellooooooooooo', '{aaa,bbb,ccc,ddd,eee}' ,100127,13,80.80,789654,'cobacana','a','2008-11-10 01:50:50','5','1.0.0.0','0.1.0.0','BB:BB:BB:BB:BB:BB' ,'90.00', '10:00:01' ,'((3,3),1)', '((2,3),(3,2))' ,'tom','((2,3),(3,2))',11, '1001101',  '2008-08-08' , '((2,3),(3,2))', '(4,4)' , '((1,2),(2,3),(3,4),(4,3),(3,2),(2,1))' , 33435, '01:05:10' ,  '2009-09-09 01:55:19',generate_series(1,10),generate_series(1,10));
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;
 INSERT INTO sync1_add_column_heap_wo_default4 SELECT a, col001 , col002 , col003, col004, col005, col006, col007 , col008, col009, col010, col011,col012 ,col013, col014, col015 , col016,  col017 ,col018 ,col019, col020, col022,col024, col025 , col026, col027 , col028, col029 , col031, col032 , col034 , col035, col036 , col037, col039 , col040 from sync1_heap1;
SELECT COUNT(*) from sync1_add_column_heap_wo_default4 ;



