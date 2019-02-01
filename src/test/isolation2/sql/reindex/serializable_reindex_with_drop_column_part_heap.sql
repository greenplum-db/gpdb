DROP TABLE IF EXISTS reindex_serialize_tab_heap_part;

CREATE TABLE reindex_serialize_tab_heap_part ( id INTEGER, owner VARCHAR, description VARCHAR, property BOX, poli POLYGON, target CIRCLE, v VARCHAR, t TEXT, f FLOAT, p POINT, c CIRCLE, filler VARCHAR DEFAULT 'Big data is difficult to work with using most relational database management systems and desktop statistics and visualization packages, requiring instead massively parallel software running on tens, hundreds, or even thousands of servers.What is considered big data varies depending on the capabilities of the organization managing the set, and on the capabilities of the applications.This is here just to take up space so that we use more pages of data and sequential scans take a lot more time. ')DISTRIBUTED BY (id) PARTITION BY RANGE (id) ( PARTITION p_one START('1') INCLUSIVE END ('10') EXCLUSIVE, DEFAULT PARTITION de_fault );
insert into reindex_serialize_tab_heap_part (id, owner, description, property, poli, target) select i, 'user' || i, 'Testing GiST Index', '((3, 1300), (33, 1330))','( (22,660), (57, 650), (68, 660) )', '( (76, 76), 76)' from generate_series(1,1000) i ;
insert into reindex_serialize_tab_heap_part (id, owner, description, property, poli, target) select i, 'user' || i, 'Testing GiST Index', '((3, 1300), (33, 1330))','( (22,660), (57, 650), (68, 660) )', '( (76, 76), 76)' from generate_series(1,1000) i ;

create index reindex_serialize_tab_heap_part_idxa on reindex_serialize_tab_heap_part(id);
create index reindex_serialize_tab_heap_part_idxb on reindex_serialize_tab_heap_part(owner);
create index reindex_serialize_tab_heap_part_idxc on reindex_serialize_tab_heap_part USING GIST(property);
create index reindex_serialize_tab_heap_part_idxd on reindex_serialize_tab_heap_part USING GIST(poli);
create index reindex_serialize_tab_heap_part_idxe on reindex_serialize_tab_heap_part USING GIST(target);
create index reindex_serialize_tab_heap_part_idxf on reindex_serialize_tab_heap_part USING BITMAP(v);
create index reindex_serialize_tab_heap_part_idxg on reindex_serialize_tab_heap_part USING GIST(c);
create index reindex_serialize_tab_heap_part_idxh on reindex_serialize_tab_heap_part(f);
1: BEGIN;
2: BEGIN;
2: SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;
2: select 'dummy select to establish snapshot';
1: alter table reindex_serialize_tab_heap_part drop column f;
1: COMMIT;
-- Remember index relfilenodes from master and segments before
-- reindex.
2: create temp table old_relfilenodes as
   (select gp_segment_id as dbid, relfilenode, oid, relname from gp_dist_random('pg_class')
    where relname like 'reindex_serialize_tab_heap_part%_idx%'
    union all
    select gp_segment_id as dbid, relfilenode, oid, relname from pg_class
    where relname like 'reindex_serialize_tab_heap_part%_idx%');
2: reindex table reindex_serialize_tab_heap_part;
2: COMMIT;
-- Validate that reindex changed all index relfilenodes on master as well as
-- segments.  The following query should return 0 tuples.
2: select oldrels.* from old_relfilenodes oldrels join
   (select gp_segment_id as dbid, relfilenode, relname from gp_dist_random('pg_class')
    where relname like 'reindex_serialize_tab_heap_part%_idx%'
    union all
    select gp_segment_id as dbid, relfilenode, relname from pg_class
    where relname like 'reindex_serialize_tab_heap_part%_idx%') newrels
    on oldrels.relfilenode = newrels.relfilenode
    and oldrels.dbid = newrels.dbid
    and oldrels.relname = newrels.relname;
3: select count(*) from  reindex_serialize_tab_heap_part where id  < 5;
3: set enable_seqscan=false;
3: set enable_indexscan=true;
3: select count(*) from  reindex_serialize_tab_heap_part where id = 1;

-- expect index to be dropped
3: select 1-count(*) as index_dropped from (select * from pg_class union all select * from gp_dist_random('pg_class')) t where t.relname = 'reindex_serialize_tab_heap_part_idxh';

-- expect column to be dropped
3: select 1-count(*) as attribute_dropped from (select * from pg_attribute union all select * from gp_dist_random('pg_attribute')) t where t.attrelid = 'reindex_serialize_tab_heap_part'::regclass and t.attname = 'f';
