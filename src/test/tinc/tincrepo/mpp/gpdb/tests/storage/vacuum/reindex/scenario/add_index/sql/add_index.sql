-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
\c gpreindextest;

--Heap tables with indexes
create index a4_idx on heap_add_index(a);
