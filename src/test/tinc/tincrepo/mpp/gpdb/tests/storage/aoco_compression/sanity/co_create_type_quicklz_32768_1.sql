-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
DROP type if exists int_quicklz cascade ; 

CREATE type int_quicklz;
CREATE FUNCTION int_quicklz_in(cstring) 
 RETURNS int_quicklz
 AS 'int4in' 
 LANGUAGE internal IMMUTABLE STRICT; 

CREATE FUNCTION int_quicklz_out(int_quicklz) 
 RETURNS cstring
 AS 'int4out' 
 LANGUAGE internal IMMUTABLE STRICT; 

CREATE TYPE int_quicklz( 
 input = int_quicklz_in ,
 output = int_quicklz_out ,
 internallength = 4, 
 default =55, 
 passedbyvalue, 
 compresstype=quicklz,
 blocksize=32768,
 compresslevel=1);

--Drop and recreate the data type 

 Drop type if exists int_quicklz cascade;

CREATE FUNCTION int_quicklz_in(cstring) 
 RETURNS int_quicklz
 AS 'int4in' 
 LANGUAGE internal IMMUTABLE STRICT; 


CREATE FUNCTION int_quicklz_out(int_quicklz) 
 RETURNS cstring
 AS 'int4out' 
 LANGUAGE internal IMMUTABLE STRICT; 


CREATE TYPE int_quicklz( 
 input = int_quicklz_in ,
 output = int_quicklz_out ,
 internallength = 4, 
 default =55, 
 passedbyvalue, 
 compresstype=quicklz,
 blocksize=32768,
 compresslevel=1);


select typoptions from pg_type_encoding where typid='int_quicklz '::regtype;

DROP type if exists char_quicklz cascade ; 

CREATE type char_quicklz;
CREATE FUNCTION char_quicklz_in(cstring) 
 RETURNS char_quicklz
 AS 'charin' 
 LANGUAGE internal IMMUTABLE STRICT; 

CREATE FUNCTION char_quicklz_out(char_quicklz) 
 RETURNS cstring
 AS 'charout' 
 LANGUAGE internal IMMUTABLE STRICT; 

CREATE TYPE char_quicklz( 
 input = char_quicklz_in ,
 output = char_quicklz_out ,
 internallength = 4, 
 default = 'asd' , 
 passedbyvalue, 
 compresstype=quicklz,
 blocksize=32768,
 compresslevel=1);

--Drop and recreate the data type 

 Drop type if exists char_quicklz cascade;

CREATE FUNCTION char_quicklz_in(cstring) 
 RETURNS char_quicklz
 AS 'charin' 
 LANGUAGE internal IMMUTABLE STRICT; 


CREATE FUNCTION char_quicklz_out(char_quicklz) 
 RETURNS cstring
 AS 'charout' 
 LANGUAGE internal IMMUTABLE STRICT; 


CREATE TYPE char_quicklz( 
 input = char_quicklz_in ,
 output = char_quicklz_out ,
 internallength = 4, 
 default = 'asd' , 
 passedbyvalue, 
 compresstype=quicklz,
 blocksize=32768,
 compresslevel=1);


select typoptions from pg_type_encoding where typid='char_quicklz '::regtype;

DROP type if exists text_quicklz cascade ; 

CREATE type text_quicklz;
CREATE FUNCTION text_quicklz_in(cstring) 
 RETURNS text_quicklz
 AS 'textin' 
 LANGUAGE internal IMMUTABLE STRICT; 

CREATE FUNCTION text_quicklz_out(text_quicklz) 
 RETURNS cstring
 AS 'textout' 
 LANGUAGE internal IMMUTABLE STRICT; 

CREATE TYPE text_quicklz( 
 input = text_quicklz_in ,
 output = text_quicklz_out ,
 internallength = variable, 
 default = 'hfkdshfkjsdhflkshadfkhsadflkh' , 
 compresstype=quicklz,
 blocksize=32768,
 compresslevel=1);

--Drop and recreate the data type 

 Drop type if exists text_quicklz cascade;

CREATE FUNCTION text_quicklz_in(cstring) 
 RETURNS text_quicklz
 AS 'textin' 
 LANGUAGE internal IMMUTABLE STRICT; 


CREATE FUNCTION text_quicklz_out(text_quicklz) 
 RETURNS cstring
 AS 'textout' 
 LANGUAGE internal IMMUTABLE STRICT; 


CREATE TYPE text_quicklz( 
 input = text_quicklz_in ,
 output = text_quicklz_out ,
 internallength = variable, 
 default = 'hfkdshfkjsdhflkshadfkhsadflkh' , 
 compresstype=quicklz,
 blocksize=32768,
 compresslevel=1);


select typoptions from pg_type_encoding where typid='text_quicklz '::regtype;

DROP type if exists varchar_quicklz cascade ; 

CREATE type varchar_quicklz;
CREATE FUNCTION varchar_quicklz_in(cstring) 
 RETURNS varchar_quicklz
 AS 'varcharin' 
 LANGUAGE internal IMMUTABLE STRICT; 

CREATE FUNCTION varchar_quicklz_out(varchar_quicklz) 
 RETURNS cstring
 AS 'varcharout' 
 LANGUAGE internal IMMUTABLE STRICT; 

CREATE TYPE varchar_quicklz( 
 input = varchar_quicklz_in ,
 output = varchar_quicklz_out ,
 internallength = variable, 
 default = 'ajhgdjagdjasdkjashk' , 
 compresstype=quicklz,
 blocksize=32768,
 compresslevel=1);

--Drop and recreate the data type 

 Drop type if exists varchar_quicklz cascade;

CREATE FUNCTION varchar_quicklz_in(cstring) 
 RETURNS varchar_quicklz
 AS 'varcharin' 
 LANGUAGE internal IMMUTABLE STRICT; 


CREATE FUNCTION varchar_quicklz_out(varchar_quicklz) 
 RETURNS cstring
 AS 'varcharout' 
 LANGUAGE internal IMMUTABLE STRICT; 


CREATE TYPE varchar_quicklz( 
 input = varchar_quicklz_in ,
 output = varchar_quicklz_out ,
 internallength = variable, 
 default = 'ajhgdjagdjasdkjashk' , 
 compresstype=quicklz,
 blocksize=32768,
 compresslevel=1);


select typoptions from pg_type_encoding where typid='varchar_quicklz '::regtype;

DROP type if exists date_quicklz cascade ; 

CREATE type date_quicklz;
CREATE FUNCTION date_quicklz_in(cstring) 
 RETURNS date_quicklz
 AS 'date_in' 
 LANGUAGE internal IMMUTABLE STRICT; 

CREATE FUNCTION date_quicklz_out(date_quicklz) 
 RETURNS cstring
 AS 'date_out' 
 LANGUAGE internal IMMUTABLE STRICT; 

CREATE TYPE date_quicklz( 
 input = date_quicklz_in ,
 output = date_quicklz_out ,
 internallength = 4, 
 default = '2001-12-11' , 
 passedbyvalue, 
 compresstype=quicklz,
 blocksize=32768,
 compresslevel=1);

--Drop and recreate the data type 

 Drop type if exists date_quicklz cascade;

CREATE FUNCTION date_quicklz_in(cstring) 
 RETURNS date_quicklz
 AS 'date_in' 
 LANGUAGE internal IMMUTABLE STRICT; 


CREATE FUNCTION date_quicklz_out(date_quicklz) 
 RETURNS cstring
 AS 'date_out' 
 LANGUAGE internal IMMUTABLE STRICT; 


CREATE TYPE date_quicklz( 
 input = date_quicklz_in ,
 output = date_quicklz_out ,
 internallength = 4, 
 default = '2001-12-11' , 
 passedbyvalue, 
 compresstype=quicklz,
 blocksize=32768,
 compresslevel=1);


select typoptions from pg_type_encoding where typid='date_quicklz '::regtype;

DROP type if exists timestamp_quicklz cascade ; 

CREATE type timestamp_quicklz;
CREATE FUNCTION timestamp_quicklz_in(cstring) 
 RETURNS timestamp_quicklz
 AS 'timestamp_in' 
 LANGUAGE internal IMMUTABLE STRICT; 

CREATE FUNCTION timestamp_quicklz_out(timestamp_quicklz) 
 RETURNS cstring
 AS 'timestamp_out' 
 LANGUAGE internal IMMUTABLE STRICT; 

CREATE TYPE timestamp_quicklz( 
 input = timestamp_quicklz_in ,
 output = timestamp_quicklz_out ,
 internallength = 4, 
 default = '2001-12-24 02:26:11' , 
 passedbyvalue, 
 compresstype=quicklz,
 blocksize=32768,
 compresslevel=1);

--Drop and recreate the data type 

 Drop type if exists timestamp_quicklz cascade;

CREATE FUNCTION timestamp_quicklz_in(cstring) 
 RETURNS timestamp_quicklz
 AS 'timestamp_in' 
 LANGUAGE internal IMMUTABLE STRICT; 


CREATE FUNCTION timestamp_quicklz_out(timestamp_quicklz) 
 RETURNS cstring
 AS 'timestamp_out' 
 LANGUAGE internal IMMUTABLE STRICT; 


CREATE TYPE timestamp_quicklz( 
 input = timestamp_quicklz_in ,
 output = timestamp_quicklz_out ,
 internallength = 4, 
 default = '2001-12-24 02:26:11' , 
 passedbyvalue, 
 compresstype=quicklz,
 blocksize=32768,
 compresslevel=1);


select typoptions from pg_type_encoding where typid='timestamp_quicklz '::regtype;

DROP table if exists co_create_type_quicklz_32768_1; 
-- Create table 
CREATE TABLE co_create_type_quicklz_32768_1
	 (id serial,  a1 int_quicklz, a2 char_quicklz, a3 text_quicklz, a4 date_quicklz, a5 varchar_quicklz, a6 timestamp_quicklz ) WITH (appendonly=true, orientation=column) distributed randomly;


\d+ co_create_type_quicklz_32768_1

INSERT into co_create_type_quicklz_32768_1 DEFAULT VALUES ; 
Select * from co_create_type_quicklz_32768_1;

Insert into co_create_type_quicklz_32768_1 select * from co_create_type_quicklz_32768_1; 
Insert into co_create_type_quicklz_32768_1 select * from co_create_type_quicklz_32768_1; 
Insert into co_create_type_quicklz_32768_1 select * from co_create_type_quicklz_32768_1; 
Insert into co_create_type_quicklz_32768_1 select * from co_create_type_quicklz_32768_1; 
Insert into co_create_type_quicklz_32768_1 select * from co_create_type_quicklz_32768_1; 
Insert into co_create_type_quicklz_32768_1 select * from co_create_type_quicklz_32768_1; 

Select * from co_create_type_quicklz_32768_1;

--Alter table drop a column 
Alter table co_create_type_quicklz_32768_1 Drop column a2; 
Insert into co_create_type_quicklz_32768_1(a1,a3,a4,a5,a6)  select a1,a3,a4,a5,a6 from co_create_type_quicklz_32768_1 ;
Select count(*) from co_create_type_quicklz_32768_1; 

--Alter table rename a column 
Alter table co_create_type_quicklz_32768_1 Rename column a3 TO after_rename_a3; 
--Insert data to the table, select count(*)
Insert into co_create_type_quicklz_32768_1(a1,after_rename_a3,a4,a5,a6)  select a1,after_rename_a3,a4,a5,a6 from co_create_type_quicklz_32768_1 ;
Select count(*) from co_create_type_quicklz_32768_1; 

Alter type int_quicklz set default encoding (compresstype=zlib,compresslevel=1);

--Add a column 
  Alter table co_create_type_quicklz_32768_1 Add column new_cl int_quicklz default '5'; 

\d+ co_create_type_quicklz_32768_1

Insert into co_create_type_quicklz_32768_1(a1,after_rename_a3,a4,a5,a6)  select a1,after_rename_a3,a4,a5,a6 from co_create_type_quicklz_32768_1 ;
Select count(*) from co_create_type_quicklz_32768_1; 

