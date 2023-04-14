-- https://github.com/greenplum-db/gpdb/issues/1109
--
-- ALTER TABLE ... SET WITH (REORGANIZE = true); should always redistribute the
-- data even with matching distribution policy
create table ats_dist_by_c (c int, d int) distributed by (c);
create table ats_dist_by_d (c int, d int) distributed by (d);

insert into ats_dist_by_c select i, 0 from generate_series(1, 47) i;
copy ats_dist_by_c to '/tmp/ats_dist_by_c<SEGID>' on segment;

-- load the data back from the segment file, but wrong distribution
set gp_enable_segment_copy_checking = false;
show gp_enable_segment_copy_checking;
copy ats_dist_by_d from '/tmp/ats_dist_by_c<SEGID>' on segment;

-- try to use the reorganize = true to fix it
alter table ats_dist_by_d set with (reorganize = true);
-- construct expected table
create table ats_expected_by_d (c int, d int) distributed by (d);
insert into ats_expected_by_d select * from ats_dist_by_c;
-- expect to see data distributed in the same way as the freshly constructed
-- table
select count(*) = 0 as has_same_distribution from
(select gp_segment_id, * from ats_dist_by_d except
	select gp_segment_id, * from ats_expected_by_d) t;

-- reload for random distribution test
truncate table ats_dist_by_d;
copy ats_dist_by_d from '/tmp/ats_dist_by_c<SEGID>' on segment;

-- we expect the new random distribution to differ from both the
-- distributed-by-c table and the distributed-by-d table
alter table ats_dist_by_d set with (reorganize = true) distributed randomly;
select count(*) > 0 as has_different_distribution from
(select gp_segment_id, * from ats_dist_by_d except
	select gp_segment_id, * from ats_dist_by_c) t;
select count(*) > 0 as has_different_distribution from
(select gp_segment_id, * from ats_dist_by_d except
	select gp_segment_id, * from ats_expected_by_d) t;

-- ALTER TABLE ... SET for partition tables
PREPARE attribute_encoding_check AS
SELECT c.relname, a.attname, e.filenum, e.attoptions FROM pg_attribute_encoding e, pg_class c, pg_attribute a
WHERE e.attrelid = c.oid AND e.attnum = a.attnum and e.attrelid = a.attrelid AND c.relname LIKE $1;

CREATE TABLE part_relopt(a int, b int) WITH (fillfactor=90) PARTITION BY RANGE(a);
CREATE TABLE part_relopt_1 partition OF part_relopt FOR VALUES FROM (1) to (100);
CREATE TABLE part_relopt_2 partition OF part_relopt FOR VALUES FROM (100) to (200) PARTITION BY RANGE(a);
CREATE TABLE part_relopt_2_1 partition OF part_relopt_2 FOR VALUES FROM (100) to (150);
CREATE TABLE part_relopt_2_2 partition OF part_relopt_2 FOR VALUES FROM (150) to (200);
INSERT INTO part_relopt SELECT i,i FROM generate_series(1,199) i;

-- All tables inherit the same reloptions as the root.
SELECT c.relname, c.reloptions FROM pg_class c WHERE c.relname LIKE 'part_relopt%';

-- Set reloptions of the root, all child including subpartitions should inherit them too.
ALTER TABLE part_relopt SET (fillfactor=80);
SELECT c.relname, c.reloptions FROM pg_class c WHERE c.relname LIKE 'part_relopt%';

-- Altering it again with the same reloptions. Nothing should change.
ALTER TABLE part_relopt SET (fillfactor=80);
SELECT c.relname, c.reloptions FROM pg_class c WHERE c.relname LIKE 'part_relopt%';

-- Altering a subpartition root, should apply and only apply to the subpartition root/child.
ALTER TABLE part_relopt_2 SET (fillfactor=70);
SELECT c.relname, c.reloptions FROM pg_class c WHERE c.relname LIKE 'part_relopt%';

-- Altering the partition root but with ONLY keyword. Should affect future child but not affect existing child.
ALTER TABLE ONLY part_relopt SET (fillfactor=60);
CREATE TABLE part_relopt_3 partition OF part_relopt FOR VALUES FROM (200) to (300);
SELECT c.relname, c.reloptions FROM pg_class c WHERE c.relname LIKE 'part_relopt%';

-- Altering one child partition, should only affect that child.
ALTER TABLE part_relopt_1 SET (fillfactor=50);
SELECT c.relname, c.reloptions FROM pg_class c WHERE c.relname LIKE 'part_relopt%';

-- RESET one subpartition, then the entire hierarchy
ALTER TABLE part_relopt_2 RESET(fillfactor);
SELECT c.relname, c.reloptions FROM pg_class c WHERE c.relname LIKE 'part_relopt%';
ALTER TABLE part_relopt RESET(fillfactor);
SELECT c.relname, c.reloptions FROM pg_class c WHERE c.relname LIKE 'part_relopt%';

-- Check setting reloptions for AO and AOCO tables too.
-- Also, for these tables we check pg_class and pg_attribute_encoding too.
-- AO table:
ALTER TABLE part_relopt SET ACCESS METHOD ao_row WITH (compresstype=zlib, compresslevel=5);
SELECT c.relname, am.amname, c.reloptions FROM pg_class c JOIN pg_am am on am.oid=c.relam WHERE c.relname LIKE 'part_relopt%';
ALTER TABLE part_relopt SET (compresslevel=7);
SELECT c.relname, am.amname, c.reloptions FROM pg_class c JOIN pg_am am on am.oid=c.relam WHERE c.relname LIKE 'part_relopt%';
--AOCO table: Also check setting column encoding
ALTER TABLE part_relopt SET ACCESS METHOD ao_column WITH (compresstype=rle_type, compresslevel=1), ALTER COLUMN a SET ENCODING (compresstype=rle_type, compresslevel=2), ALTER COLUMN b SET ENCODING (compresstype=zlib, compresslevel=3);
SELECT c.relname, am.amname, c.reloptions FROM pg_class c JOIN pg_am am on am.oid=c.relam WHERE c.relname LIKE 'part_relopt%';
execute attribute_encoding_check('part_relopt%');
ALTER TABLE part_relopt SET (compresslevel=3);
SELECT c.relname, am.amname, c.reloptions FROM pg_class c JOIN pg_am am on am.oid=c.relam WHERE c.relname LIKE 'part_relopt%';
execute attribute_encoding_check('part_relopt%');
ALTER TABLE part_relopt ALTER COLUMN a SET ENCODING (compresstype=rle_type, compresslevel=4), ALTER COLUMN b SET ENCODING (compresstype=zlib, compresslevel=5);
execute attribute_encoding_check('part_relopt%');
-- Check double edit on same column
ALTER TABLE part_relopt ALTER COLUMN a SET ENCODING (compresstype=rle_type, compresslevel=4), ALTER COLUMN a SET ENCODING (compresstype=zlib, compresslevel=5);
execute attribute_encoding_check('part_relopt%');
-- Check double edit of same option
ALTER TABLE part_relopt ALTER COLUMN a SET ENCODING (compresslevel=3, compresslevel=4);
execute attribute_encoding_check('part_relopt%');
-- Check double edit of same column different options
ALTER TABLE part_relopt ALTER COLUMN a SET ENCODING (compresstype=zlib), ALTER COLUMN a SET ENCODING (compresslevel=7);
execute attribute_encoding_check('part_relopt%');

ALTER TABLE part_relopt ALTER COLUMN a SET ENCODING (compresstype=zlib, compresslevel=5);
execute attribute_encoding_check('part_relopt%');


-- Data is intact
SELECT * from part_relopt order by a limit 10;
SELECT count(*) FROM part_relopt;


-- with empty segfiles
CREATE TABLE aoco_relopt(a int, b int) WITH (appendoptimized = true, orientation = column);
ALTER TABLE aoco_relopt ALTER COLUMN a SET ENCODING (compresstype=rle_type, compresslevel=4), ALTER COLUMN b SET ENCODING (compresstype=zlib, compresslevel=5);
execute attribute_encoding_check('aoco_relopt%');

DROP TABLE aoco_relopt;

-- Check mixed AMs in the partition hierarchy. Currently we error out.
CREATE TABLE part_relopt2(a int, b int) PARTITION BY RANGE(a);
CREATE TABLE part_relopt2_1 partition OF part_relopt2 FOR VALUES FROM (100) to (200);
CREATE TABLE part_relopt2_2 partition OF part_relopt2 FOR VALUES FROM (200) to (300) PARTITION BY RANGE (a);
CREATE TABLE part_relopt2_2_1 partition OF part_relopt2_2 FOR VALUES FROM (200) to (250);
-- error out because of the first child
ALTER TABLE part_relopt2_1 SET ACCESS METHOD ao_row;
ALTER TABLE part_relopt2 SET (fillfactor=70);
-- check subpartition too: error out because of the subpartition child
ALTER TABLE part_relopt2_1 SET ACCESS METHOD heap;
ALTER TABLE part_relopt2_2_1 SET ACCESS METHOD ao_row;
ALTER TABLE part_relopt2 SET (fillfactor=70);
-- SET individual child, and check if RESET works.
ALTER TABLE part_relopt2_1 SET (fillfactor=70);
ALTER TABLE part_relopt2_2_1 SET (blocksize=65536);
SELECT c.relname, c.reloptions FROM pg_class c WHERE c.relname = 'part_relopt2_1' OR c.relname = 'part_relopt2_2_1';
ALTER TABLE part_relopt2 RESET(fillfactor);
SELECT c.relname, c.reloptions FROM pg_class c WHERE c.relname = 'part_relopt2_1' OR c.relname = 'part_relopt2_2_1';
ALTER TABLE part_relopt2 RESET(blocksize);
SELECT c.relname, c.reloptions FROM pg_class c WHERE c.relname = 'part_relopt2_1' OR c.relname = 'part_relopt2_2_1';

DROP TABLE part_relopt2;

-- Check mixed AM aoco column encoding
CREATE TABLE part_relopt3(a int, b int) WITH (appendoptimized = true, orientation = column, compresstype=zlib, compresslevel=5) PARTITION BY RANGE(a);
CREATE TABLE part_relopt3_1 partition OF part_relopt3 FOR VALUES FROM (100) to (200);
CREATE TABLE part_relopt3_2 partition OF part_relopt3 FOR VALUES FROM (200) to (300) PARTITION BY RANGE (a);
CREATE TABLE part_relopt3_2_1 partition OF part_relopt3_2 FOR VALUES FROM (200) to (250);
INSERT INTO part_relopt3 SELECT i,i FROM generate_series(101,249) i;
execute attribute_encoding_check('part_relopt3%');
-- error out because of the first child
ALTER TABLE part_relopt3_1 SET ACCESS METHOD heap;
ALTER TABLE part_relopt3 ALTER COLUMN a SET ENCODING (compresslevel=3, compresstype=zlib);
-- check subpartition too: error out because of the subpartition child
ALTER TABLE part_relopt3_1 SET ACCESS METHOD ao_column;
ALTER TABLE part_relopt3_2_1 SET ACCESS METHOD heap;

ALTER TABLE part_relopt3 ALTER COLUMN a SET ENCODING (compresslevel=3, compresstype=zlib);
execute attribute_encoding_check('part_relopt3%');

ALTER TABLE part_relopt3_1 ALTER COLUMN a SET ENCODING (compresslevel=3, compresstype=zlib);
execute attribute_encoding_check('part_relopt3%');

ALTER TABLE part_relopt3_2_1 SET ACCESS METHOD ao_column, ALTER COLUMN a SET ENCODING (compresslevel=4, compresstype=zlib);
execute attribute_encoding_check('part_relopt3%');
ALTER TABLE part_relopt3_2 ALTER COLUMN a SET ENCODING (compresslevel=1, compresstype=rle_type);
execute attribute_encoding_check('part_relopt3%');
ALTER TABLE part_relopt3 ALTER COLUMN a SET ENCODING (compresslevel=4, compresstype=rle_type);
execute attribute_encoding_check('part_relopt3%');

-- Check if table is rewritten if encoding doesn't change
CREATE TEMP TABLE relfilebeforeredo AS
SELECT -1 segid, relname, relfilenode FROM pg_class WHERE relname LIKE 'part_relopt3%'
UNION SELECT gp_segment_id segid, relname, relfilenode FROM gp_dist_random('pg_class')
WHERE relname LIKE 'part_relopt3%' ORDER BY segid;

ALTER TABLE part_relopt3 ALTER COLUMN a SET ENCODING (compresslevel=4, compresstype=rle_type);
execute attribute_encoding_check('part_relopt3%');

CREATE TEMP TABLE relfileafterredo AS
SELECT -1 segid, relname, relfilenode FROM pg_class WHERE relname LIKE 'part_relopt3%'
UNION SELECT gp_segment_id segid, relname, relfilenode FROM gp_dist_random('pg_class')
WHERE relname LIKE 'part_relopt3%' ORDER BY segid;

-- Alter table shouldn't have rewritten the table since the options aren't changing
SELECT * FROM relfilebeforeredo EXCEPT SELECT * FROM relfileafterredo;

-- Check alter column set encoding for root partition ONLY
ALTER TABLE ONLY part_relopt3 ALTER COLUMN a SET ENCODING (compresslevel=2, compresstype=zlib);
execute attribute_encoding_check('part_relopt3%');

-- Check alter column set encoding for mid level root partition ONLY
ALTER TABLE ONLY part_relopt3_2 ALTER COLUMN a SET ENCODING (compresslevel=3, compresstype=zlib);
execute attribute_encoding_check('part_relopt3%');

-- altering when changing am to heap should error out for root partition
ALTER TABLE part_relopt3 SET ACCESS METHOD heap, ALTER COLUMN a SET ENCODING (compresslevel=4, compresstype=rle_type);
execute attribute_encoding_check('part_relopt3%');

-- altering when changing am to heap should error out for child partition
ALTER TABLE part_relopt3_2_1 SET ACCESS METHOD heap, ALTER COLUMN a SET ENCODING (compresslevel=4, compresstype=rle_type);
execute attribute_encoding_check('part_relopt3%');

-- set access method and then alter column set encoding should generate same encoding values as
-- set access method + alter column set encoding
ALTER TABLE part_relopt3 SET ACCESS METHOD heap;
ALTER TABLE part_relopt3 SET ACCESS METHOD ao_column, ALTER COLUMN a SET ENCODING (compresslevel=4);
execute attribute_encoding_check('part_relopt3%');

ALTER TABLE part_relopt3 SET ACCESS METHOD heap;
ALTER TABLE part_relopt3 SET ACCESS METHOD ao_column;
ALTER TABLE part_relopt3 ALTER COLUMN a SET ENCODING (compresslevel=4);
execute attribute_encoding_check('part_relopt3%');

DROP TABLE part_relopt3;
