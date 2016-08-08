-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
\c gpreindextest;
reindex table heap_add_index;
