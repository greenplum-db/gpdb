-- 
-- @created 2014-10-9 12:00:00
-- @modified 2014-10-9 12:00:00
-- @tags storage
-- @description Test for checksum GUC on/off

-- Tests with checksum ON
-- AO table
DROP TABLE IF EXISTS checksum_guc1;
CREATE TABLE checksum_guc1 (i int ) with (appendonly=true);
SELECT count(*) as one from pg_appendonly where checksum=True and relid = (select oid from pg_class where relname = 'checksum_guc1');

-- Partition CO table
DROP TABLE IF EXISTS checksum_guc_part2;
CREATE TABLE checksum_guc_part2 (
    c_custkey integer,
    c_name character varying(25),
    c_comment text, 
    c_rating float,
    c_phone character(15),
    c_acctbal numeric(15,2),
    c_date date,
    c_timestamp timestamp 
)
WITH (appendonly=true, orientation=column, compresstype=quicklz, compresslevel=1) DISTRIBUTED BY (c_custkey)
partition by range(c_custkey)  subpartition by range( c_rating) 
subpartition template ( default subpartition subothers,start (0.0) end(1.9) every (2.0) ) 
(default partition others, partition p1 start(1) end(5000), partition p2 start(5000) end(10000), partition p3 start(10000) end(15000));
SELECT count(*) as one from pg_appendonly where checksum=True and relid = (select oid from pg_class where relname = 'checksum_guc_part2');


-- Partition AO table
DROP TABLE IF EXISTS checksum_guc_part3;
CREATE TABLE checksum_guc_part3 (
    c_custkey integer,
    c_name character varying(25),
    c_comment text, 
    c_rating float,
    c_phone character(15),
    c_acctbal numeric(15,2),
    c_date date,
    c_timestamp timestamp 
)
WITH (appendonly=true, compresstype=quicklz, compresslevel=1) DISTRIBUTED BY (c_custkey)
partition by range(c_custkey)  subpartition by range( c_rating) 
subpartition template ( default subpartition subothers,start (0.0) end(1.9) every (2.0) ) 
(default partition others, partition p1 start(1) end(5000), partition p2 start(5000) end(10000), partition p3 start(10000) end(15000));
SELECT count(*) as one from pg_appendonly where checksum=True and relid = (select oid from pg_class where relname = 'checksum_guc_part3');

-- CO table
DROP TABLE IF EXISTS checksum_guc4;
CREATE TABLE checksum_guc4 (i int ) with (appendonly=true);
SELECT count(*) as one from pg_appendonly where checksum=True and relid = (select oid from pg_class where relname = 'checksum_guc4');

-- Tests with checksum OFF
SET gp_default_storage_options = 'checksum=false';
-- AO table
DROP TABLE IF EXISTS checksum_guc1;
CREATE TABLE checksum_guc1 (i int ) with (appendonly=true);
SELECT count(*)  as zero from pg_appendonly where checksum=True and relid = (select oid from pg_class where relname = 'checksum_guc1');

-- Partition CO table
DROP TABLE IF EXISTS checksum_guc_part2;
CREATE TABLE checksum_guc_part2 (
    c_custkey integer,
    c_name character varying(25),
    c_comment text, 
    c_rating float,
    c_phone character(15),
    c_acctbal numeric(15,2),
    c_date date,
    c_timestamp timestamp 
)
WITH (appendonly=true, orientation=column, compresstype=quicklz, compresslevel=1) DISTRIBUTED BY (c_custkey)
partition by range(c_custkey)  subpartition by range( c_rating) 
subpartition template ( default subpartition subothers,start (0.0) end(1.9) every (2.0) ) 
(default partition others, partition p1 start(1) end(5000), partition p2 start(5000) end(10000), partition p3 start(10000) end(15000));
SELECT count(*) as zero  from pg_appendonly where checksum=True and relid = (select oid from pg_class where relname = 'checksum_guc_part2');


-- Partition AO table
DROP TABLE IF EXISTS checksum_guc_part3;
CREATE TABLE checksum_guc_part3 (
    c_custkey integer,
    c_name character varying(25),
    c_comment text, 
    c_rating float,
    c_phone character(15),
    c_acctbal numeric(15,2),
    c_date date,
    c_timestamp timestamp 
)
WITH (appendonly=true compresstype=quicklz, compresslevel=1) DISTRIBUTED BY (c_custkey)
partition by range(c_custkey)  subpartition by range( c_rating) 
subpartition template ( default subpartition subothers,start (0.0) end(1.9) every (2.0) ) 
(default partition others, partition p1 start(1) end(5000), partition p2 start(5000) end(10000), partition p3 start(10000) end(15000));
SELECT count(*)  as zero from pg_appendonly where checksum=True and relid = (select oid from pg_class where relname = 'checksum_guc_part3');

-- CO table
DROP TABLE IF EXISTS checksum_guc4;
CREATE TABLE checksum_guc4 (i int ) with (appendonly=true);
SELECT count(*)  as zero from pg_appendonly where checksum=True and relid = (select oid from pg_class where relname = 'checksum_guc4');

