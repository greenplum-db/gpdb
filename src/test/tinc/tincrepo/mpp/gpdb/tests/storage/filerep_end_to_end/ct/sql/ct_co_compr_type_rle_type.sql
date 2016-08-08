-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE type ct_int_rle_type_1;
CREATE FUNCTION ct_int_rle_type_1_in(cstring)
 RETURNS ct_int_rle_type_1
 AS 'int4in'
 LANGUAGE internal IMMUTABLE STRICT;

CREATE FUNCTION ct_int_rle_type_1_out(ct_int_rle_type_1)
 RETURNS cstring
 AS 'int4out'
 LANGUAGE internal IMMUTABLE STRICT;
 
 CREATE TYPE ct_int_rle_type_1(
 input = ct_int_rle_type_1_in ,
 output = ct_int_rle_type_1_out ,
 internallength = 4,
 default =55,
 passedbyvalue,
 compresstype=rle_type,
 blocksize=32768,
 compresslevel=1);
 
CREATE type ct_int_rle_type_2;
CREATE FUNCTION ct_int_rle_type_2_in(cstring)
 RETURNS ct_int_rle_type_2
 AS 'int4in'
 LANGUAGE internal IMMUTABLE STRICT;

CREATE FUNCTION ct_int_rle_type_2_out(ct_int_rle_type_2)
 RETURNS cstring
 AS 'int4out'
 LANGUAGE internal IMMUTABLE STRICT;
 
 CREATE TYPE ct_int_rle_type_2(
 input = ct_int_rle_type_2_in ,
 output = ct_int_rle_type_2_out ,
 internallength = 4,
 default =55,
 passedbyvalue,
 compresstype=rle_type,
 blocksize=32768,
 compresslevel=2);
  
CREATE type ct_int_rle_type_3;
CREATE FUNCTION ct_int_rle_type_3_in(cstring)
 RETURNS ct_int_rle_type_3
 AS 'int4in'
 LANGUAGE internal IMMUTABLE STRICT;

CREATE FUNCTION ct_int_rle_type_3_out(ct_int_rle_type_3)
 RETURNS cstring
 AS 'int4out'
 LANGUAGE internal IMMUTABLE STRICT;
 
 CREATE TYPE ct_int_rle_type_3(
 input = ct_int_rle_type_3_in ,
 output = ct_int_rle_type_3_out ,
 internallength = 4,
 default =55,
 passedbyvalue,
 compresstype=rle_type,
 blocksize=32768,
 compresslevel=3);
   
CREATE type ct_int_rle_type_4;
CREATE FUNCTION ct_int_rle_type_4_in(cstring)
 RETURNS ct_int_rle_type_4
 AS 'int4in'
 LANGUAGE internal IMMUTABLE STRICT;

CREATE FUNCTION ct_int_rle_type_4_out(ct_int_rle_type_4)
 RETURNS cstring
 AS 'int4out'
 LANGUAGE internal IMMUTABLE STRICT;
 
 CREATE TYPE ct_int_rle_type_4(
 input = ct_int_rle_type_4_in ,
 output = ct_int_rle_type_4_out ,
 internallength = 4,
 default =55,
 passedbyvalue,
 compresstype=rle_type,
 blocksize=32768,
 compresslevel=4);
    

CREATE type ct_int_rle_type_5;
CREATE FUNCTION ct_int_rle_type_5_in(cstring)
 RETURNS ct_int_rle_type_5
 AS 'int4in'
 LANGUAGE internal IMMUTABLE STRICT;

CREATE FUNCTION ct_int_rle_type_5_out(ct_int_rle_type_5)
 RETURNS cstring
 AS 'int4out'
 LANGUAGE internal IMMUTABLE STRICT;
 
 CREATE TYPE ct_int_rle_type_5(
 input = ct_int_rle_type_5_in ,
 output = ct_int_rle_type_5_out ,
 internallength = 4,
 default =55,
 passedbyvalue,
 compresstype=rle_type,
 blocksize=32768,
 compresslevel=1);

  

--sync1
 
--Alter type
 
Alter type sync1_int_rle_type_4 set default encoding (compresstype=zlib,compresslevel=1);
 
 --Drop type
 
Drop type if exists sync1_int_rle_type_4 cascade;


 
--ck_sync1
 
 
--Alter type
 
Alter type ck_sync1_int_rle_type_3 set default encoding (compresstype=zlib,compresslevel=1);
 
 --Drop type
 
Drop type if exists ck_sync1_int_rle_type_3 cascade;


--ct
 
 
--Alter type
 
Alter type ct_int_rle_type_1 set default encoding (compresstype=zlib,compresslevel=1);
 
 --Drop type
 
Drop type if exists ct_int_rle_type_1 cascade; 
