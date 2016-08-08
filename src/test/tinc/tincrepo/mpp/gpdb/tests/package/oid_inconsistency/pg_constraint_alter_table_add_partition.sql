DROP table if exists pg_table_alter_add_part;

CREATE table pg_table_alter_add_part (i int,t text) distributed by (i) partition by range(i)  (start (1) end (10) every (5) );

ALTER table pg_table_alter_add_part add partition part3 start (11) end (15);

select oid, gp_segment_id, conname from pg_constraint where conrelid = 'pg_table_alter_add_part_1_prt_part3'::regclass;
select oid, gp_segment_id,conname from gp_dist_random('pg_constraint') where conrelid = 'pg_table_alter_add_part_1_prt_part3'::regclass;
