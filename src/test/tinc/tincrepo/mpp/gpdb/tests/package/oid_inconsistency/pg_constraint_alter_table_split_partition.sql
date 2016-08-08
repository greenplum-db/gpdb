DROP table if exists pg_table_alter_split_part;

CREATE table pg_table_alter_split_part (i int,t text) distributed by (i) partition by range(i)  (start (1) end (10) every (10) );

ALTER table pg_table_alter_split_part split partition for ('1') at('5') into (partition p3, partition p4);

select oid, gp_segment_id, conname from pg_constraint where conrelid = 'pg_table_alter_split_part_1_prt_p3'::regclass;
select oid, gp_segment_id,conname from gp_dist_random('pg_constraint') where conrelid = 'pg_table_alter_split_part_1_prt_p3'::regclass;
