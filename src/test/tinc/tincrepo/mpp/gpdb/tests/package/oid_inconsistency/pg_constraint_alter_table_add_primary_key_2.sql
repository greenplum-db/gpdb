DROP table if exists pg_table_alter_add_primary_2 cascade;

CREATE table pg_table_alter_add_primary_2 (i int , t text) distributed by (i);

ALTER table pg_table_alter_add_primary_2 add constraint prm2 primary key(i);

select conrelid, gp_segment_id, conname from pg_constraint where conrelid = 'pg_table_alter_add_primary_2'::regclass;
select conrelid, gp_segment_id,conname from gp_dist_random('pg_constraint') where conrelid = 'pg_table_alter_add_primary_2'::regclass;


