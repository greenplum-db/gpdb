-- this is a SQL file invoked by dispatch.sql, and should not be executed
-- directly in any test schedule;

BEGIN;
DECLARE c1 CURSOR FOR SELECT pg_sleep(300) FROM gp_dist_random('gp_id');

-- brutally exit this session without calling CLOSE, COMMIT or ABORT, verify all the
-- QEs of this session are terminated in gang_mgmt.sql
SET gp_debug_linger = 0;
SELECT gp_fault_inject(5, 0);
