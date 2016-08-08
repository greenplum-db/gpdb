-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE INDEX cr_ao_btree_idx1 ON cr_ao_table_btree_index (numeric_col);
