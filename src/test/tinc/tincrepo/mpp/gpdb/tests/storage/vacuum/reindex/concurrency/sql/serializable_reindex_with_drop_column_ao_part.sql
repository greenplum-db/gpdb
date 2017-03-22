-- @product_version gpdb: [4.3.4.0 -],4.3.4.0O2
1: BEGIN;
2: BEGIN;
2: SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;
1: alter table reindex_serialize_tab_ao_part drop column f;
1: COMMIT;
2: reindex table reindex_serialize_tab_ao_part;
2: COMMIT;
3: select count(*) from  reindex_serialize_tab_ao_part where id  < 5; 
3: set enable_seqscan=false;
3: set enable_indexscan=true;
3: select count(*) from  reindex_serialize_tab_ao_part where id = 1;

-- expect to have all the segments update relfilenode by 'reindex table'
-- by exposing the invisible rows, we can see the historical updates to the relfilenode of given index
-- aggregate by gp_segment_id and oid can verify total number of updates
-- finally compare total number of segments + master to ensure all segments and master got reindexed
3: set gp_select_invisible=on;
3: select relname, sum(relfilenode_updated_count)::int/(select count(*) from gp_segment_configuration where role='p' and xmax=0) as all_segs_reindexed_count from (select oid, relname, (count(relfilenode)-1) as relfilenode_updated_count from (select gp_segment_id, oid, relfilenode, relname from pg_class union all select gp_segment_id, oid, relfilenode, relname from gp_dist_random('pg_class')) all_pg_class where relname like 'idx%_reindex_serialize_tab_ao_part%' group by gp_segment_id, oid, relname) per_seg_filenode_updated group by oid, relname;
3: set gp_select_invisible=off;

-- expect index to be dropped
3: select 1-count(*) as index_dropped from (select * from pg_class union all select * from gp_dist_random('pg_class')) t where t.relname = 'idxi_reindex_serialize_tab_ao_part';

-- expect column to be dropped
3: select 1-count(*) as attribute_dropped from (select * from pg_attribute union all select * from gp_dist_random('pg_attribute')) t where t.attrelid = 'reindex_serialize_tab_ao_part'::regclass and t.attname = 'f';
