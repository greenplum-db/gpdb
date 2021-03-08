alter system set autovacuum = off;
select gp_segment_id, pg_reload_conf() from gp_id union select gp_segment_id, pg_reload_conf() from gp_dist_random('gp_id');
