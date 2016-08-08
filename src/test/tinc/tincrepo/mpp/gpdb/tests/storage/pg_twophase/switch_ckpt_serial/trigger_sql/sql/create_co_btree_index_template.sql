-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE INDEX pg2_co_btree_idx1_template ON pg2_co_table_btree_index_template (numeric_col);
