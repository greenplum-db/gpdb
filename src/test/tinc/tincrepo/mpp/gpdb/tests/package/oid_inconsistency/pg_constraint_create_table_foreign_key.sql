DROP table if exists pg_table_foreign cascade;
DROP table if exists pg_table_primary1 cascade;

CREATE table pg_table_primary1 (i int, t text, constraint primary1 primary key (i)) distributed by (i);

CREATE table pg_table_foreign (a1 int, a2 int, a3 text, constraint foreign1 foreign key (a2) references pg_table_primary1(i)) distributed by (a1);

select oid, gp_segment_id, conname from pg_constraint where conrelid = 'pg_table_primary1'::regclass;
select oid, gp_segment_id,conname from gp_dist_random('pg_constraint') where conrelid = 'pg_table_primary1'::regclass;

