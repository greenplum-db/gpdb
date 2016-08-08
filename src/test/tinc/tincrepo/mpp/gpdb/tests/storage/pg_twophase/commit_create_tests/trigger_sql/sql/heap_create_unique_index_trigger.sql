-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE UNIQUE INDEX cr_heap_unq_idx1 ON cr_heap_table_unique_index (numeric_col);
