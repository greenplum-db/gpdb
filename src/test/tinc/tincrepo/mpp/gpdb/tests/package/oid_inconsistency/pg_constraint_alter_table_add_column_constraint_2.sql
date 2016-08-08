DROP table if exists pg_table_alter_add_column_check_2 cascade;

CREATE table pg_table_alter_add_column_check_2 (i int , t text) distributed by (i);

ALTER table pg_table_alter_add_column_check_2 add column k int constraint check1 CHECK (k> 99);

select conrelid, gp_segment_id, conname from pg_constraint where conrelid = 'pg_table_alter_add_column_check_2'::regclass;
select conrelid, gp_segment_id,conname from gp_dist_random('pg_constraint') where conrelid = 'pg_table_alter_add_column_check_2'::regclass;

