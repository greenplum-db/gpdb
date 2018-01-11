-----------------------------------------------------------------------
-- Basic syntax
-- Expect: success
-----------------------------------------------------------------------

create database column_compression;
\c column_compression

prepare ccddlcheck as
select attrelid::regclass as relname,
attnum, attoptions from pg_class c, pg_attribute_encoding e
where c.relname like 'ccddl%' and c.oid=e.attrelid
order by relname, attnum;

-- default encoding clause
create table ccddl (
	i int,
	j int,
	default column encoding (compresstype=zlib)
) with (appendonly=true, orientation=column);

execute ccddlcheck;

-- This is enough to force compression, specially since we'll hash
-- all to a single segment and the values are all the same.
insert into ccddl select 1, 2 from generate_series(1, 100);
select * from ccddl;

drop table ccddl;

-- MPP-17012 default encoding clause with extra options in with clause
create table ccddl (
	i int,
	j int,
	default column encoding (compresstype=zlib, compresslevel=5)
) with (appendonly=true, orientation=column, oids=false);

execute ccddlcheck;

drop table ccddl;

create table ccddl (
	i int,
	j int,
	default column encoding (compresstype=zlib)
) with (appendonly=true, orientation=column, fillfactor=11);

execute ccddlcheck;

-- This is enough to force compression, specially since we'll hash
-- all to a single segment and the values are all the same.
insert into ccddl select 1, 2 from generate_series(1, 100);
select * from ccddl;

drop table ccddl;

-- Make sure we cleaned up after ourselves: should return zero rows.
-- We exclude the other tables leftover from other tests.
select * from pg_attribute_encoding where
	attrelid not in (select oid from pg_class where relname <> 'ccddl');

-- mix inline and default
create table ccddl (
	i int,
	j int encoding (compresstype=RLE_TYPE),
	default column encoding (compresstype=zlib)
) with (appendonly=true, orientation=column);

execute ccddlcheck;

drop table ccddl;

-- mix column reference and default
create table ccddl (
	i int,
	j int,
	default column encoding (compresstype=zlib),
	column j encoding (compresstype=RLE_TYPE)
) with (appendonly=true, orientation=column);

execute ccddlcheck;
drop table ccddl;

-- encoding clause for only some columns, others should
-- have no encoding
create table ccddl (
	i int,
	j text encoding (compresstype=zlib, blocksize=65536, compresslevel=1)
)  with (appendonly=true, orientation=column);

execute ccddlcheck;

-- Should see the encoding information for the new column
alter table ccddl add column k timestamp default now()
	encoding (compresstype=zlib);
execute ccddlcheck;

-- no encoding information for this one though
alter table ccddl add column l numeric
	default 3.141;
execute ccddlcheck;

drop table ccddl;

-- Decipher encoding clause references
create table ccddl (i int, j int encoding(compresstype=zlib), column i encoding
(compresstype=zlib), default column encoding (compresstype=RLE_TYPE)) with
(appendonly=true, orientation=column);
execute ccddlcheck;
drop table ccddl;

-- WITH (..., compresstype=<type>) should act as a default clause
create table ccddl (i int, j int) with(appendonly = true, orientation = column,
compresstype=zlib);
execute ccddlcheck;
drop table ccddl;

-- Mix case for compresstype since it should be case insensitive
create table ccddl (i int, j int encoding (COMPRESSTYPE="ZlIb"))
	with(appendonly = true, orientation = column);
execute ccddlcheck;
drop table ccddl;

-- CREATE TABLE (LIKE) WITH (...) must honour the directives in the WITH clause.
create table ccddl (i int);
create table ccddl_co(LIKE ccddl)
  with (appendonly = true, orientation=column);
execute ccddlcheck;
drop table ccddl_co;

create table ccddl_co(LIKE ccddl)
  with (appendonly = true, orientation=column, compresstype=zlib);
execute ccddlcheck;
drop table ccddl_co, ccddl;

-----------------------------------------------------------------------
-- Basic syntax
-- Expect: failure
-----------------------------------------------------------------------

-- only support CO tables
create table ccddl (i int encoding (compresstype=zlib));
create table ccddl (i int encoding (compresstype=zlib))
	with (appendonly = true);

-- can't add encoding to a non-CO table
create table ccddl (i int);
alter table ccddl add column j int encoding (compresstype=zlib);
drop table ccddl;

create table ccddl (i int) with (appendonly=true);
alter table ccddl add column j int encoding (compresstype=zlib);
drop table ccddl;

-- check that we validate the encoding clause for add column
create table ccddl (i int) with (appendonly=true, orientation=column);
alter table ccddl add column j int encoding(compresstype=yawn);
alter table ccddl add column j int encoding(compresstype=zlib,
compresslevel=100000);
alter table ccddl add column j int encoding(a=b);
drop table ccddl;

-- encoding clause has higher precedence than WITH clause
create table ccddl (i int encoding(compresstype=RLE_TYPE)) with(compresstype=zlib,
appendonly=true, orientation=column);
execute ccddlcheck;
drop table ccddl;

-- conflicting WITH and DEFAULT COLUMN ENCODING clause
create table t1 (i int, j int, default column encoding (compresstype=RLE_TYPE))
with (compresstype=zlib, appendonly=true, orientation=column);

-- Invalid encoding clauses
create table t1 (i int encoding (compresstype=zlib, compresslevel=19))
with (appendonly=true, orientation=column);

create table t1 (i int encoding (compresstype=zlib, ahhhh=boooooo))
with (appendonly=true, orientation=column);

-- Inheritance: check that we don't support inheritance on tables using
-- column compression
create table ccddlparent (i int encoding (compresstype=zlib))
with (appendonly = true, orientation = column);
create table ccddlchild (j int encoding (compresstype=zlib))
inherits(ccddlparent) with (appendonly = true, orientation = column);
drop table ccddlparent cascade;

-- Conflict between default and with, in the LIKE case
create table ccddl (i int);
create table ccddl_co (like ccddl, 
					   default column encoding(compresstype=RLE_TYPE)) 
with (appendonly=true, orientation=column, compresstype=zlib);
drop table ccddl;

-- Make sure we preserve WITH() in the presence of CTAS 
create table ccddl (a, b)
with (appendonly=true, orientation=column, compresstype=zlib) as
select 1, 1;
execute ccddlcheck;
drop table ccddl;

-- encoding clause has higher precedence than WITH clause even in the LIKE case.
-- (this change the behavior of MPP-15120) 
create table ccddl (i int, j int);
create table ccddl_co (like ccddl, column i encoding(compresstype=RLE_TYPE))
with (appendonly=true, orientation=column, compresstype=zlib);
execute ccddlcheck;
drop table ccddl;
drop table ccddl_co;

-----------------------------------------------------------------------
-- Partitioning support
-- Expect: success
-----------------------------------------------------------------------

-- trivial partitioning case
create table ccddl (i int, j int)
with (appendonly = true, orientation=column)
partition by range(j)
(partition p1 start(1) end(10),
 partition p2 start(10) end(20),
 column i encoding(compresstype=zlib),
 column j encoding(compresstype=RLE_TYPE)
);

execute ccddlcheck;

insert into ccddl select 1,2 from generate_series(1, 100);
select * from ccddl;
drop table ccddl;

-- subpartition template
create table ccddl (i int, j int, k int, l int)
with (appendonly = true, orientation=column)
partition by range(j)
	subpartition by list (k)
	subpartition template(
		subpartition sp1 values(1, 2, 3, 4, 5),
 		column i encoding(compresstype=zlib),
		column j encoding(compresstype=RLE_TYPE),
		column k encoding(compresstype=zlib),
		column l encoding(compresstype=zlib)
	)

(partition p1 start(1) end(10),
 partition p2 start(10) end(20)
);

execute ccddlcheck;

select parencattnum, parencattoptions from
pg_partition_encoding e, pg_partition p, pg_class c
where c.relname = 'ccddl' and c.oid = p.parrelid and p.oid = e.parencoid;

insert into ccddl select 1, (i % 19) + 1, ((i+3) % 5) + 1, i+3 from generate_series(1, 100) i;

select * from ccddl;

-- Verify dependency handling
alter table ccddl drop column l;

insert into ccddl select 1, (i % 19) + 1, ((i+3) % 5) + 1 from generate_series(1, 100) i;

select parencattnum, parencattoptions from
pg_partition_encoding e, pg_partition p, pg_class c
where c.relname = 'ccddl' and c.oid = p.parrelid and p.oid = e.parencoid;

select * from ccddl;

drop table ccddl;

-- Add partition should 'inherit' the subpartition template storage encodings
create table ccddl (i int, j int, k int, l int)
with
(appendonly = true, orientation=column)
partition by range(j)
subpartition by list (k)
subpartition template(
subpartition sp1 values(1, 2, 3, 4, 5),
column i encoding(compresstype=zlib),
column j encoding(compresstype=RLE_TYPE),
column k encoding(compresstype=zlib),
column l encoding(compresstype=zlib))
(partition p1 start(1) end(10),
partition p2 start(10) end(20)
);

execute ccddlcheck;

alter table ccddl add partition p3 start(20) end(30);

execute ccddlcheck;

drop table ccddl;

-- Make sure the AO/CO case didn't screw up the non CO case
create table ccddl (i int, j int, k int, l int)
with
(appendonly = true)
partition by range(j)
subpartition by list (k)
subpartition template(
subpartition sp1 values(1, 2, 3, 4, 5))
(partition p1 start(1) end(10),
partition p2 start(10) end(20)
);

execute ccddlcheck;

alter table ccddl add partition p3 start(20) end(30);

execute ccddlcheck;

drop table ccddl;

-- Should be nothing in pg_partition_encoding now
select * from pg_partition_encoding;

-- Split support. We must preserve the column encodings of the split partition
create table ccddl (i int encoding (compresstype=zlib))
with (appendonly = true, orientation=column)
partition by range(i)
(partition p1 start(1) end(10));
execute ccddlcheck;

alter table ccddl split partition p1 at (5) into (partition p2, partition p3);
execute ccddlcheck;

drop table ccddl;

-- With subpartitioning
create table ccddl (i int, j int)
with (appendonly=true, orientation=column)
partition by range(i) subpartition by range(j)
subpartition template (subpartition sp1 start(1) end(20),
	column i encoding (compresstype=zlib),
	column j encoding (compresstype=RLE_TYPE))
(partition p1 start(1) end(20));

execute ccddlcheck;

alter table ccddl alter partition p1 split partition sp1 at (10) into (partition sp2, partition sp3);
execute ccddlcheck;

alter table ccddl alter partition p1 split partition sp2 at (5) into (partition sp2, partition sp2_5);
execute ccddlcheck;

drop table ccddl;


-- MPP-14407
-- Multi level partitioning: the expansion of the multi-level partitioning
-- configuration duplicates the encoding for `month'. Make sure we 
-- handle this and produce sane results

CREATE TABLE ccddl (id int, year int, month int, day int, region text) 
	with (appendonly=true, orientation=column)
	DISTRIBUTED BY (id)
	PARTITION BY RANGE (year)
		SUBPARTITION BY RANGE (month)
			SUBPARTITION TEMPLATE
			(
				START (1) END (13),
				COLUMN month ENCODING (compresstype=RLE_TYPE)
			)
		SUBPARTITION BY LIST (region)
		SUBPARTITION TEMPLATE
		(
			SUBPARTITION usa VALUES ('usa'),
			COLUMN region ENCODING (compresstype=RLE_TYPE)
		)
	( START (2008) END (2010) );
execute ccddlcheck;

-- Ensure we can read and write
insert into ccddl select 1, 2008, 1, 2, 'usa' from generate_series(1, 100);
select * from ccddl;
drop table ccddl;

-- Partition specific column encoding
create table ccddl (
	i int,
	j int,
	k int,
	l int
	)
	with (appendonly=true, orientation=column)
	partition by range(i)
	(
		partition p1 start(1) end(2) column i encoding(compresstype=zlib),
	 	partition p2 start(2) end(3) column j encoding(compresstype=RLE_TYPE)
									 column k encoding(blocksize=8192),
	 	partition p3 start(3) end(4) column i encoding(compresstype=RLE_TYPE)
									 column j encoding(blocksize=8192)
									 default column encoding
									 	(compresstype=zlib),
		column i encoding (blocksize=65536),
		default column encoding (compresstype=RLE_TYPE)
	);

execute ccddlcheck;
drop table ccddl;


create table ccddl (i int, j int, k int, l int )
with (appendonly=true, orientation=column)
partition by range(i) subpartition by range(j)
 (
 partition p1 start(1) end(2)
		(subpartition sp1 start(1) end(2) column i encoding(compresstype=zlib),
		 column i encoding (blocksize=65536),
		 default column encoding (compresstype=RLE_TYPE)
		),
 partition p2 start(2) end(3)
 		(subpartition sp1 start(1) end(2)
			column j encoding(compresstype=zlib)
			column k encoding(blocksize=8192),
		 column i encoding (blocksize=65536),
		 default column encoding (compresstype=RLE_TYPE)
		)
);

execute ccddlcheck;
drop table ccddl;


-- Precedence test: c3 in the partition child must be zlib, not RLE_TYPE
CREATE TABLE ccddl ( c1 int ENCODING (compresstype=zlib),
                     c2 char ENCODING (compresstype=RLE_TYPE, blocksize=65536),
				     c3 date,
					 COLUMN c3 ENCODING (compresstype=RLE_TYPE))
WITH (appendonly=true, orientation=column)
	PARTITION BY RANGE (c3) (
		START ('1900-01-01'::DATE)   END ('2100-12-31'::DATE),
		        COLUMN c3 ENCODING (compresstype=zlib),
				COLUMN c2 ENCODING (compresstype=RLE_TYPE)
	);

execute ccddlcheck;
drop table ccddl;

-- Should be able to turn have a partition ignore a column encoding clause
-- if it's explicitly marked appendonly=false. This is to support
-- what dump has been doing all along
create table ccddl
	(i int, j int encoding (compresstype=RLE_TYPE))
	with (appendonly=true, orientation=column)
	partition by range(i)
	(
		partition p1 start(1) end(2) with(appendonly=false),
		partition p2 start(2) end(3),
		default column encoding (compresstype=zlib)
	);
execute ccddlcheck;
drop table ccddl;

-- MPP-16875: ensure that WITH () is honoured
CREATE TABLE ccddl (a int, b text)
with (appendonly=true, orientation=column,
	  compresstype=zlib, compresslevel=1)
partition by list(b)
(partition s_abc values ('abc')
	with (appendonly=true, orientation=column, compresstype=zlib,
		  compresslevel=1));

alter table ccddl add partition "s_xyz" values ('xyz')
	WITH (appendonly=true, orientation=column,
		  compresstype=zlib, compresslevel=1);
execute ccddlcheck;
drop table ccddl;

-- Ensure that WITH () and subpartition template column encoding rules
-- play well together
create table ccddl (i int, d date, j int)
  WITH (APPENDONLY=TRUE, ORIENTATION=COLUMN)
  partition by range(d) subpartition by list(j)
  	subpartition template (subpartition sp1 values(1),
						   default column encoding (compresstype=zlib))
  (start('2010-01-01') end('2010-01-05') every('1 day'::interval));

alter table ccddl add partition newp 
	start('2010-01-06') end('2010-01-07')
	with (appendonly=true, orientation=column, compresstype=RLE_TYPE); 

execute ccddlcheck;
drop table ccddl;

-----------------------------------------------------------------------
-- Partitioning support
-- Expect: failure
-----------------------------------------------------------------------

-- Make sure we validate the storage directives
create table gg (i int, k int) with (appendonly=true, orientation=column)
partition by range(k) (partition p1 start(1) end(2), column i encoding(a=b));

create table gg (i int, k int) with (appendonly=true, orientation=column)
partition by range(k) (partition p1 start(1) end(2), column i
encoding(compresstype=sdf2sdf));

-- We don't support partition element specific encoding clauses in subpartition
-- templates as we have no place to store them.
create table a (i int, j int) with (appendonly=true, orientation=column)
      partition by range(i) subpartition by range(j)
      subpartition template(start(1) end(10) default column encoding (compresstype=zlib),
                            start(10) end(20))
(partition p1 start(1) end(10));

-- partition level mention of column encoding but the table isn't heap oriented
CREATE TABLE ccddl
(a1 int,a2 char(5),a3 text,a4 timestamp ,a5 date) 
partition by range(a1) 
	(   
		start(1) end(1000) every(500),
		COLUMN a1 ENCODING (compresstype=zlib,compresslevel=4,blocksize=32768)
	);

-----------------------------------------------------------------------
-- Type support
-- Expect: success
-----------------------------------------------------------------------

-- The basics
drop type if exists int42 cascade;
create type int42;
CREATE FUNCTION int42_in(cstring)
RETURNS int42
AS 'int4in'
LANGUAGE internal IMMUTABLE STRICT;

CREATE FUNCTION int42_out(int42)
RETURNS cstring
AS 'int4out'
LANGUAGE internal IMMUTABLE STRICT;

CREATE TYPE int42 (
internallength = 4,
input = int42_in,
output = int42_out,
alignment = int4,
default = 42,
passedbyvalue,
compresstype="zlib",
blocksize=65536,
compresslevel=1
);

select typoptions from pg_type_encoding where typid='public.int42'::regtype;

create table ccddl (i int42) with(appendonly = true, orientation=column);
execute ccddlcheck;

alter type int42 set default encoding (compresstype=zlib);
alter table ccddl add column j int42 default '1'::int42;
execute ccddlcheck;

drop table ccddl;

-- Shouldn't apply type default encoding in these cases
create table ccddl (i int42);
execute ccddlcheck;
drop table ccddl;

create table ccddl (i int42) with (appendonly = true);
execute ccddlcheck;
drop table ccddl;

create table ccddl (i int42) with (appendonly = true, orientation=column,
compresstype=none);
execute ccddlcheck;
drop table ccddl;

-- We used to accept SQL standard aliases for the built-in types,
-- like "character varying", "integer", before GPDB 6, but not anymore.
alter type character varying set default encoding (compresstype=zlib);
alter type varchar set default encoding (compresstype=zlib);

select typoptions from pg_type t, pg_type_encoding e where
  t.typname = 'varchar' and t.oid = e.typid;

alter type character set default encoding (compresstype=zlib);
alter type bpchar set default encoding (compresstype=zlib);

select typoptions from pg_type t, pg_type_encoding e where
  t.typname = 'bpchar' and t.oid = e.typid;

alter type timestamp with time zone set default encoding(compresstype=zlib);
alter type timestamptz set default encoding (compresstype=zlib);

select typoptions from pg_type t, pg_type_encoding e where
  t.typname = 'timestamptz' and t.oid = e.typid;

-- schema qualification
alter type pg_catalog.text set default encoding (compresstype=zlib);
select typoptions from pg_type t, pg_type_encoding e where
  t.typname = 'text' and t.oid = e.typid;

-----------------------------------------------------------------------
-- Type support
-- Expect: failure
-----------------------------------------------------------------------
-- We should reject any extraneous type information during alter type
alter type numeric(10, 2) set default encoding(compresstype=zlib);
alter type int[2][3] set default encoding(compresstype=RLE_TYPE);
-- permissions checks
create role typcheck;
set session authorization typcheck;
alter type int42 set default encoding (compresstype=zlib, compresslevel=1);
reset session authorization;
drop role typcheck;
-- Verify that we validate the storage clause
alter type text set default encoding (a=b);
alter type text set default encoding (compresstype=10);
alter type text set default encoding (compresstype=zlib, compresslevel=100);

-- dependency check on drop type
drop type int42 cascade;

select * from pg_type_encoding;

-- cleanup
deallocate ccddlcheck;


-----------------------------------------------------------------------
-- RLE sanity checks
-- Expect: failure
-----------------------------------------------------------------------

-- RLE_TYPE is not supported 
create table ccddl (i int) with(appendonly = true, compresstype = rle_type);

-----------------------------------------------------------------------
-- MPP-14381 cdbfast regression: Dropping and adding a column to AO partitioned tables fails with SIGSEGV
-- Expect: success
-----------------------------------------------------------------------
DROP TABLE IF EXISTS ccddl;

CREATE TABLE ccddl (
                    P_PARTKEY INTEGER,
                    P_SIZE integer,
                    P_CONTAINER CHAR(10),
                    P_RETAILPRICE decimal,
                    P_COMMENT VARCHAR(23)
                    )
  partition by range (p_size)
  (
    partition p1 start('1')  WITH (appendonly=true, checksum=true, blocksize=819200, compresslevel=8)
  , partition p2 start('21') end('28')
  , partition p3 start('28') WITH (appendonly=true, checksum=true, blocksize=819200, compresslevel=8)
  , partition p4 start('32') end('33')
  , partition p5 start('33') WITH (appendonly=true, checksum=true, blocksize=819200, compresslevel=8)
);

INSERT INTO ccddl values ( generate_series(1,25000), 3, 'JUMBO CASE', 1001.00, 'ronic dependencies d' );

INSERT INTO ccddl values ( generate_series(1,25000), 23, 'JUMBO CASE', 1001.00, 'ronic dependencies d' );

INSERT INTO ccddl values ( generate_series(1,25000), 29, 'JUMBO CASE', 1001.00, 'ronic dependencies d' );

INSERT INTO ccddl values ( generate_series(1,25000), 37, 'JUMBO CASE', 1001.00, 'ronic dependencies d' );

ALTER TABLE ccddl DROP COLUMN P_PARTKEY;

ALTER TABLE ccddl ADD COLUMN   P_PARTKEY  INTEGER  DEFAULT 100;


-----------------------------------------------------------------------
-- MPP-14477 cdbfast regression: Bitmap Index scan on AO tables with compression fails with SIGSEGV
-- Expect: success
-----------------------------------------------------------------------
DROP TABLE IF EXISTS ccddl cascade;

CREATE TABLE ccddl ( 
   id INTEGER
 , owner VARCHAR
 , property BOX
  ) 
WITH (APPENDONLY=True, COMPRESSTYPE=ZLIB, COMPRESSLEVEL=1)  
DISTRIBUTED BY (id);

insert into ccddl values (59,'Hypatia','( (6050, 20), (7052, 250) )');


CREATE INDEX ccddl_propertyBoxIndex ON ccddl USING Gist (property);

SET enable_seqscan = FALSE;

-- to the same internal representation.
SELECT owner, property FROM ccddl
 WHERE property ~= '((7052,250),(6050,20))';
drop table ccddl;

-----------------------------------------------------------------------
-- Dump / restore
-----------------------------------------------------------------------

-- We can only test partition dumping here, since pg_dump does table level
-- dump/restore

create table ccddl (i int, j int)
with (appendonly=true, orientation=column)
partition by range(i) subpartition by range(j)
	subpartition template
		( subpartition sp1 start(1) end(2),
		  subpartition sp2 start(2) end (3),
		  column i encoding (compresstype=zlib),
		  column j encoding (blocksize=65536)
		)
	(partition p1 start(1) end (2));

select pg_get_partition_def('ccddl'::regclass, true);

select pg_get_partition_template_def('ccddl'::regclass, true, false);

drop table ccddl;

\c regression
drop database column_compression;

------------------------------------------------------------------------------
-- Test to validate block offset is correctly recorded in block directory for
-- column oriented table which has large content blocks.
------------------------------------------------------------------------------
CREATE TABLE col_large_content_block (id BIGINT, arr DOUBLE PRECISION[])
WITH (appendonly=true, orientation=column, blocksize=8192) DISTRIBUTED BY (id);

CREATE INDEX col_large_content_block_idx ON col_large_content_block USING btree (id);

-- insert data to create large content header for CO table
INSERT INTO col_large_content_block
SELECT 1,
       array_fill(1234567890.12, ARRAY[1100])
       FROM generate_series(0, 2);

-- select via index to confirm AO blockdirectory has correct offsets and read
-- succeeds.
SET enable_seqscan TO OFF;
INSERT INTO col_large_content_block
SELECT *
FROM col_large_content_block
WHERE id=1;
SELECT count(*) from col_large_content_block;
SET enable_seqscan TO ON;

------------------------------------------------------------------------------
-- Test to validate block offset is correctly recorded in block directory for
-- column oriented table which has large content blocks, for ADD COLUMN case.
------------------------------------------------------------------------------
CREATE TABLE col_large_content_block_add_col (id BIGINT, id1 INT)
WITH (appendonly=true, orientation=column, blocksize=8192) DISTRIBUTED BY (id);

CREATE INDEX col_large_content_block_add_col_idx ON col_large_content_block_add_col USING btree (id);

INSERT INTO col_large_content_block_add_col VALUES (1,1), (1,2);

-- add column with default value which produces large content block
ALTER TABLE col_large_content_block_add_col ADD COLUMN large_content DOUBLE PRECISION[] DEFAULT '{1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12,1234567890.12}' ENCODING (compresstype=none,blocksize=8192,compresslevel=0);

-- select via index to confirm AO blockdirectory has correct offsets and read
-- succeeds.
SET enable_seqscan TO OFF;
INSERT INTO col_large_content_block_add_col
SELECT *
FROM col_large_content_block_add_col
WHERE id=1;
-- Lets validate above insert worked.
SELECT count(*) from col_large_content_block;
SET enable_seqscan TO ON;
