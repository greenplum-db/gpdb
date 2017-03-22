-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
\c gpreindextest
SELECT 1 AS oid_same_on_all_segs_a2_idx from gp_dist_random('pg_class') WHERE relname = 'a2_idx' GROUP BY oid having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);
SELECT 1 AS oid_same_on_all_segs_a3_idx from gp_dist_random('pg_class') WHERE relname = 'a3_idx' GROUP BY oid having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);
SELECT 1 AS oid_same_on_all_segs_a4_idx from gp_dist_random('pg_class') WHERE relname = 'a4_idx' GROUP BY oid having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE role='p' AND content > -1);
