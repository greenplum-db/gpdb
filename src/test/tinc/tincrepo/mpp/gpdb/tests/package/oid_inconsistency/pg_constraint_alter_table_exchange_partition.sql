DROP table if exists exchange_part;
DROP table if exists pg_table_alter_exchange_part;

CREATE table pg_table_alter_exchange_part (i int,t text, constraint check2 check(i<10)) distributed by (i) partition by range(i)  
                                          (default partition p0, partition p1  start (1) end (5) ,
                                           partition p2 start (6) end (10));

CREATE table exchange_part(i int,t text) distributed by (i);

ALTER table pg_table_alter_exchange_part exchange partition p2 with table exchange_part;

select oid, gp_segment_id, conname from pg_constraint where conname ~ 'pg_table_alter_exchange_part_1_prt_p2' and oid = (select max(oid) from pg_constraint where conname ~ 'pg_table_alter_exchange_part_1_prt_p2');
select oid, gp_segment_id,conname from gp_dist_random('pg_constraint') where conname ~ 'pg_table_alter_exchange_part_1_prt_p2' and oid = (select max(oid) from gp_dist_random('pg_constraint') where conname ~ 'pg_table_alter_exchange_part_1_prt_p2');

