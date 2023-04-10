--
-- BTREE_INDEX
-- test retrieval of min/max keys for each index
--

SELECT b.*
   FROM bt_i4_heap b
   WHERE b.seqno < 1;

SELECT b.*
   FROM bt_i4_heap b
   WHERE b.seqno >= 9999;

SELECT b.*
   FROM bt_i4_heap b
   WHERE b.seqno = 4500;

SELECT b.*
   FROM bt_name_heap b
   WHERE b.seqno < '1'::name;

SELECT b.*
   FROM bt_name_heap b
   WHERE b.seqno >= '9999'::name;

SELECT b.*
   FROM bt_name_heap b
   WHERE b.seqno = '4500'::name;

SELECT b.*
   FROM bt_txt_heap b
   WHERE b.seqno < '1'::text;

SELECT b.*
   FROM bt_txt_heap b
   WHERE b.seqno >= '9999'::text;

SELECT b.*
   FROM bt_txt_heap b
   WHERE b.seqno = '4500'::text;

SELECT b.*
   FROM bt_f8_heap b
   WHERE b.seqno < '1'::float8;

SELECT b.*
   FROM bt_f8_heap b
   WHERE b.seqno >= '9999'::float8;

SELECT b.*
   FROM bt_f8_heap b
   WHERE b.seqno = '4500'::float8;

--
-- Check correct optimization of LIKE (special index operator support)
-- for both indexscan and bitmapscan cases
--

set enable_seqscan to false;
set enable_indexscan to true;
set enable_bitmapscan to false;
set enable_sort to false; -- GPDB needs more strong-arming to get same plans as upstream
explain (costs off)
select proname from pg_proc where proname like E'RI\\_FKey%del' order by 1;
select proname from pg_proc where proname like E'RI\\_FKey%del' order by 1;
explain (costs off)
select proname from pg_proc where proname ilike '00%foo' order by 1;
select proname from pg_proc where proname ilike '00%foo' order by 1;
explain (costs off)
select proname from pg_proc where proname ilike 'ri%foo' order by 1;

set enable_indexscan to false;
set enable_bitmapscan to true;
reset enable_sort;
explain (costs off)
select proname from pg_proc where proname like E'RI\\_FKey%del' order by 1;
select proname from pg_proc where proname like E'RI\\_FKey%del' order by 1;
explain (costs off)
select proname from pg_proc where proname ilike '00%foo' order by 1;
select proname from pg_proc where proname ilike '00%foo' order by 1;
set enable_sort to false; -- GPDB needs more strong-arming to get same plans as upstream
set enable_bitmapscan to false;
explain (costs off)
select proname from pg_proc where proname ilike 'ri%foo' order by 1;

reset enable_seqscan;
reset enable_indexscan;
reset enable_bitmapscan;
reset enable_sort;

-- Also check LIKE optimization with binary-compatible cases

create temp table btree_bpchar (f1 text collate "C");
create index on btree_bpchar(f1 bpchar_ops);
insert into btree_bpchar values ('foo'), ('foo  '), ('fool'), ('bar'), ('quux');
-- doesn't match index:
explain (costs off)
select * from btree_bpchar where f1 like 'foo';
select * from btree_bpchar where f1 like 'foo';
explain (costs off)
select * from btree_bpchar where f1 like 'foo%';
select * from btree_bpchar where f1 like 'foo%';
-- these do match the index:
explain (costs off)
select * from btree_bpchar where f1::bpchar like 'foo';
select * from btree_bpchar where f1::bpchar like 'foo';
explain (costs off)
select * from btree_bpchar where f1::bpchar like 'foo%';
select * from btree_bpchar where f1::bpchar like 'foo%';

explain (costs off)
select * from btree_bpchar where f1::bpchar ='foo';
select * from btree_bpchar where f1::bpchar ='foo';
--
-- Test B-tree fast path (cache rightmost leaf page) optimization.
--

-- First create a tree that's at least three levels deep (i.e. has one level
-- between the root and leaf levels). The text inserted is long.  It won't be
-- compressed because we use plain storage in the table.  Only a few index
-- tuples fit on each internal page, allowing us to get a tall tree with few
-- pages.  (A tall tree is required to trigger caching.)
--
-- The text column must be the leading column in the index, since suffix
-- truncation would otherwise truncate tuples on internal pages, leaving us
-- with a short tree.
create table btree_tall_tbl(id int4, t text);
alter table btree_tall_tbl alter COLUMN t set storage plain;
create index btree_tall_idx on btree_tall_tbl (t, id) with (fillfactor = 10);
insert into btree_tall_tbl select g, repeat('x', 250)
from generate_series(1, 130) g;

--
-- Test vacuum_cleanup_index_scale_factor
--

-- Simple create
create table btree_test(a int);
create index btree_idx1 on btree_test(a) with (vacuum_cleanup_index_scale_factor = 40.0);
select reloptions from pg_class WHERE oid = 'btree_idx1'::regclass;

-- Fail while setting improper values
create index btree_idx_err on btree_test(a) with (vacuum_cleanup_index_scale_factor = -10.0);
create index btree_idx_err on btree_test(a) with (vacuum_cleanup_index_scale_factor = 100.0);
create index btree_idx_err on btree_test(a) with (vacuum_cleanup_index_scale_factor = 'string');
create index btree_idx_err on btree_test(a) with (vacuum_cleanup_index_scale_factor = true);

-- Simple ALTER INDEX
alter index btree_idx1 set (vacuum_cleanup_index_scale_factor = 70.0);
select reloptions from pg_class WHERE oid = 'btree_idx1'::regclass;

--
-- Test for multilevel page deletion
--
CREATE TABLE delete_test_table (a bigint, b bigint, c bigint, d bigint);
INSERT INTO delete_test_table SELECT i, 1, 2, 3 FROM generate_series(1,80000) i;
ALTER TABLE delete_test_table ADD PRIMARY KEY (a,b,c,d);
-- Delete most entries, and vacuum, deleting internal pages and creating "fast
-- root"
DELETE FROM delete_test_table WHERE a < 79990;
VACUUM delete_test_table;

--
-- Test B-tree insertion with a metapage update (XLOG_BTREE_INSERT_META
-- WAL record type). This happens when a "fast root" page is split.  This
-- also creates coverage for nbtree FSM page recycling.
--
-- The vacuum above should've turned the leaf page into a fast root. We just
-- need to insert some rows to cause the fast root page to split.
INSERT INTO delete_test_table SELECT i, 1, 2, 3 FROM generate_series(1,1000) i;

--
-- GPDB: Test correctness of B-tree stats in consecutively VACUUM.
--
CREATE TABLE btree_stats_tbl(col_int int, col_text text, col_numeric numeric, col_unq int) DISTRIBUTED BY (col_int);
CREATE INDEX btree_stats_idx ON btree_stats_tbl(col_int);
INSERT INTO btree_stats_tbl VALUES (1, 'aa', 1001, 101), (2, 'bb', 1002, 102);
SELECT reltuples FROM pg_class WHERE relname='btree_stats_tbl';
-- inspect the state of the stats on segments
SELECT gp_segment_id, relname, reltuples FROM gp_dist_random('pg_class') WHERE relname = 'btree_stats_idx';
SELECT reltuples FROM pg_class WHERE relname='btree_stats_idx';
-- 1st VACUUM, expect reltuples = 2
vacuum btree_stats_tbl;
SELECT reltuples FROM pg_class WHERE relname='btree_stats_tbl';
-- inspect the state of the stats on segments
SELECT gp_segment_id, relname, reltuples FROM gp_dist_random('pg_class') WHERE relname = 'btree_stats_idx';
SELECT reltuples FROM pg_class WHERE relname='btree_stats_idx';
-- 2nd VACUUM, expect reltuples = 2
vacuum btree_stats_tbl;
SELECT reltuples FROM pg_class WHERE relname='btree_stats_tbl';
-- inspect the state of the stats on segments
SELECT gp_segment_id, relname, reltuples FROM gp_dist_random('pg_class') WHERE relname = 'btree_stats_idx';
SELECT reltuples FROM pg_class WHERE relname='btree_stats_idx';

-- Prior to this fix, the case would be failed here. Given the
-- scenario of updating stats during VACUUM:
-- 1) coordinator vacuums and updates stats of its own;
-- 2) then coordinator dispatches vacuum to segments;
-- 3) coordinator combines stats received from segments to overwrite the stats of its own.
-- Because upstream introduced a feature which could skip full index scan uring cleanup
-- of B-tree indexes when possible (refer to:
-- https://github.com/postgres/postgres/commit/857f9c36cda520030381bd8c2af20adf0ce0e1d4),
-- there was a case in QD-QEs distributed deployment that some QEs could skip full index scan and
-- stop updating statistics, result in QD being unable to collect all QEs' stats thus overwrote
-- a paritial accumulated value to index->reltuples. More interesting, it usually happened starting
-- from the 3rd time of consecutively VACUUM after fresh inserts due to above skipping index scan
-- criteria.
-- 3rd VACUUM, expect reltuples = 2
vacuum btree_stats_tbl;
SELECT reltuples FROM pg_class WHERE relname='btree_stats_tbl';
-- inspect the state of the stats on segments
SELECT gp_segment_id, relname, reltuples FROM gp_dist_random('pg_class') WHERE relname = 'btree_stats_idx';
SELECT reltuples FROM pg_class WHERE relname='btree_stats_idx';
