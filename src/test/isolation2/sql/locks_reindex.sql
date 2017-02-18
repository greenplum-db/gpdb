-- @Description The locks held after different operations
DROP TABLE IF EXISTS ao;
CREATE TABLE ao (a INT, b INT) WITH (appendonly=true);
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

-- Actual test begins
1: BEGIN;
1: INSERT INTO ao VALUES (200, 200);
SELECT * FROM locktest;
2U: SELECT * FROM locktest;
1: COMMIT;
1: BEGIN;
1: DELETE FROM ao WHERE a = 1;
SELECT * FROM locktest;
2U: SELECT * FROM locktest;
1: COMMIT;
1: BEGIN;
1: UPDATE ao SET b = -1 WHERE a = 2;
SELECT * FROM locktest;
2U: SELECT * FROM locktest;
1: COMMIT;
