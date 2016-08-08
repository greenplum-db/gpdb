DROP table if exists pg_part_table;

CREATE table pg_part_table (trans_id int, date date, amount 
decimal(9,2), region text) 
DISTRIBUTED BY (trans_id) 
PARTITION BY RANGE (date) 
SUBPARTITION BY LIST (region) 
SUBPARTITION TEMPLATE 
( SUBPARTITION usa VALUES ('usa'))  
( START (date '2008-01-01') INCLUSIVE 
   END (date '2009-01-01') EXCLUSIVE 
   EVERY (INTERVAL '6 month') );

select oid, gp_segment_id, conname from pg_constraint where conrelid = 'pg_part_table_1_prt_1'::regclass;
select oid, gp_segment_id, conname from gp_dist_random('pg_constraint') where conrelid = 'pg_part_table_1_prt_1'::regclass;

 
