-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
DROP TABLE IF EXISTS reindex_crtab_heap_bitmap;

CREATE TABLE reindex_crtab_heap_bitmap (a INT); 
insert into reindex_crtab_heap_bitmap select generate_series(1,1000);
insert into reindex_crtab_heap_bitmap select generate_series(1,1000);
create index idx_reindex_crtab_heap_bitmap on reindex_crtab_heap_bitmap USING BITMAP(a);
