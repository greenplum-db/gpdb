select oid, gp_segment_id, conname from pg_constraint where conrelid = 'pg_table_part_primary1_1_prt_2'::regclass and conname = 'primary1';
select oid, gp_segment_id,conname from gp_dist_random('pg_constraint') where conrelid = 'pg_table_part_primary1_1_prt_2'::regclass and conname = 'primary1';
