select oid, gp_segment_id, conname from pg_constraint where conrelid = 'pg_part_table_1_prt_1_2_prt_usa'::regclass and conname ~'usa';
select oid, gp_segment_id, conname from gp_dist_random('pg_constraint') where conrelid = 'pg_part_table_1_prt_1_2_prt_usa'::regclass and conname ~'usa';


