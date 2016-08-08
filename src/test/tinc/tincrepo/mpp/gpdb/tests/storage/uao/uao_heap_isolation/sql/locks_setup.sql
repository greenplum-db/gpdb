-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
DROP TABLE IF EXISTS ao;

CREATE TABLE ao (a INT, b INT) ;
INSERT INTO ao SELECT i as a, i as b FROM generate_series(1, 100) AS i;

DROP VIEW IF EXISTS locktest;
create view locktest as select coalesce(
    case when relname like 'pg_toast%index' then 'toast index'
	when relname like 'pg_toast%' then 'toast table'
    when relname like 'pg_aovisimap%index' then 'visimap index'
	when relname like 'pg_aovisimap%' then 'visimap table'
    when relname like 'pg_aoseg%index' then 'aoseg index'
	when relname like 'pg_aoseg%' then 'aoseg table'
    when relname like 'pg_aocsseg%index' then 'aoseg index'
	when relname like 'pg_aocsseg%' then 'aoseg table'
	else relname end, 'dropped table'),
	mode,
	locktype from
	pg_locks l, 
    pg_class c,
	pg_database d where relation is not null and 
    l.database = d.oid and
    l.relation = c.oid and
	relname != 'gp_fault_strategy' and
	d.datname = current_database() order by 1, 3, 2;
