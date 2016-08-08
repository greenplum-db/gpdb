DROP table if exists pg_table_inherit_2;
DROP table if exists pg_table_inherit_base_2;

CREATE table pg_table_inherit_base_2  (
    did integer,
    name varchar(40),
    CONSTRAINT con1 CHECK (did > 99 AND name <> '')
    )DISTRIBUTED RANDOMLY;

CREATE table pg_table_inherit_2() inherits( pg_table_inherit_base_2) distributed randomly;

select conrelid, gp_segment_id, conname from pg_constraint where conrelid = 'pg_table_inherit_2'::regclass;
select conrelid, gp_segment_id, conname from gp_dist_random('pg_constraint') where conrelid = 'pg_table_inherit_2'::regclass;


