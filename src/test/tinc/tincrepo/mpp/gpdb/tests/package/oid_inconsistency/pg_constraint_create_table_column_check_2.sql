DROP table if exists pg_table_column_check_2;

CREATE table pg_table_column_check_2  (
    did integer,
    name varchar(40) CONSTRAINT con1 CHECK (did > 99 AND name <> '')
    )DISTRIBUTED RANDOMLY;

select conrelid, gp_segment_id, conname from pg_constraint where conrelid = 'pg_table_column_check_2'::regclass;
select conrelid, gp_segment_id, conname from gp_dist_random('pg_constraint') where conrelid = 'pg_table_column_check_2'::regclass;

