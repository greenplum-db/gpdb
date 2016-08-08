DROP table if exists pg_part_table_primary;

CREATE table pg_part_table_primary (trans_id int, date1 date, amount
decimal(9,2), region text, constraint primary1 primary key(date1))
DISTRIBUTED BY (date1)
PARTITION BY RANGE (date1)
( START (date '2008-01-01') INCLUSIVE
   END (date '2009-01-01') EXCLUSIVE
   EVERY (INTERVAL '12 month') );

select oid, gp_segment_id, conname from pg_constraint where conrelid = 'pg_part_table_primary_1_prt_1'::regclass and conname = 'primary1';
select oid, gp_segment_id, conname from gp_dist_random('pg_constraint') where conrelid = 'pg_part_table_primary_1_prt_1'::regclass and conname = 'primary1';

