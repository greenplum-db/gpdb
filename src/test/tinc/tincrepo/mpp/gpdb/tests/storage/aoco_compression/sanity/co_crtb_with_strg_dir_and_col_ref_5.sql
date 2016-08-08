-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- Drop table if exists
--
DROP TABLE if exists co_crtb_with_strg_dir_and_col_ref_5 cascade;

DROP TABLE if exists co_crtb_with_strg_dir_and_col_ref_5_uncompr cascade;

--
-- Create table
--
CREATE TABLE co_crtb_with_strg_dir_and_col_ref_5 ( 
 id SERIAL,a1 int ENCODING (compresstype=zlib,compresslevel=5,blocksize=1048576),
a2 char(5) ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a3 numeric ENCODING (compresstype=rle_type,compresslevel=1,blocksize=65536),
a4 boolean DEFAULT false  ENCODING (compresstype=zlib,compresslevel=5,blocksize=1048576),
a5 char DEFAULT 'd' ENCODING (compresstype=zlib,compresslevel=5,blocksize=1048576),
a6 text ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a7 timestamp ENCODING (compresstype=rle_type,compresslevel=1,blocksize=65536),
a8 character varying(705) ENCODING (compresstype=zlib,compresslevel=5,blocksize=1048576),
a9 bigint ENCODING (compresstype=zlib,compresslevel=5,blocksize=1048576),
a10 date ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a11 varchar(600) ENCODING (compresstype=rle_type,compresslevel=1,blocksize=65536),
a12 text ENCODING (compresstype=zlib,compresslevel=5,blocksize=1048576),
a13 decimal ENCODING (compresstype=zlib,compresslevel=5,blocksize=1048576),
a14 real ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a15 bigint ENCODING (compresstype=rle_type,compresslevel=1,blocksize=65536),
a16 int4  ENCODING (compresstype=zlib,compresslevel=5,blocksize=1048576),
a17 bytea ENCODING (compresstype=zlib,compresslevel=5,blocksize=1048576),
a18 timestamp with time zone ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a19 timetz ENCODING (compresstype=rle_type,compresslevel=1,blocksize=65536),
a20 path ENCODING (compresstype=zlib,compresslevel=5,blocksize=1048576),
a21 box ENCODING (compresstype=zlib,compresslevel=5,blocksize=1048576),
a22 macaddr ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a23 interval ENCODING (compresstype=rle_type,compresslevel=1,blocksize=65536),
a24 character varying(800) ENCODING (compresstype=zlib,compresslevel=5,blocksize=1048576),
a25 lseg ENCODING (compresstype=zlib,compresslevel=5,blocksize=1048576),
a26 point ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a27 double precision ENCODING (compresstype=rle_type,compresslevel=1,blocksize=65536),
a28 circle ENCODING (compresstype=zlib,compresslevel=5,blocksize=1048576),
a29 int4 ENCODING (compresstype=zlib,compresslevel=5,blocksize=1048576),
a30 numeric(8) ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a31 polygon ENCODING (compresstype=rle_type,compresslevel=1,blocksize=65536),
a32 date ENCODING (compresstype=zlib,compresslevel=5,blocksize=1048576),
a33 real ENCODING (compresstype=zlib,compresslevel=5,blocksize=1048576),
a34 money ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a35 cidr ENCODING (compresstype=rle_type,compresslevel=1,blocksize=65536),
a36 inet ENCODING (compresstype=zlib,compresslevel=5,blocksize=1048576),
a37 time ENCODING (compresstype=zlib,compresslevel=5,blocksize=1048576),
a38 text ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a39 bit ENCODING (compresstype=rle_type,compresslevel=1,blocksize=65536),
a40 bit varying(5) ENCODING (compresstype=zlib,compresslevel=5,blocksize=1048576),
a41 smallint ENCODING (compresstype=zlib,compresslevel=5,blocksize=1048576),
a42 int ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768), COLUMN a1 ENCODING (compresstype=zlib,compresslevel=5,blocksize=1048576)
, COLUMN a2 ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768)
, COLUMN a3 ENCODING (compresstype=rle_type,compresslevel=1,blocksize=65536)
, COLUMN a4 ENCODING (compresstype=zlib,compresslevel=5,blocksize=1048576)
, COLUMN a5 ENCODING (compresstype=zlib,compresslevel=5,blocksize=1048576)
, COLUMN a6 ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768)
, COLUMN a7 ENCODING (compresstype=rle_type,compresslevel=1,blocksize=65536)
, COLUMN a8 ENCODING (compresstype=zlib,compresslevel=5,blocksize=1048576)
, COLUMN a9 ENCODING (compresstype=zlib,compresslevel=5,blocksize=1048576)
, COLUMN a10 ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768)
, COLUMN a11 ENCODING (compresstype=rle_type,compresslevel=1,blocksize=65536)
, COLUMN a12 ENCODING (compresstype=zlib,compresslevel=5,blocksize=1048576)
, COLUMN a13 ENCODING (compresstype=zlib,compresslevel=5,blocksize=1048576)
, COLUMN a14 ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768)
, COLUMN a15 ENCODING (compresstype=rle_type,compresslevel=1,blocksize=65536)
, COLUMN a16 ENCODING (compresstype=zlib,compresslevel=5,blocksize=1048576)
, COLUMN a17 ENCODING (compresstype=zlib,compresslevel=5,blocksize=1048576)
, COLUMN a18 ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768)
, COLUMN a19 ENCODING (compresstype=rle_type,compresslevel=1,blocksize=65536)
, COLUMN a20 ENCODING (compresstype=zlib,compresslevel=5,blocksize=1048576)
, COLUMN a21 ENCODING (compresstype=zlib,compresslevel=5,blocksize=1048576)
, COLUMN a22 ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768)
, COLUMN a23 ENCODING (compresstype=rle_type,compresslevel=1,blocksize=65536)
, COLUMN a24 ENCODING (compresstype=zlib,compresslevel=5,blocksize=1048576)
, COLUMN a25 ENCODING (compresstype=zlib,compresslevel=5,blocksize=1048576)
, COLUMN a26 ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768)
, COLUMN a27 ENCODING (compresstype=rle_type,compresslevel=1,blocksize=65536)
, COLUMN a28 ENCODING (compresstype=zlib,compresslevel=5,blocksize=1048576)
, COLUMN a29 ENCODING (compresstype=zlib,compresslevel=5,blocksize=1048576)
, COLUMN a30 ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768)
, COLUMN a31 ENCODING (compresstype=rle_type,compresslevel=1,blocksize=65536)
, COLUMN a32 ENCODING (compresstype=zlib,compresslevel=5,blocksize=1048576)
, COLUMN a33 ENCODING (compresstype=zlib,compresslevel=5,blocksize=1048576)
, COLUMN a34 ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768)
, COLUMN a35 ENCODING (compresstype=rle_type,compresslevel=1,blocksize=65536)
, COLUMN a36 ENCODING (compresstype=zlib,compresslevel=5,blocksize=1048576)
, COLUMN a37 ENCODING (compresstype=zlib,compresslevel=5,blocksize=1048576)
, COLUMN a38 ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768)
, COLUMN a39 ENCODING (compresstype=rle_type,compresslevel=1,blocksize=65536)
, COLUMN a40 ENCODING (compresstype=zlib,compresslevel=5,blocksize=1048576)
, COLUMN a41 ENCODING (compresstype=zlib,compresslevel=5,blocksize=1048576)
, COLUMN a42 ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),DEFAULT COLUMN ENCODING (compresstype=quicklz,blocksize=8192)) WITH (appendonly=true, orientation=column) distributed randomly;

-- 
-- Create Indexes
--
CREATE INDEX co_crtb_with_strg_dir_and_col_ref_5_idx_bitmap ON co_crtb_with_strg_dir_and_col_ref_5 USING bitmap (a1);

CREATE INDEX co_crtb_with_strg_dir_and_col_ref_5_idx_btree ON co_crtb_with_strg_dir_and_col_ref_5(a9);

--
-- Insert data to the table
--
COPY co_crtb_with_strg_dir_and_col_ref_5(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42)  FROM '/data/sivand/test/tincrepo/private/sivand/mpp/gpdb/tests/storage/aoco_compression/data/copy_base_small' DELIMITER AS '|' ;


--Create Uncompressed table of same schema definition

CREATE TABLE co_crtb_with_strg_dir_and_col_ref_5_uncompr(id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd',a6 text,a7 timestamp,a8 character varying(705),a9 bigint,a10 date,a11 varchar(600),a12 text,a13 decimal,a14 real,a15 bigint,a16 int4 ,a17 bytea,a18 timestamp with time zone,a19 timetz,a20 path,a21 box,a22 macaddr,a23 interval,a24 character varying(800),a25 lseg,a26 point,a27 double precision,a28 circle,a29 int4,a30 numeric(8),a31 polygon,a32 date,a33 real,a34 money,a35 cidr,a36 inet,a37 time,a38 text,a39 bit,a40 bit varying(5),a41 smallint,a42 int) WITH (appendonly=true, orientation=column) distributed randomly;

--
-- Insert to uncompressed table
--
COPY co_crtb_with_strg_dir_and_col_ref_5_uncompr(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42)  FROM '/data/sivand/test/tincrepo/private/sivand/mpp/gpdb/tests/storage/aoco_compression/data/copy_base_small' DELIMITER AS '|' ;

--
-- ********Validation******* 
--
\d+ co_crtb_with_strg_dir_and_col_ref_5

--
-- Compression ratio
--
select 'compression_ratio' as compr_ratio, get_ao_compression_ratio('co_crtb_with_strg_dir_and_col_ref_5'); 

--
-- Compare data with uncompressed table
--
--
-- Select number of rows from the uncompressed table 
--
SELECT count(*) as count_uncompressed from  co_crtb_with_strg_dir_and_col_ref_5_uncompr ;
--
-- Select number of rows from the compressed table 
--
SELECT count(*) as count_compressed from  co_crtb_with_strg_dir_and_col_ref_5;
--
-- Select number of rows using a FULL outer join on all the columns of the two tables 
-- Count should match with above result if the all the rows uncompressed correctly: 
--
Select count(*) as count_join from co_crtb_with_strg_dir_and_col_ref_5 t1 full outer join co_crtb_with_strg_dir_and_col_ref_5_uncompr t2 on t1.id=t2.id and t1.a1=t2.a1 and t1.a2=t2.a2 and t1.a3=t2.a3 and t1.a4=t2.a4 and t1.a5=t2.a5 and t1.a6=t2.a6 and t1.a7=t2.a7 and t1.a8=t2.a8 and t1.a9=t2.a9 and t1.a10=t2.a10 and t1.a11=t2.a11 and t1.a12=t2.a12 and t1.a13=t2.a13 and t1.a14=t2.a14 and t1.a15=t2.a15 and t1.a16=t2.a16 and t1.a17=t2.a17 and t1.a18=t2.a18 and t1.a19=t2.a19 and t1.a22=t2.a22 and t1.a23=t2.a23 and t1.a24=t2.a24 and t1.a27=t2.a27 and t1.a29=t2.a29 and t1.a30=t2.a30 and t1.a32=t2.a32 and t1.a33=t2.a33 and t1.a34=t2.a34 and t1.a35=t2.a35 and t1.a36=t2.a36 and t1.a37=t2.a37 and t1.a38=t2.a38 and t1.a39=t2.a39 and t1.a40=t2.a40 and t1.a41=t2.a41 and t1.a42=t2.a42 ;
--
-- Truncate the table 
--
TRUNCATE table co_crtb_with_strg_dir_and_col_ref_5;
--
-- Insert data again 
--
insert into co_crtb_with_strg_dir_and_col_ref_5 select * from co_crtb_with_strg_dir_and_col_ref_5_uncompr order by a1;

--
-- Compression ratio
--
select 'compression_ratio' as compr_ratio ,get_ao_compression_ratio('co_crtb_with_strg_dir_and_col_ref_5'); 

--Alter table alter type of a column 
Alter table co_crtb_with_strg_dir_and_col_ref_5 Alter column a3 TYPE int4; 
--Insert data to the table, select count(*)
Insert into co_crtb_with_strg_dir_and_col_ref_5(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_crtb_with_strg_dir_and_col_ref_5 where id =10;
Select count(*) from co_crtb_with_strg_dir_and_col_ref_5; 

--Alter table drop a column 
Alter table co_crtb_with_strg_dir_and_col_ref_5 Drop column a12; 
Insert into co_crtb_with_strg_dir_and_col_ref_5(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_crtb_with_strg_dir_and_col_ref_5 where id =10;
Select count(*) from co_crtb_with_strg_dir_and_col_ref_5; 

--Alter table rename a column 
Alter table co_crtb_with_strg_dir_and_col_ref_5 Rename column a13 TO after_rename_a13; 
--Insert data to the table, select count(*)
Insert into co_crtb_with_strg_dir_and_col_ref_5(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_crtb_with_strg_dir_and_col_ref_5 where id =10;
Select count(*) from co_crtb_with_strg_dir_and_col_ref_5; 

--Alter table add a column 
Alter table co_crtb_with_strg_dir_and_col_ref_5 Add column a12 text default 'new column'; 
--Insert data to the table, select count(*)
Insert into co_crtb_with_strg_dir_and_col_ref_5(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_crtb_with_strg_dir_and_col_ref_5 where id =10;
Select count(*) from co_crtb_with_strg_dir_and_col_ref_5; 

