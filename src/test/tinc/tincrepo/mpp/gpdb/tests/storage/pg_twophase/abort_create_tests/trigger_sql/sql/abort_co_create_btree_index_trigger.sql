-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE INDEX cr_co_btree_idx1 ON cr_co_table_btree_index (numeric_col);
