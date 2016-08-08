DROP table if exists pg_table_part_alter_add_column_check cascade;

CREATE table pg_table_part_alter_add_column_check (i int ,t text) distributed by (i) partition by range(i)  (start (1) end (10) every (5) );

ALTER table pg_table_part_alter_add_column_check add column k int constraint check2 CHECK (k> 99);

select oid, gp_segment_id, conname from pg_constraint where conrelid = 'pg_table_part_alter_add_column_check_1_prt_1'::regclass and conname = 'check2';
select oid, gp_segment_id,conname from gp_dist_random('pg_constraint') where conrelid = 'pg_table_part_alter_add_column_check_1_prt_1'::regclass and conname = 'check2';


