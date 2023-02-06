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
select adsrc from pg_attrdef pdef, pg_attribute pattr
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

-- cannot set reloption appendonly
alter table ao1 set (appendonly=true, compresslevel=5, fillfactor=50);
alter table ao1 reset (appendonly, compresslevel, fillfactor);

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
select relname, attname, adsrc from pg_class, pg_attribute, pg_attrdef where attrelid = pg_class.oid and adrelid = pg_class.oid and adnum = pg_attribute.attnum and pg_class.relname = 'ao1';


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
select relname, attname, adsrc from pg_class, pg_attribute, pg_attrdef where attrelid = pg_class.oid and adrelid = pg_class.oid and adnum = pg_attribute.attnum and pg_class.relname = 'aoco1';

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

-- Add default partition (defaults to heap storage unless set with AO)
alter table ao_multi_level_part_table add default partition yearYYYY (default subpartition def);
select count(*) from pg_appendonly where relid='ao_multi_level_part_table_1_prt_yearyyyy'::regclass;
alter table ao_multi_level_part_table drop partition yearYYYY;
alter table ao_multi_level_part_table add default partition yearYYYY with (appendonly=true) (default subpartition def);
select count(*) from pg_appendonly where relid='ao_multi_level_part_table_1_prt_yearyyyy'::regclass;

-- index on atts 1, 4
create index ao_mlp_idx on ao_multi_level_part_table(date, amount);
select indexname from pg_indexes where tablename='ao_multi_level_part_table';
alter index ao_mlp_idx rename to ao_mlp_idx_renamed;
select indexname from pg_indexes where tablename='ao_multi_level_part_table';

-- truncate partitions until table is empty
select * from ao_multi_level_part_table;
truncate ao_multi_level_part_table_1_prt_part1_2_prt_asia;
select * from ao_multi_level_part_table;
alter table ao_multi_level_part_table truncate partition for (rank(1));
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

-- Ensure no seg entries are created when there are no tuples to add.
-- Note: Having empty pg_aoseg entries caused post-upgrade vacuum freeze errors.
-- For example, while upgrading the primaries a VACUUM FREEZE on a root AOCO
-- table with a non-empty pg_aocsseg table on the master resulted in:
--  "ERROR","58P01","could not open Append-Only segment file
--  ""base/16410/20863.1665"": No such file or directory",,,,,,"VACUUM
--    (FREEZE);",0,,"aomd.c"

-- Scenario 1: Alter Distribution of AO partition table
CREATE TABLE alter_dist_key_for_ao_partition_table (a integer, b text, c integer)
    WITH (APPENDONLY=TRUE) DISTRIBUTED BY (a)
    PARTITION BY RANGE(c) (START(1) END(3) EVERY(1));
ALTER TABLE alter_dist_key_for_ao_partition_table SET DISTRIBUTED BY (b);
-- assert non-empty seg entries
SELECT * FROM gp_toolkit.__gp_aoseg('alter_dist_key_for_ao_partition_table'::regclass);

-- Scenario 2: Inserting an empty row into AO table
CREATE table ao_insert_empty_row (a integer, b text, c integer) WITH (APPENDONLY=TRUE) DISTRIBUTED BY (a);
INSERT INTO ao_insert_empty_row SELECT 1,'a',1 FROM gp_id WHERE dbid=-999;
-- assert non-empty seg entries
SELECT * FROM gp_toolkit.__gp_aoseg('ao_insert_empty_row'::regclass);
