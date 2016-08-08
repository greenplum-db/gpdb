DROP table if exists pg_table_part_alter_alter_type cascade;

CREATE table pg_table_part_alter_alter_type (i int constraint chk1 check (i<10) ,t text) distributed by (i) partition by range(i)  (start (1) end (10) every (5) );

ALTER table pg_table_part_alter_alter_type alter column i TYPE float;

select oid, gp_segment_id, conname from pg_constraint where conrelid = 'pg_table_part_alter_alter_type_1_prt_1'::regclass and conname = 'chk1';
select oid, gp_segment_id,conname from gp_dist_random('pg_constraint') where conrelid = 'pg_table_part_alter_alter_type_1_prt_1'::regclass and conname = 'chk1';

