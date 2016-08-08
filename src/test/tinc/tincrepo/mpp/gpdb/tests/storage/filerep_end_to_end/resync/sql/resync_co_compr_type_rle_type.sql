-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE type resync_int_rle_type_1;
CREATE FUNCTION resync_int_rle_type_1_in(cstring)
 RETURNS resync_int_rle_type_1
 AS 'int4in'
 LANGUAGE internal IMMUTABLE STRICT;

CREATE FUNCTION resync_int_rle_type_1_out(resync_int_rle_type_1)
 RETURNS cstring
 AS 'int4out'
 LANGUAGE internal IMMUTABLE STRICT;
 
 CREATE TYPE resync_int_rle_type_1(
 input = resync_int_rle_type_1_in ,
 output = resync_int_rle_type_1_out ,
 internallength = 4,
 default =55,
 passedbyvalue,
 compresstype=rle_type,
 blocksize=32768,
 compresslevel=1);
 
CREATE type resync_int_rle_type_2;
CREATE FUNCTION resync_int_rle_type_2_in(cstring)
 RETURNS resync_int_rle_type_2
 AS 'int4in'
 LANGUAGE internal IMMUTABLE STRICT;

CREATE FUNCTION resync_int_rle_type_2_out(resync_int_rle_type_2)
 RETURNS cstring
 AS 'int4out'
 LANGUAGE internal IMMUTABLE STRICT;
 
 CREATE TYPE resync_int_rle_type_2(
 input = resync_int_rle_type_2_in ,
 output = resync_int_rle_type_2_out ,
 internallength = 4,
 default =55,
 passedbyvalue,
 compresstype=rle_type,
 blocksize=32768,
 compresslevel=2);
  
CREATE type resync_int_rle_type_3;
CREATE FUNCTION resync_int_rle_type_3_in(cstring)
 RETURNS resync_int_rle_type_3
 AS 'int4in'
 LANGUAGE internal IMMUTABLE STRICT;

CREATE FUNCTION resync_int_rle_type_3_out(resync_int_rle_type_3)
 RETURNS cstring
 AS 'int4out'
 LANGUAGE internal IMMUTABLE STRICT;
 
 CREATE TYPE resync_int_rle_type_3(
 input = resync_int_rle_type_3_in ,
 output = resync_int_rle_type_3_out ,
 internallength = 4,
 default =55,
 passedbyvalue,
 compresstype=rle_type,
 blocksize=32768,
 compresslevel=3);
   
    
  

--sync1
 
--Alter type
 
Alter type sync1_int_rle_type_6 set default encoding (compresstype=zlib,compresslevel=1);
 
 --Drop type
 
Drop type if exists sync1_int_rle_type_6 cascade;


 
--ck_sync1
 
 
--Alter type
 
Alter type ck_sync1_int_rle_type_5 set default encoding (compresstype=zlib,compresslevel=1);
 
 --Drop type
 
Drop type if exists ck_sync1_int_rle_type_5 cascade;


 
--ct
 
 
--Alter type
 
Alter type ct_int_rle_type_3 set default encoding (compresstype=zlib,compresslevel=1);
 
 --Drop type
 
Drop type if exists ct_int_rle_type_3 cascade; 


 
--resync
 
 
--Alter type
 
Alter type resync_int_rle_type_1 set default encoding (compresstype=zlib,compresslevel=1);
 
 --Drop type
 
Drop type if exists resync_int_rle_type_1 cascade; 
