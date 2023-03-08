-- Test to ensure that catalog-ao-aux autovacuum clears bloat from appendonly auxiliary tables.
-- create and switch to database
CREATE DATABASE av_ao_aux;
\c av_ao_aux

CREATE EXTENSION gp_inject_fault;

-- speed up test
ALTER SYSTEM SET autovacuum_naptime = 5;
ALTER SYSTEM SET gp_autovacuum_scope = catalog_ao_aux;
-- start_ignore
\! gpstop -u;
-- end_ignore


CREATE TABLE autovac_ao(i int, j int) USING ao_row distributed BY (i);
-- because we don't know the name of the aux tables ahead of time, we have to
-- jump through some hoops to identify them
WITH tableNameCTE AS (
    SELECT c.relname
    FROM pg_appendonly pa, pg_class c
    WHERE pa.visimaprelid = c.oid AND pa.relid = 'autovac_ao'::regclass
)
SELECT gp_inject_fault('auto_vac_worker_after_report_activity', 'skip', '', '', relname, 1, 1, 0, dbid)
from tableNameCTE, gp_segment_configuration where role = 'p' and content != -1;

-- populate test table, use a large value to ensure we're past autovacuum threshold
INSERT INTO autovac_ao SELECT j,j FROM generate_series(1, 10000000)j;

-- Deleting tuples from an AO table will insert rows into its visimap table.
-- Aborting the delete should mark these inserted rows as invisible, and hence
-- eligible for autovacuum.
begin; delete from autovac_ao where j % 9 = 3; abort;

-- wait for autovacuum to hit the auxiliary visimap table for autovac_ao,
-- triggering a fault
SELECT gp_wait_until_triggered_fault('auto_vac_worker_after_report_activity', 1, dbid)
from gp_segment_configuration where role = 'p' and content != -1;

-- clean up fault
WITH tableNameCTE AS (
    SELECT c.relname
    FROM pg_appendonly pa, pg_class c
    WHERE pa.visimaprelid = c.oid AND pa.relid = 'autovac_ao'::regclass
)
SELECT gp_inject_fault('auto_vac_worker_after_report_activity', 'reset', dbid)
from tableNameCTE, gp_segment_configuration where role = 'p' and content != -1;

ALTER SYSTEM RESET autovacuum_naptime;
ALTER SYSTEM RESET gp_autovacuum_scope;
-- start_ignore
\! gpstop -u;
-- end_ignore

-- clean up database
\c regression
DROP DATABASE av_ao_aux;
