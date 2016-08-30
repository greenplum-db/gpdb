-- this is a SQL file invoked by gang_mgmt.sql, and should not be executed
-- directly in any test schedule;

-- preserve current session id into a table
DROP TABLE IF EXISTS tmp_sess_id;
SELECT sess_id INTO tmp_sess_id FROM pg_stat_activity WHERE procpid = pg_backend_pid();

-- UDF that returns the set of backend session ids in one segment(duplications
-- included), will be invoked by gang_mgmt.sql to do verification
CREATE OR REPLACE FUNCTION get_segment_qe_session_ids() RETURNS SETOF int AS $$ SELECT pg_stat_get_backend_session_id(pg_stat_get_backend_idset) FROM pg_stat_get_backend_idset(); $$ LANGUAGE SQL;

BEGIN;
DECLARE c1 CURSOR FOR SELECT pg_sleep(300) FROM gp_dist_random('gp_id');

-- brutally exit this session without calling CLOSE, COMMIT or ABORT, verify all the
-- QEs of this session are terminated in gang_mgmt.sql
SET gp_debug_linger = 0;
SELECT gp_fault_inject(5, 0);
