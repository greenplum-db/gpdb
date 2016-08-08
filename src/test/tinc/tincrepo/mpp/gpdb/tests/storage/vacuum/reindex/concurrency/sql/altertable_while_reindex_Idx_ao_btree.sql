-- @Description Ensures that alter table during reindex operations is ok
-- 

DELETE FROM reindex_alter_ao_btree WHERE a < 128;
1: BEGIN;
2: BEGIN;
1: REINDEX index idx_reindex_alter_ao_btree;
2&: alter table reindex_alter_ao_btree  alter column j type numeric;
1: SELECT 1 AS relfilenode_same_on_all_segs from gp_dist_random('pg_class')   WHERE relname = 'idx_reindex_alter_ao_btree' GROUP BY relfilenode having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);
1: COMMIT;
2<:
2: COMMIT;
3: INSERT INTO reindex_alter_ao_btree VALUES(2500);
3: SELECT count(*) FROM reindex_alter_ao_btree WHERE a = 2500;

