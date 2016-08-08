-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE INDEX pg2_co_gist_idx1_a0 ON pg2_co_table_gist_index_a0 USING GiST (property);
