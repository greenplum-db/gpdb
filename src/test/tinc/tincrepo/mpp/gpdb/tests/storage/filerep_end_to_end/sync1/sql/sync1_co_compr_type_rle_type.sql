-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE type sync1_int_rle_type_1;
CREATE FUNCTION sync1_int_rle_type_1_in(cstring)
 RETURNS sync1_int_rle_type_1
 AS 'int4in'
 LANGUAGE internal IMMUTABLE STRICT;

CREATE FUNCTION sync1_int_rle_type_1_out(sync1_int_rle_type_1)
 RETURNS cstring
 AS 'int4out'
 LANGUAGE internal IMMUTABLE STRICT;
 
 CREATE TYPE sync1_int_rle_type_1(
 input = sync1_int_rle_type_1_in ,
 output = sync1_int_rle_type_1_out ,
 internallength = 4,
 default =55,
 passedbyvalue,
 compresstype=rle_type,
 blocksize=32768,
 compresslevel=1);
 
CREATE type sync1_int_rle_type_2;
CREATE FUNCTION sync1_int_rle_type_2_in(cstring)
 RETURNS sync1_int_rle_type_2
 AS 'int4in'
 LANGUAGE internal IMMUTABLE STRICT;

CREATE FUNCTION sync1_int_rle_type_2_out(sync1_int_rle_type_2)
 RETURNS cstring
 AS 'int4out'
 LANGUAGE internal IMMUTABLE STRICT;
 
 CREATE TYPE sync1_int_rle_type_2(
 input = sync1_int_rle_type_2_in ,
 output = sync1_int_rle_type_2_out ,
 internallength = 4,
 default =55,
 passedbyvalue,
 compresstype=rle_type,
 blocksize=32768,
 compresslevel=2);
  
CREATE type sync1_int_rle_type_3;
CREATE FUNCTION sync1_int_rle_type_3_in(cstring)
 RETURNS sync1_int_rle_type_3
 AS 'int4in'
 LANGUAGE internal IMMUTABLE STRICT;

CREATE FUNCTION sync1_int_rle_type_3_out(sync1_int_rle_type_3)
 RETURNS cstring
 AS 'int4out'
 LANGUAGE internal IMMUTABLE STRICT;
 
 CREATE TYPE sync1_int_rle_type_3(
 input = sync1_int_rle_type_3_in ,
 output = sync1_int_rle_type_3_out ,
 internallength = 4,
 default =55,
 passedbyvalue,
 compresstype=rle_type,
 blocksize=32768,
 compresslevel=3);
   
CREATE type sync1_int_rle_type_4;
CREATE FUNCTION sync1_int_rle_type_4_in(cstring)
 RETURNS sync1_int_rle_type_4
 AS 'int4in'
 LANGUAGE internal IMMUTABLE STRICT;

CREATE FUNCTION sync1_int_rle_type_4_out(sync1_int_rle_type_4)
 RETURNS cstring
 AS 'int4out'
 LANGUAGE internal IMMUTABLE STRICT;
 
 CREATE TYPE sync1_int_rle_type_4(
 input = sync1_int_rle_type_4_in ,
 output = sync1_int_rle_type_4_out ,
 internallength = 4,
 default =55,
 passedbyvalue,
 compresstype=rle_type,
 blocksize=32768,
 compresslevel=4);
    

CREATE type sync1_int_rle_type_5;
CREATE FUNCTION sync1_int_rle_type_5_in(cstring)
 RETURNS sync1_int_rle_type_5
 AS 'int4in'
 LANGUAGE internal IMMUTABLE STRICT;

CREATE FUNCTION sync1_int_rle_type_5_out(sync1_int_rle_type_5)
 RETURNS cstring
 AS 'int4out'
 LANGUAGE internal IMMUTABLE STRICT;
 
 CREATE TYPE sync1_int_rle_type_5(
 input = sync1_int_rle_type_5_in ,
 output = sync1_int_rle_type_5_out ,
 internallength = 4,
 default =55,
 passedbyvalue,
 compresstype=rle_type,
 blocksize=32768,
 compresslevel=1);
    
   
CREATE type sync1_int_rle_type_6;
CREATE FUNCTION sync1_int_rle_type_6_in(cstring)
 RETURNS sync1_int_rle_type_6
 AS 'int4in'
 LANGUAGE internal IMMUTABLE STRICT;

CREATE FUNCTION sync1_int_rle_type_6_out(sync1_int_rle_type_6)
 RETURNS cstring
 AS 'int4out'
 LANGUAGE internal IMMUTABLE STRICT;
 
 CREATE TYPE sync1_int_rle_type_6(
 input = sync1_int_rle_type_6_in ,
 output = sync1_int_rle_type_6_out ,
 internallength = 4,
 default =55,
 passedbyvalue,
 compresstype=rle_type,
 blocksize=32768,
 compresslevel=2);
 
 
CREATE type sync1_int_rle_type_7;
CREATE FUNCTION sync1_int_rle_type_7_in(cstring)
 RETURNS sync1_int_rle_type_7
 AS 'int4in'
 LANGUAGE internal IMMUTABLE STRICT;

CREATE FUNCTION sync1_int_rle_type_7_out(sync1_int_rle_type_7)
 RETURNS cstring
 AS 'int4out'
 LANGUAGE internal IMMUTABLE STRICT;
 
 CREATE TYPE sync1_int_rle_type_7(
 input = sync1_int_rle_type_7_in ,
 output = sync1_int_rle_type_7_out ,
 internallength = 4,
 default =55,
 passedbyvalue,
 compresstype=rle_type,
 blocksize=32768,
 compresslevel=3);
  
CREATE type sync1_int_rle_type_8;
CREATE FUNCTION sync1_int_rle_type_8_in(cstring)
 RETURNS sync1_int_rle_type_8
 AS 'int4in'
 LANGUAGE internal IMMUTABLE STRICT;

CREATE FUNCTION sync1_int_rle_type_8_out(sync1_int_rle_type_8)
 RETURNS cstring
 AS 'int4out'
 LANGUAGE internal IMMUTABLE STRICT;
 
 CREATE TYPE sync1_int_rle_type_8(
 input = sync1_int_rle_type_8_in ,
 output = sync1_int_rle_type_8_out ,
 internallength = 4,
 default =55,
 passedbyvalue,
 compresstype=rle_type,
 blocksize=32768,
 compresslevel=4);
 

--sync1 

 
--Alter type
 
Alter type sync1_int_rle_type_1 set default encoding (compresstype=zlib,compresslevel=1);
 
 --Drop type
 
Drop type if exists sync1_int_rle_type_1 cascade;
 
