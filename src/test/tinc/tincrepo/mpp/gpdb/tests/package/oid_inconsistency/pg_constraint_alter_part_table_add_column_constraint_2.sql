select oid, gp_segment_id, conname from pg_constraint where conrelid = 'pg_table_part_alter_add_column_check_1_prt_2'::regclass and conname = 'check2';
select oid, gp_segment_id,conname from gp_dist_random('pg_constraint') where conrelid = 'pg_table_part_alter_add_column_check_1_prt_2'::regclass and conname = 'check2';


