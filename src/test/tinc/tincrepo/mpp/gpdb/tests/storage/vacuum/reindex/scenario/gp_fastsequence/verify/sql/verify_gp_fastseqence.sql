-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
SELECT 1 AS relfilenode_same_on_all_segs from gp_dist_random('pg_class')   WHERE relname = 'gp_fastsequence_objid_objmod_index' GROUP BY relfilenode having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);
select last_sequence from gp_dist_random('gp_fastsequence') where objid = (select segrelid from pg_appendonly where relid = (select oid from pg_class where relname = 'test_fastseqence')) limit 1;
insert into test_fastseqence select i , 'aa'||i from generate_series(1,100) i;
select last_sequence from gp_dist_random('gp_fastsequence') where objid = (select segrelid from pg_appendonly where relid = (select oid from pg_class where relname = 'test_fastseqence')) limit 1;
select 1 as correct_last_seq  from gp_dist_random('gp_fastsequence') where objid = (select segrelid from pg_appendonly where relid = (select oid from pg_class where relname = 'test_fastseqence')) group by last_sequence having count(*) = (SELECT count(*    ) FROM gp_segment_configuration WHERE role='p' AND content > -1);
SELECT 1 AS relfilenode_same_on_all_segs from gp_dist_random('pg_class')   WHERE relname = 'test_fastseqence' GROUP BY relfilenode having count(*) = (SELECT count(*    ) FROM gp_segment_configuration WHERE role='p' AND content > -1);
