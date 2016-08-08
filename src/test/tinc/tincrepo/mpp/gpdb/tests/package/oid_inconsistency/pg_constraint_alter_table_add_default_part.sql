DROP table if exists pg_table_alter_default_part;

CREATE table pg_table_alter_default_part (i int,t text, constraint check2 check(i<10)) distributed by (i) partition by range(i)
                                          (partition p1  start (1) end (5) ,
                                           partition p2 start (6) end (10));

ALTER table pg_table_alter_default_part add default partition p0;

select oid, gp_segment_id, conname from pg_constraint where conname ~ 'check2' and conrelid = ' pg_table_alter_default_part_1_prt_p0'::regclass;
select oid, gp_segment_id,conname from gp_dist_random('pg_constraint') where conname ~ 'check2' and conrelid = ' pg_table_alter_default_part_1_prt_p0'::regclass;


