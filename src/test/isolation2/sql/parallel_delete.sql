-- @Description Tests that a delete operation in progress will block all other deletes
-- until the transaction is committed.
--
DROP TABLE IF EXISTS ao;
CREATE TABLE ao (a INT) WITH (appendonly=true) DISTRIBUTED BY (a);
insert into ao select generate_series(1,100);

DROP VIEW IF EXISTS locktest;
create view locktest as select coalesce(
	case when relname like 'pg_toast%index' then 'toast index'
	when relname like 'pg_toast%' then 'toast table'
	else relname end, 'dropped table'),
	mode,
	locktype from
	pg_locks l left outer join pg_class c on (l.relation = c.oid),
	pg_database d where relation is not null and l.database = d.oid and
	l.gp_segment_id = -1 and
	relname != 'gp_fault_strategy' and
	d.datname = current_database() order by 1, 3, 2;

-- The actual test begins
1: BEGIN;
2: BEGIN;
2: DELETE FROM ao WHERE a = 1;
2: SELECT * FROM locktest WHERE coalesce = 'ao';
1&: DELETE FROM ao WHERE a = 2;
2: COMMIT;
1<:
1: COMMIT;
3: BEGIN;
3: SELECT * FROM ao WHERE a < 5 ORDER BY a;
3: COMMIT;
2U: SELECT * FROM gp_toolkit.__gp_aovisimap_name('ao');
