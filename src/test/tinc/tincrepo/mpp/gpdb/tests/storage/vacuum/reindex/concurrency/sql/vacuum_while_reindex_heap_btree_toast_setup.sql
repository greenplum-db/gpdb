-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
DROP TABLE IF EXISTS reindex_toast_heap;

CREATE TABLE reindex_toast_heap (a text, b int); 
alter table reindex_toast_heap alter column a set storage external;
insert into reindex_toast_heap select repeat('123456789',10000), i from generate_series(1,100) i;
create index idx_btree_reindex_toast_heap on reindex_toast_heap(b);
