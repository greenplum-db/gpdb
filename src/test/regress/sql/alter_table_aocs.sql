set optimizer_print_missing_stats = off;
--
-- This test case covers ALTER functionality for AOCS relations.
--

--
-- Switching on these gucs may be helpful in the event of failures.
--
-- set Debug_appendonly_print_storage_headers=true;
-- set Debug_appendonly_print_datumstream=true;
--

drop schema if exists aocs_addcol cascade;
create schema aocs_addcol;
set search_path=aocs_addcol,public;

create table addcol1 (a int) with (appendonly=true, orientation = column)
   distributed by (a);
-- create three varblocks
insert into addcol1 select i from generate_series(-10,5)i;
insert into addcol1 select i from generate_series(6,15)i;
insert into addcol1 select i from generate_series(21,30)i;
select count(*) from addcol1;

-- basic scenario with small content vablocks in new as well as existing column.
alter table addcol1
   add column b varchar default 'I am in a small content varblock';

-- verification on master catalog
-- Moreover, gp_toolkit schema is not populated in regression database
-- select segno,column_num,physical_segno,tupcount,modcount,state
--    from gp_toolkit.__gp_aocsseg(aocs_oid('addcol1')) order by segno,column_num;

-- select after alter
select b from addcol1 where a < 8 and a > 2;

-- update and delete post alter should work
update addcol1 set b = 'new value' where a < 10 and a > 0;
select * from addcol1 where a < 8 and a > 3;
delete from addcol1 where a > 25 or a < -5;
select count(*) from addcol1;
-- vacuum creates a new appendonly segment, leaving the original
-- segment active with eof=0.
vacuum addcol1;
-- alter table with one empty and one non-empty appendonly segment.
alter table addcol1 add column c float default 1.2;
select * from addcol1 where a < 8 and a > 3;

-- insert should result in two appendonly segments, each having eof > 0.
insert into addcol1
   select i, i::text, i*22/7::float
   from generate_series(31,40)i;
-- alter table with more than one non-empty appendonly segments.
alter table addcol1 add column d int default 20;
select a,c,d from addcol1 where a > 9 and a < 15 order by a;

-- try inserting after alter
insert into addcol1 select i, 'abc', 22*i/7, -i from generate_series(1,10)i;

-- add columns with compression (dense and bulk dense content varblocks)
alter table addcol1
   add column e float default to_char((22/7::float), '9.99999999999999')::float encoding (compresstype=RLE_TYPE),
   add column f int default 20 encoding (compresstype=zlib);
select * from addcol1 where a < 2 and a > -4 order by a,c;
select a,f from addcol1 where a > 20 and a < 25 order by a,c;

-- add column with existing compressed column (dense content)
create table addcol2 (a int encoding (compresstype=zlib))
   with (appendonly=true, orientation=column)
   distributed by (a);
insert into addcol2 select i/17 from generate_series(-10000,10000)i;
insert into addcol2 select i from generate_series(10001, 50000)i;

alter table addcol2 add column b varchar
   default 'hope I end up on a magnetic disk some day'
   encoding (compresstype=RLE_TYPE, blocksize=8192);

-- select after add column
select * from addcol2 where a > 9995 and a < 10006 order by a;

-- add column with existing RLE compressed column (bulk dense content)
create table addcol3 (a int encoding (compresstype=RLE_TYPE, compresslevel=2))
   with (appendonly=true, orientation=column)
   distributed by (a);
insert into addcol3 select 10 from generate_series(1, 30000);
insert into addcol3 select -10 from generate_series(1, 20000);
insert into addcol3 select
   case when i < 100000 then 1
   	    when i >= 100000 and i < 500000 then 2
		when i >=500000 and i < 1000000 then 3
   end
   from generate_series(-1000,999999)i;

alter table addcol3 add column b float
   default 22/7::float encoding (compresstype=RLE_TYPE, compresslevel=2);

-- add column with null default
alter table addcol3 add column c varchar default null;

select count(b) from addcol3;
select count(c) from addcol3;

-- verification on master catalog
-- select segno,column_num,physical_segno,tupcount,modcount,state
--    from gp_toolkit.__gp_aocsseg(aocs_oid('addcol3')) order by segno,column_num;

-- insert after add column with null default
insert into addcol3 select i, 22*i/7, 'a non-null value'
   from generate_series(1,100)i;

select count(*) from addcol3;

-- verification on master catalog
-- select segno,column_num,physical_segno,tupcount,modcount,state
--    from gp_toolkit.__gp_aocsseg(aocs_oid('addcol3')) order by segno,column_num;

-- start with a new table, with two varblocks
create table addcol4 (a int, b float)
   with (appendonly=true, orientation=column)
   distributed by (a);
insert into addcol4 select i, 31/i from generate_series(1, 20)i;
insert into addcol4 select -i, 37/i from generate_series(1, 20)i;
select count(*) from addcol4;

-- multiple alter subcommands (add column, drop column)
alter table addcol4
   add column c varchar default null encoding (compresstype=zlib),
   drop column b,
   add column d date default date('2014-05-01')
      encoding (compresstype=RLE_TYPE, compresslevel=2);

select * from addcol4 where a > 5 and a < 10 order by a;

-- verification on master catalog
-- select segno, column_num, physical_segno, tupcount, modcount, state
--    from gp_toolkit.__gp_aocsseg(aocs_oid('addcol4')) order by segno,column_num;

-- TODO: multiple subcommands (add column, add constraint, alter type)

-- block directory
create index i4a on addcol4 (a);

alter table addcol4
   add column e varchar default 'wow' encoding (compresstype=zlib);

-- enforce index scan so that block directory is used
set enable_seqscan=off;

-- index scan after adding new column
select * from addcol4 where a > 5 and a < 10 order by a;

create table addcol5 (a int, b float)
   with (appendonly=true, orientation=column)
   distributed by (a);
create index i5a on addcol5(a);
insert into addcol5
   select i, 22*i/7 from generate_series(-10,10)i;
insert into addcol5
   select i, 22*i/7 from generate_series(11,20)i;
insert into addcol5
   select i, 22*i/7 from generate_series(21,30)i;

alter table addcol5 add column c int default 1;

-- insert after adding new column
insert into addcol5
   select i, 22*i/7, 311/i from generate_series(31,35)i;

-- index scan after adding new column
set enable_seqscan=off;
select * from addcol5 where a > 25 order by a,b;

-- firstRowNum of the first block starts with a value greater than 1
-- (first insert was aborted).

create table addcol6 (a int, b int)
   with (appendonly=true, orientation=column) distributed by (a);

begin;
insert into addcol6 select i,i from generate_series(1,10)i;
-- abort the first insert, still should advance gp_fastsequence for this
-- relation.
SELECT objmod, last_sequence, gp_segment_id from gp_dist_random('gp_fastsequence') WHERE objid
IN (SELECT segrelid FROM pg_appendonly WHERE relid IN (SELECT oid FROM pg_class
WHERE relname='addcol6'));
abort;

-- check gp_fastsequence remains advanced.
SELECT objmod, last_sequence, gp_segment_id from gp_dist_random('gp_fastsequence') WHERE objid
IN (SELECT segrelid FROM pg_appendonly WHERE relid IN (SELECT oid FROM pg_class
WHERE relname='addcol6'));

insert into addcol6 select i,i/2 from generate_series(1,20)i;
alter table addcol6 add column c float default 1.2;
select a,c from addcol6 where b > 5 order by a;

-- Lets validate after alter gp_fastsequence reflects correctly.
SELECT objmod, last_sequence, gp_segment_id from gp_dist_random('gp_fastsequence') WHERE objid
IN (SELECT segrelid FROM pg_appendonly WHERE relid IN (SELECT oid FROM pg_class
WHERE relname='addcol6'));

-- add column with default value as sequence
alter table addcol6 add column d serial;

-- select, insert, update after 'add column'
select count(*) from addcol6 where d > 4;
insert into addcol6 select i, i, 71/i from generate_series(21,30)i;
select count(*) from addcol6;
update addcol6 set b = 0, c = 0 where d > 4;
select count(*) from addcol6 where b = 0 and c = 0;

-- partitioned table tests
create table addcol7 (
   timest character varying(6),
   user_id numeric(16,0) not null,
   tag1 smallint,
   tag2 varchar(2))
   with (appendonly=true, orientation=column, compresslevel=5, oids=false)
   distributed by (user_id)
   partition by list(timest) (
      partition part201202 values('201202')
         with (appendonly=true, orientation=column, compresslevel=5),
      partition part201203 values('201203')
         with (appendonly=true, orientation=column, compresslevel=5));

insert into addcol7 select '201202', 100*i, i, 'a'
   from generate_series(1,10)i;
insert into addcol7 select '201203', 101*i, i, 'b'
   from generate_series(11,20)i;

alter table addcol7 add column new1 float default 1.2;

-- select, insert post alter
select * from addcol7 where tag1 > 7 and tag1 < 13 order by tag1;
insert into addcol7 select '201202', 100*i, i, i::text, 22*i/7
   from generate_series(21,30)i;
insert into addcol7 select '201203', 101*i, i, (i+2)::text, 22*i/7
   from generate_series(31,40)i;

-- add new partition and a new column in the same alter table command
alter table addcol7
   add partition part201204 values('201204')
      with (appendonly=true, compresslevel=5),
   add column new2 varchar default 'abc';

-- insert, select, update, delete and vacuum post alter
insert into addcol7 values
   ('201202', 101, 1, 'p1', 3/5::float, 'newcol2'),
   ('201202', 102, 2, 'p1', 1/6::float, 'newcol2'),
   ('201202', 103, 3, 'p1', 22/7::float, 'newcol2'),
   ('201203', 201, 4, 'p2', 1/3::float, 'newcol2'),
   ('201203', 202, 5, 'p2', null, null),
   ('201203', 203, 6, 'p2', null, null),
   ('201204', 301, 7, 'p3', 22/7::float, 'newcol2'),
   ('201204', 302, 8, 'p3', null, null),
   ('201204', 303, 9, 'p3', null, null);
select * from addcol7 where tag2 like 'p%' order by user_id;
update addcol7 set new1 = 0, tag1 = -1 where tag2 like 'p%';
delete from addcol7 where new2 is null;
vacuum addcol7;
select * from addcol7 where tag2 like 'p%' order by user_id;

create table addcol8 (a int, b varchar(10), c int, d int)
   with (appendonly=true, orientation=column) distributed by (a);
insert into addcol8 select i, 'abc'||i, i, i from generate_series(1,10)i;
alter table addcol8
   alter column b type varchar(20),
   add column e float default 1,
   drop column c;
select * from addcol8 order by a;
\d addcol8

-- try renaming table and see if stuff still works
alter table addcol1 rename to addcol1_renamed;
alter table addcol1_renamed add column new_column int default 10;
alter table addcol1_renamed alter column new_column set not null;
alter table addcol1_renamed add column new_column2 int not null; -- should fail
select count(*) from addcol1_renamed;
alter table addcol1_renamed drop column new_column;
alter table addcol1_renamed rename to addcol1;

-- try renaming columns and see if stuff still works
alter table addcol1 rename column f to f_renamed;
alter table addcol1 alter column f_renamed set default 10;
select pg_get_expr(adbin, adrelid) from pg_attrdef pdef, pg_attribute pattr
    where pdef.adrelid='addcol1'::regclass and pdef.adrelid=pattr.attrelid and pdef.adnum=pattr.attnum and pattr.attname='f_renamed';
insert into addcol1 values (999);
select a, f_renamed from addcol1 where a = 999;

-- try dropping and adding back the column
alter table addcol1 drop column f_renamed;
select attname from pg_attribute where attrelid='addcol1'::regclass and attname='f_renamed';
alter table addcol1 add column f_renamed int default 20;
select a, f_renamed from addcol1 where a = 999;

-- try altering statistics of a column
alter table addcol1 alter column f_renamed set statistics 10000;
select attstattarget from pg_attribute where attrelid = 'aocs_addcol.addcol1'::regclass and attname = 'f_renamed';
set client_min_messages to error;
alter table addcol1 alter column f_renamed set statistics 10001; -- should limit to 10000 and give warning
set client_min_messages to notice;
select attstattarget from pg_attribute where attrelid = 'aocs_addcol.addcol1'::regclass and attname = 'f_renamed';

-- test alter distribution policy
alter table addcol1 set distributed randomly;
alter table addcol1 set distributed by (a);

alter table addcol1 add constraint tcheck check (a is not null);

-- test changing the storage type of a column
alter table addcol1 alter column f_renamed type varchar(7);
alter table addcol1 alter column f_renamed set storage plain;
select attname, attstorage from pg_attribute where attrelid='addcol1'::regclass and attname='f_renamed';
alter table addcol1 alter column f_renamed set storage main;
select attname, attstorage from pg_attribute where attrelid='addcol1'::regclass and attname='f_renamed';
alter table addcol1 alter column f_renamed set storage external;
select attname, attstorage from pg_attribute where attrelid='addcol1'::regclass and attname='f_renamed';
alter table addcol1 alter column f_renamed set storage extended;
select attname, attstorage from pg_attribute where attrelid='addcol1'::regclass and attname='f_renamed';

-- test some aocs partition table altering
create table alter_aocs_part_table (a int, b int) with (appendonly=true, orientation=column) distributed by (a)
    partition by range(b) (start (1) end (5) exclusive every (1), default partition foo);
insert into alter_aocs_part_table values (generate_series(1,10), generate_series(1,10));
alter table alter_aocs_part_table drop partition for (1);
alter table alter_aocs_part_table split default partition start(6) inclusive end(7) exclusive;
alter table alter_aocs_part_table split default partition start(6) inclusive end(8) exclusive;
alter table alter_aocs_part_table split default partition start(7) inclusive end(8) exclusive;
\d+ alter_aocs_part_table
create table alter_aocs_ao_table (a int, b int) with (appendonly=true) distributed by (a);
insert into alter_aocs_ao_table values (2,2);
alter table alter_aocs_part_table exchange partition for (2) with table alter_aocs_ao_table;
create table alter_aocs_heap_table (a int, b int) distributed by (a);
insert into alter_aocs_heap_table values (3,3);
alter table alter_aocs_part_table exchange partition for (3) with table alter_aocs_heap_table;

-- Test truncating and exchanging partition and then rolling back
begin work;
create table alter_aocs_ptable_exchange (a int, b int) with (appendonly=true, orientation=column) distributed by (a);
insert into alter_aocs_ptable_exchange values (3,3), (3,3), (3,3);
alter table alter_aocs_part_table truncate partition for (3);
select count(*) from alter_aocs_part_table;
alter table alter_aocs_part_table exchange partition for (3) with table alter_aocs_ptable_exchange;
select count(*) from alter_aocs_part_table;
rollback work;
select count(*) from alter_aocs_part_table;

-- Test AO hybrid partitioning scheme (range and list) w/ subpartitions
create table aocs_multi_level_part_table (date date, region text, region1 text, amount decimal(10,2))
  with (appendonly=true, orientation=column, compresstype=zlib, compresslevel=1)
  partition by range(date) subpartition by list(region) (
    partition part1 start(date '2008-01-01') end(date '2009-01-01')
      (subpartition usa values ('usa'), subpartition asia values ('asia'), default subpartition def),
    partition part2 start(date '2009-01-01') end(date '2010-01-01')
      (subpartition usa values ('usa'), subpartition asia values ('asia')));

-- insert some data
insert into aocs_multi_level_part_table values ('2008-02-02', 'usa', 'Texas', 10.05), ('2008-03-03', 'asia', 'China', 1.01);
insert into aocs_multi_level_part_table values ('2009-02-02', 'usa', 'Utah', 10.05), ('2009-03-03', 'asia', 'Japan', 1.01);

-- add a partition that is not a default partition
alter table aocs_multi_level_part_table add partition part3 start(date '2010-01-01') end(date '2012-01-01')
  with (appendonly=true, orientation=column)
  (subpartition usa values ('usa'), subpartition asia values ('asia'), default subpartition def);

-- Add default partition (defaults to parent table's AM)
alter table aocs_multi_level_part_table add default partition yearYYYY (default subpartition def);
SELECT am.amname FROM pg_class c LEFT JOIN pg_am am ON (c.relam = am.oid)
WHERE c.relname = 'aocs_multi_level_part_table_1_prt_yearyyyy_2_prt_def';

alter table aocs_multi_level_part_table drop partition yearYYYY;
alter table aocs_multi_level_part_table add default partition yearYYYY with (appendonly=true, orientation=column) (default subpartition def);
SELECT am.amname FROM pg_class c LEFT JOIN pg_am am ON (c.relam = am.oid)
WHERE c.relname = 'aocs_multi_level_part_table_1_prt_yearyyyy_2_prt_def';

-- index on atts 1, 4
create index ao_mlp_idx on aocs_multi_level_part_table(date, amount);
select indexname from pg_indexes where tablename='aocs_multi_level_part_table';
alter index ao_mlp_idx rename to ao_mlp_idx_renamed;
select indexname from pg_indexes where tablename='aocs_multi_level_part_table';

-- truncate partitions until table is empty
select * from aocs_multi_level_part_table;
truncate aocs_multi_level_part_table_1_prt_part1_2_prt_asia;
select * from aocs_multi_level_part_table;
alter table aocs_multi_level_part_table truncate partition for ('02-02-2008');
select * from aocs_multi_level_part_table;
alter table aocs_multi_level_part_table alter partition part2 truncate partition usa;
select * from aocs_multi_level_part_table;
alter table aocs_multi_level_part_table truncate partition part2;
select * from aocs_multi_level_part_table;

-- drop column in the partition table
select count(*) from pg_attribute where attrelid='aocs_multi_level_part_table'::regclass and attname = 'region1';
alter table aocs_multi_level_part_table drop column region1;
select count(*) from pg_attribute where attrelid='aocs_multi_level_part_table'::regclass and attname = 'region1';

-- splitting top partition of a multi-level partition should not work
alter table aocs_multi_level_part_table split partition part3 at (date '2011-01-01') into (partition part3, partition part4);

-- Test case: alter table add column with FirstRowNumber > 1
create table aocs_first_row_number (a int, b int) with (appendonly=true, orientation=column);
create index i_aocs_first_row_number on aocs_first_row_number using btree(b);
-- abort an insert transaction to generate a first row number > 1
begin;
insert into aocs_first_row_number select i,i from generate_series(1,100)i;
abort;
insert into aocs_first_row_number select i,i from generate_series(101, 200)i;
alter table aocs_first_row_number add column c int default -1;
-- At this point, block directory entry for column c starts from first row number = 1, 
-- which is not the same as first row number for columns a and b.  
-- correct result using base table
set enable_seqscan=on;
set enable_indexscan=off;
select c from aocs_first_row_number where b = 10;
set enable_seqscan=off;
set enable_indexscan=on;
-- Used to have wrong result using index: this select returns 1 tuple when no tuples should be returned.
-- expect: same result as scanning the base table
select c from aocs_first_row_number where b = 10;
reset enable_seqscan;
reset enable_indexscan;

-- cleanup so as not to affect other installcheck tests
-- (e.g. column_compression).
set client_min_messages='WARNING';
drop schema aocs_addcol cascade;

-- Test case: alter column on a table after reorganize
-- For an AOCS table with columns using rle_type compression, the
-- implementation of 'reorganize' at 62d66c063fd did not set compression type
-- for dropped columns. This led to an error 'Bad datum stream Dense block
-- version'.
create table aocs_with_compress(a smallint, b smallint, c smallint) with (appendonly=true, orientation=column, compresstype=rle_type);
insert into aocs_with_compress values (1, 1, 1), (2, 2, 2);
alter table aocs_with_compress drop column b;
alter table aocs_with_compress set with (reorganize=true);
-- The following operation must not fail
alter table aocs_with_compress alter column c type integer;

-- test case: alter AOCS table add column, the preference of the storage setting is: the encoding clause > table setting > gp_default_storage_options
CREATE TABLE aocs_alter_add_col(a int) WITH (appendonly=true, orientation=column, compresstype=rle_type, compresslevel=4, blocksize=65536);
-- use statement encoding 
ALTER TABLE aocs_alter_add_col ADD COLUMN b int ENCODING(compresstype=zlib, compresslevel=3, blocksize=16384);
-- use table setting
ALTER TABLE aocs_alter_add_col ADD COLUMN c int;
-- table setting > gp_default_storage_options
SET gp_default_storage_options ='compresstype=zlib, compresslevel=2';
ALTER TABLE aocs_alter_add_col ADD COLUMN d int;
RESET gp_default_storage_options;
\d+ aocs_alter_add_col
DROP TABLE aocs_alter_add_col;

CREATE TABLE aocs_alter_add_col_no_compress(a int) WITH (appendonly=true, orientation=column);
SET gp_default_storage_options ='compresstype=zlib, compresslevel=2, blocksize=8192';
-- use statement encoding
ALTER TABLE aocs_alter_add_col_no_compress ADD COLUMN b int ENCODING(compresstype=rle_type, compresslevel=3, blocksize=16384);
-- use table setting
ALTER TABLE aocs_alter_add_col_no_compress ADD COLUMN c int;
RESET gp_default_storage_options;
-- use default value 
ALTER TABLE aocs_alter_add_col_no_compress ADD COLUMN d int;
\d+ aocs_alter_add_col_no_compress 
DROP TABLE aocs_alter_add_col_no_compress;

-- test case: ensure reorganize keep default compresstype, compresslevel and blocksize table options
CREATE TABLE aocs_alter_add_col_reorganize(a int) WITH (appendonly=true, orientation=column, compresstype=rle_type, compresslevel=4, blocksize=65536);
ALTER TABLE aocs_alter_add_col_reorganize SET WITH (reorganize=true);
SET gp_default_storage_options ='compresstype=zlib, compresslevel=2';
-- use statement encoding
ALTER TABLE aocs_alter_add_col_reorganize ADD COLUMN b int ENCODING(compresstype=zlib, compresslevel=3, blocksize=16384);
-- use table setting
ALTER TABLE aocs_alter_add_col_reorganize ADD COLUMN c int;
RESET gp_default_storage_options;
-- use table setting
ALTER TABLE aocs_alter_add_col_reorganize ADD COLUMN d int;
\d+ aocs_alter_add_col_reorganize
DROP TABLE aocs_alter_add_col_reorganize;

-- test case: Ensure that reads don't fail after aborting an add column + insert operation and we don't project the aborted column
CREATE TABLE aocs_addcol_abort(a int, b int) USING ao_column;
INSERT INTO aocs_addcol_abort SELECT i,i FROM generate_series(1,10)i;
BEGIN;
ALTER TABLE aocs_addcol_abort ADD COLUMN c int;
INSERT INTO aocs_addcol_abort SELECT i,i,i FROM generate_series(1,10)i;
-- check state of aocsseg for entries of add column + insert
SELECT * FROM gp_toolkit.__gp_aocsseg('aocs_addcol_abort') ORDER BY segment_id, column_num;
SELECT * FROM aocs_addcol_abort;
ABORT;
-- check state of aocsseg if entries for new column are rolled back correctly
SELECT * FROM gp_toolkit.__gp_aocsseg('aocs_addcol_abort') ORDER BY segment_id, column_num;
SELECT * FROM aocs_addcol_abort;
DROP TABLE aocs_addcol_abort;

---------------------------------------------------
-- ADD COLUMN optimization for ao_column tables
---------------------------------------------------
drop table if exists relfilenodecheck;
create table relfilenodecheck(segid int, relname text, relfilenodebefore int, relfilenodeafter int, casename text);

-- capture relfilenode for a table, which will be checked by checkrelfilenodediff
prepare capturerelfilenodebefore as
insert into relfilenodecheck select -1 segid, relname, pg_relation_filenode(relname::text) as relfilenode, null::int, $1 as casename from pg_class where relname like $2
union select gp_segment_id segid, relname, pg_relation_filenode(relname::text) as relfilenode, null::int, $1 as casename  from gp_dist_random('pg_class')
where relname like $2 order by segid;

-- check to see if relfilenode changed (i.e. whether a table is rewritten), only showing result of primary seg0
prepare checkrelfilenodediff as
select a.segid, b.casename, b.relname, (relfilenodebefore != a.relfilenode) rewritten
from
    (
        select -1 segid, relname, pg_relation_filenode(relname::text) as relfilenode
        from pg_class
        where relname like $2
        union
        select gp_segment_id segid, relname, pg_relation_filenode(relname::text) as relfilenode
        from gp_dist_random('pg_class')
        where relname like $2 order by segid
    )a, relfilenodecheck b
where b.casename like $1 and b.relname like $2 and a.segid = b.segid and a.segid = 0;

-- checking pg_attribute_encoding on one of the primaries (most of tested tables are distributed replicated)
prepare checkattributeencoding as
select attnum, filenum, lastrownums, attoptions 
from gp_dist_random('pg_attribute_encoding') e
  join pg_class c
  on e.attrelid = c.oid
where c.relname = $1 and e.gp_segment_id = 0;

-- prepare the initial table
drop table if exists t_addcol_aoco;
create table t_addcol_aoco(a int) using ao_column distributed replicated;
create index t_addcol_aoco_a on t_addcol_aoco(a);
insert into t_addcol_aoco select * from generate_series(1, 10);

--
-- ADD COLUMN w/ default value
--

execute capturerelfilenodebefore('add column default 10', 't_addcol_aoco');
alter table t_addcol_aoco add column def10 int default 10;
-- no table rewrite
execute checkrelfilenodediff('add column default 10', 't_addcol_aoco');
-- select results are expected
select sum(a), sum(def10) from t_addcol_aoco;
select gp_segment_id, (gp_toolkit.__gp_aoblkdir('t_addcol_aoco')).* from gp_dist_random('gp_id');
-- pg_attribute_encoding shows expected lastrownums
execute checkattributeencoding('t_addcol_aoco');

--
-- ADD COLUMN w/ null default
--

execute capturerelfilenodebefore('add column NULL default', 't_addcol_aoco');
alter table t_addcol_aoco add column defnull1 int;
alter table t_addcol_aoco add column defnull2 int default NULL;
-- no table rewrite and results stay the same
execute checkrelfilenodediff('add column NULL default', 't_addcol_aoco');
select sum(a), sum(def10), sum(defnull1), sum(defnull2) from t_addcol_aoco;
-- pg_attribute_encoding shows more entries for the new columns
execute checkattributeencoding('t_addcol_aoco');
-- add more data
insert into t_addcol_aoco select 0 from generate_series(1, 10)i;
select sum(a), sum(def10), sum(defnull1), sum(defnull2) from t_addcol_aoco;
-- insert explict NULLs
insert into t_addcol_aoco values(1,NULL,1, NULL);
insert into t_addcol_aoco values(1,NULL,NULL, 1);
select sum(a), sum(def10), sum(defnull1), sum(defnull2) from t_addcol_aoco;

--
-- transaction abort should work fine
--
begin;
alter table t_addcol_aoco add column colabort int default 1;
insert into t_addcol_aoco values(1);
-- results updated in transaction
select sum(a), sum(def10), sum(defnull1), sum(defnull2), sum(colabort), count(*) from t_addcol_aoco;
-- pg_attribute_encoding shows the new column
execute checkattributeencoding('t_addcol_aoco');
abort;
-- results reverts to previous ones
select sum(a), sum(def10), sum(defnull1), sum(defnull2) from t_addcol_aoco;
-- error out
select colabort from t_addcol_aoco;
-- the aborted column is not visible in pg_attribute_encoding
execute checkattributeencoding('t_addcol_aoco');

--
-- column rewrite scenarios
--
alter table t_addcol_aoco add column colrewrite int default 1;
select sum(a), sum(def10), sum(defnull1), sum(defnull2), sum(colrewrite) from t_addcol_aoco;
execute checkattributeencoding('t_addcol_aoco');
alter table t_addcol_aoco alter column colrewrite type text;
-- check new column's values
select colrewrite from t_addcol_aoco;
-- lastrownums should be reset since column is rewritten
execute checkattributeencoding('t_addcol_aoco');

--
-- table rewrite scenarios
--

-- 1. reorganize
execute capturerelfilenodebefore('reorganize', 't_addcol_aoco');
alter table t_addcol_aoco set with(reorganize=true);
execute checkrelfilenodediff('reorganize', 't_addcol_aoco');
-- lastrownums are gone
execute checkattributeencoding('t_addcol_aoco');
-- results intact
select sum(a), sum(def10), sum(defnull1), sum(defnull2) from t_addcol_aoco;

--
-- DELETE and VACUUM
--

alter table t_addcol_aoco add column def20 int default 20;
select sum(a), sum(def10), sum(defnull1), sum(defnull2), sum(def20) from t_addcol_aoco;
-- delete
delete from t_addcol_aoco where a = 10;
-- the row (10, 10, NULL, NULL, 20) is deleted
select sum(a), sum(def10), sum(defnull1), sum(defnull2), sum(def20) from t_addcol_aoco;
-- visimap shows effect of the deletion
select gp_segment_id, (gp_toolkit.__gp_aovisimap('t_addcol_aoco')).* from gp_dist_random('gp_id');

-- vacuum
vacuum t_addcol_aoco;
-- results intact
select sum(a), sum(def10), sum(defnull1), sum(defnull2), sum(def20) from t_addcol_aoco;
-- insert one row and delete it
insert into t_addcol_aoco values(99);
delete from t_addcol_aoco where a = 99;
-- results stay the same, but visimap shows effect for segno=1 (which the new row is inserted)
select sum(a), sum(def10), sum(defnull1), sum(defnull2), sum(def20) from t_addcol_aoco;
select gp_segment_id, (gp_toolkit.__gp_aovisimap('t_addcol_aoco')).* from gp_dist_random('gp_id');
vacuum t_addcol_aoco;
-- segno=1 has been vacuum'ed, visimap shows effect
select gp_segment_id, (gp_toolkit.__gp_aovisimap('t_addcol_aoco')).* from gp_dist_random('gp_id');

-- delete all but the newly inserted
insert into t_addcol_aoco values(NULL, NULL, NULL, NULL, 'a', 100);
delete from t_addcol_aoco where def20 != 100;
select a, def10, defnull1, defnull2, def20 from t_addcol_aoco;

-- delete all for further testing
delete from t_addcol_aoco;
select count(*) from t_addcol_aoco;
vacuum t_addcol_aoco;
-- visimap cleared
select gp_segment_id, (gp_toolkit.__gp_aovisimap('t_addcol_aoco')).* from gp_dist_random('gp_id');

-- truncate to make test stable (more predictable segment)
truncate t_addcol_aoco;

--
-- large/toasted values
--

-- 1. new column has large default value
insert into t_addcol_aoco values(1);
alter table t_addcol_aoco add column deflarge1 text default repeat('a', 100000);
select a, def10, defnull1, defnull2, def20, char_length(deflarge1) from t_addcol_aoco;
execute checkattributeencoding('t_addcol_aoco');

-- 2. large existing value
insert into t_addcol_aoco values(1,1,1,1,'a',1,repeat('a',100001));
alter table t_addcol_aoco add column deflarge2 text default repeat('a', 1000002);
select a, def10, defnull1, defnull2, def20, char_length(deflarge1), char_length(deflarge2) from t_addcol_aoco;
execute checkattributeencoding('t_addcol_aoco');

--
-- drop column
--
-- check current pg_attribute_encoding
execute checkattributeencoding('t_addcol_aoco');
-- drop a column
alter table t_addcol_aoco drop column def20;
-- error out
select def20 from t_addcol_aoco;
-- other results intact
select a, def10, defnull1, defnull2, char_length(deflarge1), char_length(deflarge2) from t_addcol_aoco;
-- dropped column info still expected in pg_attribute_encoding
execute checkattributeencoding('t_addcol_aoco');

--
-- default value is an expression
--
-- non-volatile expressions
execute capturerelfilenodebefore('addexp_nonvolatile', 't_addcol_aoco');
alter table t_addcol_aoco add column defexp1 int default char_length(repeat('a', 100003));
alter table t_addcol_aoco add column defexp2 timestamptz default current_timestamp;
-- no table rewrite
execute checkrelfilenodediff('addexp_nonvolatile', 't_addcol_aoco');
-- results are expected
select a, def10, defnull1, defnull2, char_length(deflarge1), char_length(deflarge2), defexp1, defexp2 <= current_timestamp as expected_defexp2 from t_addcol_aoco;
-- volatile expression, do not expect a table rewrite, only column rewrite
execute capturerelfilenodebefore('addexp_volatile', 't_addcol_aoco');
execute checkattributeencoding('t_addcol_aoco');
alter table t_addcol_aoco add column defexp3 int default random()*1000::int;
execute checkrelfilenodediff('addexp_volatile', 't_addcol_aoco');
execute checkattributeencoding('t_addcol_aoco');
-- results are expected
select a, def10, defnull1, defnull2, char_length(deflarge1), char_length(deflarge2), defexp1, defexp2 <= current_timestamp as expected_defexp2, defexp3 >=0 and defexp3 <= 1000 as expected_defexp3 from t_addcol_aoco;

--
-- truncate
--
-- safe truncate
truncate t_addcol_aoco;
select count(*) from t_addcol_aoco;
-- unsafe truncate
begin;
create table t_addcol_aoco_truncate(a int) using ao_column distributed replicated;
insert into t_addcol_aoco_truncate select * from generate_series(1,10000);
alter table t_addcol_aoco_truncate add column b int default 10;
select count(*) from t_addcol_aoco_truncate;
truncate t_addcol_aoco_truncate;
select count(*) from t_addcol_aoco_truncate;
end;
-- columns gone after safe truncate
execute checkattributeencoding('t_addcol_aoco');
-- for unsafe truncate, since we don't clear up gp_fastsequence, we don't need to clear up lastrownums
execute checkattributeencoding('t_addcol_aoco_truncate');

--
-- partition table
--
create table t_addcol_aoco_part(a int, b int) using ao_column partition by range(b);
create table t_addcol_aoco_p1 partition of t_addcol_aoco_part for values from (1) to (51);
create table t_addcol_aoco_p2 partition of t_addcol_aoco_part for values from (51) to (101);
insert into t_addcol_aoco_part select i,i from generate_series(1,100)i;
-- no rewrite for child partitions (parent partition doesn't have valid relfilenode)
alter table t_addcol_aoco_part add column c int default 10;
-- results are expected
select sum(a), sum(b), sum(c) from t_addcol_aoco_part;
-- child partitions have expected lastrownums info in the pg_attribute_encoding, while parent partition doesn't
execute checkattributeencoding('t_addcol_aoco');
execute checkattributeencoding('t_addcol_aoco_p1');
execute checkattributeencoding('t_addcol_aoco_p2');

--
-- drop column
--
create table t_addcol_aoco2(a int) using ao_column;
create index t_addcol_aoco2_i on t_addcol_aoco2(a);
insert into t_addcol_aoco2 select * from generate_series(1,10);
alter table t_addcol_aoco2 add column dropcol1 int default 1;
alter table t_addcol_aoco2 add column dropcol2 int default 1;
alter table t_addcol_aoco2 add column dropcol3 int default 1;
select count(*), sum(dropcol1), sum(dropcol2) from t_addcol_aoco2;
-- drop an incomplete column
alter table t_addcol_aoco2 drop column dropcol1;
select sum(dropcol1) from t_addcol_aoco2;
select count(*), sum(dropcol2) from t_addcol_aoco2;
-- drop the only complete column, one of the incomplete column will be rewritten
alter table t_addcol_aoco2 drop column a;
execute checkattributeencoding('t_addcol_aoco2');
select a from t_addcol_aoco2;
select count(*), sum(dropcol2) from t_addcol_aoco2;
-- drop, add and alter column done together
alter table t_addcol_aoco2 drop column dropcol3, add column b int, alter column dropcol2 type text;
insert into t_addcol_aoco2 values('aaa', 1);
select * from t_addcol_aoco2;
-- drop the last complete column which is also the last column in the table,
-- and that is fine.
alter table t_addcol_aoco2 drop column b;
alter table t_addcol_aoco2 drop column dropcol2;
select * from t_addcol_aoco2;

--
-- alter the last complete column
--
create table t_addcol_aoco3(a int, b int) using ao_column;
insert into t_addcol_aoco3 select i,i+1 from generate_series(1,10)i;
alter table t_addcol_aoco3 add column c int default 1;
insert into t_addcol_aoco3 select i,i+1 from generate_series(1,10)i;
alter table t_addcol_aoco3 drop column a;
-- b should be the last complete column remaining. Alter column should work
alter table t_addcol_aoco3 alter column b type text;
-- alter the missing column should also work
alter table t_addcol_aoco3 alter column c type text;
-- add two more missing columns
alter table t_addcol_aoco3 add column d int default 2, add column e int default 3;
-- alter e, to make it complete
alter table t_addcol_aoco3 alter column e type text;
-- drop b and c, so d becomes the first non-dropped column which is also a non-complete column
alter table t_addcol_aoco3 drop column b, drop column c;
-- eof is as expected
select * from gp_toolkit.__gp_aocsseg('t_addcol_aoco3') where column_num = 3 or column_num = 4;
-- data is good
select * from t_addcol_aoco3;

--
-- index scan on the added column
--
create table t_addcol_aoco4(a int) using ao_column;
insert into t_addcol_aoco4 select * from generate_series(1,10);
alter table t_addcol_aoco4 add column b int default 10;
create index t_addcol_aoco4_i on t_addcol_aoco4(b);
-- the new column doesn't have block directory entry
select gp_segment_id, (gp_toolkit.__gp_aoblkdir('t_addcol_aoco4')).* from gp_dist_random('gp_id');
-- indexscan works
set optimizer = false;
set enable_seqscan=off;
explain select * from t_addcol_aoco4 where b = 10;
select * from t_addcol_aoco4 where b = 10;
select * from t_addcol_aoco4 where b = 0;
reset optimizer;
reset enable_seqscan;

--
-- block directory covering test
--
create table t_addcol_aoco5(a int unique) using ao_column;
insert into t_addcol_aoco5 select * from generate_series(1,10);
alter table t_addcol_aoco5 add column b int default 1, add column c int default 1;
-- unique check is working
insert into t_addcol_aoco5 values(1); -- bad
insert into t_addcol_aoco5 values(11); -- good
select * from t_addcol_aoco5;
