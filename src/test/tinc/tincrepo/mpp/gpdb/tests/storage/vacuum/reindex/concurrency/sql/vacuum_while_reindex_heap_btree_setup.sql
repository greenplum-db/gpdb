-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
DROP TABLE IF EXISTS reindex_heap;

CREATE TABLE reindex_heap (a INT); 
insert into reindex_heap select generate_series(1,1000);
insert into reindex_heap select generate_series(1,1000);
create index idx_btree_reindex_heap on reindex_heap(a);
