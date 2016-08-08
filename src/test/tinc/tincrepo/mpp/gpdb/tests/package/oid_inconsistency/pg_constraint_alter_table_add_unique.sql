DROP table if exists pg_table_alter_add_unique cascade;

CREATE table pg_table_alter_add_unique (i int , t text) distributed by (i);

ALTER table pg_table_alter_add_unique add constraint unq1 unique(i);

select oid, gp_segment_id, conname from pg_constraint where conrelid = 'pg_table_alter_add_unique'::regclass;
select oid, gp_segment_id,conname from gp_dist_random('pg_constraint') where conrelid = 'pg_table_alter_add_unique'::regclass;

