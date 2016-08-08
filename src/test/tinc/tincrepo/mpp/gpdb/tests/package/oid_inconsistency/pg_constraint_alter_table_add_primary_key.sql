DROP table if exists pg_table_alter_add_primary cascade;

CREATE table pg_table_alter_add_primary (i int , t text) distributed by (i);

ALTER table pg_table_alter_add_primary add constraint prm1 primary key(i);

select oid, gp_segment_id, conname from pg_constraint where conrelid = 'pg_table_alter_add_primary'::regclass;
select oid, gp_segment_id,conname from gp_dist_random('pg_constraint') where conrelid = 'pg_table_alter_add_primary'::regclass;


