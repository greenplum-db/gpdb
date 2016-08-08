-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
DROP TABLE IF EXISTS reindex_aoco;

CREATE TABLE reindex_aoco (a INT) WITH (appendonly=true, orientation=column);
insert into reindex_aoco select generate_series(1,1000);
insert into reindex_aoco select generate_series(1,1000);
create index idx_btree_reindex_aoco on reindex_aoco(a);
SELECT 1 AS relfilenode_same_on_all_segs from gp_dist_random('pg_class')   WHERE relname = 'idx_btree_reindex_aoco' GROUP BY relfilenode having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);
