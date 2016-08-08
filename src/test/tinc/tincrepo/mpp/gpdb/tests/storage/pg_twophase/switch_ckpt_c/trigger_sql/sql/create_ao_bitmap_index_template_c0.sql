-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE INDEX pg2_ao_bitmap_idx1_c0 ON pg2_ao_table_bitmap_index_c0 USING bitmap (numeric_col);
