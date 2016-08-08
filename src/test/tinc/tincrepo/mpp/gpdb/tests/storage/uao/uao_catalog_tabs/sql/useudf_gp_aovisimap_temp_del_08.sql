-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
\pset tuples_only
\pset footer off
select port, hostname from gp_segment_configuration sc , gp_dist_random('pg_aoseg.pg_aoseg_26420') dr where dr.gp_segment_id=sc.dbid  and tupcount > 0 order by port desc limit 1;