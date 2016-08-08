DROP table if exists pg_table_column_check;

CREATE TABLE pg_table_column_check  (
    did integer,
    name varchar(40) CONSTRAINT con1 CHECK (did > 99 AND name <> '')
    )DISTRIBUTED RANDOMLY;

select oid, gp_segment_id, conname from pg_constraint where conrelid = 'pg_table_column_check'::regclass;
select oid, gp_segment_id, conname from gp_dist_random('pg_constraint') where conrelid = 'pg_table_column_check'::regclass;

