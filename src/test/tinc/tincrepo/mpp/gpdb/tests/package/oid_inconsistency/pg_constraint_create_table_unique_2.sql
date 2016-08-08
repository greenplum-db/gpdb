DROP table if exists pg_table_unique_2 cascade;

CREATE table pg_table_unique_2 (i int, t text, constraint unique2 unique(i)) distributed by (i);

select conrelid, gp_segment_id, conname from pg_constraint where conrelid = 'pg_table_unique_2'::regclass;
select conrelid, gp_segment_id,conname from gp_dist_random('pg_constraint') where conrelid = 'pg_table_unique_2'::regclass;

