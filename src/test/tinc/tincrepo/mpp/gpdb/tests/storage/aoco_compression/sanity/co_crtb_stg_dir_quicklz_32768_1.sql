-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- Drop table if exists
--
DROP TABLE if exists co_crtb_stg_dir_quicklz_32768_1 cascade;

DROP TABLE if exists co_crtb_stg_dir_quicklz_32768_1_uncompr cascade;

--
-- Create table
--
CREATE TABLE co_crtb_stg_dir_quicklz_32768_1 (id SERIAL,
	 a1 int ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a2 char(5) ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a3 numeric ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a4 boolean DEFAULT false  ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a5 char DEFAULT 'd' ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a6 text ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a7 timestamp ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a8 character varying(705) ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a9 bigint ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a10 date ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a11 varchar(600) ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a12 text ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a13 decimal ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a14 real ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a15 bigint ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a16 int4  ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a17 bytea ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a18 timestamp with time zone ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a19 timetz ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a20 path ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a21 box ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a22 macaddr ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a23 interval ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a24 character varying(800) ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a25 lseg ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a26 point ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a27 double precision ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a28 circle ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a29 int4 ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a30 numeric(8) ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a31 polygon ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a32 date ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a33 real ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a34 money ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a35 cidr ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a36 inet ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a37 time ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a38 text ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a39 bit ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a40 bit varying(5) ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a41 smallint ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a42 int ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768)) WITH (appendonly=true, orientation=column) distributed randomly;

-- 
-- Create Indexes
--
CREATE INDEX co_crtb_stg_dir_quicklz_32768_1_idx_bitmap ON co_crtb_stg_dir_quicklz_32768_1 USING bitmap (a1);

CREATE INDEX co_crtb_stg_dir_quicklz_32768_1_idx_btree ON co_crtb_stg_dir_quicklz_32768_1(a9);

--
-- Insert data to the table
--
COPY co_crtb_stg_dir_quicklz_32768_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42)  FROM '/data/sivand/test/tincrepo/private/sivand/mpp/gpdb/tests/storage/aoco_compression/data/copy_base_small' DELIMITER AS '|' ;


--Create Uncompressed table of same schema definition

CREATE TABLE co_crtb_stg_dir_quicklz_32768_1_uncompr(id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd',a6 text,a7 timestamp,a8 character varying(705),a9 bigint,a10 date,a11 varchar(600),a12 text,a13 decimal,a14 real,a15 bigint,a16 int4 ,a17 bytea,a18 timestamp with time zone,a19 timetz,a20 path,a21 box,a22 macaddr,a23 interval,a24 character varying(800),a25 lseg,a26 point,a27 double precision,a28 circle,a29 int4,a30 numeric(8),a31 polygon,a32 date,a33 real,a34 money,a35 cidr,a36 inet,a37 time,a38 text,a39 bit,a40 bit varying(5),a41 smallint,a42 int) WITH (appendonly=true, orientation=column) distributed randomly;

--
-- Insert to uncompressed table
--
COPY co_crtb_stg_dir_quicklz_32768_1_uncompr(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42)  FROM '/data/sivand/test/tincrepo/private/sivand/mpp/gpdb/tests/storage/aoco_compression/data/copy_base_small' DELIMITER AS '|' ;

--
-- ********Validation******* 
--
\d+ co_crtb_stg_dir_quicklz_32768_1

--
-- Compression ratio
--
select 'compression_ratio' as compr_ratio, get_ao_compression_ratio('co_crtb_stg_dir_quicklz_32768_1'); 

--Select from pg_attribute_encoding to see the table entry 
select attrelid::regclass as relname, attnum, attoptions from pg_class c, pg_attribute_encoding e  where c.relname = 'co_crtb_stg_dir_quicklz_32768_1' and c.oid=e.attrelid  order by relname, attnum limit 3; 
--
-- Compare data with uncompressed table
--
--
-- Select number of rows from the uncompressed table 
--
SELECT count(*) as count_uncompressed from  co_crtb_stg_dir_quicklz_32768_1_uncompr ;
--
-- Select number of rows from the compressed table 
--
SELECT count(*) as count_compressed from  co_crtb_stg_dir_quicklz_32768_1;
--
-- Select number of rows using a FULL outer join on all the columns of the two tables 
-- Count should match with above result if the all the rows uncompressed correctly: 
--
Select count(*) as count_join from co_crtb_stg_dir_quicklz_32768_1 t1 full outer join co_crtb_stg_dir_quicklz_32768_1_uncompr t2 on t1.id=t2.id and t1.a1=t2.a1 and t1.a2=t2.a2 and t1.a3=t2.a3 and t1.a4=t2.a4 and t1.a5=t2.a5 and t1.a6=t2.a6 and t1.a7=t2.a7 and t1.a8=t2.a8 and t1.a9=t2.a9 and t1.a10=t2.a10 and t1.a11=t2.a11 and t1.a12=t2.a12 and t1.a13=t2.a13 and t1.a14=t2.a14 and t1.a15=t2.a15 and t1.a16=t2.a16 and t1.a17=t2.a17 and t1.a18=t2.a18 and t1.a19=t2.a19 and t1.a22=t2.a22 and t1.a23=t2.a23 and t1.a24=t2.a24 and t1.a27=t2.a27 and t1.a29=t2.a29 and t1.a30=t2.a30 and t1.a32=t2.a32 and t1.a33=t2.a33 and t1.a34=t2.a34 and t1.a35=t2.a35 and t1.a36=t2.a36 and t1.a37=t2.a37 and t1.a38=t2.a38 and t1.a39=t2.a39 and t1.a40=t2.a40 and t1.a41=t2.a41 and t1.a42=t2.a42 ;
--
-- Truncate the table 
--
TRUNCATE table co_crtb_stg_dir_quicklz_32768_1;
--
-- Insert data again 
--
insert into co_crtb_stg_dir_quicklz_32768_1 select * from co_crtb_stg_dir_quicklz_32768_1_uncompr order by a1;

--
-- Compression ratio
--
select 'compression_ratio' as compr_ratio ,get_ao_compression_ratio('co_crtb_stg_dir_quicklz_32768_1'); 

--Alter table alter type of a column 
Alter table co_crtb_stg_dir_quicklz_32768_1 Alter column a3 TYPE int4; 
--Insert data to the table, select count(*)
Insert into co_crtb_stg_dir_quicklz_32768_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_crtb_stg_dir_quicklz_32768_1 where id =10;
Select count(*) from co_crtb_stg_dir_quicklz_32768_1; 

--Alter table drop a column 
Alter table co_crtb_stg_dir_quicklz_32768_1 Drop column a12; 
Insert into co_crtb_stg_dir_quicklz_32768_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_crtb_stg_dir_quicklz_32768_1 where id =10;
Select count(*) from co_crtb_stg_dir_quicklz_32768_1; 

--Alter table rename a column 
Alter table co_crtb_stg_dir_quicklz_32768_1 Rename column a13 TO after_rename_a13; 
--Insert data to the table, select count(*)
Insert into co_crtb_stg_dir_quicklz_32768_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_crtb_stg_dir_quicklz_32768_1 where id =10;
Select count(*) from co_crtb_stg_dir_quicklz_32768_1; 

--Alter table add a column 
Alter table co_crtb_stg_dir_quicklz_32768_1 Add column a12 text default 'new column'; 
--Insert data to the table, select count(*)
Insert into co_crtb_stg_dir_quicklz_32768_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42) select a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,after_rename_a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42 from co_crtb_stg_dir_quicklz_32768_1 where id =10;
Select count(*) from co_crtb_stg_dir_quicklz_32768_1; 

--Drop table 
DROP table co_crtb_stg_dir_quicklz_32768_1; 

--Create table again and insert data 
CREATE TABLE co_crtb_stg_dir_quicklz_32768_1 (id SERIAL,
	 a1 int ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a2 char(5) ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a3 numeric ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a4 boolean DEFAULT false  ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a5 char DEFAULT 'd' ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a6 text ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a7 timestamp ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a8 character varying(705) ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a9 bigint ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a10 date ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a11 varchar(600) ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a12 text ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a13 decimal ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a14 real ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a15 bigint ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a16 int4  ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a17 bytea ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a18 timestamp with time zone ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a19 timetz ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a20 path ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a21 box ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a22 macaddr ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a23 interval ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a24 character varying(800) ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a25 lseg ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a26 point ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a27 double precision ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a28 circle ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a29 int4 ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a30 numeric(8) ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a31 polygon ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a32 date ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a33 real ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a34 money ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a35 cidr ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a36 inet ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a37 time ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a38 text ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a39 bit ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a40 bit varying(5) ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a41 smallint ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768),
a42 int ENCODING (compresstype=quicklz,compresslevel=1,blocksize=32768)) WITH (appendonly=true, orientation=column) distributed randomly;
COPY co_crtb_stg_dir_quicklz_32768_1(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42)  FROM '/data/sivand/test/tincrepo/private/sivand/mpp/gpdb/tests/storage/aoco_compression/data/copy_base_small' DELIMITER AS '|' ;

--Alter table drop a column 
Alter table co_crtb_stg_dir_quicklz_32768_1 Drop column a12; 
--Create CTAS table 

 Drop table if exists co_crtb_stg_dir_quicklz_32768_1_ctas ;
--Create a CTAS table 
CREATE TABLE co_crtb_stg_dir_quicklz_32768_1_ctas  WITH (appendonly=true, orientation=column) AS Select * from co_crtb_stg_dir_quicklz_32768_1;

