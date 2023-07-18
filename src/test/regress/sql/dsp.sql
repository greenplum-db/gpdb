-- Test suite for appendonly default storage parameters.
--
-- Scope: database, role and session level defaults.
drop database if exists dsp1;
create database dsp1;
alter database dsp1 set default_table_access_method = ao_column;
drop database if exists dsp2;
create database dsp2;
alter database dsp2 set default_table_access_method = ao_row;
alter database dsp2 set gp_default_storage_options = "checksum=true";

-- Testing pg_authid.rolconfig is currently deferred in
-- installcheck-good because it becomes contrived.  These tests will
-- be covered in detail in Divya's test plan for default storage
-- options.
--
-- drop role if exists dsprole1;
-- create role dsprole1 with login;
-- drop role if exists dsprole2;
-- create role dsprole2 with login;
-- -- Allow all users to login from local host.
-- \! cp $(psql -d postgres -t -c "show data_directory")/pg_hba.conf /tmp/
-- \! echo "local    all    all    trust" >> /tmp/pg_hba.conf
-- \! cp /tmp/pg_hba.conf $(psql -d postgres -t -c "show data_directory")/pg_hba.conf
-- \! gpstop -qau

-- alter role dsprole1 set gp_default_storage_options to
-- 	"blocksize=8192";
-- alter role dsprole2 set gp_default_storage_options to
-- 	"compresslevel=2";
--
-- \c dsp1 dsprole2
--
-- Leaving roles around affects others, e.g. auth_constraints.sql.
-- Therefore drop them in the end.

\c dsp1
show default_table_access_method;
show gp_default_storage_options;
create table t1 (a int, b int) distributed by (a);
\d+ t1
SELECT am.amname FROM pg_class c LEFT JOIN pg_am am ON (c.relam = am.oid) WHERE c.relname = 't1';
insert into t1 select i, i from generate_series(1,5)i;
update t1 set b = 50 where a < 50;

set default_table_access_method = ao_column;
set gp_default_storage_options = "blocksize=8192";
show default_table_access_method;
show gp_default_storage_options;

create table t2 (a int, b varchar, c text) distributed by (a);
\d+ t2
SELECT am.amname FROM pg_class c LEFT JOIN pg_am am ON (c.relam = am.oid) WHERE c.relname = 't2';
create table t3 (a int, b float) with (appendonly=false)
	distributed by (a);
\d+ t3
SELECT am.amname FROM pg_class c LEFT JOIN pg_am am ON (c.relam = am.oid) WHERE c.relname = 't3';
create table t4 (a int, b float, c text) with
	(appendonly=true,orientation=row) distributed by (a);
-- Set default access method to heap, verify basic operations.
set default_table_access_method="heap";
show gp_default_storage_options;
create table h1 (a int, b int) distributed by (a);
create index ih1 on h1(a);
insert into h1 select i, i*2 from generate_series(1,50)i;
set enable_seqscan=off;
show enable_seqscan;
select * from h1 where a > 5 and a < 10 order by a,b;

select c.relname, am.amname, c.relkind, c.reloptions
	from pg_class c left join pg_am am on (c.relam = am.oid)
	where c.relkind='r' and c.relnamespace=2200 order by relname;

\c dsp2
set default_table_access_method = ao_row;
set gp_default_storage_options = "compresslevel=2,checksum=true";
show default_table_access_method;
show gp_default_storage_options;
create table t1 (a int, b int) distributed by (a);
\d+ t1
SELECT am.amname FROM pg_class c LEFT JOIN pg_am am ON (c.relam = am.oid) WHERE c.relname = 't1';
-- should fail because current default access method is not aoco
create table t2 (
	a int encoding(blocksize=65536),
	b float encoding(compresstype=zlib),
	c int encoding(compresstype=rle_type, compresslevel=2),
	d text
) distributed by (a);
-- should succeed
create table t2 (
	a int encoding(blocksize=65536),
	b float encoding(compresstype=zlib),
	c int encoding(compresstype=rle_type, compresslevel=2),
	d text
) with (appendonly=true,orientation=column) distributed by (a);
\d+ t2
SELECT am.amname FROM pg_class c LEFT JOIN pg_am am ON (c.relam = am.oid) WHERE c.relname = 't2';
insert into t2 select i, 71/i, 2*i, 'abc'||2*i from generate_series(1,5)i;
select * from t2 order by 1;
select attrelid::regclass, attnum, filenum, attoptions
	from pg_attribute_encoding order by 1,2;
-- SET operation in a session has higher precedence.  Also test if
-- compresstype is correctly inferred based on compress level.
set default_table_access_method = ao_row;
set gp_default_storage_options = "compresslevel=4,checksum=false";
show default_table_access_method;
show gp_default_storage_options;
create table t3 (a int, b int, c text) distributed by (a);
insert into t3 select i, i, 'abc' from generate_series(1,5)i;
select * from t3 order by 1;
select c.relname, am.amname, c.relkind, c.reloptions
	from pg_class c left join pg_am am on (c.relam = am.oid)
    where c.relname in ('t1', 't2', 't3') order by 1;

-- The GUC has checksum=false. Test that you can still turn it in WITH, and
-- that it affects partitions and subpartitions, too.
Create table alter_part_tab1 (id SERIAL,a1 int ,a2 char(5) ,a3 text )
WITH (appendonly=true, orientation=column, checksum=true,compresstype=zlib)
partition by list(a2) subpartition by range(a1)
(partition p1 values('val1')
  (default subpartition subothers,
   subpartition sp1 start(1) end(9) with (appendonly=true,orientation=column,checksum=true,compresstype=rle_type),
   subpartition sp2 start(11) end(20) with(appendonly=true,orientation=column,checksum=true,compresstype=none)),
 partition p2 values('val2')
   (default subpartition subothers,
   subpartition sp1 start(1) end(9) with (appendonly=true,orientation=column,checksum=true,compresstype=rle_type),
   subpartition sp2 start(11) end(20) with(appendonly=true,orientation=column,checksum=true,compresstype=none))
);

Insert into alter_part_tab1(a1,a2,a3)
  select g, 'val1', 'Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.' from generate_series(1,10) g;
Insert into alter_part_tab1(a1,a2,a3)
  select g, 'val1', 'Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.' from generate_series(11, 20) g;
select count(*) from alter_part_tab1;

-- Make sure that adding a column doesn't affect checksums.
alter table alter_part_tab1 add column a4 numeric default 5.5;

-- Check that it works for ADD PARTITION, too.
ALTER TABLE alter_part_tab1 ADD partition p31 values(1) WITH (appendonly=true, orientation=column, checksum=true,compresstype=zlib)
(default subpartition subothers,
   subpartition sp1 start(1) end(9) with (appendonly=true,orientation=column,checksum=true,compresstype=rle_type),
   subpartition sp2 start(11) end(20) with(appendonly=true,orientation=column,checksum=true,compresstype=none));

SELECT relname, reloptions from pg_class c where relname like 'alter_part_tab1%';

drop table alter_part_tab1;

-- attribute encoding tests for column oriented tables
set default_table_access_method = ao_column;
set gp_default_storage_options = "compresslevel=1";
show default_table_access_method;
show gp_default_storage_options;
create table co1 (a int, b int) distributed by (a);
create table co2 (a int, b int) with (blocksize=8192)
	distributed by (a);
create table co3 (
	a int encoding(blocksize=65536),
	b float encoding(compresstype=zlib,compresslevel=6),
	c char encoding(compresstype=rle_type))
	distributed by(a);
create table co4 (a int, b int, c varchar,
	default column encoding (compresslevel=5))
	distributed by (a);
create table co5 (a int, b int, c varchar encoding(compresslevel=0),
	default column encoding (compresslevel=5))
	distributed by (a);
create table co6(a char, b bytea, c float, d int,
	default column encoding (compresstype=RLE_TYPE))
	with (checksum=true) distributed by (a);
create table co7(
	a int encoding(blocksize=8192,compresstype=none),
	b varchar encoding(compresstype=zlib,compresslevel=6),
	c char encoding(compresstype=rle_type))
	with (checksum=false) distributed by (a);
select attrelid::regclass,attnum,filenum,attoptions
	from pg_attribute_encoding order by 1,2;
drop database if exists dsp3;
create database dsp3;
\c dsp3
set default_table_access_method = ao_column;
set gp_default_storage_options = "compresslevel=0";
show default_table_access_method;
show gp_default_storage_options;
create table co1(
	a int encoding (compresstype=rle_type),
	b float encoding (compresslevel=5),
	c char encoding (compresslevel=0,blocksize=65536),
	d float) distributed by (a);
set gp_default_storage_options= "compresslevel=3";
show gp_default_storage_options;
create table co2(
	a int encoding (compresstype=rle_type),
	b float encoding (compresslevel=5),
	c char encoding (compresslevel=0,blocksize=65536),
	d float,
	default column encoding (compresstype=none))
	distributed by (a);
-- attribute encoding should be set correctly.
set default_table_access_method = "heap";
set gp_default_storage_options = "blocksize=32768,compresstype=none,checksum=true";
show default_table_access_method;
show gp_default_storage_options;
create table co3 (a int, b float) with (appendonly=true, orientation=column) distributed by (a);
create table co4 (a int encoding (blocksize=8192), b int) distributed by (a);
set default_table_access_method = ao_row;
set gp_default_storage_options = "blocksize=32768,compresstype=none,checksum=true";
create table co5 (a int encoding (blocksize=8192), b float)
	with (appendonly=true,orientation=column, checksum=false) distributed by (a);
select c.relname, am.amname, c.relkind, c.reloptions from pg_class c left join pg_am am on (c.relam=am.oid)
where relname in ('co1', 'co2', 'co3', 'co5') order by 1;
select attrelid::regclass,attnum,filenum,attoptions
	from pg_attribute_encoding order by 1,2;

-- misc tests
alter database dsp1 set default_table_access_method = "heap";

\c dsp1

show default_table_access_method;
show gp_default_storage_options;

\c dsp3

alter database dsp1 set default_table_access_method = ao_row;
alter database dsp1 set gp_default_storage_options = "compresstype=zlib";

\c dsp1

set default_table_access_method = ao_column;
show gp_default_storage_options;
set gp_default_storage_options = "compresstype=rle_type";
show gp_default_storage_options;
set gp_default_storage_options = "compresslevel=0";
show gp_default_storage_options;
set gp_default_storage_options = "checksum=false";
show gp_default_storage_options;

\c dsp3

set default_table_access_method = ao_column;
set gp_default_storage_options = "compresslevel=5";
show gp_default_storage_options;
-- negative tests - should fail due to invalid combinations of
-- compresslevel and compresstype.
create table co6(
	a int encoding (compresstype=rle_type),
	b float encoding (blocksize=8192))
	distributed by (a);
create table co7(a int, b float,
	default column encoding (compresstype=RLE_TYPE, compresslevel=7))
	distributed by (a);
-- negative tests - session level set
set gp_default_storage_options = "compresstype=zlib,compresslevel=11";
set gp_default_storage_options = "compresslevel=5,compresstype=RLE_TYPE";
set gp_default_storage_options = "compresslevel=1,compresstype=rle";
set gp_default_storage_options = "checksum=1234";
set gp_default_storage_options = "blocksize=true";
set gp_default_storage_options = 'blocksize=8192,checksumblah=true';
show gp_default_storage_options;
-- negative tests - database level
alter database dsp1 set gp_default_storage_options = "compresslevel=-12";
alter database dsp1 set gp_default_storage_options = "checksum=invalid";
alter database dsp1 set	gp_default_storage_options = "compresstype=zlib,compresslevel=0";

-- set_config() tests
select pg_catalog.set_config('default_table_access_method', 'ao_column', false);
show default_table_access_method;
select pg_catalog.set_config('gp_default_storage_options', 'compresstype=none', false);
show gp_default_storage_options;
create table co8 (a int, b float) distributed by (a);
\d+ co8
SELECT am.amname FROM pg_class c LEFT JOIN pg_am am ON (c.relam = am.oid) WHERE c.relname = 'co8';
-- ensure that set_config() has propagated to segments by insert and
-- select into a column oriented table.
insert into co8 select i, i/2 from generate_series(1,10)i;
select count(b) from co8;

select pg_catalog.set_config('gp_default_storage_options', 'compresstype=rle_type,compresslevel=1', false);
show gp_default_storage_options;
begin;
select pg_catalog.set_config('gp_default_storage_options', 'compresslevel=2', true);
show gp_default_storage_options;
create table ao1 (a int, b int) with (appendonly=true) distributed by (a);
\d+ ao1;
SELECT am.amname FROM pg_class c LEFT JOIN pg_am am ON (c.relam = am.oid) WHERE c.relname = 'ao1';
insert into ao1 select i, i from generate_series(1,100)i;
select count(*) from ao1;
end;
-- GUC should be reset after end of transaction.
show gp_default_storage_options;
-- verify reset happendon on segments by create, insert, select.
create table co9 (x int, y float) distributed by (x);
\d+ co9
SELECT am.amname FROM pg_class c LEFT JOIN pg_am am ON (c.relam = am.oid) WHERE c.relname = 'co9';
-- ensure that set_config() has propagated to segments by insert and
-- select into a column oriented table.
insert into co9 select i, i/2 from generate_series(1,10)i;
select count(y) from co9;

set gp_default_storage_options='compresstype=none';
show gp_default_storage_options;
-- compresstype should be correctly inferred.
create table ao2 (a int, b int) with
    (appendonly=true, compresslevel=4) distributed by (a);
\d ao2
-- MPP-25073 verify if compresstype=none is represented as NULL.
set gp_default_storage_options='';
create table ao4 (a int, b int) with (appendonly=true)
    distributed by (a);
\d ao4
select amname, reloptions from pg_class left join pg_am on pg_am.oid = relam where relname = 'ao4';
set default_table_access_method = ao_row;
set gp_default_storage_options = "compresstype=NONE";
create table co10 (a int, b int, c int) with (appendonly=true,orientation=column)
    distributed by (a);
select attnum,filenum,attoptions from pg_attribute_encoding
    where attrelid = 'co10'::regclass order by attnum;

-- compression disabled by default, enable in WITH clause
set default_table_access_method = ao_row;
set gp_default_storage_options = "blocksize=32768,compresstype=none,checksum=true";
show gp_default_storage_options;
-- compresstype only
create table ao5 (a int, b int) with (compresstype=zlib)
    distributed by (a);
\d ao5
insert into ao5 select i, i from generate_series(1,10)i;
-- compresslevel only
create table ao6 with (compresslevel=4) as select * from ao5
    distributed by (a);
\d ao6
-- both compresstype and compresslevel
create table ao7 with (compresstype=zlib, compresslevel=3) as
    select * from ao6 distributed by (a);
\d ao7

-- compression enabled by default, disable in WITH clause
set gp_default_storage_options = "compresstype=zlib";
set default_table_access_method = ao_row;
show gp_default_storage_options;
-- compresstype only
create table ao8 with (compresstype=none) as select * from ao7
    distributed by (a);
\d ao8
-- compresslevel only (should fail)
create table ao9 with (compresslevel=0) as select * from ao8
    distributed by (a);
create table ao10 with (compresslevel=0, compresstype=none)
    as select * from ao8 distributed by (a);

-- MPP-14504: we need to allow compresstype=none with compresslevel>0
create table ao11 (a int, b int) with (compresstype=none, compresslevel=2)
    distributed by (a);
\d ao11
set gp_default_storage_options='compresstype=none,compresslevel=2';
show gp_default_storage_options;
create table ao12 (a int, b int) with (appendonly=true) distributed by (a);
\d ao12

--bitmap index
set default_table_access_method = ao_row;
set gp_default_storage_options = "blocksize=32768,compresstype=none,checksum=true";
create table bitmap_table(a int, b int, c varchar);
create index bitmap_i on bitmap_table using bitmap(b);

-- external tables: ensure that default access methods are ignored
-- during external table creation.
set default_table_access_method = ao_row;
set gp_default_storage_options = "blocksize=32768,compresstype=none,checksum=true";
create external table ext_t1 (a int, b int)
    location ('file:///tmp/test.txt') format 'text';
\d+ ext_t1
SELECT am.amname FROM pg_class c LEFT JOIN pg_am am ON (c.relam = am.oid) WHERE c.relname = 'ext_t1';
create external table ext_error_logging_off (a int, b int)
    location ('file:///tmp/test.txt') format 'text'
    segment reject limit 100;
\d+ ext_error_logging_off
SELECT am.amname FROM pg_class c LEFT JOIN pg_am am ON (c.relam = am.oid) WHERE c.relname = 'ext_error_logging_off';
create external table ext_t2 (a int, b int)
    location ('file:///tmp/test.txt') format 'text'
    log errors segment reject limit 100;
\d+ ext_t2
SELECT am.amname FROM pg_class c LEFT JOIN pg_am am ON (c.relam = am.oid) WHERE c.relname = 'ext_t2';

create external table ext_t3 (a int, b int)
    location ('file:///tmp/test.txt') format 'text'
    log errors persistently segment reject limit 100;
\d+ ext_t3
SELECT am.amname FROM pg_class c LEFT JOIN pg_am am ON (c.relam = am.oid) WHERE c.relname = 'ext_t3';
-- Make sure gp_default_storage_options GUC value is set in newly created cdbgangs
-- after previous idle cdbgang is stopped
SET gp_vmem_idle_resource_timeout=30;
set default_table_access_method = ao_row;
SET gp_default_storage_options = "blocksize=32768,compresstype=none,checksum=false";
\! sleep 1
CREATE TABLE check_guc_value_after_new_cdbgang (a int) DISTRIBUTED RANDOMLY;
SELECT DISTINCT relid::regclass FROM pg_appendonly WHERE relid='check_guc_value_after_new_cdbgang'::regclass;
SELECT DISTINCT relid::regclass FROM gp_dist_random('pg_appendonly') WHERE relid='check_guc_value_after_new_cdbgang'::regclass;
SELECT DISTINCT relname, reloptions FROM pg_class WHERE relname='check_guc_value_after_new_cdbgang';
SELECT DISTINCT relname, reloptions FROM gp_dist_random('pg_class') WHERE relname='check_guc_value_after_new_cdbgang';
RESET gp_vmem_idle_resource_timeout;

-- Make sure ALTER TABLE REORGANIZE does not change the table type due
-- to gp_default_storage_options GUC
set default_table_access_method = ao_row;
set gp_default_storage_options = "blocksize=32768";
create table alter_table_reorg_ao (a int, b text);
select c.relname, am.amname, c.relkind, c.reloptions
	from pg_class c left join pg_am am on (c.relam = am.oid)
    where c.relname = 'alter_table_reorg_ao';
set default_table_access_method = ao_column;
set gp_default_storage_options = "blocksize=32768";
alter table alter_table_reorg_ao set with (reorganize=true);
select c.relname, am.amname, c.relkind, c.reloptions
	from pg_class c left join pg_am am on (c.relam = am.oid)
    where c.relname = 'alter_table_reorg_ao';

set default_table_access_method = ao_column;
set gp_default_storage_options = "blocksize=32768";
create table alter_table_reorg_aoco (a int, b text);
select c.relname, am.amname, c.relkind, c.reloptions
	from pg_class c left join pg_am am on (c.relam = am.oid)
    where c.relname = 'alter_table_reorg_aoco';
set default_table_access_method = "heap";
set gp_default_storage_options = "blocksize=32768";
alter table alter_table_reorg_aoco set with (reorganize=true);
select c.relname, am.amname, c.relkind, c.reloptions
	from pg_class c left join pg_am am on (c.relam = am.oid)
    where c.relname = 'alter_table_reorg_aoco';

-- Make sure SELECT INTO uses gp_default_storage_options GUC
create table select_into_heap_from (a int, b text);
insert into select_into_heap_from select i, 'aaa' from generate_series(1,50)i;
set default_table_access_method = ao_column;
set gp_default_storage_options = "blocksize=32768";
select * into select_into_heap_to from select_into_heap_from;
select c.relname, am.amname, c.relkind, c.reloptions
	from pg_class c left join pg_am am on (c.relam = am.oid)
    where c.relname = 'select_into_heap_to';
select count(*) from select_into_heap_to;
-- Also check CTAS works fine
create table ctas_ao_from_heap as select * from select_into_heap_from;
select c.relname, am.amname, c.relkind, c.reloptions
	from pg_class c left join pg_am am on (c.relam = am.oid)
    where c.relname = 'ctas_ao_from_heap';
select count(*) from ctas_ao_from_heap;

RESET gp_default_storage_options;
create table dsp_partition1(i int, j int, k int) with(appendonly=true) partition by range(i) (start(1) end(10) every(5));
Insert into dsp_partition1 select i, i+1, i+2 from generate_series(1,9) i;
select count(*) from dsp_partition1;
select c.relname, am.amname, c.relkind, c.reloptions
	from pg_class c left join pg_am am on (c.relam = am.oid)
    where c.relname like 'dsp_partition1%' order by relname;
-- The child partition tables should default to their parent table's AM.
set default_table_access_method = ao_column;
set gp_default_storage_options = "blocksize=32768,compresstype=none,checksum=true";
-- Add partition
alter table dsp_partition1 add partition p3 start(11) end(15);
alter table dsp_partition1 add partition p4 start(16) end(18) with(appendonly=false);
select c.relname, am.amname, c.relkind, c.reloptions
	from pg_class c left join pg_am am on (c.relam = am.oid)
    where c.relname like 'dsp_partition1%' order by relname;
-- Split partition
alter table dsp_partition1 split partition for (6) at (7) into (partition split_p1, partition split_p2);
select c.relname, am.amname, c.relkind, c.reloptions
	from pg_class c left join pg_am am on (c.relam = am.oid)
    where c.relname like 'dsp_partition1%' order by relname;

RESET gp_default_storage_options;

-- cleanup
\c postgres

-- Test that gp_default_storage_options option must be the same on all nodes
-- NOTE: although gpconfig might have unittest to verity this, the GUC
-- gp_default_storage_options is so important that adding tests for it here
-- do no harm.

-- start_matchsubs
-- m/.*\[ERROR\]*/
-- s/.*\[ERROR\]/[ERROR]/gm
-- end_matchsubs
\!gpconfig -c gp_default_storage_options -v 'checksum=false' --masteronly
\!gpconfig -c gp_default_storage_options -v 'checksum=false' -m 'checksum=true'

\!gpconfig -s gp_default_storage_options
