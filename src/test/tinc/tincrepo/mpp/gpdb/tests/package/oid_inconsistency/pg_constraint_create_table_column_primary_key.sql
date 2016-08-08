DROP table if exists pg_table_column_primary cascade;

CREATE table pg_table_column_primary (i int constraint primary1 primary key, t text) distributed by (i);

select oid, gp_segment_id, conname from pg_constraint where conrelid = 'pg_table_column_primary'::regclass;
select oid, gp_segment_id,conname from gp_dist_random('pg_constraint') where conrelid = 'pg_table_column_primary'::regclass;


