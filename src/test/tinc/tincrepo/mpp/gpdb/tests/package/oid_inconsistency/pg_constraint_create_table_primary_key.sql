DROP table if exists pg_table_primary cascade;

CREATE table pg_table_primary (i int, t text, constraint primary1 primary key (i)) distributed by (i);

select oid, gp_segment_id, conname from pg_constraint where conrelid = 'pg_table_primary'::regclass;
select oid, gp_segment_id,conname from gp_dist_random('pg_constraint') where conrelid = 'pg_table_primary'::regclass;


