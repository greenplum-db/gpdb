-- disable sequential scan and index scan to force bitmap scan
SET ENABLE_SEQSCAN=OFF;
SET ENABLE_INDEXSCAN=OFF;

-- bpchar_pattern_ops index should be invalid on master and segments
SELECT DISTINCT indisvalid FROM pg_index WHERE indexrelid = 'bpchar_idx'::regclass;
SELECT DISTINCT indisvalid FROM gp_dist_random('pg_index') WHERE indexrelid = 'bpchar_idx'::regclass;
REINDEX TABLE tbl_with_bpchar_pattern_ops_index;

-- bitmap index scan should be used
EXPLAIN (COSTS OFF) SELECT * FROM tbl_with_bpchar_pattern_ops_index WHERE lower(b)::bpchar LIKE '1';
SELECT * FROM tbl_with_bpchar_pattern_ops_index WHERE lower(b)::bpchar LIKE '1';

-- bitmap index should be invalid
SELECT DISTINCT indisvalid FROM pg_index WHERE indexrelid = 'bitmap_idx'::regclass;
SELECT DISTINCT indisvalid FROM gp_dist_random('pg_index') WHERE indexrelid = 'bitmap_idx'::regclass;

-- btree index should be valid
SELECT DISTINCT indisvalid FROM pg_index WHERE indexrelid = 'btree_idx'::regclass;
SELECT DISTINCT indisvalid FROM gp_dist_random('pg_index') WHERE indexrelid = 'btree_idx'::regclass;

REINDEX TABLE tbl_with_bitmap_btree_indexes;

-- bitmap index scan should be used
EXPLAIN (COSTS OFF) SELECT * FROM tbl_with_bitmap_btree_indexes WHERE b = '1';
SELECT * FROM tbl_with_bitmap_btree_indexes WHERE b = '1';

-- enable btree index scan and disable bitmap index scan
SET ENABLE_BITMAPSCAN=OFF;
SET ENABLE_INDEXSCAN=ON;
-- btree index scan should be used
EXPLAIN (COSTS OFF) SELECT * FROM tbl_with_bitmap_btree_indexes WHERE b = '1';
SELECT * FROM tbl_with_bitmap_btree_indexes WHERE b = '1';

