-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
select gp_segment_id, oid, count(*) from gp_dist_random('pg_class') group by 1, 2 having count(*) > 1;
