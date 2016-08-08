select oid, gp_segment_id, conname from pg_constraint where conrelid = 'pg_table_foreign'::regclass;
select oid, gp_segment_id,conname from gp_dist_random('pg_constraint') where conrelid = 'pg_table_foreign'::regclass;

