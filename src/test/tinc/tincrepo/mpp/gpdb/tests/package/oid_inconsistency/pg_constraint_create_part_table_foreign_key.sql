DROP table if exists pg_table_part_foreign cascade;
DROP table if exists pg_table_part_primary1 cascade;

CREATE table pg_table_part_primary1 (i int, t text, constraint primary1 primary key (i)) distributed by (i) partition by range(i)  (start (1) end (10) every (5) );

CREATE table pg_table_part_foreign (a1 int, a2 int, a3 text, constraint foreign1 foreign key (a2) references pg_table_part_primary1(i)) distributed by (a1) 
             partition by range(a1) (start (1) end (10) every (5) );

select oid, gp_segment_id, conname from pg_constraint where conrelid = 'pg_table_part_primary1_1_prt_1'::regclass and conname = 'primary1';
select oid, gp_segment_id,conname from gp_dist_random('pg_constraint') where conrelid = 'pg_table_part_primary1_1_prt_1'::regclass and conname = 'primary1';

