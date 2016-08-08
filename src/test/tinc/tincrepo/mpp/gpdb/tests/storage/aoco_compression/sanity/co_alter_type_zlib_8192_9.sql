-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
ALTER TYPE int4 SET DEFAULT ENCODING (compresstype=zlib,compresslevel=9,blocksize=8192);

select typoptions from pg_type_encoding where typid='int4 '::regtype;

ALTER TYPE char SET DEFAULT ENCODING (compresstype=zlib,compresslevel=9,blocksize=8192);

select typoptions from pg_type_encoding where typid='char '::regtype;

ALTER TYPE numeric SET DEFAULT ENCODING (compresstype=zlib,compresslevel=9,blocksize=8192);

select typoptions from pg_type_encoding where typid='numeric '::regtype;

ALTER TYPE bool SET DEFAULT ENCODING (compresstype=zlib,compresslevel=9,blocksize=8192);

select typoptions from pg_type_encoding where typid='bool '::regtype;

ALTER TYPE text SET DEFAULT ENCODING (compresstype=zlib,compresslevel=9,blocksize=8192);

select typoptions from pg_type_encoding where typid='text '::regtype;

ALTER TYPE timestamp SET DEFAULT ENCODING (compresstype=zlib,compresslevel=9,blocksize=8192);

select typoptions from pg_type_encoding where typid='timestamp '::regtype;

ALTER TYPE int8 SET DEFAULT ENCODING (compresstype=zlib,compresslevel=9,blocksize=8192);

select typoptions from pg_type_encoding where typid='int8 '::regtype;

ALTER TYPE date SET DEFAULT ENCODING (compresstype=zlib,compresslevel=9,blocksize=8192);

select typoptions from pg_type_encoding where typid='date '::regtype;

ALTER TYPE varchar SET DEFAULT ENCODING (compresstype=zlib,compresslevel=9,blocksize=8192);

select typoptions from pg_type_encoding where typid='varchar '::regtype;

ALTER TYPE numeric SET DEFAULT ENCODING (compresstype=zlib,compresslevel=9,blocksize=8192);

select typoptions from pg_type_encoding where typid='numeric '::regtype;

ALTER TYPE float4 SET DEFAULT ENCODING (compresstype=zlib,compresslevel=9,blocksize=8192);

select typoptions from pg_type_encoding where typid='float4 '::regtype;

ALTER TYPE bytea SET DEFAULT ENCODING (compresstype=zlib,compresslevel=9,blocksize=8192);

select typoptions from pg_type_encoding where typid='bytea '::regtype;

ALTER TYPE timestamptz SET DEFAULT ENCODING (compresstype=zlib,compresslevel=9,blocksize=8192);

select typoptions from pg_type_encoding where typid='timestamptz '::regtype;

ALTER TYPE timetz SET DEFAULT ENCODING (compresstype=zlib,compresslevel=9,blocksize=8192);

select typoptions from pg_type_encoding where typid='timetz '::regtype;

ALTER TYPE path SET DEFAULT ENCODING (compresstype=zlib,compresslevel=9,blocksize=8192);

select typoptions from pg_type_encoding where typid='path '::regtype;

ALTER TYPE box SET DEFAULT ENCODING (compresstype=zlib,compresslevel=9,blocksize=8192);

select typoptions from pg_type_encoding where typid='box '::regtype;

ALTER TYPE macaddr SET DEFAULT ENCODING (compresstype=zlib,compresslevel=9,blocksize=8192);

select typoptions from pg_type_encoding where typid='macaddr '::regtype;

ALTER TYPE interval SET DEFAULT ENCODING (compresstype=zlib,compresslevel=9,blocksize=8192);

select typoptions from pg_type_encoding where typid='interval '::regtype;

ALTER TYPE lseg SET DEFAULT ENCODING (compresstype=zlib,compresslevel=9,blocksize=8192);

select typoptions from pg_type_encoding where typid='lseg '::regtype;

ALTER TYPE point SET DEFAULT ENCODING (compresstype=zlib,compresslevel=9,blocksize=8192);

select typoptions from pg_type_encoding where typid='point '::regtype;

ALTER TYPE float8 SET DEFAULT ENCODING (compresstype=zlib,compresslevel=9,blocksize=8192);

select typoptions from pg_type_encoding where typid='float8 '::regtype;

ALTER TYPE circle SET DEFAULT ENCODING (compresstype=zlib,compresslevel=9,blocksize=8192);

select typoptions from pg_type_encoding where typid='circle '::regtype;

ALTER TYPE char SET DEFAULT ENCODING (compresstype=zlib,compresslevel=9,blocksize=8192);

select typoptions from pg_type_encoding where typid='char '::regtype;

ALTER TYPE polygon SET DEFAULT ENCODING (compresstype=zlib,compresslevel=9,blocksize=8192);

select typoptions from pg_type_encoding where typid='polygon '::regtype;

ALTER TYPE date SET DEFAULT ENCODING (compresstype=zlib,compresslevel=9,blocksize=8192);

select typoptions from pg_type_encoding where typid='date '::regtype;

ALTER TYPE varbit SET DEFAULT ENCODING (compresstype=zlib,compresslevel=9,blocksize=8192);

select typoptions from pg_type_encoding where typid='varbit '::regtype;

ALTER TYPE money SET DEFAULT ENCODING (compresstype=zlib,compresslevel=9,blocksize=8192);

select typoptions from pg_type_encoding where typid='money '::regtype;

ALTER TYPE cidr SET DEFAULT ENCODING (compresstype=zlib,compresslevel=9,blocksize=8192);

select typoptions from pg_type_encoding where typid='cidr '::regtype;

ALTER TYPE inet SET DEFAULT ENCODING (compresstype=zlib,compresslevel=9,blocksize=8192);

select typoptions from pg_type_encoding where typid='inet '::regtype;

ALTER TYPE time SET DEFAULT ENCODING (compresstype=zlib,compresslevel=9,blocksize=8192);

select typoptions from pg_type_encoding where typid='time '::regtype;

ALTER TYPE bit SET DEFAULT ENCODING (compresstype=zlib,compresslevel=9,blocksize=8192);

select typoptions from pg_type_encoding where typid='bit '::regtype;

ALTER TYPE int2 SET DEFAULT ENCODING (compresstype=zlib,compresslevel=9,blocksize=8192);

select typoptions from pg_type_encoding where typid='int2 '::regtype;

--
  -- Drop table if exists
 --
DROP TABLE if exists co_alter_type_zlib_8192_9;

DROP TABLE if exists co_alter_type_zlib_8192_9_uncompr; 

-- Create table 
CREATE TABLE co_alter_type_zlib_8192_9
	(id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd',a6 text,a7 timestamp,a8 character varying(705),a9 bigint,a10 date,a11 varchar(600),a12 text,a13 decimal,a14 real,a15 bigint,a16 int4 ,a17 bytea,a18 timestamp with time zone,a19 timetz,a20 path,a21 box,a22 macaddr,a23 interval,a24 character varying(800),a25 lseg,a26 point,a27 double precision,a28 circle,a29 int4,a30 numeric(8),a31 polygon,a32 date,a33 real,a34 money,a35 cidr,a36 inet,a37 time,a38 text,a39 bit,a40 bit varying(5),a41 smallint,a42 int) WITH (appendonly=true, orientation=column) distributed randomly;


--
  -- Insert data to the table
 --
COPY co_alter_type_zlib_8192_9(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42)  FROM '/data/sivand/test/tincrepo/private/sivand/mpp/gpdb/tests/storage/aoco_compression/data/copy_base_small' DELIMITER AS '|' ;

-- Create Uncompressed table of same schema definition
-- First Alter the data types back to compression=None
ALTER TYPE int4 SET DEFAULT ENCODING (compresstype=none,compresslevel=0);

select typoptions from pg_type_encoding where typid='int4
 '::regtype;

ALTER TYPE char SET DEFAULT ENCODING (compresstype=none,compresslevel=0);

select typoptions from pg_type_encoding where typid='char
 '::regtype;

ALTER TYPE numeric SET DEFAULT ENCODING (compresstype=none,compresslevel=0);

select typoptions from pg_type_encoding where typid='numeric
 '::regtype;

ALTER TYPE bool SET DEFAULT ENCODING (compresstype=none,compresslevel=0);

select typoptions from pg_type_encoding where typid='bool
 '::regtype;

ALTER TYPE text SET DEFAULT ENCODING (compresstype=none,compresslevel=0);

select typoptions from pg_type_encoding where typid='text
 '::regtype;

ALTER TYPE timestamp SET DEFAULT ENCODING (compresstype=none,compresslevel=0);

select typoptions from pg_type_encoding where typid='timestamp
 '::regtype;

ALTER TYPE int8 SET DEFAULT ENCODING (compresstype=none,compresslevel=0);

select typoptions from pg_type_encoding where typid='int8
 '::regtype;

ALTER TYPE date SET DEFAULT ENCODING (compresstype=none,compresslevel=0);

select typoptions from pg_type_encoding where typid='date
 '::regtype;

ALTER TYPE varchar SET DEFAULT ENCODING (compresstype=none,compresslevel=0);

select typoptions from pg_type_encoding where typid='varchar
 '::regtype;

ALTER TYPE numeric SET DEFAULT ENCODING (compresstype=none,compresslevel=0);

select typoptions from pg_type_encoding where typid='numeric
 '::regtype;

ALTER TYPE float4 SET DEFAULT ENCODING (compresstype=none,compresslevel=0);

select typoptions from pg_type_encoding where typid='float4
 '::regtype;

ALTER TYPE bytea SET DEFAULT ENCODING (compresstype=none,compresslevel=0);

select typoptions from pg_type_encoding where typid='bytea
 '::regtype;

ALTER TYPE timestamptz SET DEFAULT ENCODING (compresstype=none,compresslevel=0);

select typoptions from pg_type_encoding where typid='timestamptz
 '::regtype;

ALTER TYPE timetz SET DEFAULT ENCODING (compresstype=none,compresslevel=0);

select typoptions from pg_type_encoding where typid='timetz
 '::regtype;

ALTER TYPE path SET DEFAULT ENCODING (compresstype=none,compresslevel=0);

select typoptions from pg_type_encoding where typid='path
 '::regtype;

ALTER TYPE box SET DEFAULT ENCODING (compresstype=none,compresslevel=0);

select typoptions from pg_type_encoding where typid='box
 '::regtype;

ALTER TYPE macaddr SET DEFAULT ENCODING (compresstype=none,compresslevel=0);

select typoptions from pg_type_encoding where typid='macaddr
 '::regtype;

ALTER TYPE interval SET DEFAULT ENCODING (compresstype=none,compresslevel=0);

select typoptions from pg_type_encoding where typid='interval
 '::regtype;

ALTER TYPE lseg SET DEFAULT ENCODING (compresstype=none,compresslevel=0);

select typoptions from pg_type_encoding where typid='lseg
 '::regtype;

ALTER TYPE point SET DEFAULT ENCODING (compresstype=none,compresslevel=0);

select typoptions from pg_type_encoding where typid='point
 '::regtype;

ALTER TYPE float8 SET DEFAULT ENCODING (compresstype=none,compresslevel=0);

select typoptions from pg_type_encoding where typid='float8
 '::regtype;

ALTER TYPE circle SET DEFAULT ENCODING (compresstype=none,compresslevel=0);

select typoptions from pg_type_encoding where typid='circle
 '::regtype;

ALTER TYPE char SET DEFAULT ENCODING (compresstype=none,compresslevel=0);

select typoptions from pg_type_encoding where typid='char
 '::regtype;

ALTER TYPE polygon SET DEFAULT ENCODING (compresstype=none,compresslevel=0);

select typoptions from pg_type_encoding where typid='polygon
 '::regtype;

ALTER TYPE date SET DEFAULT ENCODING (compresstype=none,compresslevel=0);

select typoptions from pg_type_encoding where typid='date
 '::regtype;

ALTER TYPE varbit SET DEFAULT ENCODING (compresstype=none,compresslevel=0);

select typoptions from pg_type_encoding where typid='varbit
 '::regtype;

ALTER TYPE money SET DEFAULT ENCODING (compresstype=none,compresslevel=0);

select typoptions from pg_type_encoding where typid='money
 '::regtype;

ALTER TYPE cidr SET DEFAULT ENCODING (compresstype=none,compresslevel=0);

select typoptions from pg_type_encoding where typid='cidr
 '::regtype;

ALTER TYPE inet SET DEFAULT ENCODING (compresstype=none,compresslevel=0);

select typoptions from pg_type_encoding where typid='inet
 '::regtype;

ALTER TYPE time SET DEFAULT ENCODING (compresstype=none,compresslevel=0);

select typoptions from pg_type_encoding where typid='time
 '::regtype;

ALTER TYPE bit SET DEFAULT ENCODING (compresstype=none,compresslevel=0);

select typoptions from pg_type_encoding where typid='bit
 '::regtype;

ALTER TYPE int2 SET DEFAULT ENCODING (compresstype=none,compresslevel=0);

select typoptions from pg_type_encoding where typid='int2 '::regtype;

CREATE TABLE co_alter_type_zlib_8192_9_uncompr (id SERIAL,a1 int,a2 char(5),a3 numeric,a4 boolean DEFAULT false ,a5 char DEFAULT 'd',a6 text,a7 timestamp,a8 character varying(705),a9 bigint,a10 date,a11 varchar(600),a12 text,a13 decimal,a14 real,a15 bigint,a16 int4 ,a17 bytea,a18 timestamp with time zone,a19 timetz,a20 path,a21 box,a22 macaddr,a23 interval,a24 character varying(800),a25 lseg,a26 point,a27 double precision,a28 circle,a29 int4,a30 numeric(8),a31 polygon,a32 date,a33 real,a34 money,a35 cidr,a36 inet,a37 time,a38 text,a39 bit,a40 bit varying(5),a41 smallint,a42 int) WITH (appendonly=true, orientation=column) distributed randomly;

--
-- Insert to uncompressed table
 --
COPY co_alter_type_zlib_8192_9_uncompr(a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30,a31,a32,a33,a34,a35,a36,a37,a38,a39,a40,a41,a42)  FROM '/data/sivand/test/tincrepo/private/sivand/mpp/gpdb/tests/storage/aoco_compression/data/copy_base_small' DELIMITER AS '|' ;

\d+ co_alter_type_zlib_8192_9

--
-- Compression ratio
--
select 'compression_ratio' as compr_ratio, get_ao_compression_ratio('co_alter_type_zlib_8192_9'); 

--Select from pg_attribute_encoding to see the table entry 
select attrelid::regclass as relname, attnum, attoptions from pg_class c, pg_attribute_encoding e  where c.relname = 'co_alter_type_zlib_8192_9' and c.oid=e.attrelid  order by relname, attnum limit 3; 
--
-- Compare data with uncompressed table
--
--
-- Select number of rows from the uncompressed table 
--
SELECT count(*) as count_uncompressed from  co_alter_type_zlib_8192_9_uncompr ;
--
-- Select number of rows from the compressed table 
--
SELECT count(*) as count_compressed from  co_alter_type_zlib_8192_9;
--
-- Select number of rows using a FULL outer join on all the columns of the two tables 
-- Count should match with above result if the all the rows uncompressed correctly: 
--
Select count(*) as count_join from co_alter_type_zlib_8192_9 t1 full outer join co_alter_type_zlib_8192_9_uncompr t2 on t1.id=t2.id and t1.a1=t2.a1 and t1.a2=t2.a2 and t1.a3=t2.a3 and t1.a4=t2.a4 and t1.a5=t2.a5 and t1.a6=t2.a6 and t1.a7=t2.a7 and t1.a8=t2.a8 and t1.a9=t2.a9 and t1.a10=t2.a10 and t1.a11=t2.a11 and t1.a12=t2.a12 and t1.a13=t2.a13 and t1.a14=t2.a14 and t1.a15=t2.a15 and t1.a16=t2.a16 and t1.a17=t2.a17 and t1.a18=t2.a18 and t1.a19=t2.a19 and t1.a22=t2.a22 and t1.a23=t2.a23 and t1.a24=t2.a24 and t1.a27=t2.a27 and t1.a29=t2.a29 and t1.a30=t2.a30 and t1.a32=t2.a32 and t1.a33=t2.a33 and t1.a34=t2.a34 and t1.a35=t2.a35 and t1.a36=t2.a36 and t1.a37=t2.a37 and t1.a38=t2.a38 and t1.a39=t2.a39 and t1.a40=t2.a40 and t1.a41=t2.a41 and t1.a42=t2.a42 ;
--
-- Truncate the table 
--
TRUNCATE table co_alter_type_zlib_8192_9;
--
-- Insert data again 
--
insert into co_alter_type_zlib_8192_9 select * from co_alter_type_zlib_8192_9_uncompr order by a1;

--
-- Compression ratio
--
select 'compression_ratio' as compr_ratio ,get_ao_compression_ratio('co_alter_type_zlib_8192_9'); 

--Alter table drop a column 
Alter table co_alter_type_zlib_8192_9 Drop column a12; 
--Alter table add a column 
Alter table co_alter_type_zlib_8192_9 Add column a12 text default 'default value' encoding (compresstype=rle_type,compresslevel=1,blocksize=32768); 
