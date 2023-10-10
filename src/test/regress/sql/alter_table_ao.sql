--
-- Test ALTER TABLE ADD COLUMN WITH NULL DEFAULT on AO TABLES
--
---
--- basic support for alter add column with NULL default to AO tables
---
drop table if exists ao1;
create table ao1(col1 varchar(2), col2 int) WITH (APPENDONLY=TRUE) distributed randomly;

insert into ao1 values('aa', 1);
insert into ao1 values('bb', 2);

-- following should be OK.
alter table ao1 add column col3 char(1) default 5;

-- the following should be supported now
alter table ao1 add column col4 char(1) default NULL;

select * from ao1;
insert into ao1 values('cc', 3);
select * from ao1;

alter table ao1 alter column col4 drop default; 
select * from ao1;
insert into ao1 values('dd', 4);
select * from ao1;

alter table ao1 alter column col2 set default 2;
select pg_get_expr(adbin, adrelid) from pg_attrdef pdef, pg_attribute pattr
    where pdef.adrelid='ao1'::regclass and pdef.adrelid=pattr.attrelid and pdef.adnum=pattr.attnum and pattr.attname='col2';

alter table ao1 rename col2 to col2_renamed;

-- check dropping column
alter table ao1 drop column col4;
select attname from pg_attribute where attrelid='ao1'::regclass and attname='col4';

-- change the storage type of a column
alter table ao1 alter column col3 set storage plain;
select attname, attstorage from pg_attribute where attrelid='ao1'::regclass and attname='col3';
alter table ao1 alter column col3 set storage main;
select attname, attstorage from pg_attribute where attrelid='ao1'::regclass and attname='col3';
alter table ao1 alter column col3 set storage external;
select attname, attstorage from pg_attribute where attrelid='ao1'::regclass and attname='col3';
alter table ao1 alter column col3 set storage extended;
select attname, attstorage from pg_attribute where attrelid='ao1'::regclass and attname='col3';

---
--- check catalog contents after alter table on AO tables 
---
drop table if exists ao1;
create table ao1(col1 varchar(2), col2 int) WITH (APPENDONLY=TRUE) distributed randomly;

-- relnatts is 2
select relname, relnatts from pg_class where relname = 'ao1';

alter table ao1 add column col3 char(1) default NULL;

-- relnatts in pg_class should be 3
select relname, relnatts from pg_class where relname = 'ao1';

-- check col details in pg_attribute
select  pg_class.relname, attname, typname from pg_attribute, pg_class, pg_type where attrelid = pg_class.oid and pg_class.relname = 'ao1' and atttypid = pg_type.oid and attname = 'col3';

-- There's an explicit entry in pg_attrdef for the NULL default (although it has
-- the same effect as no entry).
select relname, attname, pg_get_expr(adbin, adrelid) from pg_class, pg_attribute, pg_attrdef where attrelid = pg_class.oid and adrelid = pg_class.oid and adnum = pg_attribute.attnum and pg_class.relname = 'ao1';


---
--- check with IS NOT NULL constraint
---
drop table if exists ao1;
create table ao1(col1 varchar(2), col2 int) WITH (APPENDONLY=TRUE) distributed randomly;

insert into ao1 values('a', 1); 

-- should fail
alter table ao1 add column col3 char(1) not null default NULL; 

drop table if exists ao1;
create table ao1(col1 varchar(2), col2 int) WITH (APPENDONLY=TRUE) distributed randomly;

-- should pass
alter table ao1 add column col3 char(1) not null default NULL; 

-- this should fail (same behavior as heap tables)
insert into ao1(col1, col2) values('a', 10);
---
--- alter add with no default should continue to fail
---
drop table if exists ao1;
create table ao1(col1 varchar(1)) with (APPENDONLY=TRUE) distributed randomly;

insert into ao1 values('1');
insert into ao1 values('1');
insert into ao1 values('1');
insert into ao1 values('1');

alter table ao1 add column col2 char(1);
select * from ao1;

--
-- MPP-19664 
-- Test ALTER TABLE ADD COLUMN WITH NULL DEFAULT on AO/CO TABLES
--
---
--- basic support for alter add column with NULL default to AO/CO tables
---
drop table if exists aoco1;
create table aoco1(col1 varchar(2), col2 int)
WITH (APPENDONLY=TRUE, ORIENTATION=column) distributed randomly;

insert into aoco1 values('aa', 1);
insert into aoco1 values('bb', 2);

-- following should be OK.
alter table aoco1 add column col3 char(1) default 5;

-- the following should be supported now
alter table aoco1 add column col4 char(1) default NULL;

select * from aoco1;
insert into aoco1 values('cc', 3);
select * from aoco1;

alter table aoco1 alter column col4 drop default; 
select * from aoco1;
insert into aoco1 values('dd', 4);
select * from aoco1;

---
--- check catalog contents after alter table on AO/CO tables 
---
drop table if exists aoco1;
create table aoco1(col1 varchar(2), col2 int)
WITH (APPENDONLY=TRUE, ORIENTATION=column) distributed randomly;

-- relnatts is 2
select relname, relnatts from pg_class where relname = 'aoco1';

alter table aoco1 add column col3 char(1) default NULL;

-- relnatts in pg_class should be 3
select relname, relnatts from pg_class where relname = 'aoco1';

-- check col details in pg_attribute
select  pg_class.relname, attname, typname from pg_attribute, pg_class, pg_type where attrelid = pg_class.oid and pg_class.relname = 'aoco1' and atttypid = pg_type.oid and attname = 'col3';

-- There's an explicit entry in pg_attrdef for the NULL default (although it has
-- the same effect as no entry).
select relname, attname, pg_get_expr(adbin, adrelid) from pg_class, pg_attribute, pg_attrdef where attrelid = pg_class.oid and adrelid = pg_class.oid and adnum = pg_attribute.attnum and pg_class.relname = 'aoco1';

---
--- check with IS NOT NULL constraint
---
drop table if exists aoco1;
create table aoco1(col1 varchar(2), col2 int)
WITH (APPENDONLY=TRUE, ORIENTATION=column) distributed randomly;

insert into aoco1 values('a', 1); 

-- should fail (rewrite needs to do null checking) 
alter table aoco1 add column col3 char(1) not null default NULL; 
alter table aoco1 add column c5 int check (c5 IS NOT NULL) default NULL;

-- should fail (rewrite needs to do constraint checking) 
insert into aoco1(col1, col2) values('a', NULL);
alter table aoco1 alter column col2 set not null; 

-- should pass (rewrite needs to do constraint checking) 
alter table aoco1 alter column col2 type int; 

drop table if exists aoco1;
create table aoco1(col1 varchar(2), col2 int)
WITH (APPENDONLY=TRUE, ORIENTATION=column) distributed randomly;

-- should pass
alter table aoco1 add column col3 char(1) not null default NULL; 

-- this should fail (same behavior as heap tables)
insert into aoco1 (col1, col2) values('a', 10);

drop table if exists aoco1;
create table aoco1(col1 varchar(2), col2 int not null)
WITH (APPENDONLY=TRUE, ORIENTATION=column) distributed randomly;

insert into aoco1 values('aa', 1);
alter table aoco1 add column col3 char(1) default NULL;
insert into aoco1 values('bb', 2);
select * from aoco1;
alter table aoco1 add column col4 char(1) not NULL default NULL;
select * from aoco1;

---
--- alter add with no default should continue to fail
---
drop table if exists aoco1;
create table aoco1(col1 varchar(1))
WITH (APPENDONLY=TRUE, ORIENTATION=column) distributed randomly;

insert into aoco1 values('1');
insert into aoco1 values('1');
insert into aoco1 values('1');
insert into aoco1 values('1');

alter table aoco1 add column col2 char(1);
select * from aoco1;

drop table aoco1;

---
--- new column with a domain type
---
drop table if exists ao1;
create table ao1(col1 varchar(5)) with (APPENDONLY=TRUE) distributed randomly;

insert into ao1 values('abcde');

drop domain zipcode;
create domain zipcode as text
constraint c1 not null;

-- following should fail
alter table ao1 add column col2 zipcode;

alter table ao1 add column col2 zipcode default NULL;

select * from ao1;

-- cleanup
drop table ao1;
drop domain zipcode;
drop schema if exists mpp17582 cascade;
create schema mpp17582;
set search_path=mpp17582;

DROP TABLE testbug_char5;
CREATE TABLE testbug_char5
(
timest character varying(6),
user_id numeric(16,0) NOT NULL,
to_be_drop char(5), -- Iterate through different data types
tag1 char(5), -- Iterate through different data types
tag2 char(5)
)
DISTRIBUTED BY (user_id)
PARTITION BY LIST(timest)
(
PARTITION part201203 VALUES('201203') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column),
PARTITION part201204 VALUES('201204') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row),
PARTITION part201205 VALUES('201205')
);

create index testbug_char5_tag1 on testbug_char5 using btree(tag1);

insert into testbug_char5 (timest,user_id,to_be_drop) select '201203',1111,'10000';
insert into testbug_char5 (timest,user_id,to_be_drop) select '201204',1111,'10000';
insert into testbug_char5 (timest,user_id,to_be_drop) select '201205',1111,'10000';

analyze testbug_char5;

select * from testbug_char5 order by 1,2;

ALTER TABLE testbug_char5 drop column to_be_drop;

select * from testbug_char5 order by 1,2;

insert into testbug_char5 (timest,user_id,tag2) select '201203',2222,'2';
insert into testbug_char5 (timest,user_id,tag2) select '201204',2222,'2';
insert into testbug_char5 (timest,user_id,tag2) select '201205',2222,'2';

select * from testbug_char5 order by 1,2;

alter table testbug_char5 add PARTITION part201206 VALUES('201206') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=column);
alter table testbug_char5 add PARTITION part201207 VALUES('201207') WITH (APPENDONLY=true, COMPRESSLEVEL=5, ORIENTATION=row);
alter table testbug_char5 add PARTITION part201208 VALUES('201208');

insert into testbug_char5 select '201206',3333,'1','2';
insert into testbug_char5 select '201207',3333,'1','2';
insert into testbug_char5 select '201208',3333,'1','2';

select * from testbug_char5 order by 1,2;

-- Test exchanging partition and then rolling back
begin work;
create table testbug_char5_exchange (timest character varying(6), user_id numeric(16,0) NOT NULL, tag1 char(5), tag2 char(5))
  with (appendonly=true, compresstype=zlib, compresslevel=3) distributed by (user_id);
create index on testbug_char5_exchange using btree(tag1);
insert into testbug_char5_exchange values ('201205', 3333, '2', '2');
alter table testbug_char5 truncate partition part201205;
select count(*) from testbug_char5;
alter table testbug_char5 exchange partition part201205 with table testbug_char5_exchange;
select count(*) from testbug_char5;
rollback work;
select count(*) from testbug_char5;

-- Test AO hybrid partitioning scheme (range and list) w/ subpartitions
create table ao_multi_level_part_table (date date, region text, region1 text, amount decimal(10,2))
  with (appendonly=true, compresstype=zlib, compresslevel=1)
  partition by range(date) subpartition by list(region) (
    partition part1 start(date '2008-01-01') end(date '2009-01-01')
      (subpartition usa values ('usa'), subpartition asia values ('asia'), default subpartition def),
    partition part2 start(date '2009-01-01') end(date '2010-01-01')
      (subpartition usa values ('usa'), subpartition asia values ('asia')));

-- insert some data
insert into ao_multi_level_part_table values ('2008-02-02', 'usa', 'Texas', 10.05), ('2008-03-03', 'asia', 'China', 1.01);
insert into ao_multi_level_part_table values ('2009-02-02', 'usa', 'Utah', 10.05), ('2009-03-03', 'asia', 'Japan', 1.01);

-- add a partition that is not a default partition
alter table ao_multi_level_part_table add partition part3 start(date '2010-01-01') end(date '2012-01-01') with (appendonly=true)
  (subpartition usa values ('usa'), subpartition asia values ('asia'), default subpartition def);

-- Add default partition (defaults to parent table's AM)
alter table ao_multi_level_part_table add default partition yearYYYY (default subpartition def);
SELECT am.amname FROM pg_class c LEFT JOIN pg_am am ON (c.relam = am.oid)
WHERE c.relname = 'ao_multi_level_part_table_1_prt_yearyyyy_2_prt_def';

alter table ao_multi_level_part_table drop partition yearYYYY;
alter table ao_multi_level_part_table add default partition yearYYYY with (appendonly=true) (default subpartition def);
SELECT am.amname FROM pg_class c LEFT JOIN pg_am am ON (c.relam = am.oid)
WHERE c.relname = 'ao_multi_level_part_table_1_prt_yearyyyy_2_prt_def';

-- index on atts 1, 4
create index ao_mlp_idx on ao_multi_level_part_table(date, amount);
select indexname from pg_indexes where tablename='ao_multi_level_part_table';
alter index ao_mlp_idx rename to ao_mlp_idx_renamed;
select indexname from pg_indexes where tablename='ao_multi_level_part_table';

-- truncate partitions until table is empty
select * from ao_multi_level_part_table;
truncate ao_multi_level_part_table_1_prt_part1_2_prt_asia;
select * from ao_multi_level_part_table;
alter table ao_multi_level_part_table truncate partition for ('2008-02-02');
select * from ao_multi_level_part_table;
alter table ao_multi_level_part_table alter partition part2 truncate partition usa;
select * from ao_multi_level_part_table;
alter table ao_multi_level_part_table truncate partition part2;
select * from ao_multi_level_part_table;

-- drop column in the partition table
select count(*) from pg_attribute where attrelid='ao_multi_level_part_table'::regclass and attname = 'region1';
alter table ao_multi_level_part_table drop column region1;
select count(*) from pg_attribute where attrelid='ao_multi_level_part_table'::regclass and attname = 'region1';

-- splitting top partition of a multi-level partition should not work
alter table ao_multi_level_part_table split partition part3 at (date '2011-01-01') into (partition part3, partition part4);

--
-- Check index scan
--

set enable_seqscan=off;
set enable_indexscan=on;

select * from testbug_char5 where tag1='1';

--
-- Check NL Index scan plan
--

create table dim(tag1 char(5));
insert into dim values('1');

set enable_hashjoin=off;
set enable_seqscan=off;
set enable_nestloop=on;
set enable_indexscan=on;

select * from testbug_char5, dim where testbug_char5.tag1=dim.tag1;

--
-- Load from another table
--

DROP TABLE load;
CREATE TABLE load
(
timest character varying(6),
user_id numeric(16,0) NOT NULL,
tag1 char(5),
tag2 char(5)
)
DISTRIBUTED randomly;

insert into load select '20120' || i , 1111 * (i + 2), '1','2' from generate_series(3,8) i;
select * from load;

insert into testbug_char5 select * from load;

select * from testbug_char5;

--
-- Update values
--

update testbug_char5 set tag1='6' where tag1='1' and timest='201208';
update testbug_char5 set tag2='7' where tag2='1' and timest='201208';

select * from testbug_char5;

set search_path=public;
drop schema if exists mpp17582 cascade;

-- Test for tuple descriptor leak during row splitting
DROP TABLE IF EXISTS split_tupdesc_leak;
CREATE TABLE split_tupdesc_leak
(
   ym character varying(6) NOT NULL,
   suid character varying(50) NOT NULL,
   genre_ids character varying(20)[]
) 
WITH (APPENDONLY=true, ORIENTATION=row, COMPRESSTYPE=zlib, OIDS=FALSE)
DISTRIBUTED BY (suid)
PARTITION BY LIST(ym)
(
	DEFAULT PARTITION p_split_tupdesc_leak_ym  WITH (appendonly=true, orientation=row, compresstype=zlib)
);

INSERT INTO split_tupdesc_leak VALUES ('201412','0001EC1TPEvT5SaJKIR5yYXlFQ7tS','{0}');

ALTER TABLE split_tupdesc_leak SPLIT DEFAULT PARTITION AT ('201412')
	INTO (PARTITION p_split_tupdesc_leak_ym, PARTITION p_split_tupdesc_leak_ym_201412);

DROP TABLE split_tupdesc_leak;

---------------------------------------------------
-- ADD COLUMN optimization for ao_row tables
---------------------------------------------------

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
create table t_addcol(a int) using ao_row distributed replicated;
create index i_addcol_a on t_addcol(a);
insert into t_addcol select * from generate_series(1, 10);

--
-- ADD COLUMN w/ default value
--

execute capturerelfilenodebefore('add column default 10', 't_addcol');
alter table t_addcol add column def10 int default 10;
-- no table rewrite
execute checkrelfilenodediff('add column default 10', 't_addcol');
-- select results are expected
select sum(a), sum(def10) from t_addcol;
select gp_segment_id, (gp_toolkit.__gp_aoblkdir('t_addcol')).* from gp_dist_random('gp_id');
-- pg_attribute_encoding currently has one row that corresponds to the new column
execute checkattributeencoding('t_addcol');

--
-- ADD COLUMN w/ null default
--

execute capturerelfilenodebefore('add column NULL default', 't_addcol');
alter table t_addcol add column defnull1 int;
alter table t_addcol add column defnull2 int default NULL;
-- no table rewrite and results stay the same
execute checkrelfilenodediff('add column NULL default', 't_addcol');
select sum(a), sum(def10), sum(defnull1), sum(defnull2) from t_addcol;
-- pg_attribute_encoding shows more entries for the new columns
execute checkattributeencoding('t_addcol');
-- add more data
insert into t_addcol select 0 from generate_series(1, 10)i;
select sum(a), sum(def10), sum(defnull1), sum(defnull2) from t_addcol;
-- insert explict NULLs
insert into t_addcol values(1,NULL,1, NULL);
insert into t_addcol values(1,NULL,NULL, 1);
select sum(a), sum(def10), sum(defnull1), sum(defnull2) from t_addcol;

--
-- transaction abort should work fine
--
begin;
alter table t_addcol add column colabort int default 1;
insert into t_addcol values(1);
-- results updated in transaction
select sum(a), sum(def10), sum(defnull1), sum(defnull2), sum(colabort), count(*) from t_addcol;
-- pg_attribute_encoding shows the new column
execute checkattributeencoding('t_addcol');
abort;
-- results reverts to previous ones
select sum(a), sum(def10), sum(defnull1), sum(defnull2) from t_addcol;
-- error out
select colabort from t_addcol;
-- the aborted column is not visible in pg_attribute_encoding
execute checkattributeencoding('t_addcol');

--
-- table rewrite scenarios 
--

-- 1. reorganize
execute capturerelfilenodebefore('reorganize', 't_addcol');
alter table t_addcol set with(reorganize=true);
execute checkrelfilenodediff('reorganize', 't_addcol');
-- results intact
select sum(a), sum(def10), sum(defnull1), sum(defnull2) from t_addcol;

-- 2. alter access method
execute capturerelfilenodebefore('atsetam', 't_addcol');
alter table t_addcol set access method ao_column with (compresstype=rle_type, compresslevel=1);
execute checkrelfilenodediff('atsetam', 't_addcol');
-- results intact
select sum(a), sum(def10), sum(defnull1), sum(defnull2) from t_addcol;
-- should see updated pg_attribute_encoding entries, w/o lastrownums but w/ attoptions
execute checkattributeencoding('t_addcol');

-- change it back to ao_row for further testing
alter table t_addcol set access method ao_row;
select a.amname from pg_class c join pg_am a on c.relam = a.oid where c.relname = 't_addcol';

--
-- DELETE and VACUUM
--

alter table t_addcol add column def20 int default 20;
select sum(a), sum(def10), sum(defnull1), sum(defnull2), sum(def20) from t_addcol;
-- delete
delete from t_addcol where a = 10;
-- the row (10, 10, NULL, NULL, 20) is deleted
select sum(a), sum(def10), sum(defnull1), sum(defnull2), sum(def20) from t_addcol;
-- visimap shows effect of the deletion
select gp_segment_id, (gp_toolkit.__gp_aovisimap('t_addcol')).* from gp_dist_random('gp_id');

-- vacuum
vacuum t_addcol;
-- results intact
select sum(a), sum(def10), sum(defnull1), sum(defnull2), sum(def20) from t_addcol;
-- insert one row and delete it
insert into t_addcol values(99);
delete from t_addcol where a = 99;
-- results stay the same, but visimap shows effect for segno=1 (which the new row is inserted)
select sum(a), sum(def10), sum(defnull1), sum(defnull2), sum(def20) from t_addcol;
select gp_segment_id, (gp_toolkit.__gp_aovisimap('t_addcol')).* from gp_dist_random('gp_id');
vacuum t_addcol;
-- segno=1 has been vacuum'ed, visimap shows effect
select gp_segment_id, (gp_toolkit.__gp_aovisimap('t_addcol')).* from gp_dist_random('gp_id');

-- delete all but the newly inserted
insert into t_addcol values(NULL, NULL, NULL, NULL, 100);
delete from t_addcol where def20 != 100;
select sum(a), sum(def10), sum(defnull1), sum(defnull2), sum(def20) from t_addcol;

-- delete all for further testing
delete from t_addcol;
select count(*) from t_addcol;
vacuum t_addcol;
-- visimap cleared
select gp_segment_id, (gp_toolkit.__gp_aovisimap('t_addcol')).* from gp_dist_random('gp_id');

--
-- large/toasted values
--

-- we've had a few AO segments for the table now (due to vacuum etc.), and insert could be choosing
-- different segno depending on some runtime status (like the tuple order in scanning aoseg).
-- So truncate to make test stable (no segments now).
truncate t_addcol;
-- new column has large default value
insert into t_addcol values(1);
execute capturerelfilenodebefore('addlarge1', 't_addcol');
alter table t_addcol add column deflarge1 text default repeat('a', 100000);
execute checkrelfilenodediff('addlarge1', 't_addcol');
select a, def10, defnull1, defnull2, def20, char_length(deflarge1) from t_addcol;

-- large existing value
insert into t_addcol values(1,1,1,1,1, repeat('a',100001));
execute capturerelfilenodebefore('addlarge2', 't_addcol');
alter table t_addcol add column deflarge2 text default repeat('a', 1000002);
execute checkrelfilenodediff('addlarge2', 't_addcol');
select a, def10, defnull1, defnull2, def20, char_length(deflarge1), char_length(deflarge2) from t_addcol;

--
-- drop column
--
-- check current pg_attribute_encoding
execute checkattributeencoding('t_addcol');
-- drop a column
alter table t_addcol drop column def20;
-- error out
select def20 from t_addcol;
-- other results intact
select a, def10, defnull1, defnull2, char_length(deflarge1), char_length(deflarge2) from t_addcol;
-- column info still shown in pg_attribute_encoding, just like for AOCO tables
execute checkattributeencoding('t_addcol');

--
-- default value is an expression
--
-- non-volatile expressions
execute capturerelfilenodebefore('addexp_nonvolatile', 't_addcol');
alter table t_addcol add column defexp1 int default char_length(repeat('a', 100003));
alter table t_addcol add column defexp2 timestamptz default current_timestamp;
-- no table rewrite
execute checkrelfilenodediff('addexp_nonvolatile', 't_addcol');
-- results are expected
select a, def10, defnull1, defnull2, char_length(deflarge1), char_length(deflarge2), defexp1, defexp2 <= current_timestamp as expected_defexp2 from t_addcol;
-- volatile expression, expecting a rewrite
execute capturerelfilenodebefore('addexp_volatile', 't_addcol');
alter table t_addcol add column defexp3 int default random()*1000::int;
execute checkrelfilenodediff('addexp_volatile', 't_addcol');
-- results are expected
select a, def10, defnull1, defnull2, char_length(deflarge1), char_length(deflarge2), defexp1, defexp2 <= current_timestamp as expected_defexp2, defexp3 >=0 and defexp3 <= 1000 as expected_defexp3 from t_addcol;

--
-- truncate
--
-- safe truncate
truncate t_addcol;
select count(*) from t_addcol;
-- unsafe truncate
begin;
create table t_addcol_truncate(a int) distributed replicated;
insert into t_addcol_truncate select * from generate_series(1,10000);
alter table t_addcol_truncate add column b int default 10;
select count(*) from t_addcol_truncate;
truncate t_addcol_truncate;
select count(*) from t_addcol_truncate;
end;
-- columns gone after truncate in pg_attribute_encoding
execute checkattributeencoding('t_addcol');
execute checkattributeencoding('t_addcol_truncate');

--
-- partition table
--
create table t_addcol_part(a int, b int) using ao_row partition by range(b);
create table t_addcol_p1 partition of t_addcol_part for values from (1) to (51);
create table t_addcol_p2 partition of t_addcol_part for values from (51) to (101);
insert into t_addcol_part select i,i from generate_series(1,100)i;
-- no rewrite for child partitions (parent partition doesn't have valid relfilenode)
execute capturerelfilenodebefore('partition', 't_addcol_p1');
execute capturerelfilenodebefore('partition', 't_addcol_p2');
alter table t_addcol_part add column c int default 10;
execute checkrelfilenodediff('partition', 't_addcol_p1');
execute checkrelfilenodediff('partition', 't_addcol_p2');
-- results are expected
select sum(a), sum(b), sum(c) from t_addcol_part;
-- child partitions have expected lastrownums info in the pg_attribute_encoding, while parent partition doesn't
execute checkattributeencoding('t_addcol');
execute checkattributeencoding('t_addcol_p1');
execute checkattributeencoding('t_addcol_p2');
