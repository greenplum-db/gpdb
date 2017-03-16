-- @Description The locks held after different operations
DROP TABLE IF EXISTS ao;

CREATE TABLE ao (a INT, b INT) WITH (appendonly=true);
INSERT INTO ao SELECT i as a, i as b FROM generate_series(1, 100) AS i;

create or replace view locktest as
select coalesce(
  case when relname like 'pg_toast%index' then 'toast index'
       when relname like 'pg_toast%' then 'toast table'
       when relname like 'pg_aoseg%' then 'aoseg table'
       when relname like 'pg_aovisimap%index' then 'aovisimap index'
       when relname like 'pg_aovisimap%' then 'aovisimap table'
       else relname end, 'dropped table'),
  mode,
  locktype,
  case when l.gp_segment_id = -1 then 'master'
       when COUNT(*) = 1 then '1 segment'
       else 'n segments' end AS node
from pg_locks l
left outer join pg_class c on (l.relation = c.relfilenode),
pg_database d
where relation is not null
and l.database = d.oid
and (relname <> 'gp_fault_strategy' and relname not like 'pg_class%')
and d.datname = current_database()
group by l.gp_segment_id = -1, relation, relname, locktype, mode
order by 1, 3, 2;

-- Actual test begins
BEGIN;
INSERT INTO ao VALUES (200, 200);
SELECT * FROM locktest;
SELECT * FROM gp_dist_random('locktest');
COMMIT;

BEGIN;
DELETE FROM ao WHERE a = 1;
SELECT * FROM locktest;
SELECT * FROM gp_dist_random('locktest');
COMMIT;

BEGIN;
UPDATE ao SET b = -1 WHERE a = 2;
SELECT * FROM locktest;
SELECT * FROM gp_dist_random('locktest');
COMMIT;
