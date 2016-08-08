-- @Description Ensures that alter table during reindex operations on GiST index is ok
-- 

DELETE FROM reindex_aoco_gist  WHERE id < 128;
1: BEGIN;
2: BEGIN;
1: REINDEX index idx_gist_reindex_aoco;
2&: alter table reindex_aoco_gist drop column target;
1: COMMIT;
2<:
2: COMMIT;
3: insert into reindex_aoco_gist (id, owner, description, property, poli) values(1500, 'user1500' , 'Testing GiST Index', '((3, 1300), (33, 1330))','( (22,660), (57, 650), (68, 660) )') ;
3: SELECT COUNT(*) FROM reindex_aoco_gist WHERE id = 1500;
3: SELECT 1 AS relfilenode_same_on_all_segs from gp_dist_random('pg_class')   WHERE relname = 'idx_gist_reindex_aoco' GROUP BY relfilenode having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);
