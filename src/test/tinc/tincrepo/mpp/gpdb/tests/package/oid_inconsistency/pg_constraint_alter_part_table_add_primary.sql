DROP table if exists pg_table_part_add_primary;

CREATE table pg_table_part_add_primary (i int, t text) distributed by (i) partition by range(i)  (start (1) end (10) every (5) );

ALTER table pg_table_part_add_primary  add constraint primary3 primary key (i);

select oid, gp_segment_id, conname from pg_constraint where conrelid = 'pg_table_part_add_primary_1_prt_1'::regclass and conname = 'primary3';
select oid, gp_segment_id,conname from gp_dist_random('pg_constraint') where conrelid = 'pg_table_part_add_primary_1_prt_1'::regclass and conname = 'primary3';

