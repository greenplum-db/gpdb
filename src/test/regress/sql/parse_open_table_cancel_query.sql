CREATE EXTENSION IF NOT EXISTS gp_inject_fault;

drop table if exists _tmp_table;
create table _tmp_table (i1 int);
insert into _tmp_table select i from generate_series(0, 999) i;

SELECT gp_inject_fault('parse_open_table_cancel_query', 'reset', 1);
SELECT gp_inject_fault('parse_open_table_cancel_query', 'suspend', 1);

SELECT * from _tmp_table order by i1 limit 10;
SELECT pg_cancel_backend(pg_backend_pid());

SELECT gp_inject_fault('parse_open_table_cancel_query', 'status', 1);
SELECT gp_inject_fault('parse_open_table_cancel_query', 'resume', 1);
SELECT gp_inject_fault('parse_open_table_cancel_query', 'reset', 1);

drop table _tmp_table;