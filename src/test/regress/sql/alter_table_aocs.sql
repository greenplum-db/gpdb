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
-- TODO: How to run this on segments, through a TINC test?
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
   add column e float default 22/7::float encoding (compresstype=RLE_TYPE),
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
SELECT CASE WHEN xmin = 2 THEN 'FrozenXid' ELSE 'NormalXid' END, objmod,
last_sequence, gp_segment_id from gp_dist_random('gp_fastsequence') WHERE objid
IN (SELECT segrelid FROM pg_appendonly WHERE relid IN (SELECT oid FROM pg_class
WHERE relname='addcol6'));
abort;

-- check gp_fastsequence remains advanced.
SELECT CASE WHEN xmin = 2 THEN 'FrozenXid' ELSE 'NormalXid' END, objmod,
last_sequence, gp_segment_id from gp_dist_random('gp_fastsequence') WHERE objid
IN (SELECT segrelid FROM pg_appendonly WHERE relid IN (SELECT oid FROM pg_class
WHERE relname='addcol6'));

insert into addcol6 select i,i/2 from generate_series(1,20)i;
alter table addcol6 add column c float default 1.2;
select a,c from addcol6 where b > 5 order by a;

-- Lets validate after alter gp_fastsequence reflects correctly.
SELECT CASE WHEN xmin = 2 THEN 'FrozenXid' ELSE 'NormalXid' END, objmod,
last_sequence, gp_segment_id from gp_dist_random('gp_fastsequence') WHERE objid
IN (SELECT segrelid FROM pg_appendonly WHERE relid IN (SELECT oid FROM pg_class
WHERE relname='addcol6'));

-- add column with default value as sequence
alter table addcol6 add column d serial;

-- select, insert, update after 'add column'
select c,d from addcol6 where d > 15 order by d;
insert into addcol6 select i, i, 71/i from generate_series(21,30)i;
select count(*) from addcol6;
update addcol6 set b = 0, c = 0 where d > 15;
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
select adsrc from pg_attrdef pdef, pg_attribute pattr
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

-- test some constraints (unique indexes do not work for unique and pkey)
alter table addcol1 add constraint tunique unique(a);
alter table addcol1 add constraint tpkey primary key(a);
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

-- cannot set reloption appendonly
alter table addcol1 set (appendonly=true, compresslevel=5, fillfactor=50);
alter table addcol1 reset (appendonly, compresslevel, fillfactor);

-- test some aocs partition table altering
create table alter_aocs_part_table (a int, b int) with (appendonly=true, orientation=column) distributed by (a)
    partition by range(b) (start (1) end (5) exclusive every (1), default partition foo);
insert into alter_aocs_part_table values (generate_series(1,10), generate_series(1,10));
alter table alter_aocs_part_table drop partition for (rank(1));
alter table alter_aocs_part_table split default partition start(6) inclusive end(7) exclusive;
alter table alter_aocs_part_table split default partition start(6) inclusive end(8) exclusive;
alter table alter_aocs_part_table split default partition start(7) inclusive end(8) exclusive;
select partitionrangestart, partitionstartinclusive, partitionrangeend, partitionendinclusive, partitionisdefault
    from pg_partitions where tablename = 'alter_aocs_part_table';
create table alter_aocs_ao_table (a int, b int) with (appendonly=true) distributed by (a);
insert into alter_aocs_ao_table values (2,2);
alter table alter_aocs_part_table exchange partition for (rank(1)) with table alter_aocs_ao_table;
create table alter_aocs_heap_table (a int, b int) distributed by (a);
insert into alter_aocs_heap_table values (3,3);
alter table alter_aocs_part_table exchange partition for (rank(2)) with table alter_aocs_heap_table;

-- Test truncating and exchanging partition and then rolling back
begin work;
create table alter_aocs_ptable_exchange (a int, b int) with (appendonly=true, orientation=column) distributed by (a);
insert into alter_aocs_ptable_exchange values (3,3), (3,3), (3,3);
alter table alter_aocs_part_table truncate partition for (rank(2));
select count(*) from alter_aocs_part_table;
alter table alter_aocs_part_table exchange partition for (rank(2)) with table alter_aocs_ptable_exchange;
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

-- Add default partition (defaults to heap storage unless set with AO)
alter table aocs_multi_level_part_table add default partition yearYYYY (default subpartition def);
select count(*) from pg_appendonly where relid='aocs_multi_level_part_table_1_prt_yearyyyy'::regclass;
alter table aocs_multi_level_part_table drop partition yearYYYY;
alter table aocs_multi_level_part_table add default partition yearYYYY with (appendonly=true, orientation=column) (default subpartition def);
select count(*) from pg_appendonly where relid='aocs_multi_level_part_table_1_prt_yearyyyy'::regclass;

-- index on atts 1, 4
create index ao_mlp_idx on aocs_multi_level_part_table(date, amount);
select indexname from pg_indexes where tablename='aocs_multi_level_part_table';
alter index ao_mlp_idx rename to ao_mlp_idx_renamed;
select indexname from pg_indexes where tablename='aocs_multi_level_part_table';

-- truncate partitions until table is empty
select * from aocs_multi_level_part_table;
truncate aocs_multi_level_part_table_1_prt_part1_2_prt_asia;
select * from aocs_multi_level_part_table;
alter table aocs_multi_level_part_table truncate partition for (rank(1));
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
