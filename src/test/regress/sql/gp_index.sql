-- start_matchsubs

-- Note: In init_file there is a regex which will remove any line including
-- "Distributed by", so we cannot check the result e.g. "Distributed by (i)".
-- This regex is to overwrite the regex.
-- m/Distributed by/
-- s/Distributed by/Distributedby/

-- end_matchsubs

--
-- Greenplum disallows concurrent index creation. It allows concurrent index
-- drops, so we want to test for it. Though, due to this difference with
-- upstream we can not keep the tests completely in sync and we add them here.
-- Original tests are in create_index.sql
--
CREATE TABLE tbl_drop_ind_concur (f1 text, f2 text, dk text) distributed by (dk);
CREATE INDEX tbl_drop_index1 ON tbl_drop_ind_concur(f2,f1);
INSERT INTO tbl_drop_ind_concur VALUES  ('a','b', '1');
INSERT INTO tbl_drop_ind_concur VALUES  ('b','b', '1');
INSERT INTO tbl_drop_ind_concur VALUES  ('c','c', '2');
INSERT INTO tbl_drop_ind_concur VALUES  ('d','d', '3');
CREATE UNIQUE INDEX tbl_drop_index2 ON tbl_drop_ind_concur(dk, f1);
CREATE INDEX tbl_drop_index3 on tbl_drop_ind_concur(f2) WHERE f1='a';
CREATE INDEX tbl_drop_index4 on tbl_drop_ind_concur(f2) WHERE f1='x';

DROP INDEX CONCURRENTLY "tbl_drop_index2";				-- works
DROP INDEX CONCURRENTLY IF EXISTS "tbl_drop_index2";		-- notice

-- failures
DROP INDEX CONCURRENTLY "tbl_drop_index2", "tbl_drop_index3";
BEGIN;
DROP INDEX CONCURRENTLY "tbl_drop_index4";
ROLLBACK;

-- successes
DROP INDEX CONCURRENTLY IF EXISTS "tbl_drop_index3";
DROP INDEX CONCURRENTLY "tbl_drop_index4";
DROP INDEX CONCURRENTLY "tbl_drop_index1";

\d tbl_drop_ind_concur

DROP TABLE tbl_drop_ind_concur;

-- Creating UNIQUE/PRIMARY KEY index is disallowed to change the distribution
-- keys implicitly
CREATE TABLE tbl_create_index(i int, j int, k int) distributed by(i, j);
-- should fail
CREATE UNIQUE INDEX ON tbl_create_index(i);
CREATE UNIQUE INDEX ON tbl_create_index(k);
CREATE UNIQUE INDEX ON tbl_create_index(i, k);
ALTER TABLE tbl_create_index ADD CONSTRAINT PKEY PRIMARY KEY(i);
ALTER TABLE tbl_create_index ADD CONSTRAINT PKEY PRIMARY KEY(k);
ALTER TABLE tbl_create_index ADD CONSTRAINT PKEY PRIMARY KEY(i, k);
-- should success
CREATE UNIQUE INDEX tbl_create_index_ij ON tbl_create_index(i, j);
CREATE UNIQUE INDEX tbl_create_index_ijk ON tbl_create_index(i, j, k);
\d tbl_create_index
DROP INDEX tbl_create_index_ij;
DROP INDEX tbl_create_index_ijk;

ALTER TABLE tbl_create_index ADD CONSTRAINT PKEY PRIMARY KEY(i, j, k);
\d tbl_create_index
ALTER TABLE tbl_create_index DROP CONSTRAINT PKEY;

-- after changing the distribution keys, the above failed clause should success
ALTER TABLE tbl_create_index SET DISTRIBUTED BY(k);
CREATE UNIQUE INDEX ON tbl_create_index(k);
CREATE UNIQUE INDEX ON tbl_create_index(i, k);
ALTER TABLE tbl_create_index ADD CONSTRAINT PKEY PRIMARY KEY(i, k);
\d tbl_create_index

DROP TABLE tbl_create_index;

-- create partition table with dist keys (a,b,c)
CREATE TABLE foo1 (a int, b int, c int)  DISTRIBUTED BY (a,b,c) PARTITION BY RANGE(a)
(PARTITION p1 START (1) END (10000) INCLUSIVE,
PARTITION p2 START (10001) END (100000) INCLUSIVE,
PARTITION p3 START (100001) END (1000000) INCLUSIVE);

-- create unique index with same keys but different order (a,c,b)
create unique index acb_idx on public.foo1 using btree(a,c,b);

-- alter table by add partition
alter table public.foo1 add partition p4 START (1000001) END (2000000) INCLUSIVE;

-- check the status of the new partition: new dist keys should be consistent
-- to the parent table
\d+ foo1_1_prt_p4

-- alter table by split partition
alter table public.foo1 split partition p1 at(500) into (partition p1_0, partition p1_1);

-- check the status of the split partitions: new dist keys should be consistent
-- to the parent table
\d+ foo1_1_prt_p1_0
\d+ foo1_1_prt_p1_1

DROP TABLE foo1;

-- Coverage to ensure that reltuples, relpages and relallvisible are updated
-- correctly upon an index build (i.e. CREATE INDEX) on heap tables.
-- Note: relallvisible is not maintained for indexes.

CREATE TABLE index_build_relstats_heap(a int);
INSERT INTO index_build_relstats_heap SELECT generate_series(1, 10);

CREATE INDEX ON index_build_relstats_heap(a);

-- Validate QEs
SELECT gp_segment_id, count(*) FROM index_build_relstats_heap
GROUP BY gp_segment_id ORDER BY gp_segment_id;
SELECT gp_segment_id, reltuples, relpages, relallvisible FROM gp_dist_random('pg_class')
WHERE relname='index_build_relstats_heap' ORDER BY gp_segment_id;
SELECT gp_segment_id, reltuples, relpages, relallvisible FROM gp_dist_random('pg_class')
WHERE relname='index_build_relstats_heap_a_idx' ORDER BY gp_segment_id;
-- Validate on QD
SELECT reltuples, relpages, relallvisible FROM pg_class WHERE relname='index_build_relstats_heap';
SELECT reltuples, relpages, relallvisible FROM pg_class WHERE relname='index_build_relstats_heap_a_idx';

-- Run VACUUM to populate relallvisible.
VACUUM index_build_relstats_heap;

-- Validate QEs
SELECT gp_segment_id, relallvisible FROM gp_dist_random('pg_class')
WHERE relname='index_build_relstats_heap' ORDER BY gp_segment_id;
SELECT gp_segment_id, relallvisible FROM gp_dist_random('pg_class')
WHERE relname='index_build_relstats_heap_a_idx' ORDER BY gp_segment_id;
-- Validate on QD
SELECT relallvisible FROM pg_class WHERE relname='index_build_relstats_heap';
SELECT relallvisible FROM pg_class WHERE relname='index_build_relstats_heap_a_idx';

-- Now drop the index and re-build.
DROP INDEX index_build_relstats_heap_a_idx;
CREATE INDEX ON index_build_relstats_heap(a);

-- Now check that relallvisible remains the same on QEs and QDs.
-- Validate QEs
SELECT gp_segment_id, relallvisible FROM gp_dist_random('pg_class')
WHERE relname='index_build_relstats_heap' ORDER BY gp_segment_id;
SELECT gp_segment_id, relallvisible FROM gp_dist_random('pg_class')
WHERE relname='index_build_relstats_heap_a_idx' ORDER BY gp_segment_id;
-- Validate on QD
SELECT relallvisible FROM pg_class WHERE relname='index_build_relstats_heap';
SELECT relallvisible FROM pg_class WHERE relname='index_build_relstats_heap_a_idx';

-- Limitation: If even one QE is empty in terms of reltuples, we will not update
-- the relstats on the QD, even though they are updated on the QEs.
CREATE TABLE index_build_relstats_heap_skew(a int);
CREATE INDEX ON index_build_relstats_heap_skew(a);

-- Segs 0 and 2 will be empty
INSERT INTO index_build_relstats_heap SELECT 1 FROM generate_series(1, 10) i;

-- Validate QEs
SELECT gp_segment_id, count(*) FROM index_build_relstats_heap
GROUP BY gp_segment_id ORDER BY gp_segment_id;
SELECT gp_segment_id, reltuples, relpages, relallvisible FROM gp_dist_random('pg_class')
WHERE relname='index_build_relstats_heap_skew' ORDER BY gp_segment_id;
SELECT gp_segment_id, reltuples, relpages, relallvisible FROM gp_dist_random('pg_class')
WHERE relname='index_build_relstats_heap_skew_a_idx' ORDER BY gp_segment_id;

-- Validate on QD
SELECT reltuples, relpages, relallvisible FROM pg_class WHERE relname='index_build_relstats_heap_skew';
SELECT reltuples, relpages, relallvisible FROM pg_class WHERE relname='index_build_relstats_heap_skew_a_idx';
