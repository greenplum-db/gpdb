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
3: SELECT 1 AS relfilenode_same_on_all_segs from gp_dist_random('pg_class')   WHERE relname = 'reindex_serialize_tab_ao_part' GROUP BY relfilenode having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);
3: SELECT 1 AS relfilenode_same_on_all_segs from gp_dist_random('pg_class')   WHERE relname = 'idxa_reindex_serialize_tab_ao_part_1_prt_p_one' GROUP BY relfilenode having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);

SELECT 1 AS relfilenode_same_on_all_segs_maintable from gp_dist_random('pg_class') WHERE relname = 'reindex_serialize_tab_ao_part' and relfilenode = oid GROUP BY relfilenode having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);

select 1 AS relfilenode_same_on_all_segs_mainidx from gp_dist_random('pg_class') WHERE relname = 'idxi_reindex_serialize_tab_ao_part' and relfilenode != oid GROUP BY relfilenode having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);

select  1 AS relfilenode_same_on_all_segs_partition_default_data from gp_dist_random('pg_class') WHERE relname = 'reindex_serialize_tab_ao_part_1_prt_de_fault' and relfilenode = oid GROUP BY relfilenode having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);


select  1 AS relfilenode_same_on_all_segs_partition_default_idx from gp_dist_random('pg_class') WHERE relname = 'idxi_reindex_serialize_tab_ao_part_1_prt_de_fault' and relfilenode != oid GROUP BY relfilenode having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);


select 1 AS relfilenode_same_on_all_segs_partition_1_data from gp_dist_random('pg_class') WHERE relname = 'reindex_serialize_tab_ao_part_1_prt_p_one' and relfilenode = oid GROUP BY relfilenode having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);

select 1 AS relfilenode_same_on_all_segs_partition_1_idx from gp_dist_random('pg_class') WHERE relname = 'idxi_reindex_serialize_tab_ao_part_1_prt_p_one' and relfilenode != oid GROUP BY relfilenode having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);
select 1 AS relfilenode_same_on_all_segs_partition_1_idx from gp_dist_random('pg_class') WHERE relname = 'idxa_reindex_serialize_tab_ao_part_1_prt_p_one' and relfilenode != oid GROUP BY relfilenode having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);

