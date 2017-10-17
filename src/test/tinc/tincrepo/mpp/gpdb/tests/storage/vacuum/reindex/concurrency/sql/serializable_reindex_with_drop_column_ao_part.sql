-- @product_version gpdb: [4.3.4.0 -],4.3.4.0O2
1: BEGIN;
2: BEGIN;
2: SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;
1: alter table reindex_serialize_tab_ao_part drop column f;
1: COMMIT;
-- Remember index relfilenodes from master and segments before
-- reindex.
2: create temp table old_relfilenodes as
   (select gp_segment_id as dbid, relfilenode, oid, relname from gp_dist_random('pg_class')
    where relname like 'idx%_reindex_serialize_tab_ao_part%'
    union all
    select gp_segment_id as dbid, relfilenode, oid, relname from pg_class
    where relname like 'idx%_reindex_serialize_tab_ao_part%');
2: reindex table reindex_serialize_tab_ao_part;
2: COMMIT;
-- Validate that reindex changed all index relfilenodes on master as well as
-- segments.  The following query should return 0 tuples.
2: select oldrels.* from old_relfilenodes oldrels join
   (select gp_segment_id as dbid, relfilenode, relname from gp_dist_random('pg_class')
    where relname like 'idx%_reindex_serialize_tab_ao_part%'
    union all
    select gp_segment_id as dbid, relfilenode, relname from pg_class
    where relname like 'idx%_reindex_serialize_tab_ao_part%') newrels
    on oldrels.relfilenode = newrels.relfilenode
    and oldrels.dbid = newrels.dbid
    and oldrels.relname = newrels.relname;
3: select count(*) from  reindex_serialize_tab_ao_part where id  < 5;
3: set enable_seqscan=false;
3: set enable_indexscan=true;
3: select count(*) from  reindex_serialize_tab_ao_part where id = 1;

-- expect index to be dropped
3: select 1-count(*) as index_dropped from (select * from pg_class union all select * from gp_dist_random('pg_class')) t where t.relname = 'idxi_reindex_serialize_tab_ao_part';

-- expect column to be dropped
3: select 1-count(*) as attribute_dropped from (select * from pg_attribute union all select * from gp_dist_random('pg_attribute')) t where t.attrelid = 'reindex_serialize_tab_ao_part'::regclass and t.attname = 'f';
