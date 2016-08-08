DROP table if exists pg_table_alter_add_check_2;

CREATE table pg_table_alter_add_check_2  (
    did integer,
    name varchar(40)
    )DISTRIBUTED RANDOMLY;

ALTER table pg_table_alter_add_check_2 add constraint check1 CHECK (did > 99 AND name <> '') ;

select conrelid, gp_segment_id, conname from pg_constraint where conrelid = 'pg_table_alter_add_check_2'::regclass;
select conrelid, gp_segment_id, conname from gp_dist_random('pg_constraint') where conrelid = 'pg_table_alter_add_check_2'::regclass;

