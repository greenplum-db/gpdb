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
-- Also, for these tables we check pg_appendonly and pg_attribute_encoding too.
-- AO table:
ALTER TABLE part_relopt SET ACCESS METHOD ao_row WITH (compresstype=zlib, compresslevel=5);
SELECT c.relname, c.reloptions, a.blocksize, a.compresslevel FROM pg_class c LEFT JOIN pg_appendonly a ON a.relid = c.oid WHERE c.relname LIKE 'part_relopt%';
ALTER TABLE part_relopt SET (compresslevel=7);
SELECT c.relname, c.reloptions, a.blocksize, a.compresslevel FROM pg_class c LEFT JOIN pg_appendonly a ON a.relid = c.oid WHERE c.relname LIKE 'part_relopt%';
--AOCO table:
ALTER TABLE part_relopt SET ACCESS METHOD ao_column WITH (compresstype=rle_type, compresslevel=1);
SELECT c.relname, c.reloptions, a.blocksize, a.compresslevel FROM pg_class c LEFT JOIN pg_appendonly a ON a.relid = c.oid WHERE c.relname LIKE 'part_relopt%';
SELECT c.relname, a.attnum, a.attoptions FROM pg_attribute_encoding a, pg_class c WHERE a.attrelid = c.oid AND c.relname LIKE 'part_relopt%';
ALTER TABLE part_relopt SET (compresslevel=3);
SELECT c.relname, c.reloptions, a.blocksize, a.compresslevel FROM pg_class c LEFT JOIN pg_appendonly a ON a.relid = c.oid WHERE c.relname LIKE 'part_relopt%';
SELECT c.relname, a.attnum, a.attoptions FROM pg_attribute_encoding a, pg_class c WHERE a.attrelid = c.oid AND c.relname LIKE 'part_relopt%';

-- Check mixed AMs in the partition hierarchy. Currently we error out.
CREATE TABLE part_relopt2(a int, b int) PARTITION BY RANGE(a);
CREATE TABLE part_1 partition OF part_relopt2 FOR VALUES FROM (100) to (200);
CREATE TABLE part_2 partition OF part_relopt2 FOR VALUES FROM (200) to (300) PARTITION BY RANGE (a);
CREATE TABLE part_2_1 partition OF part_2 FOR VALUES FROM (200) to (250);
-- error out because of the first child
ALTER TABLE part_1 SET ACCESS METHOD ao_row;
ALTER TABLE part_relopt2 SET (fillfactor=70);
-- check subpartition too: error out because of the subpartition child
ALTER TABLE part_1 SET ACCESS METHOD heap;
ALTER TABLE part_2_1 SET ACCESS METHOD ao_row;
ALTER TABLE part_relopt2 SET (fillfactor=70);
-- SET individual child, and check if RESET works. 
ALTER TABLE part_1 SET (fillfactor=70);
ALTER TABLE part_2_1 SET (blocksize=65536);
SELECT c.relname, c.reloptions FROM pg_class c WHERE c.relname = 'part_1' OR c.relname = 'part_2_1';
ALTER TABLE part_relopt2 RESET(fillfactor);
SELECT c.relname, c.reloptions FROM pg_class c WHERE c.relname = 'part_1' OR c.relname = 'part_2_1';
ALTER TABLE part_relopt2 RESET(blocksize);
SELECT c.relname, c.reloptions FROM pg_class c WHERE c.relname = 'part_1' OR c.relname = 'part_2_1';

-- Data is intact
SELECT count(*) FROM part_relopt;

DROP TABLE part_relopt;
DROP TABLE part_relopt2;
