DROP TABLE IF EXISTS reindex_heap_gist;

CREATE TABLE reindex_heap_gist (
 id INTEGER,
 owner VARCHAR,
 description VARCHAR,
 property BOX, 
 poli POLYGON,
 target CIRCLE,
 v VARCHAR,
 t TEXT,
 f FLOAT, 
 p POINT,
 c CIRCLE,
 filler VARCHAR DEFAULT 'Big data is difficult to work with using most relational database management systems and desktop statistics and visualization packages, requiring instead massively parallel software running on tens, hundreds, or even thousands of servers.What is considered big data varies depending on the capabilities of the organization managing the set, and on the capabilities of the applications.This is here just to take up space so that we use more pages of data and sequential scans take a lot more time. ' 
 )
 DISTRIBUTED BY (id)
 PARTITION BY RANGE (id)
  (
  PARTITION p_one START('1') INCLUSIVE END ('10') EXCLUSIVE,
  DEFAULT PARTITION de_fault
  ) ;

insert into reindex_heap_gist (id, owner, description, property, poli, target) select i, 'user' || i, 'Testing GiST Index', '((3, 1300), (33, 1330))','( (22,660), (57, 650), (68, 660) )', '( (76, 76), 76)' from  generate_series(1,1000) i ;
insert into reindex_heap_gist (id, owner, description, property, poli, target) select i, 'user' || i, 'Testing GiST Index', '((3, 1300), (33, 1330))','( (22,660), (57, 650), (68, 660) )', '( (76, 76), 76)' from  generate_series(1,1000) i ;

create index idx_gist_reindex_heap on reindex_heap_gist USING Gist(target);

-- Verify oid is same on all the segments
SELECT 1 AS oid_same_on_all_segs from gp_dist_random('pg_class')   WHERE relname = 'idx_gist_reindex_heap' GROUP BY oid having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);
-- @Description Ensures that a create index during reindex operations on GiST index is ok
-- 

DELETE FROM reindex_heap_gist  WHERE id < 128;
1: BEGIN;
1: REINDEX index idx_gist_reindex_heap;
2: create index idx_gist_reindex_heap2 on reindex_heap_gist USING Gist(target);
1: COMMIT;
2: COMMIT;
3: SELECT COUNT(*) FROM reindex_heap_gist WHERE id = 1500;
3: select count(*) from reindex_heap_gist;
3: set enable_seqscan=false;
3: set enable_indexscan=true;
3: select count(*) from reindex_heap_gist;

3: insert into reindex_heap_gist (id, owner, description, property, poli, target) values(1500, 'gpadmin', 'Reindex Concurrency test', '((1500, 1500), (1560, 1580))', '( (111, 112), (114, 115), (110, 110) )', '( (96, 86), 96)' );
3:  SELECT COUNT(*) FROM reindex_heap_gist WHERE id = 1500;
3: SELECT 1 AS oid_same_on_all_segs from gp_dist_random('pg_class')   WHERE relname = 'idx_gist_reindex_heap' GROUP BY oid having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);
3: SELECT 1 AS oid_same_on_all_segs from gp_dist_random('pg_class')   WHERE relname = 'idx_gist_reindex_heap2' GROUP BY oid having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);
