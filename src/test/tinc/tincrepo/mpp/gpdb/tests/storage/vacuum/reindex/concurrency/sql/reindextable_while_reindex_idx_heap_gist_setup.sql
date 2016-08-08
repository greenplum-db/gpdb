-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
DROP TABLE IF EXISTS reindex_crtab_heap_gist;

CREATE TABLE reindex_crtab_heap_gist (id INTEGER, box1 BOX) WITH (appendonly=true, orientation=column);
insert into reindex_crtab_heap_gist (id, box1) VALUES (300, '( (1, 1), (2, 2) )');
insert into reindex_crtab_heap_gist (id, box1) VALUES (301, '( (3, 4), (4, 5) )');
insert into reindex_crtab_heap_gist (id, box1) VALUES (304, '( (2, 2), (4, 4) )');
create index idx_reindex_crtab_heap_gist on reindex_crtab_heap_gist USING Gist(box1);
select 1 as relfilenode_same_on_all_segs from gp_dist_random('pg_class')   where relname = 'idx_reindex_crtab_heap_gist' group by relfilenode having count(*) = (select count(*) from gp_segment_configuration where role='p' and content > -1);

