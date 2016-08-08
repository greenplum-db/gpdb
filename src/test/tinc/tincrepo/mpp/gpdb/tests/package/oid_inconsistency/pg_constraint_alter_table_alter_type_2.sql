DROP table if exists pg_table_alter_alter_type_2 cascade;

CREATE table pg_table_alter_alter_type_2 (i int constraint con1 check (i<99), t text) distributed by (i);

ALTER table pg_table_alter_alter_type_2 alter column i TYPE float;

select conrelid, gp_segment_id, conname from pg_constraint where conrelid = 'pg_table_alter_alter_type_2'::regclass;
select conrelid, gp_segment_id,conname from gp_dist_random('pg_constraint') where conrelid = 'pg_table_alter_alter_type_2'::regclass;


