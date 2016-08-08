DROP table if exists pg_table_unique cascade;

CREATE table pg_table_unique (i int, t text, constraint unique1 unique(i)) distributed by (i);

select oid, gp_segment_id, conname from pg_constraint where conrelid = 'pg_table_unique'::regclass;
select oid, gp_segment_id, conname from gp_dist_random('pg_constraint') where conrelid = 'pg_table_unique'::regclass;

