DROP table if exists pg_alter_table_foreign cascade;
DROP table if exists pg_alter_table_primary1 cascade;

CREATE table pg_alter_table_primary1 (i int, t text, constraint primary1 primary key (i)) distributed by (i);

CREATE table pg_alter_table_foreign (a1 int, a2 int, a3 text) distributed by (a1);

ALTER table pg_alter_table_foreign add constraint foreign1 foreign key (a2) references pg_alter_table_primary1(i);

select oid, gp_segment_id, conname from pg_constraint where conrelid = 'pg_alter_table_primary1'::regclass;
select oid, gp_segment_id,conname from gp_dist_random('pg_constraint') where conrelid = 'pg_alter_table_primary1'::regclass;

