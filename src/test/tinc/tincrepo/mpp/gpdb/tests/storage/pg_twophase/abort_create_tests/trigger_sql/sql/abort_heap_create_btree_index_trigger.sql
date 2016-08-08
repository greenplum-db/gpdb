-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE INDEX cr_heap_btree_idx1 ON cr_heap_table_btree_index (numeric_col);
