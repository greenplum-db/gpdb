DROP table if exists pg_table_alter_add_unique_2 cascade;

CREATE table pg_table_alter_add_unique_2 (i int , t text) distributed by (i);

ALTER table pg_table_alter_add_unique_2 add constraint unq2 unique(i);

select conrelid, gp_segment_id, conname from pg_constraint where conrelid = 'pg_table_alter_add_unique_2'::regclass;
select conrelid, gp_segment_id,conname from gp_dist_random('pg_constraint') where conrelid = 'pg_table_alter_add_unique_2'::regclass;

