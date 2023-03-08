-- Test to ensure that catalog-only autovacuum clears catalog bloat.
-- create and switch to database
CREATE DATABASE av_catalog;
\c av_catalog

-- speed up test
ALTER SYSTEM SET autovacuum_naptime = 5;
-- start_ignore
\! gpstop -u;
-- end_ignore

-- create extension and set faults for testing
CREATE EXTENSION gp_inject_fault;
SELECT gp_inject_fault('auto_vac_worker_after_report_activity', 'skip', '', '', 'pg_class', 1, 1, 0, 1);

-- generate bloat on catalog tables to trigger autovacuum
BEGIN;
CREATE TABLE bloat_tbl(i int, j int, k int, l int, m int, n int, o int, p int) DISTRIBUTED BY (i)
PARTITION BY RANGE (j) (START (0) END (1000) EVERY (1));
ABORT;

-- wait for autovacuum to hit pg_class, triggering a fault
SELECT gp_wait_until_triggered_fault('auto_vac_worker_after_report_activity', 1, 1);

-- clean up fault
SELECT gp_inject_fault('auto_vac_worker_after_report_activity', 'reset', 1);


ALTER SYSTEM RESET autovacuum_naptime;
-- start_ignore
\! gpstop -u;
-- end_ignore

-- clean up database
\c regression
DROP DATABASE av_catalog;
