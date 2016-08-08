psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.makearray(anyelement)
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.logany("any")
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.loganyelement(anyelement)
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.logcomplex(javatest.complex)
psql:/path/sql_file:1: NOTICE:  drop cascades to table javatest.complextest
psql:/path/sql_file:1: NOTICE:  drop cascades to type javatest.complex[]
psql:/path/sql_file:1: NOTICE:  drop cascades to type javatest.complex
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.complex_send(javatest.complex)
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.complex_recv(internal)
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.complex_out(javatest.complex)
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.complex_in(cstring)
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.countnulls(integer[])
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.countnulls(record)
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.executeselecttorecords(character varying)
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.mytest(text)
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.executeselect(character varying)
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.callmetadatamethod(character varying)
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.getmetadataints()
psql:/path/sql_file:1: NOTICE:  drop cascades to type javatest.metadataints
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.getmetadatastrings()
psql:/path/sql_file:1: NOTICE:  drop cascades to type javatest.metadatastrings
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.getmetadatabooleans()
psql:/path/sql_file:1: NOTICE:  drop cascades to type javatest.metadatabooleans
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.binarycolumntest()
psql:/path/sql_file:1: NOTICE:  drop cascades to type javatest.binarycolumnpair
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.logmessage(character varying,character varying)
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.gettimeasstring()
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.getdateasstring()
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.testtransactionrecovery()
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.testsavepointsanity()
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.listnonsupers()
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.listsupers()
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.randomints(integer)
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.scalarpropertyexample()
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.resultsetpropertyexample()
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.propertyexample()
psql:/path/sql_file:1: NOTICE:  drop cascades to type javatest._properties
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.nestedstatements(integer)
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.maxfromsetreturnexample(integer,integer)
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.hugenonimmutableresult(integer)
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.hugeresult(integer)
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.setreturnexample(integer,integer)
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.tuplereturntostring(javatest._testsetreturn)
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.tuplereturnexample(integer,integer)
psql:/path/sql_file:1: NOTICE:  drop cascades to type javatest._testsetreturn
psql:/path/sql_file:1: NOTICE:  drop cascades to table javatest.employees2
psql:/path/sql_file:1: NOTICE:  drop cascades to table javatest.employees1
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.transferpeople(integer)
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.create_temp_file_trusted()
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.java_getsystemproperty(character varying)
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.nulloneven(integer)
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.java_addone(integer)
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.printobj(integer[])
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.print(double precision[])
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.print(double precision)
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.print(real[])
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.print(real)
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.print(bigint[])
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.print(bigint)
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.print(integer[])
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.print(integer)
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.print(smallint[])
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.print(smallint)
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.print(bytea)
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.print("char")
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.print(timestamp with time zone)
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.print(time with time zone)
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.print(date)
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.java_gettimestamptz()
psql:/path/sql_file:1: NOTICE:  drop cascades to function javatest.java_gettimestamp()
psql:/path/sql_file:1: NOTICE:  drop cascades to table javatest.test
DROP SCHEMA
CREATE SCHEMA
SET
SET
SET
psql:/path/sql_file:1: ERROR:  language "pljava" already exists
SELECT 1
CREATE FUNCTION
    java_gettimestamp    
-------------------------
 2014-09-23 11:40:18.186
(1 row)

   java_gettimestamp    
------------------------
 2014-09-23 11:40:18.45
(1 row)

    java_gettimestamp    
-------------------------
 2014-09-23 11:40:18.452
(1 row)

CREATE FUNCTION
    java_gettimestamptz     
----------------------------
 2014-09-23 11:40:18.456-07
(1 row)

    java_gettimestamptz    
---------------------------
 2014-09-23 11:40:18.46-07
(1 row)

    java_gettimestamptz    
---------------------------
 2014-09-23 11:40:18.46-07
(1 row)

CREATE FUNCTION
psql:/path/examples.sql:35: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters Local Date is Sunday, October 10, 2010
psql:/path/examples.sql:35: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters UTC Date is Sunday, October 10, 2010
psql:/path/examples.sql:35: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters TZ =  Pacific Standard Time
 print 
-------
 
(1 row)

psql:/path/examples.sql:36: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters Local Date is Sunday, October 10, 2010  (seg0 slice1 rh55-qavm61:40000 pid=11119)
psql:/path/examples.sql:36: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters UTC Date is Sunday, October 10, 2010  (seg0 slice1 rh55-qavm61:40000 pid=11119)
psql:/path/examples.sql:36: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters TZ =  Pacific Standard Time  (seg0 slice1 rh55-qavm61:40000 pid=11119)
 print 
-------
 
(1 row)

psql:/path/examples.sql:37: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters Local Date is Sunday, October 10, 2010
psql:/path/examples.sql:37: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters UTC Date is Sunday, October 10, 2010
psql:/path/examples.sql:37: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters TZ =  Pacific Standard Time
 print 
-------
 
(1 row)

CREATE FUNCTION
psql:/path/examples.sql:44: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters Local Time is 13:00:00 PDT -0700
psql:/path/examples.sql:44: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters UTC Time is 20:00:00 UTC +0000
psql:/path/examples.sql:44: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters TZ =  Pacific Standard Time
 print 
-------
 
(1 row)

psql:/path/examples.sql:45: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters Local Time is 13:00:00 PDT -0700  (seg0 slice1 rh55-qavm61:40000 pid=11119)
psql:/path/examples.sql:45: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters UTC Time is 20:00:00 UTC +0000  (seg0 slice1 rh55-qavm61:40000 pid=11119)
psql:/path/examples.sql:45: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters TZ =  Pacific Standard Time  (seg0 slice1 rh55-qavm61:40000 pid=11119)
 print 
-------
 
(1 row)

psql:/path/examples.sql:46: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters Local Time is 13:00:00 PDT -0700
psql:/path/examples.sql:46: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters UTC Time is 20:00:00 UTC +0000
psql:/path/examples.sql:46: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters TZ =  Pacific Standard Time
 print 
-------
 
(1 row)

CREATE FUNCTION
psql:/path/sql_file:1: ERROR:  invalid input syntax for type timestamp with time zone: "12:00 PST"
LINE 1: SELECT javatest.print('12:00 PST'::timestamptz);
                              ^
psql:/path/sql_file:1: ERROR:  invalid input syntax for type timestamp with time zone: "12:00 PST"
LINE 1: SELECT javatest.print('12:00 PST'::timestamptz) FROM javates...
                              ^
psql:/path/sql_file:1: ERROR:  invalid input syntax for type timestamp with time zone: "12:00 PST"
LINE 1: SELECT * FROM javatest.print('12:00 PST'::timestamptz);
                                     ^
CREATE FUNCTION
psql:/path/sql_file:1: ERROR:  function javatest.print(character) does not exist
LINE 1: SELECT javatest.print('a'::char);
               ^
HINT:  No function matches the given name and argument types. You may need to add explicit type casts.
psql:/path/sql_file:1: ERROR:  function javatest.print(character) does not exist
LINE 1: SELECT javatest.print('a'::char) FROM javatest.test;
               ^
HINT:  No function matches the given name and argument types. You may need to add explicit type casts.
psql:/path/sql_file:1: ERROR:  function javatest.print(character) does not exist
LINE 1: SELECT * FROM javatest.print('a'::char);
                      ^
HINT:  No function matches the given name and argument types. You may need to add explicit type casts.
CREATE FUNCTION
psql:/path/examples.sql:71: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters byte[] of size 1 {97}
 print 
-------
 a
(1 row)

psql:/path/examples.sql:72: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters byte[] of size 1 {97}  (seg0 slice1 rh55-qavm61:40000 pid=11119)
 print 
-------
 a
(1 row)

psql:/path/examples.sql:73: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters byte[] of size 1 {97}
 print 
-------
 a
(1 row)

CREATE FUNCTION
psql:/path/examples.sql:81: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters short 2
 print 
-------
     2
(1 row)

psql:/path/examples.sql:82: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters short 2  (seg0 slice1 rh55-qavm61:40000 pid=11119)
 print 
-------
     2
(1 row)

psql:/path/examples.sql:83: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters short 2
 print 
-------
     2
(1 row)

CREATE FUNCTION
psql:/path/examples.sql:90: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters short[] of size 1 {2}
 print 
-------
 {2}
(1 row)

psql:/path/examples.sql:91: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters short[] of size 1 {2}  (seg0 slice1 rh55-qavm61:40000 pid=11119)
 print 
-------
 {2}
(1 row)

psql:/path/sql_file:1: ERROR:  syntax error at or near "]"
LINE 1: SELECT * FROM javatest.print('{2}'::int2[]]);
                                                  ^
CREATE FUNCTION
psql:/path/examples.sql:99: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters int 4
 print 
-------
     4
(1 row)

psql:/path/examples.sql:100: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters int 4  (seg0 slice1 rh55-qavm61:40000 pid=11119)
 print 
-------
     4
(1 row)

psql:/path/examples.sql:101: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters int 4
 print 
-------
     4
(1 row)

CREATE FUNCTION
psql:/path/examples.sql:109: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters int[] of size 1 {4}
 print 
-------
 {4}
(1 row)

psql:/path/examples.sql:110: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters int[] of size 1 {4}  (seg0 slice1 rh55-qavm61:40000 pid=11119)
 print 
-------
 {4}
(1 row)

psql:/path/sql_file:1: ERROR:  syntax error at or near "]"
LINE 1: SELECT * FROM javatest.print('{4}'::int4[]]);
                                                  ^
CREATE FUNCTION
psql:/path/examples.sql:118: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters long 8
 print 
-------
     8
(1 row)

psql:/path/examples.sql:119: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters long 8  (seg0 slice1 rh55-qavm61:40000 pid=11119)
 print 
-------
     8
(1 row)

psql:/path/examples.sql:120: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters long 8
 print 
-------
     8
(1 row)

CREATE FUNCTION
psql:/path/examples.sql:127: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters long[] of size 1 {8}
 print 
-------
 {8}
(1 row)

psql:/path/examples.sql:128: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters long[] of size 1 {8}  (seg0 slice1 rh55-qavm61:40000 pid=11119)
 print 
-------
 {8}
(1 row)

psql:/path/examples.sql:129: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters long[] of size 1 {8}
 print 
-------
 {8}
(1 row)

CREATE FUNCTION
psql:/path/examples.sql:136: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters float 4.4
 print 
-------
   4.4
(1 row)

psql:/path/examples.sql:137: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters float 4.4  (seg0 slice1 rh55-qavm61:40000 pid=11119)
 print 
-------
   4.4
(1 row)

psql:/path/examples.sql:138: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters float 4.4
 print 
-------
   4.4
(1 row)

CREATE FUNCTION
psql:/path/examples.sql:145: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters float[] of size 1 {4.4}
 print 
-------
 {4.4}
(1 row)

psql:/path/examples.sql:146: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters float[] of size 1 {4.4}  (seg0 slice1 rh55-qavm61:40000 pid=11119)
 print 
-------
 {4.4}
(1 row)

psql:/path/examples.sql:147: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters float[] of size 1 {4.4}
 print 
-------
 {4.4}
(1 row)

CREATE FUNCTION
psql:/path/examples.sql:154: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters double 8.8
 print 
-------
   8.8
(1 row)

psql:/path/examples.sql:155: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters double 8.8  (seg0 slice1 rh55-qavm61:40000 pid=11119)
 print 
-------
   8.8
(1 row)

psql:/path/examples.sql:156: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters double 8.8
 print 
-------
   8.8
(1 row)

CREATE FUNCTION
psql:/path/examples.sql:163: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters double[] of size 1 {8.8}
 print 
-------
 {8.8}
(1 row)

psql:/path/examples.sql:164: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters double[] of size 1 {8.8}  (seg0 slice1 rh55-qavm61:40000 pid=11119)
 print 
-------
 {8.8}
(1 row)

psql:/path/examples.sql:165: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters double[] of size 1 {8.8}
 print 
-------
 {8.8}
(1 row)

CREATE FUNCTION
psql:/path/examples.sql:172: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters Integer[] of size 1 {4}
 printobj 
----------
 {4}
(1 row)

psql:/path/examples.sql:173: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters Integer[] of size 1 {4}  (seg0 slice1 rh55-qavm61:40000 pid=11119)
 printobj 
----------
 {4}
(1 row)

psql:/path/examples.sql:174: INFO:  23 Sep 14 11:40:17 org.postgresql.example.Parameters Integer[] of size 1 {4}
 printobj 
----------
 {4}
(1 row)

CREATE FUNCTION
 java_addone 
-------------
           2
(1 row)

 java_addone 
-------------
           2
(1 row)

 java_addone 
-------------
           2
(1 row)

CREATE FUNCTION
 nulloneven 
------------
          1
(1 row)

 nulloneven 
------------
           
(1 row)

 nulloneven 
------------
          1
(1 row)

 nulloneven 
------------
           
(1 row)

 nulloneven 
------------
          1
(1 row)

 nulloneven 
------------
           
(1 row)

CREATE FUNCTION
 java_getsystemproperty 
------------------------
 /opt/jdk1.6.0_21/jre
(1 row)

 java_getsystemproperty 
------------------------
 /opt/jdk1.6.0_21/jre
(1 row)

 java_getsystemproperty 
------------------------
 /opt/jdk1.6.0_21/jre
(1 row)

CREATE FUNCTION
psql:/path/sql_file:1: ERROR:  java.lang.ExceptionInInitializerError (JNICalls.c:70)
psql:/path/sql_file:1: ERROR:  java.lang.ExceptionInInitializerError (JNICalls.c:70)  (seg0 slice1 rh55-qavm61:40000 pid=11119) (cdbdisp.c:1526)
psql:/path/sql_file:1: ERROR:  java.lang.NoClassDefFoundError: Could not initialize class java.io.File$LazyInitialization (JNICalls.c:70)
CREATE FUNCTION
psql:/path/sql_file:1: NOTICE:  CREATE TABLE / PRIMARY KEY will create implicit index "employees1_pkey" for table "employees1"
CREATE TABLE
psql:/path/sql_file:1: NOTICE:  CREATE TABLE / PRIMARY KEY will create implicit index "employees2_pkey" for table "employees2"
CREATE TABLE
INSERT 0 1
INSERT 0 1
INSERT 0 1
INSERT 0 1
psql:/path/examples.sql:250: INFO:  23 Sep 14 11:40:18 org.postgresql.example.SPIActions SELECT id, name, salary FROM employees1 WHERE salary > ?
psql:/path/examples.sql:250: INFO:  23 Sep 14 11:40:18 org.postgresql.example.SPIActions INSERT INTO employees2(id, name, salary, transferDay, transferTime) VALUES (?, ?, ?, ?, ?)
psql:/path/examples.sql:250: INFO:  23 Sep 14 11:40:18 org.postgresql.example.SPIActions DELETE FROM employees1 WHERE id = ?
psql:/path/examples.sql:250: INFO:  23 Sep 14 11:40:18 org.postgresql.example.SPIActions assigning parameter value 1
psql:/path/examples.sql:250: INFO:  23 Sep 14 11:40:18 org.postgresql.example.SPIActions Executing query
psql:/path/examples.sql:250: INFO:  23 Sep 14 11:40:18 org.postgresql.example.SPIActions Doing next
psql:/path/examples.sql:250: INFO:  23 Sep 14 11:40:18 org.postgresql.example.SPIActions Processing row 1
psql:/path/examples.sql:250: INFO:  23 Sep 14 11:40:18 org.postgresql.example.SPIActions Insert processed 1 rows
psql:/path/examples.sql:250: INFO:  23 Sep 14 11:40:18 org.postgresql.example.SPIActions Delete processed 1 rows
psql:/path/examples.sql:250: INFO:  23 Sep 14 11:40:18 org.postgresql.example.SPIActions Doing next
psql:/path/examples.sql:250: INFO:  23 Sep 14 11:40:18 org.postgresql.example.SPIActions Processing row 2
psql:/path/examples.sql:250: INFO:  23 Sep 14 11:40:18 org.postgresql.example.SPIActions Insert processed 1 rows
psql:/path/examples.sql:250: INFO:  23 Sep 14 11:40:18 org.postgresql.example.SPIActions Delete processed 1 rows
psql:/path/examples.sql:250: INFO:  23 Sep 14 11:40:18 org.postgresql.example.SPIActions Doing next
psql:/path/examples.sql:250: INFO:  23 Sep 14 11:40:18 org.postgresql.example.SPIActions Processing row 3
psql:/path/examples.sql:250: INFO:  23 Sep 14 11:40:18 org.postgresql.example.SPIActions Insert processed 1 rows
psql:/path/examples.sql:250: INFO:  23 Sep 14 11:40:18 org.postgresql.example.SPIActions Delete processed 1 rows
psql:/path/examples.sql:250: INFO:  23 Sep 14 11:40:18 org.postgresql.example.SPIActions Doing next
psql:/path/examples.sql:250: INFO:  23 Sep 14 11:40:18 org.postgresql.example.SPIActions Processing row 4
psql:/path/examples.sql:250: INFO:  23 Sep 14 11:40:18 org.postgresql.example.SPIActions Insert processed 1 rows
psql:/path/examples.sql:250: INFO:  23 Sep 14 11:40:18 org.postgresql.example.SPIActions Delete processed 1 rows
psql:/path/examples.sql:250: INFO:  23 Sep 14 11:40:18 org.postgresql.example.SPIActions Doing next
 transferpeople 
----------------
              4
(1 row)

 id | name | salary 
----+------+--------
(0 rows)

 id | name  | salary | transferday | transfertime 
----+-------+--------+-------------+--------------
  1 | Adam  |    100 | 2014-09-23  | 11:40:18.381
  2 | Brian |    200 | 2014-09-23  | 11:40:18.384
  3 | Caleb |    300 | 2014-09-23  | 11:40:18.386
  4 | David |    400 | 2014-09-23  | 11:40:18.387
(4 rows)

psql:/path/examples.sql:253: INFO:  23 Sep 14 11:40:18 org.postgresql.example.SPIActions SELECT id, name, salary FROM employees1 WHERE salary > ?  (seg0 slice1 rh55-qavm61:40000 pid=11119)
psql:/path/examples.sql:253: INFO:  23 Sep 14 11:40:18 org.postgresql.example.SPIActions INSERT INTO employees2(id, name, salary, transferDay, transferTime) VALUES (?, ?, ?, ?, ?)  (seg0 slice1 rh55-qavm61:40000 pid=11119)
psql:/path/examples.sql:253: INFO:  23 Sep 14 11:40:18 org.postgresql.example.SPIActions DELETE FROM employees1 WHERE id = ?  (seg0 slice1 rh55-qavm61:40000 pid=11119)
psql:/path/examples.sql:253: INFO:  23 Sep 14 11:40:18 org.postgresql.example.SPIActions assigning parameter value 1  (seg0 slice1 rh55-qavm61:40000 pid=11119)
psql:/path/examples.sql:253: INFO:  23 Sep 14 11:40:18 org.postgresql.example.SPIActions Executing query  (seg0 slice1 rh55-qavm61:40000 pid=11119)
psql:/path/sql_file:1: ERROR:  function cannot execute on segment because it accesses relation "javatest.employees1" (functions.c:155)  (seg0 slice1 rh55-qavm61:40000 pid=11119) (cdbdisp.c:1526)
DETAIL:  SQL statement "SELECT id, name, salary FROM employees1 WHERE salary > $1"
CREATE TYPE
CREATE FUNCTION
         tuplereturnexample         
------------------------------------
 (2,6,"2014-09-23 11:40:19.414-07")
(1 row)

 base | incbase |           ctime            
------+---------+----------------------------
    2 |       6 | 2014-09-23 11:40:19.414-07
(1 row)

         tuplereturnexample         
------------------------------------
 (2,6,"2014-09-23 11:40:19.416-07")
(1 row)

 base | incbase |           ctime            
------+---------+----------------------------
    2 |       6 | 2014-09-23 11:40:19.418-07
(1 row)

 base | incbase |           ctime           
------+---------+---------------------------
    2 |       6 | 2014-09-23 11:40:19.42-07
(1 row)

CREATE FUNCTION
                     tuplereturntostring                      
--------------------------------------------------------------
 Base = "2", incbase = "6", ctime = "2014-09-23 11:40:19.424"
(1 row)

                     tuplereturntostring                      
--------------------------------------------------------------
 Base = "2", incbase = "6", ctime = "2014-09-23 11:40:19.426"
(1 row)

                     tuplereturntostring                      
--------------------------------------------------------------
 Base = "2", incbase = "6", ctime = "2014-09-23 11:40:19.428"
(1 row)

CREATE FUNCTION
          setreturnexample           
-------------------------------------
 (2,2,"2014-09-23 11:40:19.434-07")
 (2,6,"2014-09-23 11:40:19.434-07")
 (2,10,"2014-09-23 11:40:19.434-07")
 (2,14,"2014-09-23 11:40:19.434-07")
 (2,18,"2014-09-23 11:40:19.434-07")
 (2,22,"2014-09-23 11:40:19.434-07")
 (2,26,"2014-09-23 11:40:19.434-07")
 (2,30,"2014-09-23 11:40:19.434-07")
 (2,34,"2014-09-23 11:40:19.434-07")
 (2,38,"2014-09-23 11:40:19.434-07")
 (2,42,"2014-09-23 11:40:19.434-07")
 (2,46,"2014-09-23 11:40:19.434-07")
(12 rows)

 base | incbase |           ctime            
------+---------+----------------------------
    2 |       2 | 2014-09-23 11:40:19.436-07
    2 |       6 | 2014-09-23 11:40:19.436-07
    2 |      10 | 2014-09-23 11:40:19.436-07
    2 |      14 | 2014-09-23 11:40:19.436-07
    2 |      18 | 2014-09-23 11:40:19.436-07
    2 |      22 | 2014-09-23 11:40:19.436-07
    2 |      26 | 2014-09-23 11:40:19.436-07
    2 |      30 | 2014-09-23 11:40:19.436-07
    2 |      34 | 2014-09-23 11:40:19.436-07
    2 |      38 | 2014-09-23 11:40:19.436-07
    2 |      42 | 2014-09-23 11:40:19.436-07
    2 |      46 | 2014-09-23 11:40:19.436-07
(12 rows)

          setreturnexample          
------------------------------------
 (2,2,"2014-09-23 11:40:19.44-07")
 (2,6,"2014-09-23 11:40:19.44-07")
 (2,10,"2014-09-23 11:40:19.44-07")
 (2,14,"2014-09-23 11:40:19.44-07")
 (2,18,"2014-09-23 11:40:19.44-07")
 (2,22,"2014-09-23 11:40:19.44-07")
 (2,26,"2014-09-23 11:40:19.44-07")
 (2,30,"2014-09-23 11:40:19.44-07")
 (2,34,"2014-09-23 11:40:19.44-07")
 (2,38,"2014-09-23 11:40:19.44-07")
 (2,42,"2014-09-23 11:40:19.44-07")
 (2,46,"2014-09-23 11:40:19.44-07")
(12 rows)

 base | incbase |           ctime            
------+---------+----------------------------
    2 |       2 | 2014-09-23 11:40:19.442-07
    2 |       6 | 2014-09-23 11:40:19.442-07
    2 |      10 | 2014-09-23 11:40:19.442-07
    2 |      14 | 2014-09-23 11:40:19.442-07
    2 |      18 | 2014-09-23 11:40:19.442-07
    2 |      22 | 2014-09-23 11:40:19.442-07
    2 |      26 | 2014-09-23 11:40:19.442-07
    2 |      30 | 2014-09-23 11:40:19.442-07
    2 |      34 | 2014-09-23 11:40:19.442-07
    2 |      38 | 2014-09-23 11:40:19.442-07
    2 |      42 | 2014-09-23 11:40:19.442-07
    2 |      46 | 2014-09-23 11:40:19.442-07
(12 rows)

 base | incbase |           ctime            
------+---------+----------------------------
    2 |       2 | 2014-09-23 11:40:19.444-07
    2 |       6 | 2014-09-23 11:40:19.444-07
    2 |      10 | 2014-09-23 11:40:19.444-07
    2 |      14 | 2014-09-23 11:40:19.444-07
    2 |      18 | 2014-09-23 11:40:19.444-07
    2 |      22 | 2014-09-23 11:40:19.444-07
    2 |      26 | 2014-09-23 11:40:19.444-07
    2 |      30 | 2014-09-23 11:40:19.444-07
    2 |      34 | 2014-09-23 11:40:19.444-07
    2 |      38 | 2014-09-23 11:40:19.444-07
    2 |      42 | 2014-09-23 11:40:19.444-07
    2 |      46 | 2014-09-23 11:40:19.444-07
(12 rows)

CREATE FUNCTION
psql:/path/examples.sql:313: INFO:  23 Sep 14 11:40:18 org.postgresql.example.HugeResultSet HugeResultSet ends
                 hugeresult                  
---------------------------------------------
 (0,-645689783,"2014-09-23 11:40:19.45-07")
 (1,428878451,"2014-09-23 11:40:19.45-07")
 (2,876991375,"2014-09-23 11:40:19.45-07")
 (3,-1693080359,"2014-09-23 11:40:19.45-07")
(4 rows)

psql:/path/examples.sql:314: INFO:  23 Sep 14 11:40:18 org.postgresql.example.HugeResultSet HugeResultSet ends
 base |   incbase   |           ctime            
------+-------------+----------------------------
    0 |  -644535536 | 2014-09-23 11:40:19.452-07
    1 |  1257392019 | 2014-09-23 11:40:19.452-07
    2 | -1307571536 | 2014-09-23 11:40:19.452-07
    3 |   869796446 | 2014-09-23 11:40:19.452-07
(4 rows)

psql:/path/examples.sql:315: INFO:  23 Sep 14 11:40:18 org.postgresql.example.HugeResultSet HugeResultSet ends  (seg0 slice1 rh55-qavm61:40000 pid=11119)
                  hugeresult                  
----------------------------------------------
 (0,-644920285,"2014-09-23 11:40:19.454-07")
 (1,-1882090701,"2014-09-23 11:40:19.454-07")
 (2,-579383899,"2014-09-23 11:40:19.454-07")
 (3,15504178,"2014-09-23 11:40:19.454-07")
(4 rows)

psql:/path/examples.sql:316: INFO:  23 Sep 14 11:40:18 org.postgresql.example.HugeResultSet HugeResultSet ends  (seg0 slice1 rh55-qavm61:40000 pid=11119)
 base |  incbase   |           ctime            
------+------------+----------------------------
    0 | -647228778 | 2014-09-23 11:40:19.458-07
    1 |  755849459 | 2014-09-23 11:40:19.458-07
    2 | -505225372 | 2014-09-23 11:40:19.458-07
    3 | -815282136 | 2014-09-23 11:40:19.458-07
(4 rows)

psql:/path/examples.sql:317: INFO:  23 Sep 14 11:40:18 org.postgresql.example.HugeResultSet HugeResultSet ends
 base |  incbase   |           ctime            
------+------------+----------------------------
    0 | -647228778 | 2014-09-23 11:40:19.458-07
    1 |  755849459 | 2014-09-23 11:40:19.46-07
    2 | -505225372 | 2014-09-23 11:40:19.46-07
    3 | -815282136 | 2014-09-23 11:40:19.46-07
(4 rows)

CREATE FUNCTION
psql:/path/examples.sql:324: INFO:  23 Sep 14 11:40:18 org.postgresql.example.HugeResultSet HugeResultSet ends
           hugenonimmutableresult            
---------------------------------------------
 (0,-648383025,"2014-09-23 11:40:19.464-07")
 (1,-72664109,"2014-09-23 11:40:19.464-07")
 (2,1679337540,"2014-09-23 11:40:19.464-07")
 (3,916808355,"2014-09-23 11:40:19.464-07")
(4 rows)

psql:/path/examples.sql:325: INFO:  23 Sep 14 11:40:18 org.postgresql.example.HugeResultSet HugeResultSet ends
 base |   incbase   |           ctime            
------+-------------+----------------------------
    0 |  -648767774 | 2014-09-23 11:40:19.466-07
    1 |  1082820467 | 2014-09-23 11:40:19.466-07
    2 | -1887442119 | 2014-09-23 11:40:19.466-07
    3 |    62516086 | 2014-09-23 11:40:19.466-07
(4 rows)

psql:/path/examples.sql:326: INFO:  23 Sep 14 11:40:18 org.postgresql.example.HugeResultSet HugeResultSet ends  (seg0 slice1 rh55-qavm61:40000 pid=11119)
            hugenonimmutableresult            
----------------------------------------------
 (0,-647613527,"2014-09-23 11:40:19.468-07")
 (1,1911334035,"2014-09-23 11:40:19.468-07")
 (2,222962266,"2014-09-23 11:40:19.468-07")
 (3,-1669574405,"2014-09-23 11:40:19.468-07")
(4 rows)

psql:/path/examples.sql:328: INFO:  23 Sep 14 11:40:18 org.postgresql.example.HugeResultSet HugeResultSet ends  (seg0 slice1 rh55-qavm61:40000 pid=11119)
 base |   incbase   |           ctime           
------+-------------+---------------------------
    0 |  -647998276 | 2014-09-23 11:40:19.47-07
    1 | -1228148685 | 2014-09-23 11:40:19.47-07
    2 |   951149903 | 2014-09-23 11:40:19.47-07
    3 |  1771100623 | 2014-09-23 11:40:19.47-07
(4 rows)

psql:/path/examples.sql:329: INFO:  23 Sep 14 11:40:18 org.postgresql.example.HugeResultSet HugeResultSet ends
 base |   incbase   |           ctime            
------+-------------+----------------------------
    0 |  -637610056 | 2014-09-23 11:40:19.472-07
    1 |  1933506132 | 2014-09-23 11:40:19.472-07
    2 | -1530047118 | 2014-09-23 11:40:19.472-07
    3 |  -932811907 | 2014-09-23 11:40:19.472-07
(4 rows)

CREATE FUNCTION
 maxfromsetreturnexample 
-------------------------
                       2
(1 row)

 maxfromsetreturnexample 
-------------------------
                       2
(1 row)

 maxfromsetreturnexample 
-------------------------
                       2
(1 row)

CREATE FUNCTION
psql:/path/examples.sql:345: INFO:  23 Sep 14 11:40:18 org.postgresql.example.HugeResultSet HugeResultSet ends
 nestedstatements 
------------------
 
(1 row)

psql:/path/sql_file:1: ERROR:  function cannot execute on segment because it issues a non-SELECT statement (functions.c:135)  (seg0 slice1 rh55-qavm61:40000 pid=11119) (cdbdisp.c:1526)
DETAIL:  SQL statement "DELETE FROM javatest.employees1"
psql:/path/examples.sql:347: INFO:  23 Sep 14 11:40:19 org.postgresql.example.HugeResultSet HugeResultSet ends
 nestedstatements 
------------------
 
(1 row)

CREATE TYPE
CREATE FUNCTION
CREATE FUNCTION
CREATE FUNCTION
CREATE FUNCTION
 randomints 
------------
 -874230629
 -408050789
 -371249005
(3 rows)

 randomints  
-------------
  -876539123
 -2065077925
  -297090478
(3 rows)

 randomints  
-------------
  -876539123
 -2065077925
  -297090478
(3 rows)

CREATE FUNCTION
              listsupers              
--------------------------------------
 (@user@,10,t,t,t,********,,)
(1 row)

   usename   | usesysid | usecreatedb | usesuper | usecatupd |  passwd  | valuntil | useconfig 
-------------+----------+-------------+----------+-----------+----------+----------+-----------
 @user@     |       10 | t           | t        | t         | ******** |          | 
(1 row)

              listsupers              
--------------------------------------
 (@user@,10,t,t,t,********,,)
(1 row)

   usename   | usesysid | usecreatedb | usesuper | usecatupd |  passwd  | valuntil | useconfig 
-------------+----------+-------------+----------+-----------+----------+----------+-----------
 @user@     |       10 | t           | t        | t         | ******** |          | 
(1 row)

   usename   | usesysid | usecreatedb | usesuper | usecatupd |  passwd  | valuntil | useconfig 
-------------+----------+-------------+----------+-----------+----------+----------+-----------
 @user@     |       10 | t           | t        | t         | ******** |          | 
(1 row)

CREATE FUNCTION
            listnonsupers            
-------------------------------------
 (pltestuser,44212,f,f,f,********,,)
(1 row)

  usename   | usesysid | usecreatedb | usesuper | usecatupd |  passwd  | valuntil | useconfig 
------------+----------+-------------+----------+-----------+----------+----------+-----------
 pltestuser |    44212 | f           | f        | f         | ******** |          | 
(1 row)

            listnonsupers            
-------------------------------------
 (pltestuser,44212,f,f,f,********,,)
(1 row)

  usename   | usesysid | usecreatedb | usesuper | usecatupd |  passwd  | valuntil | useconfig 
------------+----------+-------------+----------+-----------+----------+----------+-----------
 pltestuser |    44212 | f           | f        | f         | ******** |          | 
(1 row)

  usename   | usesysid | usecreatedb | usesuper | usecatupd |  passwd  | valuntil | useconfig 
------------+----------+-------------+----------+-----------+----------+----------+-----------
 pltestuser |    44212 | f           | f        | f         | ******** |          | 
(1 row)

CREATE FUNCTION
CREATE FUNCTION
CREATE FUNCTION
 getdateasstring 
-----------------
 2014-09-23
(1 row)

 getdateasstring 
-----------------
 2014-09-23
(1 row)

 getdateasstring 
-----------------
 2014-09-23
(1 row)

CREATE FUNCTION
 gettimeasstring 
-----------------
 11:40:19
(1 row)

 gettimeasstring 
-----------------
 11:40:19
(1 row)

 gettimeasstring 
-----------------
 11:40:19
(1 row)

CREATE FUNCTION
psql:/path/sql_file:1: ERROR:  23 Sep 14 11:40:19 org.postgresql.example.LoggerTest hello (Backend.c:898)
psql:/path/sql_file:1: WARNING:  23 Sep 14 11:40:19 org.postgresql.example.LoggerTest hello
 logmessage 
------------
 
(1 row)

psql:/path/examples.sql:438: INFO:  23 Sep 14 11:40:19 org.postgresql.example.LoggerTest hello
 logmessage 
------------
 
(1 row)

 logmessage 
------------
 
(1 row)

CREATE TYPE
CREATE FUNCTION
psql:/path/sql_file:1: ERROR:  java.lang.ClassNotFoundException: org.postgresql.example.BinaryColumnTest (JNICalls.c:70)
psql:/path/sql_file:1: ERROR:  java.lang.ClassNotFoundException: org.postgresql.example.BinaryColumnTest (JNICalls.c:70)  (seg0 slice1 rh55-qavm61:40000 pid=11119) (cdbdisp.c:1526)
CREATE TYPE
CREATE FUNCTION
                    getmetadatabooleans                    
-----------------------------------------------------------
 (allProceduresAreCallable,t)
 (allTablesAreSelectable,t)
 (autoCommitFailureClosesAllResultSets,f)
 (dataDefinitionCausesTransactionCommit,f)
 (dataDefinitionIgnoredInTransactions,f)
 (doesMaxRowSizeIncludeBlobs,f)
 (isCatalogAtStart,t)
 (isReadOnly,f)
 (locatorsUpdateCopy,t)
 (nullPlusNonNullIsNull,t)
 (nullsAreSortedAtEnd,f)
 (nullsAreSortedAtStart,f)
 (nullsAreSortedHigh,t)
 (nullsAreSortedLow,f)
 (storesLowerCaseIdentifiers,t)
 (storesLowerCaseQuotedIdentifiers,f)
 (storesMixedCaseIdentifiers,f)
 (storesMixedCaseQuotedIdentifiers,f)
 (storesUpperCaseIdentifiers,f)
 (storesUpperCaseQuotedIdentifiers,f)
 (supportsANSI92EntryLevelSQL,t)
 (supportsANSI92FullSQL,f)
 (supportsANSI92IntermediateSQL,f)
 (supportsAlterTableWithAddColumn,t)
 (supportsAlterTableWithDropColumn,t)
 (supportsBatchUpdates,t)
 (supportsCatalogsInDataManipulation,f)
 (supportsCatalogsInIndexDefinitions,f)
 (supportsCatalogsInPrivilegeDefinitions,f)
 (supportsCatalogsInProcedureCalls,f)
 (supportsCatalogsInTableDefinitions,f)
 (supportsColumnAliasing,t)
 (supportsConvert,f)
 (supportsCoreSQLGrammar,f)
 (supportsCorrelatedSubqueries,t)
 (supportsDataDefinitionAndDataManipulationTransactions,t)
 (supportsDataManipulationTransactionsOnly,f)
 (supportsDifferentTableCorrelationNames,f)
 (supportsExpressionsInOrderBy,t)
 (supportsExtendedSQLGrammar,f)
 (supportsFullOuterJoins,t)
 (supportsGetGeneratedKeys,f)
 (supportsGroupBy,t)
 (supportsGroupByBeyondSelect,t)
 (supportsGroupByUnrelated,t)
 (supportsIntegrityEnhancementFacility,t)
 (supportsLikeEscapeClause,t)
 (supportsLimitedOuterJoins,t)
 (supportsMinimumSQLGrammar,t)
 (supportsMixedCaseIdentifiers,f)
 (supportsMixedCaseQuotedIdentifiers,t)
 (supportsMultipleOpenResults,f)
 (supportsMultipleResultSets,f)
 (supportsMultipleTransactions,t)
 (supportsNamedParameters,f)
 (supportsNonNullableColumns,t)
 (supportsOpenCursorsAcrossCommit,f)
 (supportsOpenCursorsAcrossRollback,f)
 (supportsOpenStatementsAcrossCommit,t)
 (supportsOpenStatementsAcrossRollback,t)
 (supportsOrderByUnrelated,t)
 (supportsOuterJoins,t)
 (supportsPositionedDelete,f)
 (supportsPositionedUpdate,f)
 (supportsSavepoints,t)
 (supportsSchemasInDataManipulation,t)
 (supportsSchemasInIndexDefinitions,t)
 (supportsSchemasInPrivilegeDefinitions,t)
 (supportsSchemasInProcedureCalls,t)
 (supportsSchemasInTableDefinitions,t)
 (supportsSelectForUpdate,t)
 (supportsStatementPooling,f)
 (supportsStoredFunctionsUsingCallSyntax,f)
 (supportsStoredProcedures,f)
 (supportsSubqueriesInComparisons,t)
 (supportsSubqueriesInExists,t)
 (supportsSubqueriesInIns,t)
 (supportsSubqueriesInQuantifieds,t)
 (supportsTableCorrelationNames,t)
 (supportsTransactions,t)
 (supportsUnion,t)
 (supportsUnionAll,t)
 (usesLocalFilePerTable,f)
 (usesLocalFiles,f)
(84 rows)

                    getmetadatabooleans                    
-----------------------------------------------------------
 (allProceduresAreCallable,t)
 (allTablesAreSelectable,t)
 (autoCommitFailureClosesAllResultSets,f)
 (dataDefinitionCausesTransactionCommit,f)
 (dataDefinitionIgnoredInTransactions,f)
 (doesMaxRowSizeIncludeBlobs,f)
 (isCatalogAtStart,t)
 (isReadOnly,f)
 (locatorsUpdateCopy,t)
 (nullPlusNonNullIsNull,t)
 (nullsAreSortedAtEnd,f)
 (nullsAreSortedAtStart,f)
 (nullsAreSortedHigh,t)
 (nullsAreSortedLow,f)
 (storesLowerCaseIdentifiers,t)
 (storesLowerCaseQuotedIdentifiers,f)
 (storesMixedCaseIdentifiers,f)
 (storesMixedCaseQuotedIdentifiers,f)
 (storesUpperCaseIdentifiers,f)
 (storesUpperCaseQuotedIdentifiers,f)
 (supportsANSI92EntryLevelSQL,t)
 (supportsANSI92FullSQL,f)
 (supportsANSI92IntermediateSQL,f)
 (supportsAlterTableWithAddColumn,t)
 (supportsAlterTableWithDropColumn,t)
 (supportsBatchUpdates,t)
 (supportsCatalogsInDataManipulation,f)
 (supportsCatalogsInIndexDefinitions,f)
 (supportsCatalogsInPrivilegeDefinitions,f)
 (supportsCatalogsInProcedureCalls,f)
 (supportsCatalogsInTableDefinitions,f)
 (supportsColumnAliasing,t)
 (supportsConvert,f)
 (supportsCoreSQLGrammar,f)
 (supportsCorrelatedSubqueries,t)
 (supportsDataDefinitionAndDataManipulationTransactions,t)
 (supportsDataManipulationTransactionsOnly,f)
 (supportsDifferentTableCorrelationNames,f)
 (supportsExpressionsInOrderBy,t)
 (supportsExtendedSQLGrammar,f)
 (supportsFullOuterJoins,t)
 (supportsGetGeneratedKeys,f)
 (supportsGroupBy,t)
 (supportsGroupByBeyondSelect,t)
 (supportsGroupByUnrelated,t)
 (supportsIntegrityEnhancementFacility,t)
 (supportsLikeEscapeClause,t)
 (supportsLimitedOuterJoins,t)
 (supportsMinimumSQLGrammar,t)
 (supportsMixedCaseIdentifiers,f)
 (supportsMixedCaseQuotedIdentifiers,t)
 (supportsMultipleOpenResults,f)
 (supportsMultipleResultSets,f)
 (supportsMultipleTransactions,t)
 (supportsNamedParameters,f)
 (supportsNonNullableColumns,t)
 (supportsOpenCursorsAcrossCommit,f)
 (supportsOpenCursorsAcrossRollback,f)
 (supportsOpenStatementsAcrossCommit,t)
 (supportsOpenStatementsAcrossRollback,t)
 (supportsOrderByUnrelated,t)
 (supportsOuterJoins,t)
 (supportsPositionedDelete,f)
 (supportsPositionedUpdate,f)
 (supportsSavepoints,t)
 (supportsSchemasInDataManipulation,t)
 (supportsSchemasInIndexDefinitions,t)
 (supportsSchemasInPrivilegeDefinitions,t)
 (supportsSchemasInProcedureCalls,t)
 (supportsSchemasInTableDefinitions,t)
 (supportsSelectForUpdate,t)
 (supportsStatementPooling,f)
 (supportsStoredFunctionsUsingCallSyntax,f)
 (supportsStoredProcedures,f)
 (supportsSubqueriesInComparisons,t)
 (supportsSubqueriesInExists,t)
 (supportsSubqueriesInIns,t)
 (supportsSubqueriesInQuantifieds,t)
 (supportsTableCorrelationNames,t)
 (supportsTransactions,t)
 (supportsUnion,t)
 (supportsUnionAll,t)
 (usesLocalFilePerTable,f)
 (usesLocalFiles,f)
(84 rows)

                      method_name                      | result 
-------------------------------------------------------+--------
 allProceduresAreCallable                              | t
 allTablesAreSelectable                                | t
 autoCommitFailureClosesAllResultSets                  | f
 dataDefinitionCausesTransactionCommit                 | f
 dataDefinitionIgnoredInTransactions                   | f
 doesMaxRowSizeIncludeBlobs                            | f
 isCatalogAtStart                                      | t
 isReadOnly                                            | f
 locatorsUpdateCopy                                    | t
 nullPlusNonNullIsNull                                 | t
 nullsAreSortedAtEnd                                   | f
 nullsAreSortedAtStart                                 | f
 nullsAreSortedHigh                                    | t
 nullsAreSortedLow                                     | f
 storesLowerCaseIdentifiers                            | t
 storesLowerCaseQuotedIdentifiers                      | f
 storesMixedCaseIdentifiers                            | f
 storesMixedCaseQuotedIdentifiers                      | f
 storesUpperCaseIdentifiers                            | f
 storesUpperCaseQuotedIdentifiers                      | f
 supportsANSI92EntryLevelSQL                           | t
 supportsANSI92FullSQL                                 | f
 supportsANSI92IntermediateSQL                         | f
 supportsAlterTableWithAddColumn                       | t
 supportsAlterTableWithDropColumn                      | t
 supportsBatchUpdates                                  | t
 supportsCatalogsInDataManipulation                    | f
 supportsCatalogsInIndexDefinitions                    | f
 supportsCatalogsInPrivilegeDefinitions                | f
 supportsCatalogsInProcedureCalls                      | f
 supportsCatalogsInTableDefinitions                    | f
 supportsColumnAliasing                                | t
 supportsConvert                                       | f
 supportsCoreSQLGrammar                                | f
 supportsCorrelatedSubqueries                          | t
 supportsDataDefinitionAndDataManipulationTransactions | t
 supportsDataManipulationTransactionsOnly              | f
 supportsDifferentTableCorrelationNames                | f
 supportsExpressionsInOrderBy                          | t
 supportsExtendedSQLGrammar                            | f
 supportsFullOuterJoins                                | t
 supportsGetGeneratedKeys                              | f
 supportsGroupBy                                       | t
 supportsGroupByBeyondSelect                           | t
 supportsGroupByUnrelated                              | t
 supportsIntegrityEnhancementFacility                  | t
 supportsLikeEscapeClause                              | t
 supportsLimitedOuterJoins                             | t
 supportsMinimumSQLGrammar                             | t
 supportsMixedCaseIdentifiers                          | f
 supportsMixedCaseQuotedIdentifiers                    | t
 supportsMultipleOpenResults                           | f
 supportsMultipleResultSets                            | f
 supportsMultipleTransactions                          | t
 supportsNamedParameters                               | f
 supportsNonNullableColumns                            | t
 supportsOpenCursorsAcrossCommit                       | f
 supportsOpenCursorsAcrossRollback                     | f
 supportsOpenStatementsAcrossCommit                    | t
 supportsOpenStatementsAcrossRollback                  | t
 supportsOrderByUnrelated                              | t
 supportsOuterJoins                                    | t
 supportsPositionedDelete                              | f
 supportsPositionedUpdate                              | f
 supportsSavepoints                                    | t
 supportsSchemasInDataManipulation                     | t
 supportsSchemasInIndexDefinitions                     | t
 supportsSchemasInPrivilegeDefinitions                 | t
 supportsSchemasInProcedureCalls                       | t
 supportsSchemasInTableDefinitions                     | t
 supportsSelectForUpdate                               | t
 supportsStatementPooling                              | f
 supportsStoredFunctionsUsingCallSyntax                | f
 supportsStoredProcedures                              | f
 supportsSubqueriesInComparisons                       | t
 supportsSubqueriesInExists                            | t
 supportsSubqueriesInIns                               | t
 supportsSubqueriesInQuantifieds                       | t
 supportsTableCorrelationNames                         | t
 supportsTransactions                                  | t
 supportsUnion                                         | t
 supportsUnionAll                                      | t
 usesLocalFilePerTable                                 | f
 usesLocalFiles                                        | f
(84 rows)

CREATE TYPE
CREATE FUNCTION
psql:/path/examples.sql:472: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getCatalogSeparator => Success
psql:/path/examples.sql:472: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getCatalogTerm => Success
psql:/path/examples.sql:472: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getDatabaseProductName => Success
psql:/path/examples.sql:472: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getDatabaseProductVersion => Success
psql:/path/examples.sql:472: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getDriverName => Success
psql:/path/examples.sql:472: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getDriverVersion => Success
psql:/path/examples.sql:472: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getExtraNameCharacters => Success
psql:/path/examples.sql:472: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getIdentifierQuoteString => Success
psql:/path/examples.sql:472: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getNumericFunctions => Success
psql:/path/examples.sql:472: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getProcedureTerm => Success
psql:/path/examples.sql:472: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getSQLKeywords => Success
psql:/path/examples.sql:472: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getSchemaTerm => Success
psql:/path/examples.sql:472: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getSearchStringEscape => Success
psql:/path/examples.sql:472: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getStringFunctions => Success
psql:/path/examples.sql:472: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getSystemFunctions => Success
psql:/path/examples.sql:472: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getTimeDateFunctions => Success
psql:/path/examples.sql:472: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getURL => Success
psql:/path/examples.sql:472: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getUserName => Success
                                                                                                                                                                     getmetadatastrings                                                                                                                                                                     
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 (getCatalogSeparator,.)
 (getCatalogTerm,database)
 (getDatabaseProductName,PostgreSQL)
 (getDatabaseProductVersion,8.2.15)
 (getDriverName,"PostgreSQL pljava SPI Driver")
 (getDriverVersion,1.0)
 (getExtraNameCharacters,"")
 (getIdentifierQuoteString,"""")
 (getNumericFunctions,"abs,acos,asin,atan,atan2,ceiling,cos,cot,degrees,exp,floor,log,log10,mod,pi,power,radians,rand,round,sign,sin,sqrt,tan,truncate")
 (getProcedureTerm,function)
 (getSQLKeywords,"abort,acl,add,aggregate,append,archive,arch_store,backward,binary,boolean,change,cluster,copy,database,delimiter,delimiters,do,extend,explain,forward,heavy,index,inherits,isnull,light,listen,load,merge,nothing,notify,notnull,oids,purge,rename,replace,retrieve,returns,rule,recipe,setof,stdin,stdout,store,vacuum,verbose,version")
 (getSchemaTerm,schema)
 (getSearchStringEscape,"\\")
 (getStringFunctions,"ascii,char,concat,lcase,left,length,ltrim,repeat,rtrim,space,substring,ucase,replace")
 (getSystemFunctions,"database,ifnull,user")
 (getTimeDateFunctions,"curdate,curtime,dayname,dayofmonth,dayofweek,dayofyear,hour,minute,month,monthname,now,quarter,second,week,year")
 (getURL,jdbc:default:connection)
 (getUserName,@user@)
(18 rows)

psql:/path/examples.sql:473: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getCatalogSeparator => Success  (seg0 slice1 rh55-qavm61:40000 pid=11119)
psql:/path/examples.sql:473: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getCatalogTerm => Success  (seg0 slice1 rh55-qavm61:40000 pid=11119)
psql:/path/examples.sql:473: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getDatabaseProductName => Success  (seg0 slice1 rh55-qavm61:40000 pid=11119)
psql:/path/examples.sql:473: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getDatabaseProductVersion => Success  (seg0 slice1 rh55-qavm61:40000 pid=11119)
psql:/path/examples.sql:473: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getDriverName => Success  (seg0 slice1 rh55-qavm61:40000 pid=11119)
psql:/path/examples.sql:473: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getDriverVersion => Success  (seg0 slice1 rh55-qavm61:40000 pid=11119)
psql:/path/examples.sql:473: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getExtraNameCharacters => Success  (seg0 slice1 rh55-qavm61:40000 pid=11119)
psql:/path/examples.sql:473: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getIdentifierQuoteString => Success  (seg0 slice1 rh55-qavm61:40000 pid=11119)
psql:/path/examples.sql:473: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getNumericFunctions => Success  (seg0 slice1 rh55-qavm61:40000 pid=11119)
psql:/path/examples.sql:473: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getProcedureTerm => Success  (seg0 slice1 rh55-qavm61:40000 pid=11119)
psql:/path/examples.sql:473: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getSQLKeywords => Success  (seg0 slice1 rh55-qavm61:40000 pid=11119)
psql:/path/examples.sql:473: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getSchemaTerm => Success  (seg0 slice1 rh55-qavm61:40000 pid=11119)
psql:/path/examples.sql:473: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getSearchStringEscape => Success  (seg0 slice1 rh55-qavm61:40000 pid=11119)
psql:/path/examples.sql:473: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getStringFunctions => Success  (seg0 slice1 rh55-qavm61:40000 pid=11119)
psql:/path/examples.sql:473: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getSystemFunctions => Success  (seg0 slice1 rh55-qavm61:40000 pid=11119)
psql:/path/examples.sql:473: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getTimeDateFunctions => Success  (seg0 slice1 rh55-qavm61:40000 pid=11119)
psql:/path/examples.sql:473: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getURL => Success  (seg0 slice1 rh55-qavm61:40000 pid=11119)
psql:/path/examples.sql:473: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getUserName => Success  (seg0 slice1 rh55-qavm61:40000 pid=11119)
                                                                                                                                                                     getmetadatastrings                                                                                                                                                                     
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 (getCatalogSeparator,.)
 (getCatalogTerm,database)
 (getDatabaseProductName,PostgreSQL)
 (getDatabaseProductVersion,8.2.15)
 (getDriverName,"PostgreSQL pljava SPI Driver")
 (getDriverVersion,1.0)
 (getExtraNameCharacters,"")
 (getIdentifierQuoteString,"""")
 (getNumericFunctions,"abs,acos,asin,atan,atan2,ceiling,cos,cot,degrees,exp,floor,log,log10,mod,pi,power,radians,rand,round,sign,sin,sqrt,tan,truncate")
 (getProcedureTerm,function)
 (getSQLKeywords,"abort,acl,add,aggregate,append,archive,arch_store,backward,binary,boolean,change,cluster,copy,database,delimiter,delimiters,do,extend,explain,forward,heavy,index,inherits,isnull,light,listen,load,merge,nothing,notify,notnull,oids,purge,rename,replace,retrieve,returns,rule,recipe,setof,stdin,stdout,store,vacuum,verbose,version")
 (getSchemaTerm,schema)
 (getSearchStringEscape,"\\")
 (getStringFunctions,"ascii,char,concat,lcase,left,length,ltrim,repeat,rtrim,space,substring,ucase,replace")
 (getSystemFunctions,"database,ifnull,user")
 (getTimeDateFunctions,"curdate,curtime,dayname,dayofmonth,dayofweek,dayofyear,hour,minute,month,monthname,now,quarter,second,week,year")
 (getURL,jdbc:default:connection)
 (getUserName,@user@)
(18 rows)

psql:/path/examples.sql:474: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getCatalogSeparator => Success
psql:/path/examples.sql:474: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getCatalogTerm => Success
psql:/path/examples.sql:474: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getDatabaseProductName => Success
psql:/path/examples.sql:474: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getDatabaseProductVersion => Success
psql:/path/examples.sql:474: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getDriverName => Success
psql:/path/examples.sql:474: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getDriverVersion => Success
psql:/path/examples.sql:474: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getExtraNameCharacters => Success
psql:/path/examples.sql:474: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getIdentifierQuoteString => Success
psql:/path/examples.sql:474: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getNumericFunctions => Success
psql:/path/examples.sql:474: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getProcedureTerm => Success
psql:/path/examples.sql:474: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getSQLKeywords => Success
psql:/path/examples.sql:474: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getSchemaTerm => Success
psql:/path/examples.sql:474: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getSearchStringEscape => Success
psql:/path/examples.sql:474: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getStringFunctions => Success
psql:/path/examples.sql:474: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getSystemFunctions => Success
psql:/path/examples.sql:474: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getTimeDateFunctions => Success
psql:/path/examples.sql:474: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getURL => Success
psql:/path/examples.sql:474: INFO:  23 Sep 14 11:40:19 org.postgresql.example.MetaDataStrings Method: getUserName => Success
        method_name        |                                                                                                                                                                 result                                                                                                                                                                  
---------------------------+-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 getCatalogSeparator       | .
 getCatalogTerm            | database
 getDatabaseProductName    | PostgreSQL
 getDatabaseProductVersion | 8.2.15
 getDriverName             | PostgreSQL pljava SPI Driver
 getDriverVersion          | 1.0
 getExtraNameCharacters    | 
 getIdentifierQuoteString  | "
 getNumericFunctions       | abs,acos,asin,atan,atan2,ceiling,cos,cot,degrees,exp,floor,log,log10,mod,pi,power,radians,rand,round,sign,sin,sqrt,tan,truncate
 getProcedureTerm          | function
 getSQLKeywords            | abort,acl,add,aggregate,append,archive,arch_store,backward,binary,boolean,change,cluster,copy,database,delimiter,delimiters,do,extend,explain,forward,heavy,index,inherits,isnull,light,listen,load,merge,nothing,notify,notnull,oids,purge,rename,replace,retrieve,returns,rule,recipe,setof,stdin,stdout,store,vacuum,verbose,version
 getSchemaTerm             | schema
 getSearchStringEscape     | \
 getStringFunctions        | ascii,char,concat,lcase,left,length,ltrim,repeat,rtrim,space,substring,ucase,replace
 getSystemFunctions        | database,ifnull,user
 getTimeDateFunctions      | curdate,curtime,dayname,dayofmonth,dayofweek,dayofyear,hour,minute,month,monthname,now,quarter,second,week,year
 getURL                    | jdbc:default:connection
 getUserName               | @user@
(18 rows)

CREATE TYPE
CREATE FUNCTION
          getmetadataints           
------------------------------------
 (getDatabaseMajorVersion,8)
 (getDatabaseMinorVersion,2)
 (getDefaultTransactionIsolation,2)
 (getDriverMajorVersion,1)
 (getDriverMinorVersion,0)
 (getJDBCMajorVersion,3)
 (getJDBCMinorVersion,0)
 (getMaxBinaryLiteralLength,0)
 (getMaxCatalogNameLength,63)
 (getMaxCharLiteralLength,0)
 (getMaxColumnNameLength,63)
 (getMaxColumnsInGroupBy,0)
 (getMaxColumnsInIndex,32)
 (getMaxColumnsInOrderBy,0)
 (getMaxColumnsInSelect,0)
 (getMaxColumnsInTable,1600)
 (getMaxConnections,8192)
 (getMaxCursorNameLength,63)
 (getMaxIndexLength,0)
 (getMaxProcedureNameLength,63)
 (getMaxRowSize,1073741824)
 (getMaxSchemaNameLength,63)
 (getMaxStatementLength,0)
 (getMaxStatements,1)
 (getMaxTableNameLength,63)
 (getMaxTablesInSelect,0)
 (getMaxUserNameLength,63)
 (getResultSetHoldability,1)
 (getSQLStateType,2)
(29 rows)

          getmetadataints           
------------------------------------
 (getDatabaseMajorVersion,8)
 (getDatabaseMinorVersion,2)
 (getDefaultTransactionIsolation,2)
 (getDriverMajorVersion,1)
 (getDriverMinorVersion,0)
 (getJDBCMajorVersion,3)
 (getJDBCMinorVersion,0)
 (getMaxBinaryLiteralLength,0)
 (getMaxCatalogNameLength,63)
 (getMaxCharLiteralLength,0)
 (getMaxColumnNameLength,63)
 (getMaxColumnsInGroupBy,0)
 (getMaxColumnsInIndex,32)
 (getMaxColumnsInOrderBy,0)
 (getMaxColumnsInSelect,0)
 (getMaxColumnsInTable,1600)
 (getMaxConnections,8192)
 (getMaxCursorNameLength,63)
 (getMaxIndexLength,0)
 (getMaxProcedureNameLength,63)
 (getMaxRowSize,1073741824)
 (getMaxSchemaNameLength,63)
 (getMaxStatementLength,0)
 (getMaxStatements,1)
 (getMaxTableNameLength,63)
 (getMaxTablesInSelect,0)
 (getMaxUserNameLength,63)
 (getResultSetHoldability,1)
 (getSQLStateType,2)
(29 rows)

          method_name           |   result   
--------------------------------+------------
 getDatabaseMajorVersion        |          8
 getDatabaseMinorVersion        |          2
 getDefaultTransactionIsolation |          2
 getDriverMajorVersion          |          1
 getDriverMinorVersion          |          0
 getJDBCMajorVersion            |          3
 getJDBCMinorVersion            |          0
 getMaxBinaryLiteralLength      |          0
 getMaxCatalogNameLength        |         63
 getMaxCharLiteralLength        |          0
 getMaxColumnNameLength         |         63
 getMaxColumnsInGroupBy         |          0
 getMaxColumnsInIndex           |         32
 getMaxColumnsInOrderBy         |          0
 getMaxColumnsInSelect          |          0
 getMaxColumnsInTable           |       1600
 getMaxConnections              |       8192
 getMaxCursorNameLength         |         63
 getMaxIndexLength              |          0
 getMaxProcedureNameLength      |         63
 getMaxRowSize                  | 1073741824
 getMaxSchemaNameLength         |         63
 getMaxStatementLength          |          0
 getMaxStatements               |          1
 getMaxTableNameLength          |         63
 getMaxTablesInSelect           |          0
 getMaxUserNameLength           |         63
 getResultSetHoldability        |          1
 getSQLStateType                |          2
(29 rows)

CREATE FUNCTION
                                                               callmetadatamethod                                                               
------------------------------------------------------------------------------------------------------------------------------------------------
 TABLE_CAT(java.lang.String);TABLE_SCHEM(java.lang.String);TABLE_NAME(java.lang.String);TABLE_TYPE(java.lang.String);REMARKS(java.lang.String);
 <NULL>;javatest;employees1;TABLE;<NULL>;
 <NULL>;javatest;employees2;TABLE;<NULL>;
 <NULL>;javatest;test;TABLE;<NULL>;
(4 rows)

CREATE FUNCTION
     executeselect     
-----------------------
 I(java.lang.Integer);
 1;
(2 rows)

psql:/path/sql_file:1: ERROR:  function cannot execute on segment because it accesses relation "javatest.test" (functions.c:155)  (seg0 slice1 rh55-qavm61:40000 pid=11119) (cdbdisp.c:1526)
DETAIL:  SQL statement "select * from javatest.test"
                           executeselect                            
--------------------------------------------------------------------
 OID(org.postgresql.pljava.internal.Oid);RELNAME(java.lang.String);
 OID(1259);pg_class;
(2 rows)

                           executeselect                            
--------------------------------------------------------------------
 OID(org.postgresql.pljava.internal.Oid);RELNAME(java.lang.String);
 OID(1259);pg_class;
(2 rows)

CREATE FUNCTION
DROP FUNCTION
CREATE FUNCTION
CREATE FUNCTION
 i 
---
 1
(1 row)

CREATE FUNCTION
CREATE FUNCTION
 countnulls 
------------
          2
(1 row)

 countnulls 
------------
          2
(1 row)

 countnulls 
------------
          2
(1 row)

CREATE TYPE
psql:/path/sql_file:1: NOTICE:  return type javatest.complex is only a shell
CREATE FUNCTION
psql:/path/sql_file:1: NOTICE:  argument type javatest.complex is only a shell
CREATE FUNCTION
psql:/path/sql_file:1: NOTICE:  return type javatest.complex is only a shell
CREATE FUNCTION
psql:/path/sql_file:1: NOTICE:  argument type javatest.complex is only a shell
CREATE FUNCTION
CREATE TYPE
psql:/path/sql_file:1: NOTICE:  Table doesn't have 'distributed by' clause, and no column type is suitable for a distribution key. Creating a NULL policy entry.
CREATE TABLE
psql:/path/examples.sql:610: INFO:  23 Sep 14 11:40:19 org.postgresql.example.ComplexScalar javatest.complex from string
psql:/path/examples.sql:610: INFO:  23 Sep 14 11:40:19 org.postgresql.example.ComplexScalar javatest.complex to SQLOutput
INSERT 0 1
psql:/path/examples.sql:611: INFO:  23 Sep 14 11:40:20 org.postgresql.example.ComplexScalar javatest.complex from SQLInput
psql:/path/examples.sql:611: INFO:  23 Sep 14 11:40:20 org.postgresql.example.ComplexScalar javatest.complex toString
     x     
-----------
 (1.0,2.0)
(1 row)

CREATE FUNCTION
CREATE FUNCTION
psql:/path/examples.sql:640: INFO:  23 Sep 14 11:40:20 org.postgresql.example.AnyTest logAnyElement received an object of class class java.lang.Short
 loganyelement 
---------------
             1
(1 row)

psql:/path/examples.sql:641: INFO:  23 Sep 14 11:40:20 org.postgresql.example.AnyTest logAnyElement received an object of class class java.lang.Integer
 loganyelement 
---------------
             1
(1 row)

psql:/path/examples.sql:642: INFO:  23 Sep 14 11:40:20 org.postgresql.example.AnyTest logAnyElement received an object of class class java.lang.Long
 loganyelement 
---------------
             1
(1 row)

psql:/path/examples.sql:643: INFO:  23 Sep 14 11:40:20 org.postgresql.example.AnyTest logAnyElement received an object of class class java.lang.Float
 loganyelement 
---------------
             1
(1 row)

psql:/path/examples.sql:644: INFO:  23 Sep 14 11:40:20 org.postgresql.example.AnyTest logAnyElement received an object of class class java.lang.Double
 loganyelement 
---------------
             1
(1 row)

psql:/path/examples.sql:645: INFO:  23 Sep 14 11:40:20 org.postgresql.example.AnyTest logAnyElement received an object of class class java.math.BigDecimal
 loganyelement 
---------------
             1
(1 row)

psql:/path/examples.sql:646: INFO:  23 Sep 14 11:40:20 org.postgresql.example.AnyTest logAnyElement received an object of class class java.lang.String
 loganyelement 
---------------
 1
(1 row)

psql:/path/examples.sql:647: INFO:  23 Sep 14 11:40:20 org.postgresql.example.AnyTest logAnyElement received an object of class class [B
 loganyelement 
---------------
 1
(1 row)

psql:/path/examples.sql:648: INFO:  23 Sep 14 11:40:20 org.postgresql.example.AnyTest logAnyElement received an object of class class java.lang.String
 loganyelement 
---------------
 (0,0)
(1 row)

psql:/path/examples.sql:649: INFO:  23 Sep 14 11:40:20 org.postgresql.example.AnyTest logAnyElement received an object of class class java.lang.Integer  (seg0 slice1 rh55-qavm61:40000 pid=11119)
 loganyelement 
---------------
             1
(1 row)

psql:/path/examples.sql:650: INFO:  23 Sep 14 11:40:20 org.postgresql.example.AnyTest logAnyElement received an object of class class java.lang.Integer
 loganyelement 
---------------
             1
(1 row)

CREATE FUNCTION
psql:/path/examples.sql:658: INFO:  23 Sep 14 11:40:20 org.postgresql.example.AnyTest logAny received an object of class class java.lang.Integer
 logany 
--------
 
(1 row)

psql:/path/examples.sql:659: INFO:  23 Sep 14 11:40:20 org.postgresql.example.AnyTest logAny received an object of class class java.lang.Integer  (seg0 slice1 rh55-qavm61:40000 pid=11119)
 logany 
--------
 
(1 row)

psql:/path/examples.sql:660: INFO:  23 Sep 14 11:40:20 org.postgresql.example.AnyTest logAny received an object of class class java.lang.Integer
 logany 
--------
 
(1 row)

CREATE FUNCTION
 makearray 
-----------
 {1}
(1 row)

 makearray 
-----------
 {1}
(1 row)

 makearray 
-----------
 {1}
(1 row)

