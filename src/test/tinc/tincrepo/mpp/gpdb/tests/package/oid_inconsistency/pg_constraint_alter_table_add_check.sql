DROP table  if exists pg_table_alter_add_check;

CREATE table pg_table_alter_add_check  (
    did integer,
    name varchar(40)
    )DISTRIBUTED RANDOMLY;

ALTER table pg_table_alter_add_check add constraint check1 CHECK (did > 99 AND name <> '') ;

select oid, gp_segment_id, conname from pg_constraint where conrelid = 'pg_table_alter_add_check'::regclass;
select oid, gp_segment_id, conname from gp_dist_random('pg_constraint') where conrelid = 'pg_table_alter_add_check'::regclass;


