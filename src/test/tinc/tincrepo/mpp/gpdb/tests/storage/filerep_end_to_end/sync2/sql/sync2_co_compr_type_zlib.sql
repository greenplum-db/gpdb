-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE type sync2_int_zlib_1;
CREATE FUNCTION sync2_int_zlib_1_in(cstring)
 RETURNS sync2_int_zlib_1
 AS 'int4in'
 LANGUAGE internal IMMUTABLE STRICT;

CREATE FUNCTION sync2_int_zlib_1_out(sync2_int_zlib_1)
 RETURNS cstring
 AS 'int4out'
 LANGUAGE internal IMMUTABLE STRICT;
 
 CREATE TYPE sync2_int_zlib_1(
 input = sync2_int_zlib_1_in ,
 output = sync2_int_zlib_1_out ,
 internallength = 4,
 default =55,
 passedbyvalue,
 compresstype=zlib,
 blocksize=32768,
 compresslevel=1);
 
CREATE type sync2_int_zlib_2;
CREATE FUNCTION sync2_int_zlib_2_in(cstring)
 RETURNS sync2_int_zlib_2
 AS 'int4in'
 LANGUAGE internal IMMUTABLE STRICT;

CREATE FUNCTION sync2_int_zlib_2_out(sync2_int_zlib_2)
 RETURNS cstring
 AS 'int4out'
 LANGUAGE internal IMMUTABLE STRICT;
 
 CREATE TYPE sync2_int_zlib_2(
 input = sync2_int_zlib_2_in ,
 output = sync2_int_zlib_2_out ,
 internallength = 4,
 default =55,
 passedbyvalue,
 compresstype=zlib,
 blocksize=32768,
 compresslevel=1);
  
    
  

--sync1
 
--Alter type
 
Alter type sync1_int_zlib_7 set default encoding (compresstype=zlib,compresslevel=1);
 
 --Drop type
 
Drop type if exists sync1_int_zlib_7 cascade;


 
--ck_sync1
 
 
--Alter type
 
Alter type ck_sync1_int_zlib_6 set default encoding (compresstype=zlib,compresslevel=1);
 
 --Drop type
 
Drop type if exists ck_sync1_int_zlib_6 cascade;


--ct
 
 
--Alter type
 
Alter type ct_int_zlib_4 set default encoding (compresstype=zlib,compresslevel=1);
 
 --Drop type
 
Drop type if exists ct_int_zlib_4 cascade; 


 
--resync
 
 
--Alter type
 
Alter type resync_int_zlib_2 set default encoding (compresstype=zlib,compresslevel=1);
 
 --Drop type
 
Drop type if exists resync_int_zlib_2 cascade; 


--sync2
 
 
--Alter type
 
Alter type sync2_int_zlib_1 set default encoding (compresstype=zlib,compresslevel=1);
 
 --Drop type
 
Drop type if exists sync2_int_zlib_1 cascade; 
