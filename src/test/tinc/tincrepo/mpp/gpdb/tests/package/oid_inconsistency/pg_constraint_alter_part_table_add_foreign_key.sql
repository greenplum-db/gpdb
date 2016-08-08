DROP table if exists pg_table_part_foreign_2;
DROP table if exists pg_table_part_primary2;

CREATE table pg_table_part_primary2 (i int, t text, constraint primary2 primary key (i)) distributed by (i) partition by range(i)  (start (1) end (10) every (5) );

CREATE table pg_table_part_foreign_2 (a1 int, a2 int, a3 text) distributed by (a1) 
             partition by range(a1) (start (1) end (10) every (5) );

ALTER table pg_table_part_foreign_2  add constraint foreign1 foreign key (a2) references pg_table_part_primary2(i);

select oid, gp_segment_id, conname from pg_constraint where conrelid = 'pg_table_part_primary2_1_prt_1'::regclass and conname = 'primary2';
select oid, gp_segment_id,conname from gp_dist_random('pg_constraint') where conrelid = 'pg_table_part_primary2_1_prt_1'::regclass and conname = 'primary2';

