-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE INDEX cr_heap_gist_idx1 ON cr_heap_table_gist_index USING GiST (property);
