-- ALTER TABLE ... SET DISTRIBUTED BY
-- This is the main interface for system expansion
\set DATA values(1, 2), (2, 3), (3, 4)
-- Basic sanity tests
set optimizer_print_missing_stats = off;
create table atsdb (i int, j text) distributed by (i);
insert into atsdb :DATA;

-- should fail
alter table atsdb set distributed by ();
alter table atsdb set distributed by (m);
alter table atsdb set distributed by (i, i);
alter table atsdb set distributed by (i, m);
alter table atsdb set distributed by (i);

-- should work
alter table atsdb set distributed randomly;
select localoid::regclass, distkey from gp_distribution_policy where localoid = 'atsdb'::regclass;
-- not possible to correctly verify random distribution

alter table atsdb set distributed by (j);
select localoid::regclass, distkey from gp_distribution_policy where localoid = 'atsdb'::regclass;
-- verify that the data is correctly redistributed by building a fresh
-- table with the same policy
create table ats_test (i int, j text) distributed by (j);
insert into ats_test :DATA;
select gp_segment_id, * from ats_test except
select gp_segment_id, * from atsdb;
drop table ats_test;

alter table atsdb set distributed by (i, j);
select localoid::regclass, distkey from gp_distribution_policy where localoid = 'atsdb'::regclass;
-- verify
create table ats_test (i int, j text) distributed by (i, j);
insert into ats_test :DATA;
select gp_segment_id, * from ats_test except
select gp_segment_id, * from atsdb;
drop table ats_test;

alter table atsdb set distributed by (j, i);
select localoid::regclass, distkey from gp_distribution_policy where localoid = 'atsdb'::regclass;
-- verify
create table ats_test (i int, j text) distributed by (j, i);
insert into ats_test :DATA;
select gp_segment_id, * from ats_test except
select gp_segment_id, * from atsdb;
drop table ats_test;

-- Now make sure indexes work.
create index atsdb_i_idx on atsdb(i);
set enable_seqscan to off;
explain select * from atsdb where i = 1;
select * from atsdb where i = 1;
alter table atsdb set distributed by (i);
explain select * from atsdb where i = 1;
select * from atsdb where i = 1;

drop table atsdb;

-- Now try AO
create table atsdb_ao (i int, j text) with (appendonly=true) distributed by (i);
insert into atsdb_ao select i, (i+1)::text from generate_series(1, 100) i;
insert into atsdb_ao select i, (i+1)::text from generate_series(1, 100) i;
-- check that we're an AO table
select count(*) from pg_appendonly where relid='atsdb_ao'::regclass;
select count(*) from atsdb_ao;
alter table atsdb_ao set distributed by (j);
-- Still AO?
select count(*) from pg_appendonly where relid='atsdb_ao'::regclass;
select count(*) from atsdb_ao;
-- check alter, vacuum analyze, and then alter
delete from atsdb_ao where i = any(array(select generate_series(1,90)));
vacuum analyze atsdb_ao;
alter table atsdb_ao set distributed randomly;
select count(*) from atsdb_ao;
drop table atsdb_ao;

-- Can't redistribute system catalogs
alter table pg_class set distributed by (relname);
alter table pg_class set with(appendonly = true);

alter table pg_class set with(appendonly = true);

-- WITH clause
create table atsdb (i int, j text) distributed by (j);
insert into atsdb select i, i::text from generate_series(1, 10) i;
alter table atsdb set with(appendonly = true);
select relname, segrelid != 0, reloptions from pg_class, pg_appendonly where pg_class.oid =
'atsdb'::regclass and relid = pg_class.oid;
select * from atsdb;
drop table atsdb;

create view distcheck as select relname as rel, attname from
gp_distribution_policy g, pg_attribute p, pg_class c
where g.localoid = p.attrelid and attnum = any(g.distkey) and
c.oid = p.attrelid;

-- dropped columns
create table atsdb (i int, j int, t text, n numeric) distributed by (j);
insert into atsdb select i, i+1, i+2, i+3 from generate_series(1, 100) i;
alter table atsdb drop column i;
select * from atsdb;
alter table atsdb set distributed by (t);
select * from distcheck where rel = 'atsdb';
alter table atsdb drop column n;

alter table atsdb set with(appendonly = true, compresslevel = 3);
select relname, segrelid != 0, reloptions from pg_class, pg_appendonly where pg_class.oid =
'atsdb'::regclass and relid = pg_class.oid;

select * from distcheck where rel = 'atsdb';

select * from atsdb;
alter table atsdb set distributed by (j);

select * from distcheck where rel = 'atsdb';
select relname, segrelid != 0, reloptions from pg_class, pg_appendonly where pg_class.oid =
'atsdb'::regclass and relid = pg_class.oid;

select * from atsdb;
-- validate parameters
alter table atsdb set with (appendonly = ff);
alter table atsdb set with (reorganize = true);
alter table atsdb set with (fgdfgef = asds);
alter table atsdb set with(reorganize = true, reorganize = false) distributed
randomly;
drop table atsdb;

-- check distribution after dropping distribution key column.
create table atsdb (i int, j int, t text, n numeric) distributed by (i, j);
insert into atsdb select i, i+1, i+2, i+3 from generate_series(1, 20) i;
alter table atsdb drop column i;
select * from atsdb;
select * from distcheck where rel = 'atsdb';
drop table atsdb;

-- Check that we correctly cascade for partitioned tables
create table atsdb (i int, j int, k int) distributed by (i) partition by range(k)
(start(1) end(10) every(1));
insert into atsdb select i+2, i+1, i from generate_series(1, 9) i;
select * from distcheck where rel like 'atsdb%';
alter table atsdb set distributed by (j);
select * from distcheck where rel like 'atsdb%';
select * from atsdb order by 1, 2, 3;
alter table atsdb set with(appendonly = true);
select relname, a.blocksize, compresslevel, compresstype, checksum from pg_class c, pg_appendonly a where
relname  like 'atsdb%' and c.oid = a.relid order by 1;
select * from atsdb order by 1, 2, 3;
insert into atsdb select i+2, i+1, i from generate_series(1, 9) i;
select * from atsdb order by 1, 2, 3;
drop table atsdb;
drop view distcheck;

-- MPP-5452
-- Should succeed
create table atsdb (i int, k int) distributed by (i) partition by range(i) (start (1) end(10)
every(1));
alter table atsdb alter partition for(rank(5)) set distributed by (i);
alter table atsdb alter partition for(rank(5)) set distributed by (i);
alter table atsdb alter partition for(rank(5)) set distributed by (i);
drop table atsdb;

--MPP-5500

CREATE TABLE test_add_drop_rename_column_change_datatype(
 text_col text,
 bigint_col bigint,
 char_vary_col character varying(30),
 numeric_col numeric,
 int_col int4,
 float_col float4,
 int_array_col int[],
 drop_col numeric,
 before_rename_col int4,
 change_datatype_col numeric,
 a_ts_without timestamp without time zone,
 b_ts_with timestamp with time zone,
 date_column date) distributed randomly;

insert into test_add_drop_rename_column_change_datatype values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
insert into test_add_drop_rename_column_change_datatype values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
insert into test_add_drop_rename_column_change_datatype values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');

ALTER TABLE test_add_drop_rename_column_change_datatype ADD COLUMN added_col character varying(30);
ALTER TABLE test_add_drop_rename_column_change_datatype DROP COLUMN drop_col ;
ALTER TABLE test_add_drop_rename_column_change_datatype RENAME COLUMN before_rename_col TO after_rename_col;
ALTER TABLE test_add_drop_rename_column_change_datatype ALTER COLUMN change_datatype_col TYPE int4;
alter table test_add_drop_rename_column_change_datatype set with(reorganize =
true) distributed randomly;
select * from test_add_drop_rename_column_change_datatype ;
drop table test_add_drop_rename_column_change_datatype ;

-- MPP-5501
-- should run without error
create table atsdb with (appendonly=true) as select * from
generate_series(1,1000);
alter table only atsdb set with(reorganize=true) distributed by (generate_series);
select count(*) from atsdb;
drop table atsdb;

-- MPP-5746
create table mpp5746 (c int[], t text);
insert into mpp5746 select array[i], i from generate_series(1, 100) i;
alter table mpp5746 set with (reorganize=true, appendonly = true);
select * from mpp5746 order by 1;
alter table mpp5746 drop column t;
select * from mpp5746 order by 1;
alter table mpp5746 set with (reorganize=true, appendonly = false);
select * from mpp5746 order by 1;
drop table mpp5746;

-- MPP-5738
create table mpp5738 (a int, b int, c int, d int)
partition by range(d) (start(1) end(10) inclusive every(1));
insert into mpp5738 select i, i+1, i+2, i from generate_series(1, 10) i;
select * from mpp5738;
alter table mpp5738 alter partition for(rank(1)) set with (appendonly=true);
select * from mpp5738;
drop table mpp5738;

drop table if exists mpp5754;
CREATE TABLE mpp5754 (
             N_NATIONKEY INTEGER,
             N_NAME CHAR(25),
             N_REGIONKEY INTEGER,
             N_COMMENT VARCHAR(152)
             ) with (appendonly = true, checksum = true)
             distributed by (N_NATIONKEY);

copy mpp5754 from stdin with delimiter '|';
0|ALGERIA|0| haggle. carefully final deposits detect slyly agai
\.
select * from mpp5754 order by n_nationkey;
alter table mpp5754 set distributed randomly;
select count(*) from mpp5754;
alter table mpp5754 set distributed by (n_nationkey);
select * from mpp5754 order by n_nationkey;
drop table mpp5754;


-- MPP-5918
create role atsdb;
create table owner_test(i int, toast text) distributed randomly;
alter table owner_test owner to atsdb;
alter table owner_test set with (reorganize = true) distributed by (i);
-- verify, atsdb should own all three
select a.relname,
       x.rolname as relowner,
       y.rolname as toastowner,
       z.rolname as toastidxowner
from pg_class a
     inner join pg_class b on b.oid = a.reltoastrelid
     inner join pg_index ti on ti.indrelid = b.oid
     inner join pg_class c on c.oid = ti.indexrelid,
     pg_authid x, pg_authid y, pg_authid z
where a.relname='owner_test'
  and x.oid = a.relowner
  and y.oid = b.relowner
  and z.oid = c.relowner;

-- MPP-9663 - Check that the ownership is consistent on the segments as well
select a.relname,
       x.rolname as relowner,
       y.rolname as toastowner,
       z.rolname as toastidxowner
from gp_dist_random('pg_class') a
     inner join gp_dist_random('pg_class') b on b.oid = a.reltoastrelid
     inner join pg_index ti on ti.indrelid = b.oid
     inner join gp_dist_random('pg_class') c on c.oid = ti.indexrelid,
     pg_authid x, pg_authid y, pg_authid z
where a.relname='owner_test'
  and x.oid = a.relowner
  and y.oid = b.relowner
  and z.oid = c.relowner
  and a.gp_segment_id = 0
  and b.gp_segment_id = 0
  and c.gp_segment_id = 0;

-- MPP-9663 - The code path is different when the table has dropped columns
alter table owner_test add column d text;
alter table owner_test drop column d;
alter table owner_test set with (reorganize = true) distributed by (i);

select a.relname,
       x.rolname as relowner,
       y.rolname as toastowner,
       z.rolname as toastidxowner
from gp_dist_random('pg_class') a
     inner join gp_dist_random('pg_class') b on b.oid = a.reltoastrelid
     inner join pg_index ti on ti.indrelid = b.oid
     inner join gp_dist_random('pg_class') c on c.oid = ti.indexrelid,
     pg_authid x, pg_authid y, pg_authid z
where a.relname='owner_test'
  and x.oid = a.relowner
  and y.oid = b.relowner
  and z.oid = c.relowner
  and a.gp_segment_id = 0
  and b.gp_segment_id = 0
  and c.gp_segment_id = 0;

drop table owner_test;
drop role atsdb;

-- MPP-6332
create table abc (a int, b int, c int) distributed by (a);
Alter table abc set distributed randomly;
Alter table abc set with (reorganize=false) distributed randomly;
drop table abc;

-- disallow, so fails
create table atsdb (i int, j text) distributed by (j);
alter table atsdb set with(appendonly = true);
drop table atsdb;

-- MPP-18660: duplicate entry in gp_distribution_policy
set enable_indexscan=on;
set enable_seqscan=off;

drop table if exists distrib_index_test;
create table distrib_index_test (a int, b text) distributed by (a);
select count(*) from gp_distribution_policy
	where localoid in (select oid from pg_class where relname='distrib_index_test');

begin;
drop table distrib_index_test;
rollback;

select count(*) from gp_distribution_policy
	where localoid in (select oid from pg_class where relname='distrib_index_test');

reset enable_indexscan;
reset enable_seqscan;
drop table distrib_index_test;

-- alter partitioned table crash
-- Alter partitioned table set distributed by will crash when:
--     1. reorganize = false.
--     2. table have index.
--     3. partition table have "with" option.
drop index if exists distrib_part_test_idx;
drop table if exists distrib_part_test;
CREATE TABLE distrib_part_test
(
  col1 int,
  col2 decimal,
  col3 text,
  col4 bool
)
distributed by (col1)
partition by list(col2)
(
    partition part1 values(1,2,3,4,5,6,7,8,9,10) WITH (appendonly=false )
);
create index distrib_part_test_idx on distrib_part_test(col1);
ALTER TABLE public.distrib_part_test SET with (reorganize=false) DISTRIBUTED RANDOMLY;

-- MPP-23801
--
-- ALTER TABLE set distribution key should check compatible with unique index.

-- case 1

CREATE TABLE t_dist1(col1 INTEGER, col2 INTEGER, CONSTRAINT pk_t_dist1 PRIMARY KEY(col2)) DISTRIBUTED BY(col2);
ALTER TABLE t_dist1 SET DISTRIBUTED BY(col1);

-- case 2

CREATE TABLE t_dist2(col1 INTEGER, col2 INTEGER, col3 INTEGER, col4 INTEGER) DISTRIBUTED BY(col1);

CREATE UNIQUE INDEX idx1_t_dist2 ON t_dist2(col1, col2);
CREATE UNIQUE INDEX idx2_t_dist2 ON t_dist2(col1, col2, col3);
CREATE UNIQUE INDEX idx3_t_dist2 ON t_dist2(col1, col2, col4);

ALTER TABLE t_dist2 SET DISTRIBUTED BY(col1);
ALTER TABLE t_dist2 SET DISTRIBUTED BY(col2);
ALTER TABLE t_dist2 SET DISTRIBUTED BY(col1, col2);

ALTER TABLE t_dist2 SET DISTRIBUTED BY(col1, col2, col3);
ALTER TABLE t_dist2 SET DISTRIBUTED BY(col3);
ALTER TABLE t_dist2 SET DISTRIBUTED BY(col4);

-- Altering distribution policy for temp tables
create temp table atsdb (c1 int, c2 int) distributed randomly;
select * from atsdb;
alter table atsdb set distributed by (c1);
select * from atsdb;
alter table atsdb set distributed by (c2);
select * from atsdb;

--
-- ALTER TABLE SET DATA TYPE tests, where the column is part of the
-- distribution key.
--
CREATE TABLE distpol_typechange (i int2) DISTRIBUTED BY (i);
INSERT INTO distpol_typechange values (123);
ALTER TABLE distpol_typechange ALTER COLUMN i SET DATA TYPE int4;
DROP TABLE distpol_typechange;
CREATE TABLE distpol_typechange (p text) DISTRIBUTED BY (p);

-- This should throw an error, you can't change the datatype of a distribution
-- key column.
INSERT INTO distpol_typechange VALUES ('(1,1)');
ALTER TABLE distpol_typechange ALTER COLUMN p TYPE point USING p::point;

-- unless it's completely empty! But 'point' doesn't have hash a opclass,
-- so it cannot be part of the distribution key. We silently turn the
-- table randomly distributed.
TRUNCATE distpol_typechange;
ALTER TABLE distpol_typechange ALTER COLUMN p TYPE point USING p::point;

select policytype, distkey, distclass from gp_distribution_policy where localoid='distpol_typechange'::regclass;


-- Similar case, but with CREATE UNIQUE INDEX, rather than ALTER TABLE.
-- Creating a unique index on a completely empty table automatically updates
-- the distribution key to match the unique index.  (This allows the common
-- case, where no DISTRIBUTED BY was given explicitly, and the system just
-- picked the first column, which isn't compatible with the unique index
-- that's created later, to work.) But not if the unique column doesn't
-- have a hash opclass!
CREATE TABLE tstab (i int4, t tsvector) distributed by (i);
CREATE UNIQUE INDEX tstab_idx ON tstab(t);
INSERT INTO tstab VALUES (1, 'foo');

-- ALTER TABLE SET DISTRIBUTED RANDOMLY should not work on a table
-- that has a primary key or unique index.
CREATE TABLE alter_table_with_primary_key (a int primary key);
ALTER TABLE alter_table_with_primary_key SET DISTRIBUTED RANDOMLY;
CREATE TABLE alter_table_with_unique_index (a int unique);
ALTER TABLE alter_table_with_unique_index SET DISTRIBUTED RANDOMLY;

-- Enable reorg partition leaf table
create table reorg_leaf (a int, b int, c int) distributed by (c)
partition by range(a)
subpartition by range (b)
subpartition template
(start(0) end (10) every (5))
(partition p0 start (0) end (5),
	partition p1 start (5) end (10));
insert into reorg_leaf select i, i, i from generate_series(0, 9) i;
select *, gp_segment_id from reorg_leaf_1_prt_p0;
alter table reorg_leaf_1_prt_p0 set with (reorganize=true) distributed by(b);
alter table reorg_leaf_1_prt_p0 set with (reorganize=true) distributed by(c);
alter table reorg_leaf_1_prt_p0 set with (reorganize=true);
alter table reorg_leaf_1_prt_p0_2_prt_1 set with (reorganize=true) distributed by(b);
alter table reorg_leaf_1_prt_p0_2_prt_1 set with (reorganize=true) distributed by(c);
select *, gp_segment_id from reorg_leaf_1_prt_p0;
alter table reorg_leaf_1_prt_p0_2_prt_1 set with (reorganize=true);
select *, gp_segment_id from reorg_leaf_1_prt_p0;
