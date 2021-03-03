--
-- Test, run Bitmap Heap Scan on AO/AOCS table's sparse bitmap index.
-- Here start two transactions in the same time to insert tuples
-- into different seg file. Then create bitmap index on it.
-- This will lead to very sparse bitmap index.
-- Since the tid in bitmap index for AO is composed of segfile no,
-- and row no.
--

-- Test AO table.
CREATE TABLE ao_sparse (id int) with(appendonly = true) DISTRIBUTED BY (id);

1: begin;
2: begin;

1: INSERT INTO ao_sparse SELECT i%10000 FROM generate_series(1, 1000000) AS i;
2: INSERT INTO ao_sparse SELECT i%10000 FROM generate_series(1, 1000000) AS i;

1: commit;
2: commit;

-- Let's check the total tuple count with id=97,99 without bitmap index.
SELECT count(*) FROM ao_sparse WHERE id >= 97 and id <= 99 and gp_segment_id = 0;

CREATE INDEX idx_ao_sparse_id ON ao_sparse USING bitmap (id);


-- Should generate Bitmap Heap Scan on the bitmap index.
1: set optimizer = off;
1: EXPLAIN (COSTS OFF) SELECT * FROM ao_sparse WHERE id >= 97 and id <= 99 and gp_segment_id = 0;

-- We used to hit assertion failure since it generates a empty bitmap for a block's PagetableEntry.
-- In BitmapHeapNext, if table_scan_bitmap_next_block returns false(which means the block should be
-- skipped), but we still try to fetch tuple through table_scan_bitmap_next_tuple, and it didn't find
-- the PagetableEntry is empty.
-- This error happens only when we fetch multiple LOVs when doing bitmap heap scan on bitmap index for
-- AO tables. AOCS table and "SELECT count(*) FROM ao_sparse WHERE id = 97" works fine.
1: SELECT count(*) FROM ao_sparse WHERE id >= 97 and id <= 99 and gp_segment_id = 0;

-- This query doesn't have any issue.
1: SELECT count(*) FROM ao_sparse WHERE id = 97;


-- Test AOCS table.
CREATE TABLE aocs_sparse (id int) with(appendonly = true, orientation = COLUMN) DISTRIBUTED BY (id);

1: begin;
2: begin;

1: INSERT INTO aocs_sparse SELECT i%10000 FROM generate_series(1, 1000000) AS i;
2: INSERT INTO aocs_sparse SELECT i%10000 FROM generate_series(1, 1000000) AS i;

1: commit;
2: commit;

-- Let's check the total tuple count with id=97 without bitmap index.
SELECT count(*) FROM aocs_sparse WHERE id >= 97 and id <= 99 and gp_segment_id = 0;

CREATE INDEX idx_ao_sparse_id ON aocs_sparse USING bitmap (id);


-- Should generate Bitmap Heap Scan on the bitmap index.
1: EXPLAIN (COSTS OFF) SELECT * FROM aocs_sparse WHERE id >= 97 and id <= 99 and gp_segment_id = 0;

-- This doesn't have any issue, but let's make sure it will not make any error in future.
1: SELECT count(*) FROM aocs_sparse WHERE id >= 97 and id <= 99 and gp_segment_id = 0;

-- This query doesn't have any issue.
1: SELECT count(*) FROM ao_sparse WHERE id = 97;

