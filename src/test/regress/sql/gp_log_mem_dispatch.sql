-- Test that gp_log_backend_memory_contexts dispatches and
-- handles responses correctly

-- show expected number of successful responses to logging a
-- known-good session with no target contentID
WITH sessionCTE AS (
    SELECT sess_id
    FROM pg_stat_activity
    WHERE application_name = 'pg_regress/gp_log_mem_dispatch'
)
SELECT gp_log_backend_memory_contexts(sess_id) FROM sessionCTE;

-- show warnings and 0 successful responses to logging a
-- known-bad session
WITH noSessionCTE AS (
    SELECT MIN(sess_id) + 1 as no_sess_id
    FROM pg_stat_activity
    WHERE sess_id + 1 NOT IN (SELECT sess_id FROM pg_stat_activity)
)
SELECT gp_log_backend_memory_contexts(no_sess_id) FROM noSessionCTE;

-- show expected number of successful responses to logging a
-- known-good session with a target contentID
WITH sessionCTE AS (
    SELECT sess_id
    FROM pg_stat_activity
    WHERE application_name = 'pg_regress/gp_log_mem_dispatch'
)
SELECT gp_log_backend_memory_contexts(sess_id, 0) FROM sessionCTE;

-- show warnings and 0 successful responses to logging a
-- known-bad contentID
WITH sessionCTE AS (
    SELECT sess_id
    FROM pg_stat_activity
    WHERE application_name = 'pg_regress/gp_log_mem_dispatch'
)
SELECT gp_log_backend_memory_contexts(sess_id, -3) FROM sessionCTE;
