-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE type ct_int_zlib_1;
CREATE FUNCTION ct_int_zlib_1_in(cstring)
 RETURNS ct_int_zlib_1
 AS 'int4in'
 LANGUAGE internal IMMUTABLE STRICT;

CREATE FUNCTION ct_int_zlib_1_out(ct_int_zlib_1)
 RETURNS cstring
 AS 'int4out'
 LANGUAGE internal IMMUTABLE STRICT;
 
 CREATE TYPE ct_int_zlib_1(
 input = ct_int_zlib_1_in ,
 output = ct_int_zlib_1_out ,
 internallength = 4,
 default =55,
 passedbyvalue,
 compresstype=zlib,
 blocksize=32768,
 compresslevel=1);
 
CREATE type ct_int_zlib_2;
CREATE FUNCTION ct_int_zlib_2_in(cstring)
 RETURNS ct_int_zlib_2
 AS 'int4in'
 LANGUAGE internal IMMUTABLE STRICT;

CREATE FUNCTION ct_int_zlib_2_out(ct_int_zlib_2)
 RETURNS cstring
 AS 'int4out'
 LANGUAGE internal IMMUTABLE STRICT;
 
 CREATE TYPE ct_int_zlib_2(
 input = ct_int_zlib_2_in ,
 output = ct_int_zlib_2_out ,
 internallength = 4,
 default =55,
 passedbyvalue,
 compresstype=zlib,
 blocksize=32768,
 compresslevel=1);
  
CREATE type ct_int_zlib_3;
CREATE FUNCTION ct_int_zlib_3_in(cstring)
 RETURNS ct_int_zlib_3
 AS 'int4in'
 LANGUAGE internal IMMUTABLE STRICT;

CREATE FUNCTION ct_int_zlib_3_out(ct_int_zlib_3)
 RETURNS cstring
 AS 'int4out'
 LANGUAGE internal IMMUTABLE STRICT;
 
 CREATE TYPE ct_int_zlib_3(
 input = ct_int_zlib_3_in ,
 output = ct_int_zlib_3_out ,
 internallength = 4,
 default =55,
 passedbyvalue,
 compresstype=zlib,
 blocksize=32768,
 compresslevel=1);
   
CREATE type ct_int_zlib_4;
CREATE FUNCTION ct_int_zlib_4_in(cstring)
 RETURNS ct_int_zlib_4
 AS 'int4in'
 LANGUAGE internal IMMUTABLE STRICT;

CREATE FUNCTION ct_int_zlib_4_out(ct_int_zlib_4)
 RETURNS cstring
 AS 'int4out'
 LANGUAGE internal IMMUTABLE STRICT;
 
 CREATE TYPE ct_int_zlib_4(
 input = ct_int_zlib_4_in ,
 output = ct_int_zlib_4_out ,
 internallength = 4,
 default =55,
 passedbyvalue,
 compresstype=zlib,
 blocksize=32768,
 compresslevel=1);
    

CREATE type ct_int_zlib_5;
CREATE FUNCTION ct_int_zlib_5_in(cstring)
 RETURNS ct_int_zlib_5
 AS 'int4in'
 LANGUAGE internal IMMUTABLE STRICT;

CREATE FUNCTION ct_int_zlib_5_out(ct_int_zlib_5)
 RETURNS cstring
 AS 'int4out'
 LANGUAGE internal IMMUTABLE STRICT;
 
 CREATE TYPE ct_int_zlib_5(
 input = ct_int_zlib_5_in ,
 output = ct_int_zlib_5_out ,
 internallength = 4,
 default =55,
 passedbyvalue,
 compresstype=zlib,
 blocksize=32768,
 compresslevel=1);

  

--sync1
 
--Alter type
 
Alter type sync1_int_zlib_4 set default encoding (compresstype=zlib,compresslevel=1);
 
 --Drop type
 
Drop type if exists sync1_int_zlib_4 cascade;


 
--ck_sync1
 
 
--Alter type
 
Alter type ck_sync1_int_zlib_3 set default encoding (compresstype=zlib,compresslevel=1);
 
 --Drop type
 
Drop type if exists ck_sync1_int_zlib_3 cascade;


--ct
 
 
--Alter type
 
Alter type ct_int_zlib_1 set default encoding (compresstype=zlib,compresslevel=1);
 
 --Drop type
 
Drop type if exists ct_int_zlib_1 cascade; 
