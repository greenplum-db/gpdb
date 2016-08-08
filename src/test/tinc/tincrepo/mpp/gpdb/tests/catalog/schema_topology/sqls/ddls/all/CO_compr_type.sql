-- 
-- @created 2009-01-27 14:00:00
-- @modified 2013-06-24 17:00:00
-- @tags ddl schema_topology

DROP type if exists int_type1 cascade ; 

CREATE type int_type1;
CREATE FUNCTION int_type1_in(cstring) 
 RETURNS int_type1
 AS 'int4in' 
 LANGUAGE internal IMMUTABLE STRICT; 

CREATE FUNCTION int_type1_out(int_type1) 
 RETURNS cstring
 AS 'int4out' 
 LANGUAGE internal IMMUTABLE STRICT; 

CREATE TYPE int_type1( 
 input = int_type1_in ,
 output = int_type1_out ,
 internallength = 4, 
 default =55, 
 passedbyvalue, 
 compresstype=zlib,
 blocksize=32768,
 compresslevel=3);

--Drop and recreate the data type 

 Drop type if exists int_type1 cascade;

CREATE FUNCTION int_type1_in(cstring) 
 RETURNS int_type1
 AS 'int4in' 
 LANGUAGE internal IMMUTABLE STRICT; 


CREATE FUNCTION int_type1_out(int_type1) 
 RETURNS cstring
 AS 'int4out' 
 LANGUAGE internal IMMUTABLE STRICT; 


CREATE TYPE int_type1( 
 input = int_type1_in ,
 output = int_type1_out ,
 internallength = 4, 
 default =55, 
 passedbyvalue, 
 compresstype=zlib,
 blocksize=32768,
 compresslevel=2);


select typoptions from pg_type_encoding where typid='int_type1 '::regtype;

DROP type if exists char_type1 cascade ; 

CREATE type char_type1;
CREATE FUNCTION char_type1_in(cstring) 
 RETURNS char_type1
 AS 'charin' 
 LANGUAGE internal IMMUTABLE STRICT; 

CREATE FUNCTION char_type1_out(char_type1) 
 RETURNS cstring
 AS 'charout' 
 LANGUAGE internal IMMUTABLE STRICT; 

CREATE TYPE char_type1( 
 input = char_type1_in ,
 output = char_type1_out ,
 internallength = 4, 
 default = 'asd' , 
 passedbyvalue, 
 compresstype=quicklz,
 blocksize=32768,
 compresslevel=1);

--Drop and recreate the data type 

 Drop type if exists char_type1 cascade;

CREATE FUNCTION char_type1_in(cstring) 
 RETURNS char_type1
 AS 'charin' 
 LANGUAGE internal IMMUTABLE STRICT; 


CREATE FUNCTION char_type1_out(char_type1) 
 RETURNS cstring
 AS 'charout' 
 LANGUAGE internal IMMUTABLE STRICT; 


CREATE TYPE char_type1( 
 input = char_type1_in ,
 output = char_type1_out ,
 internallength = 4, 
 default = 'asd' , 
 passedbyvalue, 
 compresstype=zlib,
 blocksize=32768,
 compresslevel=2);


select typoptions from pg_type_encoding where typid='char_type1 '::regtype;

DROP type if exists text_type1 cascade ; 

CREATE type text_type1;
CREATE FUNCTION text_type1_in(cstring) 
 RETURNS text_type1
 AS 'textin' 
 LANGUAGE internal IMMUTABLE STRICT; 

CREATE FUNCTION text_type1_out(text_type1) 
 RETURNS cstring
 AS 'textout' 
 LANGUAGE internal IMMUTABLE STRICT; 

CREATE TYPE text_type1( 
 input = text_type1_in ,
 output = text_type1_out ,
 internallength = variable, 
 default = 'hfkdshfkjsdhflkshadfkhsadflkh' , 
 compresstype=zlib,
 blocksize=32768,
 compresslevel=2);

--Drop and recreate the data type 

 Drop type if exists text_type1 cascade;

CREATE FUNCTION text_type1_in(cstring) 
 RETURNS text_type1
 AS 'textin' 
 LANGUAGE internal IMMUTABLE STRICT; 


CREATE FUNCTION text_type1_out(text_type1) 
 RETURNS cstring
 AS 'textout' 
 LANGUAGE internal IMMUTABLE STRICT; 


CREATE TYPE text_type1( 
 input = text_type1_in ,
 output = text_type1_out ,
 internallength = variable, 
 default = 'hfkdshfkjsdhflkshadfkhsadflkh' , 
 compresstype=zlib,
 blocksize=32768,
 compresslevel=7);


select typoptions from pg_type_encoding where typid='text_type1 '::regtype;

DROP type if exists varchar_type1 cascade ; 

CREATE type varchar_type1;
CREATE FUNCTION varchar_type1_in(cstring) 
 RETURNS varchar_type1
 AS 'varcharin' 
 LANGUAGE internal IMMUTABLE STRICT; 

CREATE FUNCTION varchar_type1_out(varchar_type1) 
 RETURNS cstring
 AS 'varcharout' 
 LANGUAGE internal IMMUTABLE STRICT; 

CREATE TYPE varchar_type1( 
 input = varchar_type1_in ,
 output = varchar_type1_out ,
 internallength = variable, 
 default = 'ajhgdjagdjasdkjashk' , 
 compresstype=zlib,
 blocksize=32768,
 compresslevel=1);

--Drop and recreate the data type 

 Drop type if exists varchar_type1 cascade;

CREATE FUNCTION varchar_type1_in(cstring) 
 RETURNS varchar_type1
 AS 'varcharin' 
 LANGUAGE internal IMMUTABLE STRICT; 


CREATE FUNCTION varchar_type1_out(varchar_type1) 
 RETURNS cstring
 AS 'varcharout' 
 LANGUAGE internal IMMUTABLE STRICT; 


CREATE TYPE varchar_type1( 
 input = varchar_type1_in ,
 output = varchar_type1_out ,
 internallength = variable, 
 default = 'ajhgdjagdjasdkjashk' , 
 compresstype=zlib,
 blocksize=32768,
 compresslevel=9);


select typoptions from pg_type_encoding where typid='varchar_type1 '::regtype;

DROP type if exists date_type1 cascade ; 

CREATE type date_type1;
CREATE FUNCTION date_type1_in(cstring) 
 RETURNS date_type1
 AS 'date_in' 
 LANGUAGE internal IMMUTABLE STRICT; 

CREATE FUNCTION date_type1_out(date_type1) 
 RETURNS cstring
 AS 'date_out' 
 LANGUAGE internal IMMUTABLE STRICT; 

CREATE TYPE date_type1( 
 input = date_type1_in ,
 output = date_type1_out ,
 internallength = 4, 
 default = '2001-12-11' , 
 passedbyvalue, 
 compresstype=rle_type,
 blocksize=32768,
 compresslevel=1);

--Drop and recreate the data type 

 Drop type if exists date_type1 cascade;

CREATE FUNCTION date_type1_in(cstring) 
 RETURNS date_type1
 AS 'date_in' 
 LANGUAGE internal IMMUTABLE STRICT; 


CREATE FUNCTION date_type1_out(date_type1) 
 RETURNS cstring
 AS 'date_out' 
 LANGUAGE internal IMMUTABLE STRICT; 


CREATE TYPE date_type1( 
 input = date_type1_in ,
 output = date_type1_out ,
 internallength = 4, 
 default = '2001-12-11' , 
 passedbyvalue, 
 compresstype=zlib,
 blocksize=32768,
 compresslevel=2);


select typoptions from pg_type_encoding where typid='date_type1 '::regtype;

DROP type if exists timestamp_type1 cascade ; 

CREATE type timestamp_type1;
CREATE FUNCTION timestamp_type1_in(cstring) 
 RETURNS timestamp_type1
 AS 'timestamp_in' 
 LANGUAGE internal IMMUTABLE STRICT; 

CREATE FUNCTION timestamp_type1_out(timestamp_type1) 
 RETURNS cstring
 AS 'timestamp_out' 
 LANGUAGE internal IMMUTABLE STRICT; 

CREATE TYPE timestamp_type1( 
 input = timestamp_type1_in ,
 output = timestamp_type1_out ,
 internallength = 4, 
 default = '2001-12-24 02:26:11' , 
 passedbyvalue, 
 compresstype=rle_type,
 blocksize=32768,
 compresslevel=2);

--Drop and recreate the data type 

 Drop type if exists timestamp_type1 cascade;

CREATE FUNCTION timestamp_type1_in(cstring) 
 RETURNS timestamp_type1
 AS 'timestamp_in' 
 LANGUAGE internal IMMUTABLE STRICT; 


CREATE FUNCTION timestamp_type1_out(timestamp_type1) 
 RETURNS cstring
 AS 'timestamp_out' 
 LANGUAGE internal IMMUTABLE STRICT; 


CREATE TYPE timestamp_type1( 
 input = timestamp_type1_in ,
 output = timestamp_type1_out ,
 internallength = 4, 
 default = '2001-12-24 02:26:11' , 
 passedbyvalue, 
 compresstype=rle_type,
 blocksize=32768,
 compresslevel=4);


select typoptions from pg_type_encoding where typid='timestamp_type1 '::regtype;

DROP table if exists co_compr_type_tb; 
-- Create table 
CREATE TABLE co_compr_type_tb
	 (id serial,  a1 int_type1, a2 char_type1, a3 text_type1, a4 date_type1, a5 varchar_type1, a6 timestamp_type1)  WITH (appendonly=true, orientation=column) distributed randomly;


\d+ co_compr_type_tb

INSERT into co_compr_type_tb DEFAULT VALUES ; 
Select * from co_compr_type_tb;

Insert into co_compr_type_tb select * from co_compr_type_tb; 
Insert into co_compr_type_tb select * from co_compr_type_tb; 
Insert into co_compr_type_tb select * from co_compr_type_tb; 
Insert into co_compr_type_tb select * from co_compr_type_tb; 
Insert into co_compr_type_tb select * from co_compr_type_tb; 
Insert into co_compr_type_tb select * from co_compr_type_tb; 

Select * from co_compr_type_tb;

--Alter table drop a column 
Alter table co_compr_type_tb Drop column a2; 
Insert into co_compr_type_tb(a1,a3,a4,a5,a6)  select a1,a3,a4,a5,a6 from co_compr_type_tb ;
Select count(*) from co_compr_type_tb; 

--Alter table rename a column 
Alter table co_compr_type_tb Rename column a3 TO after_rename_a3; 
--Insert data to the table, select count(*)
Insert into co_compr_type_tb(a1,after_rename_a3,a4,a5,a6)  select a1,after_rename_a3,a4,a5,a6 from co_compr_type_tb ;
Select count(*) from co_compr_type_tb; 

Alter type int_type1 set default encoding (compresstype=rle_type,compresslevel=3);

\d+ co_compr_type_tb

Insert into co_compr_type_tb(a1,after_rename_a3,a4,a5,a6)  select a1,after_rename_a3,a4,a5,a6 from co_compr_type_tb ;
Select count(*) from co_compr_type_tb; 

