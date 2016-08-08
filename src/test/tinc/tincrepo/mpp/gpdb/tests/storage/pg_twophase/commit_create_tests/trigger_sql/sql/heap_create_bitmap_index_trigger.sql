-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE INDEX cr_heap_bitmap_idx1 ON cr_heap_table_bitmap_index USING bitmap (numeric_col);
