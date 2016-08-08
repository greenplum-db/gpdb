-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE UNIQUE INDEX pg2_heap_unq_idx1_template ON pg2_heap_table_unique_index_template (text_col);
