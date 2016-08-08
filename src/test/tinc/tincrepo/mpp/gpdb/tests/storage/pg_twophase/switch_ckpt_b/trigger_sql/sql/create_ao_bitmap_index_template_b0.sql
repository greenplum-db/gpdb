-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE INDEX pg2_ao_bitmap_idx1_b0 ON pg2_ao_table_bitmap_index_b0 USING bitmap (numeric_col);
