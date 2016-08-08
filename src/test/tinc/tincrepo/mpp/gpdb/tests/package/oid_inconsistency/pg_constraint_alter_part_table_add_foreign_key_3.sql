select oid, gp_segment_id, conname from pg_constraint where conrelid = 'pg_table_part_foreign_2_1_prt_1'::regclass and conname ~ "fkey";
select oid, gp_segment_id,conname from gp_dist_random('pg_constraint') where conrelid = 'pg_table_part_foreign_2_1_prt_1'::regclass conname ~ "fkey";
