DROP table if exists pg_table_alter_add_column_check cascade;

CREATE table pg_table_alter_add_column_check (i int , t text) distributed by (i);

ALTER table pg_table_alter_add_column_check add column k int constraint check1 CHECK (k> 99);

select oid, gp_segment_id, conname from pg_constraint where conrelid = 'pg_table_alter_add_column_check'::regclass;
select oid, gp_segment_id,conname from gp_dist_random('pg_constraint') where conrelid = 'pg_table_alter_add_column_check'::regclass;

