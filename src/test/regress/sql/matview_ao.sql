CREATE ROLE matview_ao_role;
SET ROLE matview_ao_role;
CREATE TABLE t_matview_ao (id int NOT NULL PRIMARY KEY, type text NOT NULL, amt numeric NOT NULL);
INSERT INTO t_matview_ao VALUES
  (1, 'x', 2),
  (2, 'x', 3),
  (3, 'y', 5),
  (4, 'y', 7),
  (5, 'z', 11);

CREATE MATERIALIZED VIEW IF NOT EXISTS m_heap AS SELECT type, sum(amt) AS totamt FROM t_matview_ao GROUP BY type WITH NO DATA distributed by(type);
CREATE UNIQUE INDEX m_heap_index ON m_heap(type);
SELECT * from m_heap;
REFRESH MATERIALIZED VIEW CONCURRENTLY m_heap;
REFRESH MATERIALIZED VIEW m_heap;
SELECT * FROM m_heap;
REFRESH MATERIALIZED VIEW CONCURRENTLY m_heap;
SELECT * FROM m_heap;
REFRESH MATERIALIZED VIEW m_heap WITH NO DATA;
SELECT * FROM m_heap;
-- test WITH NO DATA is also dispatched to QEs
select relispopulated from gp_dist_random('pg_class') where relname = 'm_heap';
REFRESH MATERIALIZED VIEW m_heap;
SELECT * FROM m_heap;

CREATE MATERIALIZED VIEW IF NOT EXISTS m_ao with (appendonly=true) AS SELECT type, sum(amt) AS totamt FROM t_matview_ao GROUP BY type WITH NO DATA  distributed by(type);
SELECT * from m_ao;
REFRESH MATERIALIZED VIEW m_ao;
SELECT * FROM m_ao;
REFRESH MATERIALIZED VIEW m_ao WITH NO DATA;
SELECT * FROM m_ao;
REFRESH MATERIALIZED VIEW m_ao;
SELECT * FROM m_ao;


CREATE MATERIALIZED VIEW IF NOT EXISTS m_aocs with (appendonly=true, orientation=column) AS SELECT type, sum(amt) AS totamt FROM t_matview_ao GROUP BY type WITH NO DATA  distributed by(type);
SELECT * from m_aocs;
REFRESH MATERIALIZED VIEW m_aocs;
SELECT * FROM m_aocs;
REFRESH MATERIALIZED VIEW m_aocs WITH NO DATA;
SELECT * FROM m_aocs;
REFRESH MATERIALIZED VIEW m_aocs;
SELECT * FROM m_aocs;

\dm m_heap
\dm m_ao
\dm m_aocs
RESET ROLE;
DROP OWNED BY matview_ao_role CASCADE;
DROP ROLE matview_ao_role;
