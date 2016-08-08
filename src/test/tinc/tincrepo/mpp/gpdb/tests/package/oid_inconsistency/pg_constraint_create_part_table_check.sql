DROP table if exists pg_table_part_check;

CREATE table pg_table_part_check (i int, t text, CONSTRAINT part_con1 CHECK (i> 99 AND t <> '')) distributed by (i) 
                                      partition by range(i)  (start (1) end (10) every (5) );

select oid, gp_segment_id, conname from pg_constraint where conrelid = 'pg_table_part_check_1_prt_1'::regclass and conname = 'part_con1';
select oid, gp_segment_id,conname from gp_dist_random('pg_constraint') where conrelid = 'pg_table_part_check_1_prt_1'::regclass and conname = 'part_con1';

