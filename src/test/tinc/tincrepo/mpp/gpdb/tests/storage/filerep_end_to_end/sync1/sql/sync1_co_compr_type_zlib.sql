-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE type sync1_int_zlib_1;
CREATE FUNCTION sync1_int_zlib_1_in(cstring)
 RETURNS sync1_int_zlib_1
 AS 'int4in'
 LANGUAGE internal IMMUTABLE STRICT;

CREATE FUNCTION sync1_int_zlib_1_out(sync1_int_zlib_1)
 RETURNS cstring
 AS 'int4out'
 LANGUAGE internal IMMUTABLE STRICT;
 
 CREATE TYPE sync1_int_zlib_1(
 input = sync1_int_zlib_1_in ,
 output = sync1_int_zlib_1_out ,
 internallength = 4,
 default =55,
 passedbyvalue,
 compresstype=zlib,
 blocksize=32768,
 compresslevel=1);
 
CREATE type sync1_int_zlib_2;
CREATE FUNCTION sync1_int_zlib_2_in(cstring)
 RETURNS sync1_int_zlib_2
 AS 'int4in'
 LANGUAGE internal IMMUTABLE STRICT;

CREATE FUNCTION sync1_int_zlib_2_out(sync1_int_zlib_2)
 RETURNS cstring
 AS 'int4out'
 LANGUAGE internal IMMUTABLE STRICT;
 
 CREATE TYPE sync1_int_zlib_2(
 input = sync1_int_zlib_2_in ,
 output = sync1_int_zlib_2_out ,
 internallength = 4,
 default =55,
 passedbyvalue,
 compresstype=zlib,
 blocksize=32768,
 compresslevel=1);
  
CREATE type sync1_int_zlib_3;
CREATE FUNCTION sync1_int_zlib_3_in(cstring)
 RETURNS sync1_int_zlib_3
 AS 'int4in'
 LANGUAGE internal IMMUTABLE STRICT;

CREATE FUNCTION sync1_int_zlib_3_out(sync1_int_zlib_3)
 RETURNS cstring
 AS 'int4out'
 LANGUAGE internal IMMUTABLE STRICT;
 
 CREATE TYPE sync1_int_zlib_3(
 input = sync1_int_zlib_3_in ,
 output = sync1_int_zlib_3_out ,
 internallength = 4,
 default =55,
 passedbyvalue,
 compresstype=zlib,
 blocksize=32768,
 compresslevel=1);
   
CREATE type sync1_int_zlib_4;
CREATE FUNCTION sync1_int_zlib_4_in(cstring)
 RETURNS sync1_int_zlib_4
 AS 'int4in'
 LANGUAGE internal IMMUTABLE STRICT;

CREATE FUNCTION sync1_int_zlib_4_out(sync1_int_zlib_4)
 RETURNS cstring
 AS 'int4out'
 LANGUAGE internal IMMUTABLE STRICT;
 
 CREATE TYPE sync1_int_zlib_4(
 input = sync1_int_zlib_4_in ,
 output = sync1_int_zlib_4_out ,
 internallength = 4,
 default =55,
 passedbyvalue,
 compresstype=zlib,
 blocksize=32768,
 compresslevel=1);
    

CREATE type sync1_int_zlib_5;
CREATE FUNCTION sync1_int_zlib_5_in(cstring)
 RETURNS sync1_int_zlib_5
 AS 'int4in'
 LANGUAGE internal IMMUTABLE STRICT;

CREATE FUNCTION sync1_int_zlib_5_out(sync1_int_zlib_5)
 RETURNS cstring
 AS 'int4out'
 LANGUAGE internal IMMUTABLE STRICT;
 
 CREATE TYPE sync1_int_zlib_5(
 input = sync1_int_zlib_5_in ,
 output = sync1_int_zlib_5_out ,
 internallength = 4,
 default =55,
 passedbyvalue,
 compresstype=zlib,
 blocksize=32768,
 compresslevel=1);
    
   
CREATE type sync1_int_zlib_6;
CREATE FUNCTION sync1_int_zlib_6_in(cstring)
 RETURNS sync1_int_zlib_6
 AS 'int4in'
 LANGUAGE internal IMMUTABLE STRICT;

CREATE FUNCTION sync1_int_zlib_6_out(sync1_int_zlib_6)
 RETURNS cstring
 AS 'int4out'
 LANGUAGE internal IMMUTABLE STRICT;
 
 CREATE TYPE sync1_int_zlib_6(
 input = sync1_int_zlib_6_in ,
 output = sync1_int_zlib_6_out ,
 internallength = 4,
 default =55,
 passedbyvalue,
 compresstype=zlib,
 blocksize=32768,
 compresslevel=1);
 
 
CREATE type sync1_int_zlib_7;
CREATE FUNCTION sync1_int_zlib_7_in(cstring)
 RETURNS sync1_int_zlib_7
 AS 'int4in'
 LANGUAGE internal IMMUTABLE STRICT;

CREATE FUNCTION sync1_int_zlib_7_out(sync1_int_zlib_7)
 RETURNS cstring
 AS 'int4out'
 LANGUAGE internal IMMUTABLE STRICT;
 
 CREATE TYPE sync1_int_zlib_7(
 input = sync1_int_zlib_7_in ,
 output = sync1_int_zlib_7_out ,
 internallength = 4,
 default =55,
 passedbyvalue,
 compresstype=zlib,
 blocksize=32768,
 compresslevel=1);
  
CREATE type sync1_int_zlib_8;
CREATE FUNCTION sync1_int_zlib_8_in(cstring)
 RETURNS sync1_int_zlib_8
 AS 'int4in'
 LANGUAGE internal IMMUTABLE STRICT;

CREATE FUNCTION sync1_int_zlib_8_out(sync1_int_zlib_8)
 RETURNS cstring
 AS 'int4out'
 LANGUAGE internal IMMUTABLE STRICT;
 
 CREATE TYPE sync1_int_zlib_8(
 input = sync1_int_zlib_8_in ,
 output = sync1_int_zlib_8_out ,
 internallength = 4,
 default =55,
 passedbyvalue,
 compresstype=zlib,
 blocksize=32768,
 compresslevel=1);
 

--sync1 

 
--Alter type
 
Alter type sync1_int_zlib_1 set default encoding (compresstype=zlib,compresslevel=1);
 
 --Drop type
 
Drop type if exists sync1_int_zlib_1 cascade;
 
