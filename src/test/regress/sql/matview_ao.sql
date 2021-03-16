drop role if exists matview_ao_role;
create role matview_ao_role;
set role matview_ao_role;

CREATE TABLE t_matview_ao (id int NOT NULL PRIMARY KEY, type text NOT NULL, amt numeric NOT NULL);
INSERT INTO t_matview_ao VALUES
  (1, 'x', 2),
  (2, 'x', 3),
  (3, 'y', 5),
  (4, 'y', 7),
  (5, 'z', 11);

CREATE MATERIALIZED VIEW m_heap AS SELECT type, sum(amt) AS totamt FROM t_matview_ao GROUP BY type WITH NO DATA distributed by(type);
CREATE UNIQUE INDEX m_heap_index ON m_heap(type);
SELECT not relfrozenxid = '0'::xid as valid_relfrozenxid from pg_class where relname = 'm_heap';
SELECT * from m_heap;
REFRESH MATERIALIZED VIEW CONCURRENTLY m_heap;
REFRESH MATERIALIZED VIEW m_heap;
SELECT * FROM m_heap;
REFRESH MATERIALIZED VIEW CONCURRENTLY m_heap;
SELECT not relfrozenxid = '0'::xid as valid_relfrozenxid from pg_class where relname = 'm_heap';
SELECT * FROM m_heap;
REFRESH MATERIALIZED VIEW m_heap WITH NO DATA;
SELECT * FROM m_heap;
-- test WITH NO DATA is also dispatched to QEs
select relispopulated from gp_dist_random('pg_class') where relname = 'm_heap';
REFRESH MATERIALIZED VIEW m_heap;
SELECT * FROM m_heap;

-- AO case
CREATE MATERIALIZED VIEW m_ao with (appendonly=true) AS SELECT type, sum(amt) AS totamt FROM t_matview_ao GROUP BY type WITH NO DATA  distributed by(type);
SELECT relfrozenxid from pg_class where relname = 'm_ao';
SELECT * from m_ao;

-- set relfrozenxid to some valid value and validate REFRESH
-- MATERIALIZED resets it to invalid value. This is to validate
-- materialized views created before correctly setting relfrozenxid
-- for appendoptimized materialized view get fixed with REFRESH.
RESET role;
SET allow_system_table_mods to ON;
UPDATE pg_class SET relfrozenxid = '100'::xid where relname = 'm_ao';
RESET allow_system_table_mods;
SET ROLE matview_ao_role;

REFRESH MATERIALIZED VIEW m_ao;
SELECT relfrozenxid from pg_class where relname = 'm_ao';
SELECT * FROM m_ao;
REFRESH MATERIALIZED VIEW m_ao WITH NO DATA;
SELECT * FROM m_ao;
REFRESH MATERIALIZED VIEW m_ao;
SELECT * FROM m_ao;

-- CO case
CREATE MATERIALIZED VIEW m_aocs with (appendonly=true, orientation=column) AS SELECT type, sum(amt) AS totamt FROM t_matview_ao GROUP BY type WITH NO DATA  distributed by(type);
SELECT relfrozenxid from pg_class where relname = 'm_aocs';
SELECT * from m_aocs;

-- set relfrozenxid to some valid value and validate REFRESH
-- MATERIALIZED resets it to invalid value. This is to validate
-- materialized views created before correctly setting relfrozenxid
-- for appendoptimized materialized view get fixed with REFRESH.
RESET role;
SET allow_system_table_mods to ON;
UPDATE pg_class SET relfrozenxid = '100'::xid where relname = 'm_aocs';
RESET allow_system_table_mods;
SET ROLE matview_ao_role;

REFRESH MATERIALIZED VIEW m_aocs;
SELECT relfrozenxid from pg_class where relname = 'm_aocs';
SELECT * FROM m_aocs;
REFRESH MATERIALIZED VIEW m_aocs WITH NO DATA;
SELECT * FROM m_aocs;
REFRESH MATERIALIZED VIEW m_aocs;
SELECT * FROM m_aocs;

\dm m_heap
\dm m_ao
\dm m_aocs

RESET role;
